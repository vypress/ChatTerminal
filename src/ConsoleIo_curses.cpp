/*
$Id: ConsoleIo_curses.cpp 36 2011-08-09 07:35:21Z avyatkin $

Implementation of global functions from the consoleio namespace, that works with the console or redirected input/output

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/
#include <time.h>

#include <iostream>
#include <deque>
#include <string>
#include <algorithm>

#include <set> // for CHANNEL_INFO.h and USER_INFO.h
#include <vector> // for CHANNEL_INFO.h

#include <string.h> //memset, memcpy, memcmp
#include "WinNixDefs.h"
#include "NixHelper.h"

#include "NetworkIo.h"
#include "USER_INFO.h"
#include "CHANNEL_INFO.h"
#include "ConsoleIo.h"
#include "StrResources.h"

#include <errno.h>
#include <assert.h>

//#define NCURSES_NO_SETBUF
#ifndef _XOPEN_SOURCE_EXTENDED
	#define _XOPEN_SOURCE_EXTENDED
#endif

#include <cursesw.h>

using namespace std;
using namespace resources;

namespace consoleio
{
	pthread_mutex_t PrintLinesMonitor::m_ = {0};//PTHREAD_MUTEX_INITIALIZER;

	class CursesGetChMonitor
	{
	public:
		CursesGetChMonitor()
		{
			pthread_mutex_lock( &m_ );
		}

		~CursesGetChMonitor()
		{
			pthread_mutex_unlock( &m_ );
		}
	
	private:
		CursesGetChMonitor(const CursesGetChMonitor&);
		CursesGetChMonitor& operator=(const CursesGetChMonitor&);
		CursesGetChMonitor* operator&();

		static pthread_mutex_t m_;
	};
	
	pthread_mutex_t CursesGetChMonitor::m_ = PTHREAD_MUTEX_INITIALIZER;
	
	// Flag is true if the standard output is the console (not redirected)
	bool fConsoleOut = false;
	
	// Flag is true if the standard input is the console (not redirected)
	bool fConsoleIn = false;
	
	// Vertical cursor position where text begin entered
	int yStartTyping = 0;
	
	// Horizontal cursor position where text begin entered
	int xStartTyping = 0;
	
	// Amount of typed characters
	int nTyped = 0;
	
	// Flag is true if Insert mode is active
	bool fInsertMode = false;
	
	// Maximum size of the stack of executed commands, if 0 then stack is disabled
	unsigned int nCommandLinesStackSize = 100;
	
	// Stack of executed commands
	deque<wstring> CommandLines;
	
	// Currently selected command from the stack
	deque<wstring>::const_iterator posInCommandLines = CommandLines.begin();//currently selected command from the stack
	
	// Empty spaces filling character
	cchar_t cchtFill = {0,{L' ',0,0,0,0}};//L' ';//L'-';
	
	// Default text color, white
	short defTextColor = 0x07;

	// Timeout value for getch() function
	const int get_char_timeout = 200;
	
	// Max length of a printed screen line
	const unsigned int print_line_buf_size = 4096;
	
	// Screen buffer - vector of lines
	// Size of screen line in characters is stored as a first element
	std::deque<cchar_t*> screenbuf;
	
	// Maximum number of lines stored in the screenbuf
	unsigned int screenbuf_lines = 1024;
	
	// First visible line of the screen buffer
	int current_line = 0;
	
	/**
	Keeps typed text, removes it from the console, set the cursor at the beginning
	@printed - reference to a buffer that receives the typed text, this buffer must be freed using delete[] operator
	@cursor_pos - reference to a variable that receives an old position of the cursor regarding of the typed text beginning
	@return - length of the typed text
	*/
	int console_clear_typed_text(wchar_t*& printed, int& cursor_pos)
	{
		//get cursor position in the printed text
		int cury=0, curx=0;
		getyx(stdscr, cury, curx);
	
		int row=0,col=0;
		getmaxyx(stdscr,row,col);
	
		cursor_pos = (cury - yStartTyping)*col + curx - xStartTyping;
		if(cursor_pos<0) cursor_pos = 0;
	
		wchar_t* wchstr = 0;
	
		cchar_t wcval={0};
		mvin_wch(yStartTyping, xStartTyping, &wcval);
	
		//clear the invitation in case of multiline text
		cchar_t* out_line  = new cchar_t[xStartTyping];
		for(int i=0; i<xStartTyping; ++i)
		{
			memcpy(&out_line[i], &cchtFill, sizeof(cchar_t));
		}
	
		mvadd_wchnstr(yStartTyping, 0, out_line, xStartTyping);
		delete[] out_line;
	
		if(nTyped)
		{
			wchstr = new wchar_t[nTyped+1];
			memset(wchstr, 0x00, (nTyped+1)*sizeof(wchar_t));
	
			wchstr[0] = wcval.chars[0];
		}
	
		mvadd_wch(yStartTyping, xStartTyping, &cchtFill);
	
		for(int i=1; i<nTyped; i++ )
		{
			in_wch(&wcval);
			wchstr[i] = wcval.chars[0];
			add_wch(&cchtFill);
		}
		
		printed = wchstr;
		move(yStartTyping, 0);
	
		int result = nTyped;
		nTyped = 0;
		return result;
	}
	
	/**
	Wrapper around the console_clear_typed_text() function,
	does not restore previously typed text, prints the invitation for a new command
	@strTyped - reference to a string that receives the typed text
	@bMove - if true then an invitation is moved to the left bottom corner (after the Enter key)
	@return - length of the typed text
	*/
	int console_clear_typed_text(wstring& strTyped, bool bMove)
	{
		int cursor_pos = 0;
		wchar_t* printed = 0;
		int printed_len = console_clear_typed_text(printed, cursor_pos);
	
		if(printed)
		{
			if(printed_len>0)
			{
				//remove trailing spaces
				while(iswspace(*(printed+printed_len-1)))
				{
					*(printed+printed_len-1) = 0;
					--printed_len;
				}

				strTyped.assign(printed, printed_len);
			}
	
			delete[] printed;
		}

		if(bMove)
		{
			int row=0,col=0;
			getmaxyx(stdscr,row,col);
			move(row-1,0);
		}

		//print the invitation
		const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();
		if(pacchinfo && pacchinfo->name)
			addwstr(pacchinfo->name);

		//wcout<<wchInvite;
		addch(wchInvite);
	
		//cchar_t cch = {0};
		//cch.attr = attrs;
		//cch.chars[0] = wchInvite;

		//add_wchar(&cch);

		getyx(stdscr, yStartTyping, xStartTyping);

		return printed_len;
	}
	
	/**
	Moves the typed text to the right for one character regarding the cursor position.
	Called before inserting one character at the cursor position.
	@count - number of characters to shift
	*/
	bool console_scroll_to_right(int& cury, int const& curx, int const& row, int const& col, int count)
	{
		int lines = (nTyped+xStartTyping+1)/col;//cursor takes one position
		int last_pos = (nTyped+xStartTyping+1)%col;//cursor takes one position
		if(last_pos) ++lines;

		//mvprintw(0, 0, "x%d y%d t%d ys%d xs%d col%d lines%d                 ", curx, cury, nTyped, yStartTyping, xStartTyping, col, lines);
		//move(cury, curx);

		if(lines>row-1 && last_pos+count-1>col) return false;//cursor takes one position
	
		if(last_pos==0 && yStartTyping+lines>=row)//scroll to up
		{
			if(yStartTyping<1) return false;
			scrl(1);
			--yStartTyping;
			--cury;
			++current_line;
			//mvprintw(0, 0, "x%d y%d t%d ys%d xs%d col%d lines%d       scrolled          ", curx, cury, nTyped, yStartTyping, xStartTyping, col, lines);
			//move(cury, curx);

		}

		lines-=(cury - yStartTyping);//lines to move

		if(lines<1) return false;

		//save typed text
		cchar_t* wcval_prev = new cchar_t[col*lines+1];//+NULL cchar_t
		memset(wcval_prev, 0x00, sizeof(cchar_t)*(col*lines+1));
	
		for(int i =0; i<lines; i++)
		{
			if(OK != mvin_wchnstr(cury+i,0,wcval_prev+col*i, col))
			{
				delete[] wcval_prev;
				return false;
			}
		}
	
		/*example: there are 4 blocks
		curx = 2, count=2
		>a_bbbbbbcc      >aNN_bbbbbb
		eebbbbbbbcc      cceebbbbbbb
		eebbbbbbbcc      cceebbbbbbb
		eebbbbbbbcc      cceebbbbbbb
				cc
		*/
	
		int err = OK;
		//determine and scroll 'b' characters
		for(int i = 0; i<lines; i++)
		{
			err = mvadd_wchnstr(cury+i,curx+count, wcval_prev+col*i+curx , col-curx-count);
		}
	
		//determine and scroll 'e' characters
		for(int i = 1; i<lines; i++)
		{
			err= mvadd_wchnstr(cury+i,count, wcval_prev+col*i, curx);
		}
	
		//determine and scroll 'c' characters
		for(int i = 1; i<lines+1; i++)
		{
			err= mvadd_wchnstr(cury+i,0, wcval_prev+col*i -count, count);
		}
	
		delete[] wcval_prev;
		move(cury, curx);
	
		return true;
	}
	
	/**
	Moves the typed text to the left for one character regarding the cursor position.
	Called after deleting one character at the cursor position.
	@count - number of characters to shift
	*/
	bool console_scroll_to_left(int const& cury, int const& curx, int const& row, int const& col, int count)
	{
		int lines = (nTyped+xStartTyping+1)/col;//cursor takes one position
		int last_pos = (nTyped+xStartTyping+1)%col;//cursor takes one position
		if(last_pos) ++lines;
	
		lines-=(cury - yStartTyping);//lines to move
	
		//mvprintw(0, 0, "%d %d %d %d %d %d              ", curx, cury, nTyped, xStartTyping, col, lines);
		//move(cury, curx);
	
		if(lines<1) return false;
	
		//save typed text
		cchar_t* wcval_prev = new cchar_t[col*lines+1];//+NULL cchar_t
		memset(wcval_prev, 0x00, sizeof(cchar_t)*(col*lines+1));
	
		for(int i = 0; i<lines; i++)
		{
			if(OK != mvin_wchnstr(cury+i,0,wcval_prev+col*i, col))
			{
				delete[] wcval_prev;
				return false;
			}
		}
	
		/*example: there are 4 blocks
		bb at right is removed
		curx = 4, count=2
		>aaa_bbbbbb      >aaa_bbbbcc
		cceeeeebbbb      eeeeebbbbcc
		cceeeeebbbb      eeeeebbbbcc
		cceeeeebbbb      eeeeebbbbcc
		cc.....''''      .....''''
		*/
	
		/*example: there are 4 blocks
		bb at right is removed
		csbi.dwCursorPosition.X = 2
		>a_bbbbbbbb     >a_bbbbbbbc
		ceeebbbbbbb     eeebbbbbbbc
		ceeebbbbbbb     eeebbbbbbbc
		ceeebb'''''     eeebb'''''
		*/
	
		int err = OK;
		//determine and scroll 'b' characters
		for(int i = 0; i<lines; i++)
		{
			err = mvadd_wchnstr(cury+i,curx, wcval_prev+col*i+curx+count , col-curx-count);
		}
	
		//determine and scroll 'e' characters
		for(int i = 1; i<lines; i++)
		{
			err= mvadd_wchnstr(cury+i,0, wcval_prev+col*i+count, curx);
		}
	
		//determine and scroll 'c' characters
		for(int i = 0; i<lines-1; i++)
		{
			err= mvadd_wchnstr(cury+i,col-count, wcval_prev+col*(i+1), count);
		}
	
		delete[] wcval_prev;
	
		move(cury, curx);
		return true;
	}
	
	/**
	Processes a keyboard event in the console input buffer.
	key_code - a key code that identifies the given key in a curses manner
	@return - 0 if successful, error code otherwise
	*/
	int console_process_key_event(wchar_t key_code)
	{
		//printw("%o", key_code);
		//return 0;	
		switch(key_code)
		{
			case KEY_RESIZE:
			{
				//Doesn't work normally in nodelay mode with ncurses library 5.7 because of bug
				//assert(!"Window resized");
			}
			return 0;

			case KEY_NPAGE: // PAGE DOWN key
			{
				//Scroll one line up
				int line_to_print = current_line+yStartTyping;
				if(static_cast<size_t>(line_to_print) >= screenbuf.size()) break;
	
				int toscroll = yStartTyping;
				if(static_cast<size_t>(line_to_print+toscroll)>screenbuf.size()) toscroll=screenbuf.size()-line_to_print;
	
				int row=0,col=0;
				getmaxyx(stdscr,row,col);
	
				int cury=0, curx=0;
				getyx(stdscr, cury, curx);
		
				cchar_t* out_line = new cchar_t[col+1];
				memset(&out_line[col], 0x00, sizeof(cchar_t));
	
				//scroll up lines_to_print lines
				int i = 0;
				for( ; i<yStartTyping-toscroll; ++i)
				{
					if(OK != mvin_wchnstr(i+toscroll,0,out_line, col))
						break;
	
					mvadd_wchnstr(i,0, out_line, col);
				}
	
				//create buffer for empty line and empty space at right
				for(int k=0; k<col; ++k)
				{
					memcpy(&out_line[k], &cchtFill, sizeof(cchar_t));
				}
	
				for(int j=0; j<toscroll; j++)
				{
					int n = screenbuf[line_to_print+j][0].chars[0];
					mvadd_wchnstr(i+j, 0, screenbuf[line_to_print+j]+1, n);
		
					//insert an empty line or empty space at right
					//for(n; n<col; ++n)
					//{
					//	memcpy(&out_line[n], &cchtFill, sizeof(cchar_t));
					//}
		
					mvadd_wchnstr(i+j, n, out_line, col-n);
		
					++current_line;
				}
	
				delete[] out_line;
	
				move(cury, curx);
				refresh();
			}
			return 0;
	
			case KEY_PPAGE: // PAGE UP key
			{
				//Scroll one line down
				if(current_line<1) break;
	
				int toscroll = yStartTyping;
				if(toscroll>current_line) toscroll=current_line;
	
				int line_to_print = current_line-toscroll;
				if(line_to_print<0) break;
	
				int row=0,col=0;
				getmaxyx(stdscr,row,col);
	
				int cury=0, curx=0;
				getyx(stdscr, cury, curx);
	
				cchar_t* out_line = new cchar_t[col+1];
				memset(&out_line[col], 0x00, sizeof(cchar_t));
	
				//scroll down to lines_to_print lines
				int i = yStartTyping-1;
				for( ; i>toscroll-1; --i)
				{
					if(OK != mvin_wchnstr(i-toscroll,0,out_line, col))
						break;
	
					mvadd_wchnstr(i,0, out_line, col);
				}
	
				//create buffer for empty line and empty space at right
				for(int k=0; k<col; ++k)
				{
					memcpy(&out_line[k], &cchtFill, sizeof(cchar_t));
				}
	
				for(int j=0; j<toscroll; j++)
				{
					int n = screenbuf[line_to_print+toscroll-j-1][0].chars[0];
					mvadd_wchnstr(i-j, 0, screenbuf[line_to_print+toscroll-j-1]+1, n);
		
					//insert an empty line or empty space at right
					//for(n; n<col; ++n)
					//{
					//	memcpy(&out_line[n], &cchtFill, sizeof(cchar_t));
					//}
		
					mvadd_wchnstr(i-j, n, out_line, col-n);
		
					--current_line;
				}
	
				delete[] out_line;
	
				move(cury, curx);
				refresh();
			}
			return 0;
	
			case KEY_END: //END key
				{
					int row=0,col=0;
					getmaxyx(stdscr,row,col);
	
					//move the cursor
					move(yStartTyping+(nTyped+xStartTyping)/col, (xStartTyping+nTyped)%col);
				}
			return 0;
	
			case KEY_HOME: // HOME key
					//move the cursor
					move(yStartTyping, xStartTyping);
			return 0;
	
			case KEY_LEFT: // LEFT ARROW key
				{
					int row=0,col=0;
					getmaxyx(stdscr,row,col);

					int cury=0, curx=0;
					getyx(stdscr, cury, curx);

					if(cury>yStartTyping && yStartTyping>0)
					{
						if(curx<1)
						{
							curx = col;
							--cury;
						}
					}
					else
					{
						if(curx <= xStartTyping) return 0;
						//if(csbi.dwCursorPosition.X<1) return 0;
					}
	
					//move the cursor
					move(cury, --curx);
				}
			return 0;
	
			case KEY_RIGHT: // RIGHT ARROW key
				{
					int row=0,col=0;
					getmaxyx(stdscr,row,col);
	
					int cury=0, curx=0;
					getyx(stdscr, cury, curx);
	
					int lines = (nTyped+xStartTyping)/col;
					int last_pos = (nTyped+xStartTyping)%col;
	
					if(cury<yStartTyping+lines)
					{
						if(curx>=col-1)
						{
							++cury;
							curx = 0;
						}
						else
							++curx;
	
						//move the cursor
						move(cury, curx);
					}
					else
					{
						if(curx<last_pos)
						{
							++curx;
	
							//move the cursor
							move(cury, curx);
						}
					}
				}
			return 0;

			case KEY_UP: // UP ARROW key
				if(posInCommandLines>CommandLines.begin())
				{
					wstring strTyped;
					console_clear_typed_text(strTyped, false);
	
					if(strTyped.length()>0 && posInCommandLines>=CommandLines.end())
					{
						if(nCommandLinesStackSize<CommandLines.size()+1)
							CommandLines.pop_front();
	
						//posInCommandLines points to end()
						deque<wstring>::iterator removed_it = remove(CommandLines.begin(), CommandLines.end(), strTyped);
						CommandLines.erase(removed_it, CommandLines.end());
	
						CommandLines.push_back(strTyped);
	
						//set posInCommandLines again because of remove()
						posInCommandLines=CommandLines.end();
						//move to previous line
						--posInCommandLines;
					}
	
					const wstring& strCmd = *--posInCommandLines;
					//wcout<<strCmd;
					addwstr(strCmd.c_str());
					nTyped = static_cast<int>(strCmd.length());
	
					int row=0,col=0;
					getmaxyx(stdscr,row,col);
	
					int lines = (nTyped+xStartTyping+1)/col;//cursor takes one position
					int last_pos = (nTyped+xStartTyping+1)%col;//cursor takes one position
					if(last_pos) ++lines;
	
					if(yStartTyping+lines>row-1) yStartTyping=row-lines;
				}
			return 0;
	
			case KEY_DOWN: // DOWN ARROW key
				if(posInCommandLines<CommandLines.end())
				{
					if(posInCommandLines+1<CommandLines.end())
					{
						wstring strTyped;
						console_clear_typed_text(strTyped, false);
	
						const wstring& strCmd = *++posInCommandLines;
						//wcout<<strCmd;
						addwstr(strCmd.c_str());
						nTyped = static_cast<int>(strCmd.length());
	
						int row=0,col=0;
						getmaxyx(stdscr,row,col);
		
						int lines = (nTyped+xStartTyping+1)/col;//cursor takes one position
						int last_pos = (nTyped+xStartTyping+1)%col;//cursor takes one position
						if(last_pos) ++lines;
		
						if(yStartTyping+lines>row-1) yStartTyping=row-lines;
	
					}
				}
			return 0;
	
			case KEY_BACKSPACE:
				{
					int row=0,col=0;
					getmaxyx(stdscr,row,col);
				
					int cury=0, curx=0;
					getyx(stdscr, cury, curx);
				
					if(cury <= yStartTyping && curx <= xStartTyping) return 0;
				
					//line changed
					if(curx<1)
					{
						--cury;
						curx = col-1;
					}
					else
						--curx;
				
					//move the cursor
					move(cury, curx);
				
					console_scroll_to_left(cury, curx, row, col, 1);
					nTyped--;
				}
			return 0;
	
			case KEY_IC: // INS key
				fInsertMode=!fInsertMode;
			return 0;
	
			case KEY_DC: // DEL key
				{
					int row=0,col=0;
					getmaxyx(stdscr,row,col);
	
					int cury=0, curx=0;
					getyx(stdscr, cury, curx);
	
					if(nTyped+xStartTyping < (cury-yStartTyping)*col+curx+1)
						return 0;
	
					console_scroll_to_left(cury, curx, row, col, 1);
					nTyped--;
				}
			return 0;
		}
	
		return 0;
	}

	/**
	Processes a keyboard event which is translated to a Unicode character in the console input buffer.
	@ch_in - the translated Unicode character
	@strTyped - reference to a string that receives the typed text in case of line entered
	@return - 0 if successful, error code otherwise
	*/
	int console_process_input_char(wchar_t ch_in, wstring& strTyped)
	{
		if (ch_in == L'\r' || ch_in == L'\n')
		{
			int printed_len = console_clear_typed_text(strTyped, true);
	
			if(printed_len>0)
			{
				if(nCommandLinesStackSize>0) // if nCommandLinesStackSize==0 then Stack is disabled
				{
					if(nCommandLinesStackSize<CommandLines.size()+1)
						CommandLines.pop_front();
	
					deque<wstring>::iterator removed_it = remove(CommandLines.begin(), CommandLines.end(), strTyped);
					CommandLines.erase(removed_it, CommandLines.end());
	
					CommandLines.push_back(strTyped);
					posInCommandLines = CommandLines.end();
				}
	
				return static_cast<int>(strTyped.length());
			}
	
			return printed_len;
		}
	
		if (ch_in == 0x0009) //Tab
		{
			return 0;
		}
		
		//printw("%x", ch_in);
		if (ch_in == 0x0008 /*KEY_BACKSPACE*/) //Backspace
		{
			return console_process_key_event(KEY_BACKSPACE);
		}

		if (ch_in == 0x007f /*KEY_BACKSPACE*/) //Backspace
		{
			return console_process_key_event(KEY_BACKSPACE);
		}

		//char * keybound(int keycode, int count);
		//char *unctrl(chtype c);
		//char *key_name(wchar_t w);
	
		char* pc = unctrl(ch_in);
		int len = pc ? strlen(pc) : 0;//wcwidth(ch_in);
		if(len)
		{
			int row=0,col=0;
			getmaxyx(stdscr,row,col);
			
			int cury=0, curx=0;
			getyx(stdscr, cury, curx);
	
			cchar_t cch = {0};
			cch.attr = A_NORMAL;
			cch.chars[0] = ch_in;
		
			if(!fInsertMode)
			{
				if(!console_scroll_to_right(cury, curx, row, col, len)) return 0;
	
				nTyped+=len;
			}
			else
			{
				int diff = (cury-yStartTyping)*col+curx+len - nTyped-xStartTyping;
				if(diff > 0 )
					nTyped+=diff;
		
				if(cury>=row-1 && curx+len>col-1)
				{
					if(yStartTyping<1) return 0;
					--yStartTyping;
					++current_line;
				}
			}

			echo_wchar(&cch);
		}
	
		return 0;
	}
	
	/**
	Writes formatted output using a pointer to a list of arguments. Keeps previously typed text and the cursor position
	@color - text color
	@selected - if true the background-foreground inversion applied (text looks like selected)
	@format - format control
	@argptr - pointer to list of arguments
	@return - the number of characters written, not including the terminating null character,
			or a negative value if an output error occurs
	*/
	int vprint_line(unsigned char color, bool selected, const wchar_t* format, va_list argptr)
	{
		PrintLinesMonitor PRINT_LINES_MONITOR;
		CursesGetChMonitor GETCH_MONITOR;

		int nResult = 0;

		if(!fConsoleOut)
		{
			//redirected output
			if(format && *format)
			{
				nResult = vwprintf(format, argptr);
				wcout<<endl;
			}
			return nResult;
		}

		if(0==format || 0==*format)
		{
			wstring strTyped;
			console_clear_typed_text(strTyped, false);
			return nResult;
		}
	
		static wchar_t wline_buf[print_line_buf_size] = {0};
		nResult = vswprintf(wline_buf, _ARRAYSIZE(wline_buf), format, argptr);

		if(nResult<1) return nResult;

		//remove trailing NULL symbols that may be generated by vswprintf in case of wrong formatting
		//NULL symbols here may crash or hang ncurses library
		while(0 == wline_buf[nResult-1]) --nResult;
		if(nResult<1) return nResult;

		attr_t attrs = A_NORMAL;
	
		/* possible colors
		0 = Black	8 = Gray
		1 = Blue	9 = Light Blue
		2 = Green	A = Light Green
		3 = Aqua	B = Light Aqua
		4 = Red		C = Light Red
		5 = Purple	D = Light Purple
		6 = Yellow	E = Light Yellow
		7 = White	
		*/
	
		if(color>7)
		{
			attrs|=A_BOLD;
			color%=8;
		}
	
		if(selected)
			color+=8;
	
		//attrs|=COLOR_PAIR(color+1);
	
		short f=0, b=0;
		if((OK == pair_content(color+1, &f, &b)) && (f==b))
		{
			//if(attrs&A_BOLD)
			//	attrs&=~A_BOLD;
			//else
				attrs|=A_BOLD;
		}
	
		int row=0,col=0;
		getmaxyx(stdscr,row,col);
	
		int cury=0, curx=0;
		getyx(stdscr, cury, curx);
	
		cchar_t* out_line = 0;
	
		const wchar_t* seek = wline_buf;
	
		unsigned int pos_to_print = screenbuf.size() - current_line;

		while(seek-wline_buf < nResult)
		{
			//if(0 == out_line)
			{
				out_line = new cchar_t[1+col+1];
				//we keep size of a line to avoid screenbuf changing after size of a terminal changed
				out_line[0].chars[0] = col;
	
				memset(&out_line[1+col], 0x00, sizeof(cchar_t));
				for(int i=1; i<col+1; ++i)
				{
					memcpy(&out_line[i], &cchtFill, sizeof(cchar_t));
				}
			}
	
			if(screenbuf.size() == static_cast<size_t>(yStartTyping+current_line))
			{
				int lines_to_print = (nResult - (seek-wline_buf))/col;
				if((nResult - (seek-wline_buf))%col) ++lines_to_print;
				if(lines_to_print<1) break;
	
				//scroll up to lines_to_print lines
				for(int i = 0; i<yStartTyping - lines_to_print; i++)
				{
					if(OK != mvin_wchnstr(i+lines_to_print,0,out_line+1, col))
					{
						seek = wline_buf + nResult;//to break while()
						break;
					}
	
					mvadd_wchnstr(i,0, out_line+1, col);
				}

				current_line+=lines_to_print;
	
				pos_to_print = screenbuf.size() - current_line;//yStartTyping - lines_to_print;
			}
	
			int  i=1;
			for( ; (i<col+1) && *seek && (seek-wline_buf < nResult); i++)
			{
				if(*seek==L'\r')
				{
					++seek;
					if((seek-wline_buf < nResult) && (*seek==L'\n'))
						++seek;
					break;
				}
	
				if(*seek==L'\n')
				{
					++seek;
					break;
				}
	
				//out_line[i].attr = attrs;
				//out_line[i].chars[0] = *seek++;
				//out_line[i].chars[1] = 0;
				//out_line[i].chars[2] = 0;
				//out_line[i].chars[3] = 0;
				//out_line[i].chars[4] = 0;
				if(OK!=setcchar(&out_line[i], seek++, attrs, color+1, NULL))
				{
					//try to print space character
					if(OK!=setcchar(&out_line[i], L" ", attrs, color+1, NULL))
						break;
				}
			}
	
			for( ; i<col+1; i++)
			{
				memcpy(&out_line[i], &cchtFill, sizeof(cchar_t));
			}
	
			if(screenbuf.size() < static_cast<size_t>(yStartTyping+current_line))
			{
				mvadd_wchnstr(pos_to_print++, 0, out_line+1, col);
				//++current_line;
			}
	
			screenbuf.push_back(out_line);
			out_line = 0;//create new buffer for the next string
		}

		delete[] out_line;

		while(screenbuf.size() > screenbuf_lines)
		{
			if(--current_line<0)
			{
				//scroll up to one line
				cchar_t* line_buf = new cchar_t[col+1];
				memset(&line_buf[col], 0x00, sizeof(cchar_t));

				int i = 0;
				for( ; i<yStartTyping - 1; i++)
				{
					if(OK != mvin_wchnstr(i+1,0,line_buf, col))
					{
						break;
					}
			
					mvadd_wchnstr(i,0, line_buf, col);
				}

				int n = 0;
				if(screenbuf.size()>static_cast<size_t>(i+1))
				{
					n = screenbuf[i+1][0].chars[0];
					mvadd_wchnstr(i, 0, screenbuf[i+1]+1, n);
				}

				//insert an empty line or empty space at right
				for( ; n<col; ++n)
				{
					memcpy(&line_buf[n], &cchtFill, sizeof(cchar_t));
				}

				mvadd_wchnstr(i, n, line_buf, col-n);
	
				delete[] line_buf;
	
				current_line = 0;
			}
	
			delete[] screenbuf.front();
			screenbuf.pop_front();
		}
	
		move(cury, curx);
		refresh();
	
		return nResult;
	}
	
	/*
	void sig_winch(int signo)
	{
	struct winsize size;
	ioctl(fileno(stdout), TIOCGWINSZ, (char *) &size);
	resizeterm(size.ws_row, size.ws_col);
	}
	*/
	
	int console_initialize()
	{
		PrintLinesMonitor::Initialize();

		fConsoleOut = 1==isatty(fileno(stdout));
		fConsoleIn = 1==isatty(fileno(stdin));

		if(!fConsoleOut) return 0;

		WINDOW* scrwnd = initscr();
		DBG_UNREFERENCED_LOCAL_VARIABLE(scrwnd);

		//We are using KEY_RESIZE
		//signal(SIGWINCH, sig_winch);
	
		int err = start_color();
	
		//err = cbreak();
		err = raw();
	
		immedok(stdscr, FALSE);
	
		err = keypad(stdscr, TRUE);
	
		err = noecho();
		//err = echo();
	
		err = nonl();
	
		err = scrollok(stdscr, TRUE);
		idlok(stdscr, FALSE);//TRUE
		idcok(stdscr, FALSE);//TRUE

		timeout(get_char_timeout);
		//nodelay(stdscr, TRUE);

		err = getbkgrnd(&cchtFill);
		err = start_color();

		wchar_t pwch[CCHARW_MAX] = {0};
		attr_t attrs = 0;
		short fill_color_pair = 0;
		err =  getcchar(&cchtFill, pwch, &attrs, &fill_color_pair, NULL);
	
		/*
		#define COLOR_BLACK	0
		#define COLOR_RED	1
		#define COLOR_GREEN	2
		#define COLOR_YELLOW	3
		#define COLOR_BLUE	4
		#define COLOR_MAGENTA	5
		#define COLOR_CYAN	6
		#define COLOR_WHITE	7
		
		0 = Black	8 = Gray
		1 = Blue	9 = Light Blue
		2 = Green	A = Light Green
		3 = Aqua	B = Light Aqua
		4 = Red		C = Light Red
		5 = Purple	D = Light Purple
		6 = Yellow	E = Light Yellow
		7 = White	
		*/
	
		//create color pairs according DOS console colors
		short f=0, b=0;
		err = pair_content(fill_color_pair, &f, &b);
	
		defTextColor = f;

		err = init_pair(1, COLOR_BLACK, b);
		err = init_pair(2, COLOR_BLUE, b);
		err = init_pair(3, COLOR_GREEN, b);
		err = init_pair(4, COLOR_CYAN, b);
		err = init_pair(5, COLOR_RED, b);
		err = init_pair(6, COLOR_MAGENTA, b);
		err = init_pair(7, COLOR_YELLOW, b);
		err = init_pair(8, COLOR_WHITE, b);
		//colors for selected lines
		err = init_pair(9, COLOR_BLACK, f);
		err = init_pair(10, COLOR_BLUE, f);
		err = init_pair(11, COLOR_GREEN, f);
		err = init_pair(12, COLOR_CYAN, f);
		err = init_pair(13, COLOR_RED, f);
		err = init_pair(14, COLOR_MAGENTA, f);
		err = init_pair(15, COLOR_YELLOW, f);
		err = init_pair(16, COLOR_WHITE, f);

		int row=0,col=0;
		getmaxyx(stdscr,row,col);
	
		//resizeterm(300, col);
		//setscrreg(0, 300);

		//print the invitation
		cchar_t cch = {0};
		cch.attr = attrs;
		cch.chars[0] = wchInvite;

		mvadd_wch(row-1,0,&cch);
		getyx(stdscr, yStartTyping, xStartTyping);
		refresh();

		return 0;
	}

	template <class T> class delete_array_ptr
	{
	public:
		void operator()(T* p) { delete[] p; }
	};

	void console_finalize()
	{	
		PrintLinesMonitor::Delete();

		if(!fConsoleOut) return;

		CursesGetChMonitor GETCH_MONITOR;

		endwin();
	
		xStartTyping = 0;
		yStartTyping = 0;
		nTyped = 0;
		current_line = 0;

		std::for_each(screenbuf.begin(), screenbuf.end(), delete_array_ptr<cchar_t>());
		screenbuf.clear();
	}

	void console_clear_screen()
	{
		if(!fConsoleOut) return;
	
		PrintLinesMonitor PRINT_LINES_MONITOR;
		CursesGetChMonitor GETCH_MONITOR;

		clear();

		xStartTyping = 0;
		yStartTyping = 0;
		nTyped = 0;
		current_line = 0;

		std::for_each(screenbuf.begin(), screenbuf.end(), delete_array_ptr<cchar_t>());
		screenbuf.clear();

		wstring strTyped;
		console_clear_typed_text(strTyped, true);
	}
	
	void console_generate_event()
	{
	}

	void console_skip_input(int seconds)
	{
		print_line_selected(wszWaitingFor, seconds);
	
		//timeout(0);

		while (seconds-- > 0)
		{
			sleep(1);
			//napms(1000);
	
			if(!fConsoleIn) continue;
	
			if(fConsoleOut)
			{
				{//scope for the CursesGetChMonitor objetc
					CursesGetChMonitor GETCH_MONITOR;

					wint_t wch = 0;
					while(seconds && (ERR != get_wch(&wch)))
					{
						if(0x03 == wch) //Ctrl+C
						{
							seconds = 0;
							break;
						}
					}
				}

				//Check if print_line function is ready to print text
				PrintLinesMonitor PRINT_LINES_MONITOR;
			}

			//napms(get_char_timeout);
		}
	
		//timeout(get_char_timeout);
	
		print_line_selected(wszWaitingFinished);
	}
	
	int console_read_input(std::wstring& strLine)
	{
		if(!fConsoleOut)
		{
			//In case of redirected output we can get lines by normal way because of nothing is displayed
			//Standard way is fully acceptable;
			if(!std::getline(std::wcin, strLine)) return -1;
			return static_cast<int>(strLine.length());
		}
	
		int result = 0;
		int get_wch_result = ERR;
	
		{//scope for the CursesGetChMonitor objetc
			CursesGetChMonitor GETCH_MONITOR;
	
			wint_t wch = 0;
			get_wch_result = get_wch(&wch);
		
			if(OK == get_wch_result)
			{
				if(0x03 == wch) //Ctrl+C
				{
					result = -1;
				}
				else
				{
					//character key
					return console_process_input_char(wch, strLine);
				}
			}
			else
				if(KEY_CODE_YES == get_wch_result)
				{
					//arrow keys, delete etc.
					result = console_process_key_event(wch);
				}
		}
	
		if(get_wch_result == ERR)
		{
			//Check if print_line function is ready to print text
			PrintLinesMonitor PRINT_LINES_MONITOR;
			//napms(get_char_timeout);
		}
	
		return result;
	}
	
	int print_line(unsigned char color, bool selected, const wchar_t* format, ...)
	{
		va_list argptr;
		va_start(argptr, format);

		int nResult = vprint_line(color, selected, format, argptr);

		va_end(argptr);

		return nResult;
	}

	int print_line(const wchar_t* format, ...)
	{
		va_list argptr;
		va_start(argptr, format);

		int nResult = vprint_line(defTextColor, false, format, argptr);

		va_end(argptr);

		return nResult;
	}

	int print_line_selected(const wchar_t* format, ...)
	{
		va_list argptr;
		va_start(argptr, format);

		int nResult = vprint_line(defTextColor, true, format, argptr);

		va_end(argptr);

		return nResult;
	}

	void print_bytes(const unsigned char* bytes, unsigned int len)
	{
		wchar_t wszLine[65]={0};
		for(unsigned int i=1; i<=len; i++)
		{
			--i;

			wchar_t* seek = wszLine;
			for( ; (i<len) && (seek+1<wszLine+_ARRAYSIZE(wszLine)); i++)
			{
				if(bytes[i]>0x20 && isascii(bytes[i]))//isascii only works normally here
					*seek = bytes[i];
				else
					*seek = L'.';
				++seek;
			}

			// to avoid escape and formatting sequences
			print_line(L"%ls", wszLine);
			memset(wszLine, 0, sizeof(wszLine));
		}

		for(unsigned int i=1; i<=len; i++)
		{
			--i;

			wchar_t* seek = wszLine;
			for( ; (i<len) && (seek+4<wszLine+_ARRAYSIZE(wszLine)); i++)
			{
				swprintf(seek, 4, L"%02X ", bytes[i]);
				seek+=3;
			}

			print_line(wszLine);
			memset(wszLine, 0, sizeof(wszLine));
		}
	}
}
