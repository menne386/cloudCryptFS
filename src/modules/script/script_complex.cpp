// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define _SCRIPT_COMPLEX_CPP
#include "script_complex.h"
#include <stdexcept>
#include "lexer.h"
#include <modules/util/str.h>
#include <algorithm>
using namespace script;

#define _O() _GET<_objStor>()
#define _S() _GET<_strStor>()
#define _I() _GET<_intStor>()
#define _F() _GET<_fltStor>()


/**************** OBJECT functions **********************************/

storagePtr _objStor::convert(T newtype) const {
	if(newtype==T::_obj) {
		auto ret = make_unique<_objStor>();
		for(auto & i: _stor) {
			ret->_stor[i.first] = make_shared<ComplexType>();
			(*ret->_stor[i.first]) = (*i.second);
		}
		return ret;
	}
	throw std::logic_error("illegal conversion in object value");
}

str _objStor::serialize(uint32_t pretty) const {
	str N;
	if(_stor.size()==1 && _stor.begin()->first==0) {
		N+=_stor.begin()->second->serialize(pretty?pretty+1:0);
	} else {
		N+="[";
		unsigned II = 0;
		for(auto & i: _stor) {
			while(i.first>II) {
				//CLOG("Add prefix ",II);
				++II;
				N+="{},";
			}
			//CLOG("Add real OBJ ",II," ",i.second.serialize());
			N+= i.second->serialize(pretty?pretty+1:0)+",";	
			++II;
		}				
		if(N.back()==',') {
			N.pop_back();
		}
		N+="]";
	}
	return N;
}

size_t _objStor::getSize(void) const {
	if(_stor.empty()==false) {
		return _stor.rbegin()->first +(size_t)1;
	}
	return 0;
}

size_t _objStor::numElements(void) const {
	size_t N = 1;
	for(const auto & i: _stor) {
		N += i.second->numElements();
	}
	return N;
}

void _objStor::destroylast(void) {
	if(!_stor.empty()) {
		_stor.erase(_stor.rbegin()->first);
	}
}


bool _objStor::merge(const baseStorage * in) {
	_ASSERT(in->getType()==T::_obj);
	const _objStor * i = static_cast<const _objStor*>(in);
	for(const auto & itter: i->_stor) {
		if(!_stor[itter.first]) {
			_stor[itter.first] = ComplexType::newComplex();
		}
		_stor[itter.first]->merge(itter.second);
	}
	return true;
}


/**************** STRING functions **********************************/

storagePtr _strStor::convert(T newtype) const {
	if(newtype==T::_str) {
		auto ret = make_unique<_strStor>();
		ret->_stor = _stor;
		ret->_stor.shrink_to_fit();
		return ret;
	} else if(newtype == T::_int) {
		auto ret = make_unique<_intStor>();
		for(auto & s:_stor) {
			ret->_stor.push_back(std::stoi(s.c_str()));
		}
		ret->_stor.shrink_to_fit();
		return ret;
	} else if(newtype == T::_flt) {
		auto ret = make_unique<_fltStor>();
		for(auto & s:_stor) {
			ret->_stor.push_back(std::stof(s.c_str()));
		}
		ret->_stor.shrink_to_fit();
		return ret;
	}
	throw std::logic_error("illegal conversion in object value");
}

str _strStor::serialize(uint32_t pretty) const {
	str N;
	if(_stor.size() == 1) {
		return quotestring(_stor.back());
	} else {
		N = "[";
		for(const str & a:_stor) {
			N+= quotestring(a)+",";
		}
		if(N.back()==',') {
			N.pop_back();
		}
		N += "]";
	}
	return N;
}

size_t _strStor::getSize(void) const {
	return _stor.size();
}
size_t _strStor::numElements(void) const {
	return _stor.size();
}
void _strStor::destroylast(void) {
	if(!_stor.empty()) _stor.pop_back();
}


/**************** INT functions **********************************/

storagePtr _intStor::convert(T newtype) const {
	if(newtype==T::_int) {
		auto ret = make_unique<_intStor>();
		ret->_stor = _stor;
		ret->_stor.shrink_to_fit();
		return ret;
	} else if(newtype == T::_str) {
		auto ret = make_unique<_strStor>();
		for(auto & s:_stor) {
			ret->_stor.push_back(std::to_string(s).c_str());
		}
		ret->_stor.shrink_to_fit();
		return ret;
	} else if(newtype == T::_flt) {
		auto ret = make_unique<_fltStor>();
		for(auto & s:_stor) {
			ret->_stor.push_back((flt_t)s);
		}
		ret->_stor.shrink_to_fit();
		return ret;		
	}
	throw std::logic_error("illegal conversion in object value");
}

str _intStor::serialize(uint32_t pretty) const {
	
	if(_stor.size() == 1) {
		return std::to_string(_stor.back()).c_str();
	} 
	str N;
	N = "[";
	for(auto a: _stor) {
		N+= str(std::to_string(a).c_str())+",";
	}
	if(N.back()==',') {
		N.pop_back();
	}
	N += "]";
	return N;
}

size_t _intStor::getSize(void) const {
	return _stor.size();
}
size_t _intStor::numElements(void) const {
	return _stor.size();
}
void _intStor::destroylast(void) {
	if(!_stor.empty()) _stor.pop_back();
}


/**************** FLOAT functions **********************************/


storagePtr _fltStor::convert(T newtype) const {
	if(newtype==T::_flt) {
		auto ret = make_unique<_fltStor>();
		ret->_stor = _stor;
		ret->_stor.shrink_to_fit();
		return ret;
	} else if(newtype == T::_str) {
		auto ret = make_unique<_strStor>();
		for(auto & s:_stor) {
			ret->_stor.push_back(std::to_string(s).c_str());
		}
		ret->_stor.shrink_to_fit();
		return ret;
	} else if(newtype == T::_int) {
		auto ret = make_unique<_intStor>();
		for(auto & s:_stor) {
			ret->_stor.push_back((int_t)s);
		}
		ret->_stor.shrink_to_fit();
		return ret;		
	}
	throw std::logic_error("illegal conversion in object value");
}

str _fltStor::serialize(uint32_t pretty) const {
	if(_stor.size() == 1) {
		return std::to_string(_stor.back()).c_str();
	}
	str N = "[";
	for(auto a: _stor) {
		N+= str(std::to_string(a).c_str())+",";
	}
	if(N.back()==',') {
		N.pop_back();
	}
	N += "]";
	return N;
}

size_t _fltStor::getSize(void) const {
	return _stor.size();
}
size_t _fltStor::numElements(void) const {
	return _stor.size();
}

void _fltStor::destroylast(void) {
	if(!_stor.empty()) _stor.pop_back();
}

template<typename T> auto & fetchCreate(T & list,size_t Offset, size_t Size) {
	auto S = std::max(Size,(size_t)1);
	if(list.size()<Offset+S) {
		//We dont have the element: create it
		list.resize(Offset+S);
	}
	return list[Offset];
}

/**************** _Value functions **********************************/



_Value::_Value(const _Value & in) {
	_stor = in._stor->convert(in.getType());
}

_Value & _Value::operator= ( const _Value & in) {
	_stor = in._stor->convert(in.getType());
	return *this;
}

_Value::_Value(_Value && in) noexcept{
	_stor = std::move(in._stor);
}

_Value::~_Value() {
	_stor.reset();
	//CLOG("deleting value...");
}

T _Value::getType() const {
	if(_stor) {
		return _stor->getType();
	}
	return T::_undef;
}

int_t & _Value::getI(size_t Offset , size_t Size ) {
	return fetchCreate(_I(),Offset,Size);
}
flt_t & _Value::getF(size_t Offset , size_t Size ) {
	return fetchCreate(_F(),Offset,Size);
}

size_t _Value::getSize() {
	if(_stor) {
		return _stor->getSize();
	}
	
	return 0;
}



ComplexType & _Value::getO(size_t Offset ) {
	auto & objdata = _O();
	auto & ptr = objdata[Offset];
	if(ptr==nullptr) {
		ptr = make_shared<ComplexType>();
	}
	return *ptr;
}

shared_ptr<ComplexType> _Value::getOPtr(size_t Offset) {
	auto & objdata = _O();
	auto & ptr = objdata[Offset];
	if(ptr==nullptr) {
		ptr = make_shared<ComplexType>();
	}
	return ptr;
}

void _Value::setOPtr(size_t offset,shared_ptr<ComplexType> n) {
	auto & objdata = _O();
	objdata[offset] = n;
}

str_t & _Value::getS(size_t Offset , size_t Size ) {
	return fetchCreate(_S(),Offset,Size);
}
size_t _Value::numElements(void) {
	if(_stor) {
		return _stor->numElements();
	}
	
	return 0;
}

str _Value::serialize(uint32_t pretty) {
	if(_stor) {
		return _stor->serialize(pretty);
	}
	return "";
}

void _Value::merge(const _Value & in) {
	//Try merge
	if(_stor == nullptr || _stor->merge(in._stor.get()) == false) {
		//Copy if that fails..
		_stor = in._stor->convert(in.getType());
		
	}
}

void _Value::destroylast(void) {
	if(_stor) {
		_stor->destroylast();
	}
}

/**************** ComplexType functions **********************************/


shared_ptr<ComplexType> ComplexType::newComplex(void) {
	return make_shared<ComplexType>();
}


int_t & ComplexType::getI(const str & name, size_t offset, size_t Size ) {
	return _values[name].getI(offset,Size);
}
flt_t & ComplexType::getF(const str & name, size_t offset , size_t Size ) {
	return _values[name].getF(offset,Size);		
}
size_t ComplexType::getSize(const str & name) {
	auto it = _values.find(name);
	if(it!=_values.end()) {
		return _values[name].getSize();
	}
	return 0;
}
ComplexType & ComplexType::getO(const str & name, size_t offset ) {
	return _values[name].getO(offset);		
}
shared_ptr<ComplexType> ComplexType::getOPtr(const str & name, size_t offset ) {
	return _values[name].getOPtr(offset);		
}
void ComplexType::setOPtr(const str & name, size_t offset,shared_ptr<ComplexType> n)  {
	_values[name].setOPtr(offset,n);
}


str & ComplexType::getS(const str & name, size_t offset , size_t Size ) {
	return _values[name].getS(offset,Size);
}

int_t ComplexType::clearProperty(const str & name) {
	auto it = _values.find(name);
	if(it!=_values.end()) {
		_values.erase(it);
		return 1;
	}
	return 0;
}

int_t ComplexType::hasProperty(const str & name) {
	auto it = _values.find(name);
	if(it!=_values.end()) {
		return 1;
	}
	
	return 0;
}
T ComplexType::getType(const str & name) {
	return _values[name].getType();
}

size_t ComplexType::numElements() {
	size_t N =0;
	for(auto & V:_values) {
		N += V.second.numElements();
	}
	return N;
}

void ComplexType::destroyLastElementOf(const str & name) {
	_values[name].destroylast();
}

bool ComplexType::empty(void) const {
	return _values.empty();
}
void ComplexType::clear(void) {
	_values.clear();
}

void ComplexType::merge(shared_ptr<ComplexType> in) {
	for(auto & i:in->_values) {
		_values[i.first].merge(i.second);
	}
}

str ComplexType::serialize(uint32_t pretty) {
	str out="{";
	if(pretty) {out+="\n";}
		
	
	for(auto & vars: _values) {
		str N = vars.second.serialize(pretty);
		if(N.empty()==false) {
			if(pretty) {
				if(comments.find(vars.first)!=comments.end()) {
					for(auto a=0u;a<pretty;a++) {
						out+= "\t";
					}
					out+= comments[vars.first];
					if(out.back()!='\n') {
						out+="\n";
					}
				}
				
				for(auto a=0u;a<pretty;a++) {
					out+= "\t";
				}
				
			}
			out += "\""+vars.first+"\":"+N+",";
			if(pretty) {
				out+="\n";
			}
		}
	}
	if(pretty) {
		while(out.back()==',' || out.back()=='\n' ) {
			out.pop_back();
		}
		out.push_back('\n');
	} else {
		//Remove unneeded crud:
		if(out.back()==',')out.pop_back();
	}
	if(pretty>1) {
			for(auto a=1u;a<pretty;a++) {
				out+= "\t";
			}
	}
	out.push_back('}');
	return out;
}


void ComplexType::unserialize(const str & in) {
	clear();
	auto lex = make_unique<Lexer>(in);
	bool tokenIsValue = false;
	std::vector<ComplexType *> objStack;
	struct _name{
		_name(Token * it,int i,bool ia) : tok(it), idx(i),isArray(ia){};
		Token * tok=nullptr;
		int idx=0;
		bool isArray=false;
	};
	std::vector<_name> nameStack;
	bool negate = false;
	bool didCloseObj=false;
	while(1) {
		auto token = lex->getNextToken();
		_ASSERT(token!=nullptr);
		/*str my;
		for(auto & i: nameStack) {
			my += i.tok->content+">";
		}
		CLOG(token->content," ",my);*/
		switch(token->type) {
			case TokenType::compound_l:
				if(objStack.empty()) {
					objStack.push_back(this);
				} else {
					_ASSERT(nameStack.empty()==false);
					const str & name = nameStack.back().tok->content;
					objStack.push_back(&objStack.back()->getO(name,nameStack.back().idx));
					//CLOG("parsing object ",nameStack.back().idx," of ",nameStack.back().tok->content," array:",nameStack.back().isArray);
				}
				tokenIsValue = false;
				break;
			case TokenType::compound_r:
				_ASSERT(objStack.empty()==false);
				objStack.pop_back();
				if(objStack.empty()) {
					return;
				}
				_ASSERT(nameStack.empty()==false);
				if(nameStack.back().isArray==false) {
					tokenIsValue = false;
					nameStack.pop_back();//@todo, this is also already cleared by , (seperator)
					didCloseObj = true;
				}
				break;
				break;
			case TokenType::op_sub:
				negate = true;
				break;
			case TokenType::string:
			case TokenType::number:
				if(tokenIsValue) {
					if(token->type==TokenType::string) {
						objStack.back()->getS(nameStack.back().tok->content,nameStack.back().idx) = token->content;
					} else if(token->content.find(".")!=str::npos) {
						flt_t V = std::stod(token->content.c_str());
						objStack.back()->getF(nameStack.back().tok->content,nameStack.back().idx) = negate?-V:V;						
					} else {
						int_t V = std::stol(token->content.c_str());
						objStack.back()->getI(nameStack.back().tok->content,nameStack.back().idx) = negate?-V:V;
					}
				} else {
					nameStack.emplace_back(token,0,false);
					if(token->comment.empty()==false) {
						//CLOG("Added comment ",token->comment);
						objStack.back()->comments[token->content] = token->comment;
					}
				}
				negate = false;
				break;
			case TokenType::jsonassign:
				tokenIsValue = true;
				break;
			case TokenType::seperator:
				//if(!didCloseObj) {
					
				if(nameStack.empty()==false) {
					if(nameStack.back().isArray) {
						nameStack.back().idx++;
					} else {
						tokenIsValue = false;
						if(!didCloseObj) {
							nameStack.pop_back();
						}
					}
				}
				//} //@todo: why is this one incorrect?
				didCloseObj = false;
				break;
			case TokenType::subscr_l:
				_ASSERT(nameStack.empty()==false);
				nameStack.back().isArray = true;
				break;
			case TokenType::subscr_r:
				_ASSERT(nameStack.empty()==false);
				nameStack.back().isArray = false;
				break;
			default:
				throw std::logic_error(str("Unknown token in serialized object: "+str(g_TokenTypeString[(uint32_t)token->type])+":"+str(token->content)).c_str());
				break;
				
		}
	}
}

void ComplexType::runTests(void) {
	auto comp = make_unique<ComplexType>();
	auto compNest = make_unique<ComplexType>();
	
	{
		int_t * dta = &comp->getI("myarray",0,3);
		for(int_t a=0;a<3;a++) {
			dta[a] = a+1;
		}
	}
	comp->getI("myint") = 1;
	comp->getI("myint2") = 2;
	comp->getI("myint3") = -3;
	{
		str * dta2 = & comp->getS("myarraystring",0,3);
		for(int a=0;a<3;a++) {
			dta2[a] = str("stuff")+std::to_string(a+1).c_str();
		}
	}
	comp->getS("mystring") =  "stuff";

	
	*compNest = *comp;

	comp->getO("subobj",5) = *compNest;
	comp->getO("subobj",9);
	str dta = comp->serialize();
	CLOG(dta);
	auto comp2 = make_unique<ComplexType>();
	comp2->unserialize(dta);	
	str dta2 = comp2->serialize();
	CLOG(dta2);
	
	_ASSERT(dta == comp->serialize());
	
	_ASSERT(dta == dta2);
	
	comp->clear();
	comp->getS("mystring") = "\nTEST\nTEST\n";
	
	//CLOG(comp->serialize());
	_ASSERT(comp->serialize()=="{\"mystring\":\"\\nTEST\\nTEST\\n\"}");

	/*str N = R"DELIM({"num":2,"%P":[{"__construct":"_persistentRegistrar%__construct","load":"_persistentRegistrar%load","store":"_persistentRegistrar%store"}],"storage":[{"_credentials":"{\"err\":0,\"updated\":1,\"__func%parse\":\"_rpcRegister%parse\",\"nick\":\"^1AnalyticaL^7ALPHA\",\"res\":\"\",\"secret\":\"c0e5e66b4f913f6e642d4ff538cbad160f8264b9a76dae09aa86ca29d06d20ed\",\"type\":\"_login\"}","sessionkey":"","%P":[{"__construct":"_persist%__construct","dologin":"_rpcSession%doLogin","dologout":"_rpcSession%doLogout","getnick":"_rpcSession%getNick","sendregisterform":"_rpcSession%sendRegisterForm","setcredentials":"_rpcSession%setCredentials","setsessionkey":"_rpcSession%setSessionKey"}]},{"%P":[{"__construct":"_lgsTables%__construct","addtablerow":"_lgsTables%addTableRow"}]}]})DELIM";
	comp2->unserialize(N);
	CLOG(comp2->serialize());
	
	_ASSERT(N == comp2->serialize());*/

}

