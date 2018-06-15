// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#define __lexer_cpp
#include "lexer.h"

#include <string.h>
#include <algorithm>
#include "modules/util/str.h"
using namespace script;

namespace script{
const char * g_TokenTypeString[] = {
	"unknown", "EOF", ";", "{", "}", "(", ")", "[", "]",
	//Commands:
	"include", "function", "return", "if", "elseif", "else", "catchevent", "_foreach", "_while", "as", "index", "resourcedef", "timerdef", "eventdef", "warning", "for","extractor","inline_event","_break","typename",
	"defined",
	//Primary:
	"identifier", "number", "string", "variabletype",
	//Other keywords:
	"class","extends", "seperator", "json_assign", "assign","deref","construct","private","public","final",
	"__attribute__",
	//Operators:
	"op_incr", "op_decr",
	"op_add", "op_sub", "op_div", "op_mul", "op_mod", "op_and", "op_or", "op_xor",
	"cop_add", "cop_sub", "cop_mul", "cop_div", "cop_mod", "cop_and", "cop_or", "cop_xor",
	"lop_l", "lop_g", "lop_le", "lop_ge", "lop_ne", "lop_e",
	"lop_and", "lop_or", "lop_not",
	"xml_dtd","xml_cdatal","xml_cdatar","comment"

};
};

Token::Token(const str & c, TokenType t, size_t o) : content(c), type(t), offset(o), comment(""){
	content.shrink_to_fit();
}


#define singlecharop(OP,TT) if(ThisChar==OP) { AddToken(str(1,OP),TT,offset-2);  continue;}
#define doublecharop(OP1,OP2,TT) if(ThisChar == OP1 && LastChar==OP2) { LastChar = NextC(); AddToken(str(1,OP1)+str(1,OP2),TT,offset-3); continue;}
#define wordtoken(OP,TT) if(isl == OP) { AddToken(OP,TT,offset-(strlen(OP)+1),true); continue;}

Lexer::Lexer(const str & c,bool tokenizeComment) {
	//CLOG("Lex:",c);
	//Constuctor
	//Load entire textfile into mem
	int OldChar = ' ';
	int LastChar = ' ';
	int ThisChar = ' ';
	content = str(c.rbegin(), c.rend());
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
						case '\\': st+= "\\"; break;
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
				wordtoken("#uctexture", TokenType::resourcedef);
				wordtoken("#texture", TokenType::resourcedef);
				wordtoken("#sound", TokenType::resourcedef);
				wordtoken("#htmlfile", TokenType::resourcedef);
				wordtoken("#htmlelement", TokenType::resourcedef);
				wordtoken("#model", TokenType::resourcedef);
				wordtoken("#modelanim", TokenType::resourcedef);
				wordtoken("#warning", TokenType::warning);
				wordtoken("#event", TokenType::event);
				wordtoken("#timer", TokenType::timer);
				wordtoken("#include", TokenType::include);
				
				wordtoken("for", TokenType::_for);
				wordtoken("foreach", TokenType::_foreach);
				wordtoken("as", TokenType::as);
				wordtoken("return", TokenType::_return);
				wordtoken("index", TokenType::index);
				wordtoken("while", TokenType::_while);
				wordtoken("catchevent", TokenType::catchevent);
				wordtoken("catchallevents", TokenType::catchevent);
				wordtoken("function", TokenType::function);
				wordtoken("if", TokenType::_if);
				wordtoken("elseif", TokenType::_elseif);
				wordtoken("else", TokenType::_else);
				wordtoken("class", TokenType::_class);
				wordtoken("extends", TokenType::extends);
				wordtoken("_event", TokenType::inline_event);
				wordtoken("break", TokenType::_break);
				wordtoken("typename", TokenType::_typen);
				wordtoken("deref", TokenType::deref);
				wordtoken("construct", TokenType::construct);
				wordtoken("private:", TokenType::_private);
				wordtoken("public:", TokenType::_public);
				wordtoken("final", TokenType::_final);
				wordtoken("__attribute__", TokenType::_attrib);
				wordtoken("defined", TokenType::_defined);
				//Datatypes:
				wordtoken("playerglobal", TokenType::defvar);
				wordtoken("moveglobal", TokenType::defvar);
				wordtoken("global", TokenType::defvar);
				wordtoken("string", TokenType::defvar);
				wordtoken("resource", TokenType::defvar);
				wordtoken("int", TokenType::defvar);
				wordtoken("float", TokenType::defvar);
				wordtoken("var", TokenType::defvar);
				wordtoken("object", TokenType::defvar);
				wordtoken("ref", TokenType::defvar);
				wordtoken("auto", TokenType::defvar);
				
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
		OldChar = ThisChar;
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
		if(tokenizeComment) {
			if (ThisChar == '<' && LastChar == '!' &&  peek(0) == '-'&&  peek(1) == '-') {//<!--
				str cmt = "<!";
				do {
					OldChar = ThisChar;
					ThisChar = LastChar;
					LastChar = NextC();
					cmt.push_back(LastChar);
				} while (LastChar != EOF && !(OldChar == '-' && ThisChar == '-' && LastChar == '>'));
				AddToken(cmt,TokenType::comment,offset - (cmt.length()));
				LastChar = NextC();				
				continue;
			}
			
			if (ThisChar=='<' && LastChar == '?' && peek(0) == 'x' && peek(1) == 'm' && peek(2) == 'l' ) {//<?xml blah blah blah ?>
				str cmt="<?";
				do {
					ThisChar = LastChar;
					LastChar = NextC();
					cmt.push_back(LastChar);
				} while (LastChar != EOF && !(ThisChar == '?' && LastChar == '>'));
				if(tokenizeComment) {
					AddToken(cmt,TokenType::xml_dtd,offset - (cmt.length()));
				}
				LastChar = NextC();
				continue;
			}
			if (ThisChar=='<' && LastChar == '!' && peek(0)=='['&& peek(1)=='C' && peek(2)=='D'&& peek(3)=='A'&& peek(4)=='T'&& peek(5)=='A' &&peek(6)=='[' ) { //<![CDATA[
				AddToken("<![CDATA[", TokenType::xml_cdatal, offset-2);
				for(int a= 0;a<7;a++) {
					LastChar = NextC();
				}
				continue;
			}
			if (ThisChar=='<' && LastChar == 'h' && peek(0)=='t'&& peek(1)=='m' && peek(2)=='l'&& peek(3)=='>' ) { //<html>
				AddToken("<html>", TokenType::xml_dtd, offset-2);
				for(int a= 0;a<4;a++) {
					LastChar = NextC();
				}
				continue;
			}
			if (OldChar == ']' && ThisChar==']' && LastChar == '>') { // ]]>
				AddToken("]]>", TokenType::xml_cdatar, offset-3);
				LastChar = NextC();
				continue;
			}
		}

		//All Double char operators:
		doublecharop('-', '>', TokenType::extractor);
		//Logical ops
		doublecharop('<', '=', TokenType::lop_le);
		doublecharop('>', '=', TokenType::lop_ge);
		doublecharop('!', '=', TokenType::lop_ne);
		doublecharop('=', '=', TokenType::lop_e);
		doublecharop('&', '&', TokenType::lop_and);
		doublecharop('|', '|', TokenType::lop_or);

		//Increment, decrement
		doublecharop('+', '+', TokenType::op_incr);
		doublecharop('-', '-', TokenType::op_decr);
		//Arithmetic compound ops:
		doublecharop('+', '=', TokenType::cop_add);
		doublecharop('-', '=', TokenType::cop_sub);
		doublecharop('/', '=', TokenType::cop_div);
		doublecharop('*', '=', TokenType::cop_mul);
		doublecharop('%', '=', TokenType::cop_mod);
		doublecharop('&', '=', TokenType::cop_and);
		doublecharop('|', '=', TokenType::cop_or);
		doublecharop('^', '=', TokenType::cop_xor);

		//All Single char operators.
		singlecharop(',', TokenType::seperator);
		singlecharop('=', TokenType::assign);
		singlecharop(':', TokenType::jsonassign);
		singlecharop('{', TokenType::compound_l);
		singlecharop('}', TokenType::compound_r);
		singlecharop('(', TokenType::par_l);
		singlecharop(')', TokenType::par_r);
		singlecharop('[', TokenType::subscr_l);
		singlecharop(']', TokenType::subscr_r);
		//Logic:
		singlecharop('<', TokenType::lop_l);
		singlecharop('>', TokenType::lop_g);
		singlecharop('!', TokenType::lop_not);

		//Arithmetic ops:
		singlecharop('+', TokenType::op_add);
		singlecharop('-', TokenType::op_sub);
		singlecharop('/', TokenType::op_div);
		singlecharop('*', TokenType::op_mul);
		singlecharop('%', TokenType::op_mod);
		singlecharop('&', TokenType::op_and);
		singlecharop('|', TokenType::op_or);
		singlecharop('^', TokenType::op_xor);
		//end of statement
		singlecharop(';', TokenType::eos);




		str st;
		st = ThisChar;

		AddToken(st, TokenType::unknown, offset-1);
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

int Lexer::NextC() {
	if (content.empty()) return EOF;
	offset++;
	char C = content.back();
	content.pop_back();
	return static_cast<int> (C);
}

int Lexer::peek(size_t ahead ) {
	if (content.empty() || ahead >= content.size()) return EOF;
	size_t idx = content.size()-((size_t)1+ahead);
	return static_cast<int> (content[idx]);
}


