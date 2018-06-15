// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef _SCRIPT_COMPLEX_H
#define _SCRIPT_COMPLEX_H

#include "types.h"
#include <memory>
using std::make_unique;
using std::make_shared;
using std::unique_ptr;
using std::shared_ptr;
#include <map>
#include <vector>
namespace script{
	typedef int64_t int_t;
	typedef double flt_t;
	typedef str str_t;
	
	enum class T {
		_obj,_str,_int,_flt,_undef
	};
	class ComplexType;
	class baseStorage{
		public:
		typedef void * ptrtype;
		virtual void * get(void) = 0;
		virtual T getType(void)const  = 0;
		virtual unique_ptr<baseStorage> convert(T newtype) const = 0;
		virtual str serialize(uint32_t pretty) const = 0;
		virtual size_t getSize(void) const = 0;
		virtual size_t numElements(void) const = 0;
		virtual void destroylast(void) = 0;
		virtual bool merge(const baseStorage * in) {return false;};
		const static T baseType = T::_undef;
		virtual ~baseStorage() = default;//This is required to clean up the storage properly (memory leak otherwise!!!)
	};

	typedef unique_ptr<baseStorage> storagePtr;

	class _objStor final: public baseStorage{
		public:
		void * get(void) {return &_stor;}
		T getType(void)const  { return T::_obj; }
		storagePtr convert(T newtype)const;
		str serialize(uint32_t pretty) const;
		size_t getSize(void) const;
		size_t numElements(void) const;
		void destroylast(void);
		bool merge(const baseStorage * in);
		const static T baseType = T::_obj;
		
		typedef std::map<size_t,shared_ptr<ComplexType>> ptrtype;
		ptrtype _stor;
		~_objStor() {
			_stor.clear();
		}
	};
	class _intStor final: public baseStorage{
		public:
		void * get(void) {return &_stor;}
		T getType(void)const  { return T::_int; }
		storagePtr convert(T newtype)const;
		str serialize(uint32_t pretty) const;
		size_t getSize(void) const;
		size_t numElements(void) const;
		void destroylast(void);
		const static T baseType = T::_int;
		typedef std::vector<int_t> ptrtype;
		ptrtype _stor;		
		~_intStor() {
			_stor.clear();
		}
	};
	class _fltStor final: public baseStorage{
		public:
		void * get(void) {return &_stor;}
		T getType(void) const { return T::_flt; }
		storagePtr convert(T newtype)const;
		str serialize(uint32_t pretty) const;
		size_t getSize(void) const;
		size_t numElements(void) const;
		void destroylast(void);
		const static T baseType = T::_flt;
		typedef std::vector<flt_t> ptrtype;
		ptrtype _stor;		
		~_fltStor() {
			_stor.clear();
			
		}
	};


	class _strStor final: public baseStorage{
		public:
		void * get(void) {return &_stor;}
		T getType(void) const { return T::_str; }
		storagePtr convert(T newtype)const;
		str serialize(uint32_t pretty) const;
		size_t getSize(void) const;
		size_t numElements(void) const;
		void destroylast(void);
		const static T baseType = T::_str;
		typedef std::vector<str_t> ptrtype;
		ptrtype _stor;
		~_strStor() {
			_stor.clear();
		}
	};

	class _Value final {
		private:
		storagePtr _stor;
		template<typename TYPE> inline typename TYPE::ptrtype & _GET(void) {
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
		_Value() = default;
		_Value(const _Value & in);
		_Value(_Value && in) noexcept;// = default;
		_Value & operator= ( _Value && ) = default;
		_Value & operator= ( const _Value & in) ;
		~_Value();
		
		void merge(const _Value & in);
		T getType() const ;
		int_t & getI(size_t offset, size_t Size );
		flt_t & getF(size_t offset, size_t Size );
		size_t getSize(void);
		ComplexType & getO(size_t offset);
		shared_ptr<ComplexType> getOPtr(size_t offset);
		void setOPtr(size_t offset,shared_ptr<ComplexType> n);
		str_t & getS(size_t offset, size_t Size);
		size_t numElements(void) ;
		void destroylast(void);
		str serialize(uint32_t pretty = 0);
		
	};

	class ComplexType final{
	private:
		std::map<str,_Value> _values;
		std::map<str,str> comments;
	public:
		/*ComplexType() =default;//When declaring any constructor... even default, Move assignment operator is not generated, resulting in unneeded copies!*/
		static shared_ptr<ComplexType> newComplex(void);
		int_t & getI(const str & name, size_t offset =0, size_t Size = 1) ;
		flt_t & getF(const str & name, size_t offset =0, size_t Size = 1) ;
		size_t getSize(const str & name) ;
		ComplexType & getO(const str & name, size_t offset =0) ;
		shared_ptr<ComplexType> getOPtr(const str & name, size_t offset =0) ;
		void setOPtr(const str & name, size_t offset,shared_ptr<ComplexType> n) ;
		str & getS(const str & name, size_t offset =0, size_t Size = 1) ;
		int_t clearProperty(const str & name);
		int_t hasProperty(const str & name);
		T getType(const str & name);
		size_t numElements(void);
		void destroyLastElementOf(const str & name);
		bool empty(void) const;
		void clear(void);
		
		void merge(shared_ptr<ComplexType> in);
		
		auto begin(void) { return _values.begin(); }
		auto end(void) { return _values.end(); }
		
		str serialize(uint32_t pretty=0) ;
		void unserialize(const str & in);
		//@testaddfile^ lbz/cb9modules/util/str.cpp
		static void runTests(void); //@test^ ComplexType::runTests();
	};
	
	typedef std::shared_ptr<ComplexType> complextypePtr;

}
#endif
