// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef _SCRIPT_JSON_H
#define _SCRIPT_JSON_H

#include "types.h"
#include <memory>

#include <map>
#include <unordered_map>
#include <vector>
#include <typeinfo>
#include <typeindex>
#include <algorithm>

namespace script{
	
	typedef int64_t int_t;
	typedef double flt_t;
	typedef str str_t;
	class JSON;
	
	typedef std::shared_ptr<JSON> JSONPtr;
	
	namespace SLT{//Single letter typedefs: using namespace script::SLT;
		typedef int_t I;
		typedef flt_t F;
		typedef str_t S;
		typedef JSON O;
		typedef JSONPtr P;
	};
	
	enum class T {
		_obj,_str,_int,_flt,_undef
	};
	
	class JSON;
	class storageInterface;
	typedef unique_ptr<storageInterface> storagePtr;


	
	class storageInterface{
		public:
		virtual void * get(void) =0;
		virtual T getType(void) const  =0;
		virtual storagePtr convert(T newtype) const = 0;
		virtual str serialize(uint32_t pretty) const = 0;
		virtual size_t getSize(void) const = 0;
		virtual size_t numElements(void) const = 0;
		virtual bool merge(const storageInterface * in) {return false;};
		virtual ~storageInterface() = default;//This is required to clean up the storage properly (memory leak otherwise!!!)
	};
	
	template<typename PTRTYPE,T RUNTIMETYPE>
	class _storBase: public storageInterface{
		public:
		void * get(void) {return &_stor;}
		T getType(void)const  { return RUNTIMETYPE; }
		virtual size_t getSize(void) const {return _stor.size();}
		virtual size_t numElements(void) const {return _stor.size();}
		const static T baseType = RUNTIMETYPE;
		typedef std::vector<PTRTYPE> ptrtype;
		typedef PTRTYPE eltype;
		ptrtype _stor;
		virtual ~_storBase() = default;
	};

	

	class _objStor final: public _storBase<JSONPtr,T::_obj>{
		public:
		storagePtr convert(T newtype)const;
		str serialize(uint32_t pretty) const;
		size_t getSize(void) const;
		size_t numElements(void) const;
		bool merge(const storageInterface * in);
	};
	class _intStor final: public _storBase<int_t,T::_int>{
		public:
		storagePtr convert(T newtype) const;
		str serialize(uint32_t pretty) const;
	};
	class _fltStor final: public _storBase<flt_t,T::_flt>{
		public:
		storagePtr convert(T newtype)const;
		str serialize(uint32_t pretty) const;
	};


	class _strStor final: public _storBase<str_t,T::_str>{
		public:
		storagePtr convert(T newtype)const;
		str serialize(uint32_t pretty) const;
	};
	
	/*The type list, this list maps a type to the proper store type */
	typedef _storBase<void*,T::_undef> _errStor;
	template<typename myT> struct _classMap    { typedef _errStor value; static constexpr bool valid = false; typedef void*   rettype;};
	template<> struct _classMap<flt_t>         { typedef _fltStor value; static constexpr bool valid = true;  typedef flt_t   rettype;};
	template<> struct _classMap<float>         { typedef _fltStor value; static constexpr bool valid = true;  typedef flt_t   rettype;};
	
	template<> struct _classMap<int_t>         { typedef _intStor value; static constexpr bool valid = true;  typedef int_t   rettype;};
	template<> struct _classMap<int>           { typedef _intStor value; static constexpr bool valid = true;  typedef int_t   rettype;};
	template<> struct _classMap<unsigned>      { typedef _intStor value; static constexpr bool valid = true;  typedef int_t   rettype;};
	template<> struct _classMap<uint64_t>      { typedef _intStor value; static constexpr bool valid = true;  typedef int_t   rettype;};
#ifndef _WIN32
	template<> struct _classMap<unsigned long long>{ typedef _intStor value; static constexpr bool valid = true;  typedef int_t   rettype;};
#endif
	template<> struct _classMap<bool>          { typedef _intStor value; static constexpr bool valid = true;  typedef int_t   rettype;};
	
	template<> struct _classMap<str_t>         { typedef _strStor value; static constexpr bool valid = true;  typedef str_t   rettype;};
	template<> struct _classMap<const char*>   { typedef _strStor value; static constexpr bool valid = true;  typedef str_t   rettype;};
	template<> struct _classMap<JSON>          { typedef _objStor value; static constexpr bool valid = true;  typedef JSON    rettype;};
	template<> struct _classMap<JSONPtr>       { typedef _objStor value; static constexpr bool valid = true;  typedef JSONPtr rettype;};

	template<class T>
	using LST = std::initializer_list<T>;


	class ListVariant final {
		private:
		storagePtr _stor;
		template<typename TYPE> 
		inline typename TYPE::ptrtype & _GET(void) {
			if(getType()!=TYPE::baseType) {
				if(_stor) {
					_stor = _stor->convert(TYPE::baseType);
				} else {
					_stor = make_unique<TYPE>();
				}
			}
			return *static_cast<typename TYPE::ptrtype * >(_stor->get());
		}
		public:
		ListVariant() = default;
		ListVariant(const ListVariant & in);
		ListVariant(ListVariant && in) noexcept;// = default;
		ListVariant & operator= ( ListVariant && ) = default;
		ListVariant & operator= ( const ListVariant & in) ;
		~ListVariant();
		
		template<typename N>
		inline auto & getVector() {
			static_assert(_classMap<N>::valid,"N is not a valid type for ListVariant::getVector");
			return _GET<typename _classMap<N>::value>();
		}
		
		template<typename N> 
		inline typename _classMap<N>::rettype & get(size_t offset, size_t size) {
			static_assert(_classMap<N>::valid,"N is not a valid type for ListVariant::get");
			auto & list = getVector<N>();
			size_t S = std::max(size_t(1),size);
			if(list.size()<offset+S) {
				//We dont have the element: create it
				list.resize(offset+S);
			}
			return list[offset];
		}
		

		void   merge(const ListVariant & in);
		T      getType() const ;
		size_t getSize(void);
		size_t numElements(void) ;
		str    serialize(uint32_t pretty = 0);
	};
	
	template<> JSONPtr & ListVariant::get<JSONPtr>(size_t offset, size_t size) ;
	template<> JSON & ListVariant::get<JSON>(size_t offset, size_t size);
	
	
	
	class VariantProxy final{
	private:
		ListVariant * val;
		size_t offset;
	public:
		VariantProxy(ListVariant * v,size_t o): val(v),offset(o) {}
		
		inline const char * operator =(const char * in) const { val->get<str_t>(offset, 1) = in; return in;	}
		
		template<typename T> inline const T& operator =(const T& b) const {
			static_assert(_classMap<T>::valid, "N is not a valid type for VariantProxy::operator =");
			val->get<T>(offset,1) = (typename _classMap<T>::rettype) b;
			return b;
		}

		template<typename T> inline const LST<T>& operator =(const LST<T>& b) const {
			static_assert(_classMap<T>::valid, "N is not a valid type for VariantProxy::operator =(LST<T>)");
			auto * value = &val->get<T>(offset, b.size());
			for (auto & i : b) {
				*value = i; ++value;
			}
			return b;
		}
		
		template<typename N>
		inline auto & getVector() { return val->getVector<N>(); }
		
		//Compare operators:
		template<typename T> inline bool operator==(const T & in)       const { return (val->get<T>(offset,1)==in); }
		template<typename T> inline bool operator>=(const T & in)       const { return (val->get<T>(offset,1)>=in); }
		template<typename T> inline bool operator<=(const T & in)       const { return (val->get<T>(offset,1)<=in); }
		template<typename T> inline bool operator>(const T & in)        const { return (val->get<T>(offset,1)>in); }
		template<typename T> inline bool operator<(const T & in)        const { return (val->get<T>(offset,1)<in); }
		template<typename T> inline      operator T()                   const { return (T)val->get<T>(offset,1); }
		VariantProxy                     operator[](const str & idx)    const ;
		VariantProxy                     operator[](const char* idx)    const ;
		inline VariantProxy              operator[](const size_t & idx) const { return {val,idx};	}
		
		inline void   merge(const VariantProxy & in) const { val->merge(*in.val); }
		inline T      getType()                      const { return val->getType(); } 
		inline size_t getSize(void)                  const { return val->getSize(); };
		inline size_t numElements(void)              const { return val->numElements(); };
		inline str    serialize(uint32_t pretty = 0) const { return val->serialize(pretty); };
	};
	
	class JSON final{
	private:
		std::map<str,ListVariant> _values;
		std::map<str,str> comments;
		
		static JSON & _add_one_arg(JSON & obj);
		template <class A0, class B0, class ...Args> 
		static JSON & _add_one_arg(JSON & obj, const A0 & a0,const B0 & b0, const Args & ...args) {
			obj[a0] = b0;
			return _add_one_arg(obj,args...);
		}
		
	public:
		//When declaring any constructor... even default, Move assignment operator is not generated, resulting in unneeded copies!
		
		VariantProxy operator[](const str & idx) { return {&_values[idx],0}; }

		
		template<typename N>
		inline typename _classMap<N>::rettype & get(const str & name, size_t offset=0, size_t size=1) {
			static_assert(_classMap<N>::valid,"N is not a valid type for JSON::get");
			return _values[name].get<N>(offset,size);
		}

		template<typename N>
		inline void set(const str & name,const N & in,size_t offset=0,size_t size=1) {
			static_assert(_classMap<N>::valid,"N is not a valid type for JSON::set");
			_values[name].get<N>(offset,size) = in;
		}
		
		//Simple specialization for const char * (to easily set strings)
		inline void set(const str & name,const char * S,size_t offset=0,size_t size =1) {
			_values[name].get<str_t>(offset,size) = S;
		}
	
		/*Set an array for items in 1 go*/
		template<typename N>
		void set(const str & name,const LST<N> & in) {
			static_assert(_classMap<N>::valid,"N is not a valid type for JSON::set(LST<N>)");
			_values[name] = in;
		}
		
		/*Init multiple variables in 1 go: setAll("name",content,"name2",content, ....)*/
		template<class ...Args>
		void setAll(const Args & ...args) {
			static_assert(sizeof ...(Args) %2== 0," Arguments to JSON::setAll should be multiple of 2");
			_add_one_arg(*this,args...);
		}
		
		/*Initialization: init("name",content,"name2",content, ....)*/
		template<class ...Args>
		static JSONPtr init(const Args & ...args) {
			static_assert(sizeof ...(Args) %2== 0," Arguments to JSON::init should be multiple of 2, or a single string");			
			auto ret = make_shared<JSON>();
			ret->setAll(args...);
			return ret;
		}
		
		static JSONPtr init(const str & in);
		
		size_t getSize(const str & name) ;
		int_t clearProperty(const str & name);
		int_t hasProperty(const str & name);
		T getType(const str & name);
		size_t numElements(void);
		bool empty(void) const;
		void clear(void);
		
		void merge(JSONPtr in);
		
		auto begin(void) { return _values.begin(); }
		auto end(void) { return _values.end(); }
		
		str serialize(uint32_t pretty=0) ;
		
		void unserialize(const str & in);
		
		static void runTests(void);
		
	};
	
	template<class ...Args>	JSONPtr make_json(const Args& ...args) {	return JSON::init(args...);	}

}
#endif
