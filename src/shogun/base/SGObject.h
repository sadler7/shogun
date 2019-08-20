/*
 * This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Authors: Heiko Strathmann, Soeren Sonnenburg, Sergey Lisitsyn,
 *          Giovanni De Toni, Jacob Walker, Thoralf Klein, Chiyuan Zhang,
 *          Fernando Iglesias, Sanuj Sharma, Roman Votyakov, Yuyu Zhang,
 *          Viktor Gal, Bjoern Esser, Evangelos Anagnostopoulos, Pan Deng,
 *          Gil Hoben
 */

#ifndef __SGOBJECT_H__
#define __SGOBJECT_H__

#include <shogun/base/AnyParameter.h>
#include <shogun/base/Version.h>
#include <shogun/base/base_types.h>
#include <shogun/base/constraint.h>
#include <shogun/base/macros.h>
#include <shogun/base/some.h>
#include <shogun/base/unique.h>
#include <shogun/io/SGIO.h>
#include <shogun/lib/DataType.h>
#include <shogun/lib/RxCppHeader.h>
#include <shogun/lib/any.h>
#include <shogun/lib/common.h>
#include <shogun/lib/config.h>
#include <shogun/lib/exception/ShogunException.h>
#include <shogun/lib/tag.h>

#include <map>
#include <shogun/util/clone.h>
#include <unordered_map>
#include <utility>
#include <vector>

/** \namespace shogun
 * @brief all of classes and functions are contained in the shogun namespace
 */
namespace shogun
{
class RefCount;
class Parallel;
class Parameter;
class ParameterObserverInterface;
class ObservedValue;
class ParameterObserver;
class CDynamicObjectArray;

template <class T>
class ObservedValueTemplated;
	namespace io
	{
		class CDeserializer;
		class CSerializer;
	}

#ifndef SWIG
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace sgo_details
{
template <typename T1, typename T2>
bool dispatch_array_type(const CSGObject* obj, const std::string& name,
		T2&& lambda);
}
#endif // DOXYGEN_SHOULD_SKIP_THIS
#endif // SWIG

template <class T, class K> class CMap;

struct TParameter;
template <class T> class DynArray;

using stringToEnumMapType = std::unordered_map<std::string, std::unordered_map<std::string, machine_int_t>>;

/*******************************************************************************
 * define reference counter macros
 ******************************************************************************/

#define SG_REF(x) { if (x) (x)->ref(); }
#define SG_UNREF(x) { if (x) { if ((x)->unref()==0) (x)=NULL; } }
#define SG_UNREF_NO_NULL(x) { if (x) { (x)->unref(); } }

/*******************************************************************************
 * Macros for registering parameter properties
 ******************************************************************************/

#define SG_ADD3(param, name, description)                                      \
	{                                                                          \
		auto pprop =                                                           \
		    AnyParameterProperties(description, this->m_default_mask);         \
		this->m_parameters->add(param, name, description);                     \
		this->watch_param(name, param, pprop);                                 \
	}

#define SG_ADD4(param, name, description, param_properties)                    \
	{                                                                          \
		static_assert(                                                         \
		    !static_cast<bool>((param_properties)&ParameterProperties::AUTO),  \
		    "Expected a lambda when passing param with "                       \
		    "ParameterProperty::AUTO");                                        \
		auto mask = param_properties;                                          \
		mask |= this->m_default_mask;                                          \
		AnyParameterProperties pprop =                                         \
		    AnyParameterProperties(description, mask);                         \
		this->m_parameters->add(param, name, description);                     \
		this->watch_param(name, param, pprop);                                 \
		if (pprop.has_property(ParameterProperties::HYPER))                    \
			this->m_model_selection_parameters->add(param, name, description); \
		if (pprop.has_property(ParameterProperties::GRADIENT))                 \
			this->m_gradient_parameters->add(param, name, description);        \
	}

#define SG_ADD5(                                                               \
    param, name, description, param_properties, auto_or_constraint)            \
	{                                                                          \
		auto mask = param_properties;                                          \
		mask |= this->m_default_mask;                                          \
		AnyParameterProperties pprop =                                         \
		    AnyParameterProperties(description, mask);                         \
		this->m_parameters->add(param, name, description);                     \
		this->watch_param(name, param, auto_or_constraint, pprop);             \
		if (pprop.has_property(ParameterProperties::HYPER))                    \
			this->m_model_selection_parameters->add(param, name, description); \
		if (pprop.has_property(ParameterProperties::GRADIENT))                 \
			this->m_gradient_parameters->add(param, name, description);        \
	}

#define SG_ADD(...) VARARG(SG_ADD, __VA_ARGS__)

/*******************************************************************************
 * End of macros for registering parameter properties
 ******************************************************************************/

/** @brief Class SGObject is the base class of all shogun objects.
 *
 * Apart from dealing with reference counting that is used to manage shogung
 * objects in memory (erase unused object, avoid cleaning objects when they are
 * still in use), it provides interfaces for:
 *
 * -# parallel - to determine the number of used CPUs for a method (cf. Parallel)
 * -# io - to output messages and general i/o (cf. IO)
 * -# version - to provide version information of the shogun version used (cf. Version)
 *
 * All objects can be cloned and compared (deep copy, recursively)
 */
class CSGObject
{
public:
	/** Definition of observed subject */
	typedef rxcpp::subjects::subject<Some<ObservedValue>> SGSubject;
	/** Definition of observable */
	typedef rxcpp::observable<Some<ObservedValue>,
		                      rxcpp::dynamic_observable<Some<ObservedValue>>>
		SGObservable;
	/** Definition of subscriber */
	typedef rxcpp::subscriber<
		Some<ObservedValue>,
		rxcpp::observer<Some<ObservedValue>, void, void, void, void>>
		SGSubscriber;

	/** default constructor */
	CSGObject();

	/** copy constructor */
	CSGObject(const CSGObject& orig);

	/** destructor */
	virtual ~CSGObject();

	/** increase reference counter
	 *
	 * @return reference count
	 */
	int32_t ref();

	/** display reference counter
	 *
	 * @return reference count
	 */
	int32_t ref_count();

	/** decrement reference counter and deallocate object if refcount is zero
	 * before or after decrementing it
	 *
	 * @return reference count
	 */
	int32_t unref();

	/** A shallow copy.
	 * All the SGObject instance variables will be simply assigned and SG_REF-ed.
	 */
	virtual CSGObject *shallow_copy() const;

	/** A deep copy.
	 * All the instance variables will also be copied.
	 */
	virtual CSGObject *deep_copy() const;

	/** Returns the name of the SGSerializable instance.  It MUST BE
	 *  the CLASS NAME without the prefixed `C'.
	 *
	 *  @return name of the SGSerializable
	 */
	virtual const char* get_name() const = 0;

	/** If the SGSerializable is a class template then TRUE will be
	 *  returned and GENERIC is set to the type of the generic.
	 *
	 *  @param generic set to the type of the generic if returning
	 *                 TRUE
	 *
	 *  @return TRUE if a class template.
	 */
	virtual bool is_generic(EPrimitiveType* generic) const;

	/** set generic type to T
	 */
	template<class T> void set_generic();

	/** Returns generic type.
	 * @return generic type of this object
	 */
	EPrimitiveType get_generic() const
	{
		return m_generic;
	}

	/** unset generic type
	 *
	 * this has to be called in classes specializing a template class
	 */
	void unset_generic();

	virtual size_t hash() const;

	/** Save this object to file.
	 *
	 * @param ser where to save the object;
	 * @return TRUE if done, otherwise FALSE
	 */
	virtual bool serialize(io::CSerializer* ser);

	/** Load this object from file.  If it will fail (returning FALSE)
	 *  then this object will contain inconsistent data and should not
	 *  be used!
	 *
	 *  @param deser where to load from
	 *
	 *  @return TRUE if done, otherwise FALSE
	 */
	virtual bool deserialize(io::CDeserializer* deser);

	/** get the parallel object
	 *
	 * @return parallel object
	 */
	Parallel* get_global_parallel();

	/**
	 * Return the description of a registered parameter given its name
	 * @param name parameter's name
	 * @return description of the parameter as a string
	 */
	std::string get_description(const std::string& name) const;

	/** prints all parameter registered for model selection and their type */
	void print_modsel_params();

	/** Builds a dictionary of all parameters in SGObject as well of those
	 *  of SGObjects that are parameters of this object. Dictionary maps
	 *  parameters to the objects that own them.
	 *
	 * @param dict dictionary of parameters to be built.
	 */
	void build_gradient_parameter_dictionary(CMap<TParameter*, CSGObject*>* dict);

	/** Checks if object has a class parameter identified by a name.
	 *
	 * @param name name of the parameter
	 * @return true if the parameter exists with the input name
	 */
	bool has(const std::string& name) const;

	/** Checks if object has a class parameter identified by a Tag.
	 *
	 * @param tag tag of the parameter containing name and type information
	 * @return true if the parameter exists with the input tag
	 */
	template <typename T>
	bool has(const Tag<T>& tag) const
	{
		return has<T>(tag.name());
	}

	/** Checks if a type exists for a class parameter identified by a name.
	 *
	 * @param name name of the parameter
	 * @return true if the parameter exists with the input name and type
	 */
	template <typename T, typename U = void>
	bool has(const std::string& name) const noexcept(true)
	{
		BaseTag tag(name);
		if (!has_parameter(tag))
			return false;
		const Any value = get_parameter(tag).get_value();
		return value.has_type<T>();
	}

#ifndef SWIG
	/** Setter for a class parameter, identified by a Tag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name and type information of parameter
	 * @param value value of the parameter
	 */
	template <typename T,
		      typename std::enable_if_t<!is_string<T>::value>* = nullptr>
	void put(const Tag<T>& _tag, const T& value) noexcept(false)
	{
		if (has_parameter(_tag))
		{
			auto parameter_value = get_parameter(_tag).get_value();
			if (!parameter_value.cloneable())
			{
				error(
					"Cannot put parameter {}::{}.", get_name(),
					_tag.name().c_str());
			}
			try
			{
				any_cast<T>(parameter_value);
			}
			catch (const TypeMismatchException& exc)
			{
				error(
					"Cannot put parameter {}::{} of type {}, incompatible "
					"provided type {}.",
					get_name(), _tag.name().c_str(), exc.actual().c_str(),
					exc.expected().c_str());
			}
			ref_value(value);
			update_parameter(_tag, make_any(value));
		}
		else
		{
			error(
				"Parameter {}::{} does not exist.", get_name(),
				_tag.name().c_str());
		}
	}

	/** Setter for a class parameter that has values of type string,
	 * identified by a Tag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name and type information of parameter
	 * @param value value of the parameter
	 */
	template <typename T,
		      typename std::enable_if_t<is_string<T>::value>* = nullptr>
	void put(const Tag<T>& _tag, const T& value) noexcept(false)
	{
	    std::string val_string(value);

		if (m_string_to_enum_map.find(_tag.name()) == m_string_to_enum_map.end())
		{
			error(
					"There are no options for parameter {}::{}", get_name(),
					_tag.name().c_str());
		}

		auto string_to_enum = m_string_to_enum_map[_tag.name()];

		if (string_to_enum.find(val_string) == string_to_enum.end())
		{
			error(
					"Illegal option '{}' for parameter {}::{}",
                    val_string.c_str(), get_name(), _tag.name().c_str());
		}

		machine_int_t enum_value = string_to_enum[val_string];

		put(Tag<machine_int_t>(_tag.name()), enum_value);
	}
#endif

	/** Typed setter for an object class parameter of a Shogun base class type,
	 * identified by a name.
	 *
	 * @param name name of the parameter
	 * @param value value of the parameter
	 */
	template <class T,
		      class X = typename std::enable_if<is_sg_base<T>::value>::type,
		      class Z = void>
	void put(const std::string& name, T* value)
	{
		put(Tag<T*>(name), value);
	}

	/** Typed appender for an object class parameter of a Shogun base class
	* type, identified by a name.
	*
	* @param name name of the parameter
	* @param value value of the parameter
	*/
	template <class T,
		      class X = typename std::enable_if<is_sg_base<T>::value>::type>
	void add(const std::string& name, T* value)
	{
		require(
			value, "Cannot add to {}::{}, no object provided.", get_name(),
			name.c_str());

		auto push_back_lambda = [&value](auto& array) {
			array.push_back(value);
		};
		if (sgo_details::dispatch_array_type<T>(this, name, push_back_lambda))
			return;

		error(
		    "Cannot add object {} to array parameter {}::{} of type {}.",
		    value->get_name(), get_name(), name.c_str(),
			demangled_type<T>().c_str());
	}

#ifndef SWIG
	/** Typed array getter for an object array class parameter of a Shogun base
	* class
	* type, identified by a name and an index.
	*
	* Returns nullptr if parameter of desired type does not exist.
	*
	* @param name name of the parameter array
	* @param index index of the element in the array
	* @return desired element
	*/
	template <class T,
		      class X = typename std::enable_if<is_sg_base<T>::value>::type>
	T* get(const std::string& name, index_t index, std::nothrow_t) const
	{
		CSGObject* result = nullptr;

		auto get_lambda = [&index, &result](auto& array) {
			result = array.at(index);
		};
		if (sgo_details::dispatch_array_type<T>(this, name, get_lambda))
		{
			ASSERT(result);
			// guard against mixed types in the array
			return result->as<T>();
		}

		return nullptr;
	}

	template <class T,
		      class X = typename std::enable_if<is_sg_base<T>::value>::type>
	T* get(const std::string& name, index_t index) const
	{
		auto result = this->get<T>(name, index, std::nothrow);
		if (!result)
		{
			error(
				"Could not get array parameter {}::{}[{}] of type {}",
				get_name(), name.c_str(), index, demangled_type<T>().c_str());
		}
		return result;
	};
#endif

	/** Untyped getter for an object class parameter, identified by a name.
	 * Will attempt to get specified object of appropriate internal type.
	 * If this is not possible it will raise a ShogunException.
	 *
	 * @param name name of the parameter
	 * @return object parameter
	 */
	CSGObject* get(const std::string& name) const noexcept(false);

	/** Untyped getter for an object class parameter, identified by a name.
	 * Does not throw an error if class parameter object cannot be casted
	 * to appropriate internal type.
	 *
	 * @param name name of the parameter
	 * @return object parameter
	 */
	CSGObject* get(const std::string& name, std::nothrow_t) const noexcept;

	/** Untyped getter for an object array class parameter, identified by a name
	 * and an index.
	 * Will attempt to get specified object of appropriate internal type.
	 * If this is not possible it will raise a ShogunException.
	 *
	 * @param name name of the parameter
	 * @index index of the parameter
	 * @return object parameter
	 */
	CSGObject* get(const std::string& name, index_t index) const;

#ifndef SWIG
	/** Typed setter for an object class parameter of a Shogun base class type,
	 * identified by a name.
	 *
	 * @param name name of the parameter
	 * @param value value of the parameter
	 */
	template <class T, class = typename std::enable_if_t<is_sg_base<T>::value>>
	void put(const std::string& name, Some<T> value)
	{
		put(name, value.get());
	}

	/** Typed appender for an object class parameter of a Shogun base class
	* type,
	* identified by a name.
	*
	* @param name name of the parameter
	* @param value value of the parameter
	*/
	template <class T, class = typename std::enable_if_t<is_sg_base<T>::value>>
	void add(const std::string& name, Some<T> value)
	{
		add(name, value.get());
	}
#endif // SWIG

	/** Typed setter for a non-object class parameter, identified by a name.
	 *
	 * @param name name of the parameter
	 * @param value value of the parameter along with type information
	 */
	template <typename T,
		      typename T2 = typename std::enable_if<
		          !std::is_base_of<
		              CSGObject, typename std::remove_pointer<T>::type>::value,
		          T>::type>
	void put(const std::string& name, T value)
	{
		put(Tag<T>(name), value);
	}

#ifndef SWIG
	/** Getter for a class parameter, identified by a Tag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name and type information of parameter
	 * @return value of the parameter identified by the input tag
	 */
	template <typename T, typename std::enable_if_t<!is_string<T>::value>* = nullptr>
	T get(const Tag<T>& _tag) const noexcept(false)
	{
		const Any value = get_parameter(_tag).get_value();
		try
		{
			return any_cast<T>(value);
		}
		catch (const TypeMismatchException& exc)
		{
			error(
				"Cannot get parameter {}::{} of type {}, incompatible "
				"requested type {}.",
				get_name(), _tag.name().c_str(), exc.actual().c_str(),
				exc.expected().c_str());
		}
		// we won't be there
		return any_cast<T>(value);
	}

	template <typename T, typename std::enable_if_t<is_string<T>::value>* = nullptr>
	T get(const Tag<T>& _tag) const noexcept(false)
	{
		if (m_string_to_enum_map.find(_tag.name()) == m_string_to_enum_map.end())
		{
			const Any value = get_parameter(_tag).get_value();
			try
			{
				return any_cast<T>(value);
			}
			catch (const TypeMismatchException& exc)
			{
				error(
					"Cannot get parameter {}::{} of type {}, incompatible "
					"requested type {} or there are no options for parameter "
					"{}::{}.",
					get_name(), _tag.name().c_str(), exc.actual().c_str(),
					exc.expected().c_str(), get_name(), _tag.name().c_str());
			}
		}
		return string_enum_reverse_lookup(_tag.name(), get<machine_int_t>(_tag.name()));
	}
#endif

	/** Getter for a class parameter, identified by a name.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param name name of the parameter
	 * @return value of the parameter corresponding to the input name and type
	 */
	template <typename T, typename U = void>
	T get(const std::string& name) const noexcept(false)
	{
		Tag<T> tag(name);
		return get(tag);
	}

	/**
	 *
	 * @param name name of the parameter
	 * @return value of the parameter corresponding to the input name and type
	 */
	void run(const std::string& name) const noexcept(false)
	{
		Tag<bool> tag(name);
		auto param = get_function(tag);
		if (!any_cast<bool>(param.get_value()))
		{
			error("Failed to run function {}::{}", get_name(), name.c_str());
		}
	}

	/** Returns string representation of the object that contains
	 * its name and parameters.
	 *
	 */
	virtual std::string to_string() const;

#ifndef SWIG // SWIG should skip this part
	/** Returns map of parameter names and AnyParameter pairs
	 * of the object.
	 *
	 */
	std::map<std::string, std::shared_ptr<const AnyParameter>> get_params() const;

	/** Calls a function on every parameter of type T with the name of the
	 * parameter and the parameter itself as arguments
	 *
	 * @param operation the function to be called
	 */
	template <typename T>
	void for_each_param_of_type(
		std::function<void(const std::string&, T*)> operation);
#endif
	/** Specializes a provided object to the specified type.
	 * Throws exception if the object cannot be specialized.
	 *
	 * @param sgo object of CSGObject base type
	 * @return The requested type
	 */
	template<class T> static T* as(CSGObject* sgo)
	{
		require(sgo, "No object provided!");
		return sgo->as<T>();
	}

	/** Specializes the object to the specified type.
	 * Throws exception if the object cannot be specialized.
	 *
	 * @return The requested type
	 */
	template<class T> T* as()
	{
		auto c = dynamic_cast<T*>(this);
		if (c)
			return c;

		error(
			"Object of type {} cannot be converted to type {}.",
			this->get_name(),
			demangled_type<T>().c_str());
		return nullptr;
	}
#ifndef SWIG
	/**
	  * Get parameters observable
	  * @return RxCpp observable
	  */
	SGObservable* get_parameters_observable()
	{
		return m_observable_params;
	};
#endif

	/** Subscribe a parameter observer to watch over params */
	void subscribe(ParameterObserver* obs);

	/**
	 * Detach an observer from the current SGObject.
	 * @param subscription_index the index obtained by calling the subscribe
	 * procedure
	 */
	void unsubscribe(ParameterObserver* obs);

	/** Print to stdout a list of observable parameters */
	std::vector<std::string> observable_names();

	/** Get string to enum mapping */
	stringToEnumMapType get_string_to_enum_map() const
	{
		return m_string_to_enum_map;
	}

public:
	/** Can (optionally) be overridden to pre-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::LOAD_SERIALIZABLE_PRE
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void load_serializable_pre() noexcept(false);

	/** Can (optionally) be overridden to post-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::LOAD_SERIALIZABLE_POST
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void load_serializable_post() noexcept(false);

	/** Can (optionally) be overridden to pre-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::SAVE_SERIALIZABLE_PRE
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void save_serializable_pre() noexcept(false);

	/** Can (optionally) be overridden to post-initialize some member
	 *  variables which are not PARAMETER::ADD'ed.  Make sure that at
	 *  first the overridden method BASE_CLASS::SAVE_SERIALIZABLE_POST
	 *  is called.
	 *
	 *  @exception ShogunException will be thrown if an error occurs.
	 */
	virtual void save_serializable_post() noexcept(false);

	inline bool get_load_serializable_pre() const
	{
		return m_load_pre_called;
	}

	inline bool get_load_serializable_post() const
	{
		return m_load_post_called;
	}

	inline bool get_save_serializable_pre() const
	{
		return m_save_pre_called;
	}

	inline bool get_save_serializable_post() const
	{
		return m_save_post_called;
	}

protected:
	/** Registers a class parameter which is identified by a tag.
	 * This enables the parameter to be modified by put() and retrieved by
	 * get().
	 * Parameters can be registered in the constructor of the class.
	 *
	 * @param _tag name and type information of parameter
	 * @param value value of the parameter
	 */
	template <typename T>
	void register_param(Tag<T>& _tag, const T& value)
	{
		create_parameter(_tag, AnyParameter(make_any(value)));
	}

	/** Registers a class parameter which is identified by a name.
	 * This enables the parameter to be modified by put() and retrieved by
	 * get().
	 * Parameters can be registered in the constructor of the class.
	 *
	 * @param name name of the parameter
	 * @param value value of the parameter along with type information
	 */
	template <typename T>
	void register_param(const std::string& name, const T& value)
	{
		BaseTag tag(name);
		create_parameter(tag, AnyParameter(make_any(value)));
	}

	/** Puts a pointer to some parameter into the parameter map.
	 *
	 * @param name name of the parameter
	 * @param value pointer to the parameter value
	 * @param properties properties of the parameter (e.g. if model selection is supported)
	 */
	template <typename T>
	void watch_param(
		const std::string& name, T* value,
		AnyParameterProperties properties = AnyParameterProperties())
	{
		BaseTag tag(name);
		create_parameter(tag, AnyParameter(make_any_ref(value), properties));
	}

	/** Puts a pointer to some parameter into the parameter map.
	 * The parameter is expected to be initialised at runtime
	 * using the provided lambda.
	 *
	 * @param name name of the parameter
	 * @param value pointer to the parameter value
	 * @param auto_init AutoInit object to initialise the value of the parameter
	 * @param properties properties of the parameter (e.g. if model selection is supported)
	 */
	template <typename T>
	void watch_param(
			const std::string& name, T* value,
			std::shared_ptr<params::AutoInit> auto_init,
			AnyParameterProperties properties)
	{
		require(
				properties.has_property(ParameterProperties::AUTO),
				"Expected param to have ParameterProperty::AUTO");
		BaseTag tag(name);
		create_parameter(
				tag,
				AnyParameter(
						make_any_ref(value), properties, std::move(auto_init)));
	}

#ifndef SWIG
	/** Puts a pointer to some parameter into the parameter map.
	 * The parameter is the constrained to the rules from the
	 * Composer.
	 *
	 * @param name name of the parameter
	 * @param value pointer to the parameter value
	 * @param constrain_function a function pointer that checks if
	 * a given value is within the provided constraints
	 * @param properties properties of the parameter (e.g. if model
	 * selection is supported)
	 */
	template <typename T1, typename ...Args>
	void watch_param(
			const std::string& name, T1* value,
			Constraint<Args...>&& constrain_function,
			AnyParameterProperties properties)
	{
		require(
				properties.has_property(ParameterProperties::CONSTRAIN),
				"Expected param to have ParameterProperty::CONSTRAIN");
		BaseTag tag(name);
		create_parameter(
				tag, AnyParameter(
						make_any_ref(value), properties,
						[constrain_function](const auto& val) {
							std::string result;
							auto casted_val = any_cast<T1>(val);
							constrain_function.run(casted_val, result);
							return result;
						}));
	}
#endif

	/** Puts a pointer to some parameter array into the parameter map.
	 *
	 * @param name name of the parameter array
	 * @param value pointer to the first element of the parameter array
	 * @param len number of elements in the array
	 * @param properties properties of the parameter (e.g. if model selection is
	 * supported)
	 */
	template <typename T, typename S>
	void watch_param(
		const std::string& name, T** value, S* len,
		AnyParameterProperties properties = AnyParameterProperties())
	{
		BaseTag tag(name);
		create_parameter(
			tag, AnyParameter(make_any_ref(value, len), properties));
	}

	/** Puts a pointer to some 2d parameter array (i.e. a matrix) into the
	 * parameter map.
	 *
	 * @param name name of the parameter array
	 * @param value pointer to the first element of the parameter array
	 * @param rows number of rows in the array
	 * @param cols number of columns in the array
	 * @param properties properties of the parameter (e.g. if model selection is
	 * supported)
	 */
	template <typename T, typename S>
	void watch_param(
		const std::string& name, T** value, S* rows, S* cols,
		AnyParameterProperties properties = AnyParameterProperties())
	{
		BaseTag tag(name);
		create_parameter(
			tag, AnyParameter(make_any_ref(value, rows, cols), properties));
	}

#ifndef SWIG
	/** Puts a pointer to a (lazily evaluated) const function into the parameter map.
	 *
	 * @param name name of the parameter
	 * @param method pointer to the method
	 */
	template <typename T, typename S>
	void watch_method(const std::string& name, T (S::*method)() const)
	{
		BaseTag tag(name);
		AnyParameterProperties properties(
			"Dynamic parameter",
			ParameterProperties::READONLY);
		std::function<T()> bind_method =
			std::bind(method, dynamic_cast<const S*>(this));
		create_parameter(tag, AnyParameter(make_any(bind_method), properties));
	}

	/** Puts a pointer to a (lazily evaluated) function into the parameter map.
	 * The bound function can modify the class members and can only be
	 * invoked using CSGObject::run(name).
	 *
	 * @param name name of the parameter
	 * @param method pointer to the method
	 */
	template <typename T, typename S>
	void watch_method(const std::string& name, T (S::*method)())
	{
		BaseTag tag(name);
		AnyParameterProperties properties(
			"Non-const function",
			ParameterProperties::RUNFUNCTION | ParameterProperties::READONLY);
		std::function<T()> bind_method =
			std::bind(method, dynamic_cast<S*>(this));
		create_parameter(tag, AnyParameter(make_any(bind_method), properties));
	}

	/** Adds a callback function to a parameter identified by its name
	 *
	 * @param name name of the parameter
	 * @param method pointer to the function
	 */
	void add_callback_function(
		const std::string& name, std::function<void()> method);
#endif

public:
	/** Updates the hash of current parameter combination */
	virtual void update_parameter_hash() const;

	/**
	 * @return whether parameter combination has changed since last update
	 */
	virtual bool parameter_hash_changed() const;

	/** Deep comparison of two objects.
	 *
	 * @param other object to compare with
	 * @return true if all parameters are equal
	 */
	virtual bool equals(const CSGObject* other) const;

	/** Creates a clone of the current object. This is done via recursively
	 * traversing all parameters, which corresponds to a deep copy.
	 *
	 * @return Cloned object
	 */
	virtual CSGObject* clone(ParameterProperties pp = ParameterProperties::ALL) const;

	/**
	 * Looks up the option name of a parameter given the enum value.
	 *
	 * @param param the parameter name
	 * @param value the enum value to query
	 * @return the string representation of the enum (option name)
	 */
	std::string string_enum_reverse_lookup(
			const std::string& param, machine_int_t value) const;

	void visit_parameter(const BaseTag& _tag, AnyVisitor* v) const;
protected:
	/** Returns an empty instance of own type.
	 *
	 * When inheriting from CSGObject from outside the main source tree (i.e.
	 * customized classes, or in a unit test), then this method has to be
	 * overloaded manually to return an empty instance.
	 * Shogun can only instantiate empty class instances from its source tree.
	 *
	 * @return empty instance of own type
	 */
	virtual CSGObject* create_empty() const;

	/** Initialises all parameters with ParameterProperties::AUTO flag */
	void init_auto_params();

	/**
	 * Set a default mask value that is added to any other mask
	 * passed to watch_param. The default mask is by default
	 * ParameterProperties::NONE.
	 * @param mask the default mask
	 */
	void set_default_mask(ParameterProperties mask) noexcept
	{
		m_default_mask = mask;
	}

	/** The default mask for all registered parameters */
	ParameterProperties m_default_mask = ParameterProperties::NONE;

private:
	void init();

	/** Overloaded helper to increase reference counter */
	static void ref_value(CSGObject* value)
	{
		SG_REF(value);
	}

	/** Overloaded helper to increase reference counter
	 * Here a no-op for non CSGobject pointer parameters */
	template <typename T,
		      std::enable_if_t<
		          !std::is_base_of<
		              CSGObject, typename std::remove_pointer<T>::type>::value,
		          T>* = nullptr>
	static void ref_value(T value)
	{
	}

	/** Checks if object has a parameter identified by a BaseTag.
	 * This only checks for name and not type information.
	 * See its usage in has() and has<T>().
	 *
	 * @param _tag name information of parameter
	 * @return true if the parameter exists with the input tag
	 */
	bool has_parameter(const BaseTag& _tag) const;

	/** Creates a parameter identified by a BaseTag.
	 *
	 * @param _tag name information of parameter
	 * @param parameter parameter to be created
	 */
	void create_parameter(const BaseTag& _tag, const AnyParameter& parameter);

	/** Updates a parameter identified by a BaseTag.
	 *
	 * @param _tag name information of parameter
	 * @param value new value of parameter
	 */
	void update_parameter(const BaseTag& _tag, const Any& value);

	/** Getter for a class parameter, identified by a BaseTag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name information of parameter
	 * @return value of the parameter identified by the input tag
	 */
	AnyParameter get_parameter(const BaseTag& _tag) const;

	/** Getter for a class function, identified by a BaseTag.
	 * Throws an exception if the class does not have such a parameter.
	 *
	 * @param _tag name information of parameter
	 * @return value of the parameter identified by the input tag
	 */
	AnyParameter get_function(const BaseTag& _tag) const;

	class Self;
	Unique<Self> self;

	class ParameterObserverList;
	Unique<ParameterObserverList> param_obs_list;

protected:
	/**
	 * Return total subscriptions
	 * @return total number of subscriptions
	 */
	index_t get_num_subscriptions() const
	{
		return static_cast<index_t>(m_subscriptions.size());
	}

	/**
	 * Observe a parameter value and emit them to observer.
	 * @param value Observed parameter's value
	 */
	void observe(const Some<ObservedValue> value) const;

	/**
	 * Observe a parameter value, given a pointer.
	 * @param value Observed parameter's value
	 */
	void observe(ObservedValue* value) const;

	/**
	 * Observe a parameter value given custom properties for the Any.
	 * If no observer is attached this command will do nothing.
	 * @tparam T type of the parameter
	 * @param step time step
	 * @param name name of the observed value
	 * @param value observed value
	 * @param properties AnyParameterProperties used to register the observed
	 * value.
	 */
	template <class T>
	void observe(
		const int64_t step, const std::string& name, const T& value,
		const AnyParameterProperties properties) const
	{
		// If there are no observers attached, do not create/emit anything.
		if (get_num_subscriptions() == 0)
			return;

		auto obs = new ObservedValueTemplated<T>(
			step, name, static_cast<T>(clone_utils::clone(value)), properties);
		this->observe(obs);
	}

	/**
	 * Observe a parameter value given some information.
	 * If no observer is attached this command will do nothing.
	 * @tparam T value of the parameter
	 * @param step step
	 * @param name name of the observed value
	 * @param description description
	 * @param value observed value
	 */
	template <class T>
	void observe(
		const int64_t step, const std::string& name,
		const std::string& description, const T value) const
	{
		this->observe(
			step, name, value,
			AnyParameterProperties(description, ParameterProperties::READONLY));
	}

	/**
	 * Observe a registered tag.
	 * If no observer is attached this command will do nothing.
	 * @tparam T type of the tag
	 * @param step step
	 * @param name tag's name
	 */
	template <class T>
	void observe(const int64_t step, const std::string& name) const
	{
		auto param = this->get_parameter(BaseTag(name));
		auto cloned = any_cast<T>(param.get_value());
		this->observe(
			step, name, static_cast<T>(clone_utils::clone(cloned)),
			param.get_properties());
	}

	/**
	 * Register which params this object can emit.
	 * @param name the param name
	 * @param type the param type
	 * @param description a user oriented description
	 */
	void register_observable(
		const std::string& name, const std::string& description);

/**
 * Get the current step for the observed values.
 */
#ifndef SWIG
		SG_FORCED_INLINE int64_t get_step() const
		{
			int64_t step = -1;
			Tag<int64_t> tag("current_iteration");
			if (has(tag))
			{
				step = get(tag);
			}
			return step;
		}
#endif

		/** mapping from strings to enum for SWIG interface */
		stringToEnumMapType m_string_to_enum_map;

	public:
		/** parameters */
		Parameter* m_parameters;

		/** model selection parameters */
		Parameter* m_model_selection_parameters;

		/** parameters wrt which we can compute gradients */
		Parameter* m_gradient_parameters;

		/** Hash of parameter values*/
		mutable size_t m_hash;

	private:
		EPrimitiveType m_generic;
		bool m_load_pre_called;
		bool m_load_post_called;
		bool m_save_pre_called;
		bool m_save_post_called;

		RefCount* m_refcount;

		/** Subject used to create the params observer */
		SGSubject* m_subject_params;

		/** Parameter Observable */
		SGObservable* m_observable_params;

		/** Subscriber used to call onNext, onComplete etc.*/
		SGSubscriber* m_subscriber_params;

		/** List of subscription for this SGObject */
		std::map<int64_t, rxcpp::subscription> m_subscriptions;
		int64_t m_next_subscription_index;
	};

template <class T>
T* make_clone(T* orig, ParameterProperties pp = ParameterProperties::ALL)
{
	require(orig, "No object provided.");
	auto clone = orig->clone(pp);
	ASSERT(clone);
	return static_cast<T*>(clone);
}

template <class T>
const T* make_clone(const T* orig, ParameterProperties pp = ParameterProperties::ALL)
{
	require(orig, "No object provided.");
	auto clone = orig->clone(pp);
	ASSERT(clone);
	return static_cast<const T*>(clone);
}

#ifndef SWIG
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace sgo_details
{
template <typename T1, typename T2>
bool dispatch_array_type(const CSGObject* obj, const std::string& name, T2&& lambda)
{
	Tag<CDynamicObjectArray*> tag_array_sg(name);
	if (obj->has(tag_array_sg))
	{
		auto dispatched = obj->get(tag_array_sg);
		lambda(*dispatched); // is stored as a pointer
		return true;
	}

	Tag<std::vector<T1*>> tag_vector(name);
	if (obj->has(tag_vector))
	{
		auto dispatched = obj->get(tag_vector);
		lambda(dispatched);
		return true;
	}

	return false;
}

struct GetByName
{
};

struct GetByNameIndex
{
	GetByNameIndex(index_t index) : m_index(index) {}
	index_t m_index;
};

template <typename T>
CSGObject* get_if_possible(const CSGObject* obj, const std::string& name, GetByName)
{
	return obj->has<T*>(name) ? obj->get<T*>(name) : nullptr;
}

template <typename T>
CSGObject* get_if_possible(const CSGObject* obj, const std::string& name, GetByNameIndex how)
{
	CSGObject* result = nullptr;
	result = obj->get<T>(name, how.m_index, std::nothrow);
	return result;
}

template<typename T>
CSGObject* get_dispatch_all_base_types(const CSGObject* obj, const std::string& name,
		T&& how)
{
	if (auto* result = get_if_possible<CKernel>(obj, name, how))
		return result;
	if (auto* result = get_if_possible<CFeatures>(obj, name, how))
		return result;
	if (auto* result = get_if_possible<CMachine>(obj, name, how))
		return result;
	if (auto* result = get_if_possible<CLabels>(obj, name, how))
		return result;
	if (auto* result = get_if_possible<CEvaluationResult>(obj, name, how))
		return result;

	return nullptr;
}

template<class T>
CSGObject* get_by_tag(const CSGObject* obj, const std::string& name,
		T&& how)
{
	return get_dispatch_all_base_types(obj, name, how);
}
}

#endif //DOXYGEN_SHOULD_SKIP_THIS
#endif //SWIG

}
#endif // __SGOBJECT_H__
