// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "console.h"

#include "types.h"
#include <iostream>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif




using namespace util;

namespace util{
	
	void SetStdinEcho(bool enable = true)	{
		#ifdef _WIN32
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE); 
		DWORD mode;
		GetConsoleMode(hStdin, &mode);
		
		if( !enable )
			mode &= ~ENABLE_ECHO_INPUT;
		else
			mode |= ENABLE_ECHO_INPUT;
		
		SetConsoleMode(hStdin, mode );
		
		#else
		struct termios tty;
		tcgetattr(STDIN_FILENO, &tty);
		if( !enable )
			tty.c_lflag &= ~ECHO;
		else
			tty.c_lflag |= ECHO;
		
		(void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
		#endif
	}
	
	
	void getPassWord(str & passwd,const char * prompt) {
		passwd.clear();
		SetStdinEcho(false);
		std::cout << prompt;
		
		passwd.reserve(32);
		std::cin >> passwd;
		std::cout << std::endl;
		SetStdinEcho(true);
		

	}
}
