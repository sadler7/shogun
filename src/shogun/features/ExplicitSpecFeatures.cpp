/*
 * This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Authors: Soeren Sonnenburg, Vladislav Horbatiuk, Evgeniy Andreev, Viktor Gal,
 *          Evan Shelhamer, Evangelos Anagnostopoulos, Sanuj Sharma
 */

#include <shogun/features/ExplicitSpecFeatures.h>
#include <shogun/mathematics/Math.h>
#include <shogun/mathematics/linalg/LinalgNamespace.h>
#include <shogun/io/SGIO.h>

using namespace shogun;

CExplicitSpecFeatures::CExplicitSpecFeatures() :CDotFeatures()
{
	unstable(SOURCE_LOCATION);

	use_normalization = false;
	num_strings = 0;
	alphabet_size = 0;

	spec_size = 0;
	k_spectrum = NULL;
}


CExplicitSpecFeatures::CExplicitSpecFeatures(CStringFeatures<uint16_t>* str, bool normalize) : CDotFeatures()
{
	ASSERT(str)

	use_normalization=normalize;
	num_strings = str->get_num_vectors();
	spec_size = str->get_num_symbols();

	obtain_kmer_spectrum(str);

	SG_DEBUG("SPEC size={}, num_str={}", spec_size, num_strings)
}

CExplicitSpecFeatures::CExplicitSpecFeatures(const CExplicitSpecFeatures& orig) : CDotFeatures(orig),
	num_strings(orig.num_strings), alphabet_size(orig.alphabet_size), spec_size(orig.spec_size)
{
	k_spectrum= SG_MALLOC(float64_t*, num_strings);
	for (int32_t i=0; i<num_strings; i++)
		k_spectrum[i]=SGVector<float64_t>::clone_vector(k_spectrum[i], spec_size);
}

CExplicitSpecFeatures::~CExplicitSpecFeatures()
{
	delete_kmer_spectrum();
}

int32_t CExplicitSpecFeatures::get_dim_feature_space() const
{
	return spec_size;
}

float64_t CExplicitSpecFeatures::dot(int32_t vec_idx1, CDotFeatures* df, int32_t vec_idx2) const
{
	ASSERT(df)
	ASSERT(df->get_feature_type() == get_feature_type())
	ASSERT(df->get_feature_class() == get_feature_class())
	CExplicitSpecFeatures* sf = (CExplicitSpecFeatures*) df;

	ASSERT(vec_idx1 < num_strings)
	ASSERT(vec_idx2 < sf->num_strings)
	SGVector<float64_t> vec1(k_spectrum[vec_idx1], spec_size, false);
	SGVector<float64_t> vec2(sf->k_spectrum[vec_idx2], spec_size, false);

	return linalg::dot(vec1, vec2);
}

float64_t CExplicitSpecFeatures::dot(
    int32_t vec_idx1, const SGVector<float64_t>& vec2) const
{
	ASSERT(vec2.size() == spec_size)
	ASSERT(vec_idx1 < num_strings)
	SGVector<float64_t> vec1(k_spectrum[vec_idx1], spec_size, false);

	return linalg::dot(vec1, vec2);
}

void CExplicitSpecFeatures::add_to_dense_vec(float64_t alpha, int32_t vec_idx1, float64_t* vec2, int32_t vec2_len, bool abs_val) const
{
	ASSERT(vec2_len == spec_size)
	ASSERT(vec_idx1 < num_strings)
	float64_t* vec1=k_spectrum[vec_idx1];

	if (abs_val)
	{
		for (int32_t i=0; i<spec_size; i++)
			vec2[i]+=alpha*CMath::abs(vec1[i]);
	}
	else
	{
		for (int32_t i=0; i<spec_size; i++)
			vec2[i]+=alpha*vec1[i];
	}
}

void CExplicitSpecFeatures::obtain_kmer_spectrum(CStringFeatures<uint16_t>* str)
{
	k_spectrum= SG_MALLOC(float64_t*, num_strings);

	for (int32_t i=0; i<num_strings; i++)
	{
		k_spectrum[i]=SG_MALLOC(float64_t, spec_size);
		memset(k_spectrum[i], 0, sizeof(float64_t)*spec_size);

		int32_t len=0;
		bool free_fv;
		uint16_t* fv=str->get_feature_vector(i, len, free_fv);

		for (int32_t j=0; j<len; j++)
			k_spectrum[i][fv[j]]++;

		str->free_feature_vector(fv, i, free_fv);

		if (use_normalization)
		{
			float64_t n=0;
			for (int32_t j=0; j<spec_size; j++)
				n+=CMath::sq(k_spectrum[i][j]);

			n = std::sqrt(n);

			for (int32_t j=0; j<spec_size; j++)
				k_spectrum[i][j]/=n;
		}
	}
}

void CExplicitSpecFeatures::delete_kmer_spectrum()
{
	for (int32_t i=0; i<num_strings; i++)
		SG_FREE(k_spectrum[i]);

	SG_FREE(k_spectrum);
	k_spectrum=NULL;
}

CFeatures* CExplicitSpecFeatures::duplicate() const
{
	return new CExplicitSpecFeatures(*this);
}



void* CExplicitSpecFeatures::get_feature_iterator(int32_t vector_index)
{
	not_implemented(SOURCE_LOCATION);
	return NULL;
}

bool CExplicitSpecFeatures::get_next_feature(int32_t& index, float64_t& value, void* iterator)
{
	not_implemented(SOURCE_LOCATION);
	return false;
}

void CExplicitSpecFeatures::free_feature_iterator(void* iterator)
{
	not_implemented(SOURCE_LOCATION);
}

int32_t CExplicitSpecFeatures::get_nnz_features_for_vector(int32_t num) const
{
	not_implemented(SOURCE_LOCATION);
	return 0;
}

EFeatureType CExplicitSpecFeatures::get_feature_type() const
{
	return F_UNKNOWN;
}

EFeatureClass CExplicitSpecFeatures::get_feature_class() const
{
	return C_SPEC;
}

int32_t CExplicitSpecFeatures::get_num_vectors() const
{
	return num_strings;
}
