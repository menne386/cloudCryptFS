// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#ifndef __lexer_h
#define __lexer_h

#include "main.h"
#include <list>
namespace script {

	enum class TokenType : int {
		unknown, eof, eos, compound_l, compound_r, par_l, par_r, subscr_l, subscr_r,
		//Commands: 
		include, function, _return, _if, _elseif, _else, catchevent, _foreach, _while, as, index, resourcedef, timer, event, warning, _for, extractor,inline_event,_break,_typen,
		_defined,expect,
		//Primary
		identifier, number, string, defvar,
		//Other keywords:
		_class,extends, seperator,jsonassign , assign,deref,construct,_private,_public,_final,_null,_template,_operator,_brq,
		_attrib,
		//Operators:
		op_incr, op_decr,
		op_add, op_sub, op_div, op_mul, op_mod, op_and, op_or, op_xor,
		cop_add, cop_sub, cop_mul, cop_div, cop_mod, cop_and, cop_or, cop_xor,
		lop_l, lop_g, lop_le, lop_ge, lop_ne, lop_e,
		lop_and, lop_or, lop_not,
		//XmlStuff
		/*xml_dtd,xml_cdatal,xml_cdatar,*/comment
		
		

	};
#ifndef __lexer_cpp
	extern const char * g_TokenTypeString[];
#endif

	struct Token {
		Token() = default;
		Token(const str & c, TokenType t, size_t o);
		Token(const Token & in)  = default;
		Token(const Token * in) {
			*this = *in;
		}
		operator const str &() const { return content; }
		str content;
		TokenType type;
		size_t offset = 0;
		str comment;
		bool isWord= false;
	};

	class Lexer {
	public:
		Token * getNextToken(void);
		void reset(void);
		Lexer(const str & c,bool tokenizeComment = false);
	private:
		//int NextC();
		void AddToken(const str & c,TokenType t, size_t o,bool isw=false);
		std::list<Token> list;
		std::list<Token>::iterator _ct;
		//str content = "";
		//int offset = 0;
		str comment = "";

	};

}
#endif
