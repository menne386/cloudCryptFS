// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#define _SCRIPT_JSON_CPP
#include "JSON.h"
#include "main.h"
#include <stdexcept>
#include "lexer.h"
#include <modules/util/str.h>
#include <modules/util/to_string.h>
#include <functional>
#include <algorithm>
using namespace script;

std::map<T,str> g_TNames = {
	{T::_undef,"undefined"},
	{T::_obj,"object"},
	{T::_int,"int"},
	{T::_flt,"float"},
	{T::_str,"str"},
};

//Base conversion == cast
template<typename A,typename B> A convert(const B & in) { return (A)in; }

template<> str_t convert<str_t,int_t>(const int_t & in) { return util::to_string(in); }

template<> str_t convert<str_t,flt_t>(const flt_t & in) { return util::to_string(in); }

template<> int_t convert<int_t,str_t>(const str_t & in) { return std::stoi(in.c_str()); }

template<> flt_t convert<flt_t,str_t>(const str_t & in) { return std::stof(in.c_str()); }



template<class T>
storagePtr copyStor(const typename T::ptrtype & s) {
	auto ret = make_unique<T>();
	ret->_stor = s;
	return ret;
}

template<class T,class O> 
storagePtr copyStorTo(const O & list) {
	auto ret = make_unique<T>();
	for(auto & s:list) {
		ret->_stor.push_back(convert<typename T::eltype,typename O::value_type>(s));
	}
	return ret;
}


/**************** OBJECT functions **********************************/

storagePtr _objStor::convert(T newtype) const {
	if(newtype==T::_obj) {
		auto ret = make_unique<_objStor>();
		for(auto & i: _stor) {
			ret->_stor.push_back(make_shared<JSON>());
			*ret->_stor.back() = (*i);
		}
		return ret;
	}
	throw std::logic_error(str("illegal conversion from "+g_TNames[getType()]+" to "+g_TNames[newtype]).c_str());
}

str _objStor::serialize(uint32_t pretty) const {
	str N;
	
	if(_stor.size()==1) {
		N+=_stor.front()->serialize(pretty?pretty+1:0);
	} else {
		N+="[";
		for(auto & i: _stor) {
			if(i) {
				N+= i->serialize(pretty?pretty+1:0)+",";	
			} else {
				N+= "null,";
			}
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
		return _stor.size();
	}
	return 0;
}

size_t _objStor::numElements(void) const {
	size_t N = 1;
	for(const auto & i: _stor) {
		N += i->numElements();
	}
	return N;

}
bool _objStor::merge(const storageInterface * in) {
	_ASSERT(in->getType()==T::_obj);
	const _objStor * i = static_cast<const _objStor*>(in);
	unsigned idx =0;
	for(const auto & itter: i->_stor) {
		while(_stor.size()<=idx) {
			_stor.push_back(nullptr);
		}
		if(!_stor[idx]) {
			_stor[idx] = make_json();
		}
		_stor[idx]->merge(itter);
		++idx;
	}
	return true;
}


/**************** STRING functions **********************************/

storagePtr _strStor::convert(T newtype) const {
	switch(newtype) {
		case T::_str: return copyStor<_strStor>(_stor);
		case T::_int: return copyStorTo<_intStor>(_stor);
		case T::_flt: return copyStorTo<_fltStor>(_stor);
		default:
			throw std::logic_error(str("illegal conversion from "+g_TNames[getType()]+" to "+g_TNames[newtype]).c_str());
	}
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



/**************** INT functions **********************************/

storagePtr _intStor::convert(T newtype) const {
	switch(newtype) {
		case T::_int: return copyStor<_intStor>(_stor);
		case T::_str: return copyStorTo<_strStor>(_stor);
		case T::_flt: return copyStorTo<_fltStor>(_stor);
		default:throw std::logic_error(str("illegal conversion from "+g_TNames[getType()]+" to "+g_TNames[newtype]).c_str());
	}
}

str _intStor::serialize(uint32_t pretty) const {
	
	if(_stor.size() == 1) {
		return {std::to_string(_stor.back()).c_str()};
	} 
	str N;
	N = "[";
	N += listToString(_stor);
	N += "]";
	return N;
}


/**************** FLOAT functions **********************************/


storagePtr _fltStor::convert(T newtype) const {
	switch(newtype) {
		case T::_flt: return copyStor<_fltStor>(_stor);
		case T::_str: return copyStorTo<_strStor>(_stor);
		case T::_int: return copyStorTo<_intStor>(_stor);
		default:throw std::logic_error(str("illegal conversion from "+g_TNames[getType()]+" to "+g_TNames[newtype]).c_str());
	}
}

str _fltStor::serialize(uint32_t pretty) const {
	if(_stor.size() == 1) {
		return {std::to_string(_stor.back()).c_str()};
	}
	str N = "[";
	N += listToString(_stor);
	N += "]";
	return N;
}



/**************** _Value functions **********************************/



ListVariant::ListVariant (const ListVariant & in) {
	_stor = in._stor->convert(in.getType());
}

ListVariant & ListVariant::operator= ( const ListVariant & in) {
	_stor = in._stor->convert(in.getType());
	return *this;
}

ListVariant::ListVariant ( ListVariant && in) noexcept {
	_stor = std::move(in._stor);
}

ListVariant::~ListVariant() {
	//CLOG("deleting value...");
}

T ListVariant::getType() const {
	if(_stor) {
		return _stor->getType();
	}
	return T::_undef;
}


size_t ListVariant::getSize() {
	if(_stor) {
		return _stor->getSize();
	}
	
	return 0;
}

template<> 
JSONPtr & ListVariant::get<JSONPtr>(size_t offset, size_t size) {
	auto & list = _GET<_objStor>();
	_ASSERT(size==1);
	size_t S = std::max((size_t)1,size);
	if(list.size()<offset+S) {
		//We dont have the element: create it
		list.resize(offset+S);
	}

	auto & ptr = list[offset];
	if(ptr==nullptr) {
		ptr = make_json();
	}
	return ptr;	
}	
template<> 
JSON & ListVariant::get<JSON>(size_t offset, size_t size) {
	return *get<JSONPtr>(offset,size);	
}	



size_t ListVariant::numElements(void) {
	if(_stor) {
		return _stor->numElements();
	}
	
	return 0;
}

str ListVariant::serialize(uint32_t pretty) {
	if(_stor) {
		return _stor->serialize(pretty);
	}
	return "";
}

void ListVariant::merge(const ListVariant & in) {
	//Try merge
	if(_stor == nullptr || _stor->merge(in._stor.get()) == false) {
		//Copy if that fails..
		_stor = in._stor->convert(in.getType());
	}
}

/**************** VariantProxy functions **********************************/

VariantProxy VariantProxy::operator[](const str & idx) const { return val->get<JSON>(offset,1)[idx]; }
VariantProxy VariantProxy::operator[](const char* idx) const { return val->get<JSON>(offset,1)[idx]; }

/**************** ComplexType functions **********************************/

JSONPtr JSON::init(const str & in) {
	auto ret = make_shared<JSON>();
	ret->unserialize(in);
	return ret;
}





size_t JSON::getSize(const str & name) {
	return _values[name].getSize();		
}

int_t JSON::clearProperty(const str & name) {
	auto it = _values.find(name);
	if(it!=_values.end()) {
		_values.erase(it);
		return 1;
	}
	return 0;
}

int_t JSON::hasProperty(const str & name) {
	auto it = _values.find(name);
	if(it!=_values.end()) {
		return 1;
	}
	
	return 0;
}
T JSON::getType(const str & name) {
	return _values[name].getType();
}

size_t JSON::numElements() {
	size_t N =0;
	for(auto & V:_values) {
		N += V.second.numElements();
	}
	return N;
}
bool JSON::empty(void) const {
	return _values.empty();
}
void JSON::clear(void) {
	_values.clear();
}

void JSON::merge(shared_ptr<JSON> in) {
	for(auto & i:in->_values) {
		_values[i.first].merge(i.second);
	}
}

str JSON::serialize(uint32_t pretty) {
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


void JSON::unserialize(const str & in) {
	clear();
	using namespace script::SLT;
	auto lex = make_unique<Lexer>(in);
	bool tokenIsValue = false;
	std::vector<JSON *> objStack;
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
		switch(token->type) {
			case TokenType::compound_l:
				if(objStack.empty()) {
					objStack.push_back(this);
				} else {
					_ASSERT(nameStack.empty()==false);
					const str & name = nameStack.back().tok->content;
					objStack.push_back(objStack.back()->get<P>(name,nameStack.back().idx).get());
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
						objStack.back()->get<S>(nameStack.back().tok->content,nameStack.back().idx) = token->content;
					} else if(token->content.find(".")!=str::npos) {
						flt_t V = std::stof(token->content.c_str());
						objStack.back()->get<F>(nameStack.back().tok->content,nameStack.back().idx) = negate?-V:V;						
					} else {
						int_t V = std::stoi(token->content.c_str());
						objStack.back()->get<I>(nameStack.back().tok->content,nameStack.back().idx) = negate?-V:V;
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
				_ASSERT(nameStack.empty()==false);
				if(nameStack.back().isArray) {
					nameStack.back().idx++;
				} else {
					tokenIsValue = false;
					if(!didCloseObj) {
						nameStack.pop_back();
					}
				}
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
			case TokenType::_null:
				break;
			default:
				throw std::logic_error(str("Unknown token "+std::string(g_TokenTypeString[(uint32_t)token->type])+" in serialised object").c_str());
				break;
				
		}
	}
}

JSON & JSON::_add_one_arg( JSON & obj) {
	return obj;
}

void JSON::runTests(void) {
	using namespace script::SLT;
	auto comp = make_json(
		"myarray",LST<I>{1,2,3},
		"myint",1,
		"myint2",2,
		"myint3",-3,
		"myarraystring",LST<S>{"stuff1","stuff2","stuff3"},
		"mystring","stuff"

	);
	auto & obj = *comp;

	auto compNest = make_json();

	auto & objNest = *compNest;


	objNest = obj;

	obj["subobj"][5] = *compNest;
	obj["subobj"][9]["hehe"]="joke";
	
	
	str dta = obj.serialize(1);
	//CLOG(dta);
	auto comp2 = make_json(dta);
	str dta2 = comp2->serialize(1);
	//CLOG(dta2);
	
	_ASSERT(dta == comp->serialize(1));
	
	//CLOG(dta,"==",dta2);
	_ASSERT(dta == dta2);
	
	obj.clear();
	obj["mystring"] = "\nTEST\nTEST\n";
	
	//CLOG(comp->serialize());
	_ASSERT(obj.serialize()=="{\"mystring\":\"\\nTEST\\nTEST\\n\"}");

	obj.clear();
	//CLOG(comp->serialize(1));
	
	/*str N = R"DELIM({"num":2,"%P":[{"__construct":"_persistentRegistrar%__construct","load":"_persistentRegistrar%load","store":"_persistentRegistrar%store"}],"storage":[{"_credentials":"{\"err\":0,\"updated\":1,\"__func%parse\":\"_rpcRegister%parse\",\"nick\":\"^1AnalyticaL^7ALPHA\",\"res\":\"\",\"secret\":\"c0e5e66b4f913f6e642d4ff538cbad160f8264b9a76dae09aa86ca29d06d20ed\",\"type\":\"_login\"}","sessionkey":"","%P":[{"__construct":"_persist%__construct","dologin":"_rpcSession%doLogin","dologout":"_rpcSession%doLogout","getnick":"_rpcSession%getNick","sendregisterform":"_rpcSession%sendRegisterForm","setcredentials":"_rpcSession%setCredentials","setsessionkey":"_rpcSession%setSessionKey"}]},{"%P":[{"__construct":"_lgsTables%__construct","addtablerow":"_lgsTables%addTableRow"}]}]})DELIM";
	comp2->unserialize(N);
	CLOG(comp2->serialize());
	
	_ASSERT(N == comp2->serialize());*/
	
	auto neo = make_json (
		"test",S ( "Some data" ),
		"array",LST<I>{1,2,3},
		"object", make_json (
			"subvar", 1
		),
		//"object_copy",*comp,
		"TheD",0.0,
		"Bool",false
		
	);

	//Check if we can loop over the items:
	int cnt =0 ;
	for(auto & i: *neo) {
		if(i.second.getSize()>0) {
			++cnt;
		}
	}
	
	int T = (*neo)["object"]["subvar"];
	_ASSERT(T==1);
	_ASSERT((*neo)["object"]["subvar"]==1);
	_ASSERT(neo->get<O>("object").get<I>("subvar") == 1);
	_ASSERT(cnt>=5);
	_ASSERT(neo->hasProperty("test"));
	_ASSERT(neo->get<S>("test") == "Some data");
	_ASSERT(neo->getSize("array") == 3);
	_ASSERT(neo->get<bool>("Bool") == false);
	
	//CLOG("ShowAndTell:", neo->serialize(1));
	
	

}

