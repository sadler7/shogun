/*
 * This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Authors: Chiyuan Zhang, Soeren Sonnenburg, Sanuj Sharma, Sergey Lisitsyn,
 *          Viktor Gal
 */

#include <vector>
#include <stack>

#include <shogun/multiclass/tree/ConditionalProbabilityTree.h>
#include <shogun/classifier/svm/OnlineLibLinear.h>
#include <shogun/mathematics/Math.h>

using namespace shogun;
using namespace std;

CMulticlassLabels* CConditionalProbabilityTree::apply_multiclass(CFeatures* data)
{
	if (data)
	{
		if (data->get_feature_class() != C_STREAMING_DENSE)
			error("Expected StreamingDenseFeatures");
		if (data->get_feature_type() != F_SHORTREAL)
			error("Expected float32_t feature type");

		set_features(dynamic_cast<CStreamingDenseFeatures<float32_t>* >(data));
	}

	vector<int32_t> predicts;

	m_feats->start_parser();
	while (m_feats->get_next_example())
	{
		predicts.push_back(apply_multiclass_example(m_feats->get_vector()));
		m_feats->release_example();
	}
	m_feats->end_parser();

	CMulticlassLabels *labels = new CMulticlassLabels(predicts.size());
	for (size_t i=0; i < predicts.size(); ++i)
		labels->set_int_label(i, predicts[i]);
	return labels;
}

int32_t CConditionalProbabilityTree::apply_multiclass_example(SGVector<float32_t> ex)
{
	compute_conditional_probabilities(ex);
	SGVector<float64_t> probs(m_leaves.size());
	for (map<int32_t,bnode_t*>::iterator it = m_leaves.begin(); it != m_leaves.end(); ++it)
	{
		probs[it->first] = accumulate_conditional_probability(it->second);
	}
	return CMath::arg_max(probs.vector, 1, probs.vlen);
}

void CConditionalProbabilityTree::compute_conditional_probabilities(SGVector<float32_t> ex)
{
	stack<bnode_t *> nodes;
	nodes.push((bnode_t*) m_root);

	while (!nodes.empty())
	{
		bnode_t *node = nodes.top();
		nodes.pop();
		if (node->left())
		{
			nodes.push(node->left());
			nodes.push(node->right());

			// don't calculate for leaf
			node->data.p_right = predict_node(ex, node);
		}
	}
}

float64_t CConditionalProbabilityTree::accumulate_conditional_probability(bnode_t *leaf)
{
	float64_t prob = 1;
	bnode_t *par = (bnode_t*) leaf->parent();
	while (par != NULL)
	{
		if (leaf == par->left())
			prob *= (1-par->data.p_right);
		else
			prob *= par->data.p_right;

		leaf = par;
		par = (bnode_t*) leaf->parent();
	}

	return prob;
}

bool CConditionalProbabilityTree::train_machine(CFeatures* data)
{
	if (data)
	{
		if (data->get_feature_class() != C_STREAMING_DENSE)
			error("Expected StreamingDenseFeatures");
		if (data->get_feature_type() != F_SHORTREAL)
			error("Expected float32_t features");
		set_features(dynamic_cast<CStreamingDenseFeatures<float32_t> *>(data));
	}
	else
	{
		if (!m_feats)
			error("No data features provided");
	}

	m_machines->reset_array();
	SG_UNREF(m_root);
	m_root = NULL;

	m_leaves.clear();

	m_feats->start_parser();
	for (int32_t ipass=0; ipass < m_num_passes; ++ipass)
	{
		while (m_feats->get_next_example())
		{
			train_example(m_feats, static_cast<int32_t>(m_feats->get_label()));
			m_feats->release_example();
		}

		if (ipass < m_num_passes-1)
			m_feats->reset_stream();
	}
	m_feats->end_parser();

	for (int32_t i=0; i < m_machines->get_num_elements(); ++i)
	{
		COnlineLibLinear *lll = dynamic_cast<COnlineLibLinear *>(m_machines->get_element(i));
		lll->stop_train();
		SG_UNREF(lll);
	}

	return true;
}

void CConditionalProbabilityTree::train_example(CStreamingDenseFeatures<float32_t>* ex, int32_t label)
{
	if (m_root == NULL)
	{
		m_root = new bnode_t();
		m_root->data.label = label;
		m_leaves.insert(make_pair(label, (bnode_t*) m_root));
		m_root->machine(create_machine(ex));
		return;
	}

	if (m_leaves.find(label) != m_leaves.end())
	{
		train_path(ex, m_leaves[label]);
	}
	else
	{
		bnode_t *node = (bnode_t*) m_root;
		while (node->left() != NULL)
		{
			// not a leaf
			bool is_left = which_subtree(node, ex->get_vector());
			float64_t node_label;
			if (is_left)
				node_label = 0;
			else
				node_label = 1;
			train_node(ex, node_label, node);

			if (is_left)
				node = node->left();
			else
				node = node->right();
		}

		m_leaves.erase(node->data.label);

		bnode_t *left_node = new bnode_t();
		left_node->data.label = node->data.label;
		node->data.label = -1;
		COnlineLibLinear *node_mch = dynamic_cast<COnlineLibLinear *>(m_machines->get_element(node->machine()));
		COnlineLibLinear *mch = new COnlineLibLinear(node_mch);
		SG_UNREF(node_mch);
		mch->start_train();
		m_machines->push_back(mch);
		left_node->machine(m_machines->get_num_elements()-1);
		m_leaves.insert(make_pair(left_node->data.label, left_node));
		node->left(left_node);

		bnode_t *right_node = new bnode_t();
		right_node->data.label = label;
		right_node->machine(create_machine(ex));
		m_leaves.insert(make_pair(label, right_node));
		node->right(right_node);
	}
}

void CConditionalProbabilityTree::train_path(CStreamingDenseFeatures<float32_t>* ex, bnode_t *node)
{
	float64_t node_label = 0;
	train_node(ex, node_label, node);

	bnode_t *par = (bnode_t*) node->parent();
	while (par != NULL)
	{
		if (par->left() == node)
			node_label = 0;
		else
			node_label = 1;

		train_node(ex, node_label, par);
		node = par;
		par = (bnode_t*) node->parent();
	}
}

void CConditionalProbabilityTree::train_node(CStreamingDenseFeatures<float32_t>* ex, float64_t label, bnode_t *node)
{
	require(node, "Node must not be NULL");
	COnlineLibLinear *mch = dynamic_cast<COnlineLibLinear *>(m_machines->get_element(node->machine()));
	require(mch, "Instance of {} could not be casted to COnlineLibLinear", node->get_name());
	mch->train_example(ex, label);
	SG_UNREF(mch);
}

float64_t CConditionalProbabilityTree::predict_node(SGVector<float32_t> ex, bnode_t *node)
{
	require(node, "Node must not be NULL");
	COnlineLibLinear *mch = dynamic_cast<COnlineLibLinear *>(m_machines->get_element(node->machine()));
	require(mch, "Instance of {} could not be casted to COnlineLibLinear", node->get_name());
	float64_t pred = mch->apply_one(ex.vector, ex.vlen);
	SG_UNREF(mch);
	// use sigmoid function to turn the decision value into valid probability
	return 1.0 / (1 + std::exp(-pred));
}

int32_t CConditionalProbabilityTree::create_machine(CStreamingDenseFeatures<float32_t>* ex)
{
	COnlineLibLinear *mch = new COnlineLibLinear();
	mch->start_train();
	mch->train_example(ex, 0);
	m_machines->push_back(mch);
	return m_machines->get_num_elements()-1;
}
