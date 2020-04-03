// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define __lexer_cpp
#include "lexer.h"

#include <string.h>
#include <algorithm>
#include <map>
#include "modules/util/str.h"
using namespace script;

namespace script{
const char * g_TokenTypeString[] = {
	"unknown", "EOF", ";", "{", "}", "(", ")", "[", "]",
	//Commands:
	"include", "function", "return", "if", "elseif", "else", "catchevent", "_foreach", "_while", "as", "index", "resourcedef", "timerdef", "eventdef", "warning", "for","extractor","inline_event","_break","typename",
	"defined","expect",
	//Primary:
	"identifier", "number", "string", "variabletype",
	//Other keywords:
	"class","extends", "seperator", "json_assign", "assign","deref","construct","private","public","final","null","template","operator","_brq",
	"__attribute__",
	//Operators:
	"op_incr", "op_decr",
	"op_add", "op_sub", "op_div", "op_mul", "op_mod", "op_and", "op_or", "op_xor",
	"cop_add", "cop_sub", "cop_mul", "cop_div", "cop_mod", "cop_and", "cop_or", "cop_xor",
	"lop_l", "lop_g", "lop_le", "lop_ge", "lop_ne", "lop_e",
	"lop_and", "lop_or", "lop_not",
	/*"xml_dtd","xml_cdatal","xml_cdatar",*/"comment"

};

};



Token::Token(const str & c, TokenType t, size_t o) : content(c), type(t), offset(o), comment(""){
}


namespace{
	static const std::map<const str,TokenType> _wordToTokenType = {
		{"#uctexture", TokenType::resourcedef},
		{"#texture", TokenType::resourcedef},
		{"#sound", TokenType::resourcedef},
		{"#htmlfile", TokenType::resourcedef},
		{"#htmlelement", TokenType::resourcedef},
		{"#model", TokenType::resourcedef},
		{"#modelanim", TokenType::resourcedef},
		{"#warning", TokenType::warning},
		{"#event", TokenType::event},
		{"#timer", TokenType::timer},
		{"#include", TokenType::include},
		
		{"for", TokenType::_for},
		{"foreach", TokenType::_foreach},
		{"as", TokenType::as},
		{"return", TokenType::_return},
		{"index", TokenType::index},
		{"while", TokenType::_while},
		{"catchevent", TokenType::catchevent},
		{"catchallevents", TokenType::catchevent},
		{"operator", TokenType::_operator},
		{"function", TokenType::function},
		{"if", TokenType::_if},
		{"elseif", TokenType::_elseif},
		{"else", TokenType::_else},
		{"class", TokenType::_class},
		{"template", TokenType::_template},
		{"extends", TokenType::extends},
		{"_event", TokenType::inline_event},
		{"break", TokenType::_break},
		{"typename", TokenType::_typen},
		{"deref", TokenType::deref},
		{"construct", TokenType::construct},
		{"private:", TokenType::_private},
		{"null", TokenType::_null},
		{"public:", TokenType::_public},
		{"_brq", TokenType::_brq},
		{"final", TokenType::_final},
		{"__attribute__", TokenType::_attrib},
		{"defined", TokenType::_defined},
		{"expect", TokenType::expect},
		//Datatypes:
		{"playerglobal", TokenType::defvar},
		{"moveglobal", TokenType::defvar},
		{"global", TokenType::defvar},
		{"string", TokenType::defvar},
		{"resource", TokenType::defvar},
		{"int", TokenType::defvar},
		{"float", TokenType::defvar},
		{"vec4", TokenType::defvar},
		{"vec3", TokenType::defvar},
		{"var", TokenType::defvar},
		{"object", TokenType::defvar},
		{"ref", TokenType::defvar},
		{"__ptr__", TokenType::defvar},
		{"auto", TokenType::defvar},
	};	
	static const std::map<const str,TokenType> _operators = {
		{"->",TokenType::extractor},
		//Logical ops
		{"<=", TokenType::lop_le},
		{">=", TokenType::lop_ge},
		{"!=", TokenType::lop_ne},
		{"==", TokenType::lop_e},
		{"&&", TokenType::lop_and},
		{"||", TokenType::lop_or},
		
		//Increment, decrement
		{"++", TokenType::op_incr},
		{"--", TokenType::op_decr},
		//Arithmetic compound ops:
		{"+=", TokenType::cop_add},
		{"-=", TokenType::cop_sub},
		{"/=", TokenType::cop_div},
		{"*=", TokenType::cop_mul},
		{"%=", TokenType::cop_mod},
		{"&=", TokenType::cop_and},
		{"|=", TokenType::cop_or},
		{"^=", TokenType::cop_xor},
		
		//All Single char operators.
		{",", TokenType::seperator},
		{"=", TokenType::assign},
		{":", TokenType::jsonassign},
		{"{", TokenType::compound_l},
		{"}", TokenType::compound_r},
		{"(", TokenType::par_l},
		{")", TokenType::par_r},
		{"[", TokenType::subscr_l},
		{"]", TokenType::subscr_r},
		//Logic:
		{"<", TokenType::lop_l},
		{">", TokenType::lop_g},
		{"!", TokenType::lop_not},
		
		//Arithmetic ops:
		{"+", TokenType::op_add},
		{"-", TokenType::op_sub},
		{"/", TokenType::op_div},
		{"*", TokenType::op_mul},
		{"%", TokenType::op_mod},
		{"&", TokenType::op_and},
		{"|", TokenType::op_or},
		{"^", TokenType::op_xor},
		//end of statement
		{";", TokenType::eos},
	};	
};

//#define singlecharop(OP,TT) if(ThisChar==OP) { AddToken(str(1,OP),TT,offset-2);  continue;}
//#define doublecharop(OP1,OP2,TT) if(ThisChar == OP1 && LastChar==OP2) { LastChar = NextC(); AddToken(str(1,OP1)+str(1,OP2),TT,offset-3); continue;}
//#define wordtoken(OP,TT) if(isl == OP) { AddToken(OP,TT,offset-(strlen(OP)+1),true); continue;}

Lexer::Lexer(const str & c,bool tokenizeComment) {
	size_t offset = 0;
	//Constuctor
	auto NextC = [&c,&offset]() -> int{
		if (offset >= c.size()) return EOF;
		int C = c[offset];
		offset++;
		return C;
	};

	//Load entire textfile into mem
	//int OldChar = ' ';
	int LastChar = ' ';
	int ThisChar = ' ';
	//content = str(c.rbegin(), c.rend());
	while (1) {
		// Skip any whitespace.
		while (isspace(LastChar)) {
			LastChar = NextC();
		}

		//String "... " or '....'
		if (LastChar == '"' || LastChar == '\'') {
			int StrTok = LastChar;
			str st;
			LastChar = NextC();
			ThisChar = 0;
			while (LastChar != EOF) {
				if(ThisChar=='\\') {
					st.pop_back();
					switch(LastChar) {
						//@note: should also do this back in quotestring
						case 'n': st+= "\n"; break;
						case '\\': st+= "\\\\"; /*CLOG("ST:",st);*/break;
						case 't': st+= "\t"; break;
						case 'r': st+= "\r"; break;
						case 'b': st+= "\b"; break;
						case 'f': st+= "\f"; break;
						case 'v': st+= "\v"; break;
						default: st += LastChar; break;	
					}
				} else {
					if(LastChar == StrTok) {
						break;
					}
					st += LastChar;
				}
				ThisChar = LastChar;
				LastChar = NextC();
			}

			LastChar = NextC();
			AddToken(st, TokenType::string, offset-(st.length()+3));
			//CLOG("Parsed full string:",st);
			continue;
		}

		//Identifiers && Commands:
		if (LastChar == '#' || LastChar == '$' || LastChar == '_' || isalpha(LastChar) ) { // identifier: [a-zA-Z][a-zA-Z0-9]*
			str is;
			is = LastChar;
			bool skipKeyWords = false;
			if(LastChar=='$') {
				is ="";
				skipKeyWords = true;
			}
			LastChar = NextC();
			while (isalnum(LastChar) || LastChar == '_' ) {
				is += LastChar;
				LastChar = NextC();
			}
			if(!skipKeyWords) {
				if(LastChar == ':') {
					if(is=="public" || is=="private") {
						//Special exception for private and public:
						//Label:
						is += LastChar;
						LastChar = NextC();
					}
				}
				const str isl = strtolower(is);	

				auto itr = _wordToTokenType.find(isl);
				if(itr!=_wordToTokenType.end()) {
					AddToken(itr->first,itr->second,offset-(itr->first.length()+1),true);
					continue;
				}
			}
			//All other tokens are identifiers:
			AddToken(is, TokenType::identifier, offset-(is.length()+1),true);
			continue;
		}
		//Numerals:
		if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9.]+
			str NumStr;
			do {
				NumStr += LastChar;
				LastChar = NextC();
			} while (isdigit(LastChar) || LastChar == '.');

			AddToken(NumStr, TokenType::number, offset-(NumStr.length()+1));
			continue;
		}

		// Check for end of file.  Don't eat the EOF.
		if (LastChar == EOF) {
			AddToken("", TokenType::eof, offset);
			break;
		}

		// Otherwise, just return the character as its ascii value.
		//OldChar = ThisChar;
		ThisChar = LastChar;
		LastChar = NextC();

		//Check for line comment:
		if (ThisChar == '/' && LastChar == '/') {
			str cmt = "//";
			do {
				LastChar = NextC();
				cmt.push_back(LastChar);
			} while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
			if(tokenizeComment) {
				AddToken(cmt,TokenType::comment,offset - (cmt.length()));
			} else {
				comment = cmt;
			}
			LastChar = NextC();
			continue;
		}

		//Check for Multiline comment:
		if (ThisChar == '/' && LastChar == '*') {
			str cmt = "/*";
			do {
				ThisChar = LastChar;
				LastChar = NextC();
				cmt.push_back(LastChar);
			} while (LastChar != EOF && !(ThisChar == '*' && LastChar == '/'));
			if(tokenizeComment) {
				AddToken(cmt,TokenType::comment,offset - (cmt.length()));
			} else {
				comment = cmt;
			}
			LastChar = NextC();
			continue;
		}


		//Find operator consisting of 2 characters
		str doublec(1,ThisChar);
		doublec.push_back(LastChar);
		auto itr = _operators.find(doublec);
		if(itr!=_operators.end()) {
			LastChar = NextC(); 
			AddToken(itr->first,itr->second,offset-3); 
			continue;
		}
		
		//Find operators consisting of 1 character:
		itr = _operators.find(str(1,ThisChar));
		if(itr!=_operators.end()) {
			AddToken(itr->first,itr->second,offset-2);  
			continue;
		}
		

		AddToken(str(1,ThisChar), TokenType::unknown, offset-1);
	}
	reset();
}


Token * Lexer::getNextToken(void) {
	if(_ct == list.end()) {
		return nullptr;
	}
	Token * ret = &(*_ct);
	++_ct;
	return ret;
}

void Lexer::reset(void) {
	_ct = list.begin();
}

void Lexer::AddToken(const str & c,TokenType t, size_t o,bool isw) {
	list.emplace_back(c,t,o);
	list.back().isWord = isw;
	if(t==TokenType::identifier || t==TokenType::string) {
		list.back().comment = comment;
		comment.clear();
	}
}

/*int Lexer::NextC() {
	if (content.empty()) return EOF;
	offset++;
	char C = content.back();
	content.pop_back();
	return static_cast<int> (C);
}

int Lexer::peek(unsigned ahead ) {
	if (content.empty() || ahead >= content.size()) return EOF;
	unsigned idx = content.size()-(1+ahead);
	return static_cast<int> (content[idx]);
}*/


