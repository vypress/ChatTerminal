/*
$Id: ConsoleIo_win.cpp 25 2010-08-19 19:39:59Z avyatkin $

Implementation of global functions from the consoleio namespace, that works with the console or redirected input/output

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#include <time.h>

#include <iostream>
#include <deque>
#include <set>
#include <vector>
#include <algorithm>
#include <string>

#include "NetworkIo.h"
#include "USER_INFO.h"
#include "CHANNEL_INFO.h"
#include "ConsoleIo.h"
#include "StrResources.h"

#include <WinDef.h>
#include <Winnt.h>
#include <WinBase.h>
#include <Wincon.h>

using namespace std;
using namespace resources;

namespace consoleio
{
	// A handle to the standard input
	HANDLE hStdout = 0;

	// A handle to the standard output
	HANDLE hStdin = 0;

	// Flag is true if the standard output is the console (not redirected)
	bool fConsoleOut = false;

	// Flag is true if the standard input is the console (not redirected)
	bool fConsoleIn = false;

	// Previous standard input mode of the console, before the application starts
	DWORD dwOldStdinMode = 0;

	// Current standard input mode of the console
	DWORD dwStdinMode = dwOldStdinMode;

	// Vertical cursor position where text begin entered
	volatile SHORT yStartTyping = 0;

	// Horizontal cursor position where text begin entered
	volatile SHORT xStartTyping = 0;

	// Amount of typed characters
	volatile SHORT nTyped = 0;

	// Flag is true if Insert mode is active
	bool fInsertMode = false;

	// Maximum size of the stack of executed commands, if 0 then stack is disabled
	unsigned int nCommandLinesStackSize = 100;

	// Stack of executed commands
	deque<wstring> CommandLines;

	// Currently selected command from the stack
	deque<wstring>::const_iterator posInCommandLines = CommandLines.begin();//currently selected command from the stack

	// Default text color, white on black background
	unsigned char byteTextColor = 0x0F;

	// Empty spaces filling character
	const wchar_t wchFill = L' ';//L'-';

	// Controls access to the print_bytes(), print_line(), console_read_input(), and other functions
	CRITICAL_SECTION PrintLinesMonitor::cs_ = {0};

	/**
	Keeps typed text, removes it from the console, set the cursor at the beginning
	@printed - reference to a buffer that receives the typed text, this buffer must be freed using delete[] operator
	@cursor_pos - reference to a variable that receives an old position of the cursor regarding of the typed text beginning
	@attribute - reference to a variable that receives the attributes of the characters written to a screen buffer
	@return - length of the typed text
	*/
	SHORT console_clear_typed_text(WCHAR*& printed, SHORT& cursor_pos, WORD& attribute)
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
		if(!GetConsoleScreenBufferInfo(hStdout, &csbi))
		{
			wcout << wszErrCsbi << hex << GetLastError() << endl;
			return -1;
		}

		attribute = csbi.wAttributes;

		SHORT& max_width = csbi.dwMaximumWindowSize.X;

		SHORT lines = max_width > 0 ? (nTyped + xStartTyping)/max_width : 0;
		SHORT last_pos = max_width > 0 ? (nTyped + xStartTyping)%max_width : 0;

		if(last_pos) ++lines;
		if(lines<1) return 0;

		//get cursor position in the printed text
		cursor_pos = (csbi.dwCursorPosition.Y - yStartTyping)*max_width + csbi.dwCursorPosition.X - xStartTyping;
		if(cursor_pos<0) cursor_pos = 0;

		printed = new WCHAR[max_width*lines - xStartTyping +1];// 1 + Terminating Null
		printed[max_width*lines - xStartTyping] = 0;

		COORD dwBufferSize = { max_width, lines };
		COORD dwBufferCoord = { 0, 0 }; 
		SMALL_RECT rcRegion = { 0, yStartTyping, max_width-1, yStartTyping+lines-1};

		CHAR_INFO* buffer = new CHAR_INFO[max_width*lines];

		ReadConsoleOutput( hStdout, (CHAR_INFO *)buffer, dwBufferSize, dwBufferCoord, &rcRegion );

		//remember and clear typed text
		SHORT printed_len = 0;
		for(int y=0; y<lines; y++)
		{
			for(int x=0; x<max_width; x++)
			{
				int index = y*max_width + x;
				if(index >= xStartTyping)
				{
					if(index < nTyped + xStartTyping)
					{
						printed[index-xStartTyping] = buffer[index].Char.UnicodeChar;
						printed_len++;
					}
					else
						printed[index-xStartTyping] = 0;
				}

				buffer[index].Char.UnicodeChar = wchFill;
				buffer[index].Attributes = csbi.wAttributes;
			}
		}

		//clear typed text
		WriteConsoleOutput( hStdout, buffer, dwBufferSize, dwBufferCoord, &rcRegion );
		delete[] buffer;

		xStartTyping = 0;
		nTyped = 0;

		COORD sPos = {xStartTyping, yStartTyping};
		SetConsoleCursorPosition( hStdout, sPos );

		return printed_len;
	}

	/**
	Wrapper around the console_clear_typed_text() function,
	does not restore previously typed text, prints the invitation for a new command
	@strTyped - reference to a string that receives the typed text
	@return - length of the typed text
	*/
	int console_clear_typed_text(wstring& strTyped)
	{
		WORD attribute = 0;
		SHORT cursor_pos = 0;
		WCHAR* printed = 0;
		int printed_len = console_clear_typed_text(printed, cursor_pos, attribute);

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

		//print the invitation
		const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();
		if(pacchinfo && pacchinfo->name)
		{
			wcout<<pacchinfo->name;
		}

		wcout<<wchInvite;

		CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
		if(GetConsoleScreenBufferInfo(hStdout, &csbi))
		{
			yStartTyping = csbi.dwCursorPosition.Y;
			xStartTyping = csbi.dwCursorPosition.X;
		}
		else
			wcout << wszErrCsbi << hex << GetLastError() << endl;

		return printed_len;
	}

	/**
	Moves the typed text to the right for one character regarding the cursor position.
	Called before inserting one character at the cursor position.
	@lines - number of lines to shift
	@csbi - information about the console output screen buffer
	*/
	void console_scroll_to_right(const SHORT lines, const CONSOLE_SCREEN_BUFFER_INFO& csbi)
	{
		const COORD& dwCursorPosition = csbi.dwCursorPosition;
		const SHORT& srWindowRight = csbi.srWindow.Right;

		/*example: there are 4 blocks
		b at right is removed
		csbi.dwCursorPosition.X = 3
		>a_bbbbbbbc      >aN_bbbbbbb
		eebbbbbbbbc      ceebbbbbbbb
		eebbbbbbbbc      ceebbbbbbbb
		eebbb            ceebbb     
		*/

		// Set the fill character and attributes.
		CHAR_INFO chiFill = {wchFill, csbi.wAttributes};

		CHAR_INFO* buffer = 0;
		SHORT lines_to_move = yStartTyping + lines - dwCursorPosition.Y;
		if(lines_to_move>0)
		{
			//determine and remember 'c' characters
			COORD dwBufferSize = { 1, lines_to_move };
			COORD dwBufferCoord = { 0, 0 };
			SMALL_RECT rcRegion = { srWindowRight, dwCursorPosition.Y, srWindowRight+1, yStartTyping+lines-1 };

			buffer = new CHAR_INFO[1*lines_to_move];

			ReadConsoleOutput( hStdout, (CHAR_INFO *)buffer, dwBufferSize, dwBufferCoord, &rcRegion );
			////////////////////////////////////////
		}

		//determine and scroll 'b' characters
		SMALL_RECT srctScrollRect = {dwCursorPosition.X-1, dwCursorPosition.Y, srWindowRight, yStartTyping+lines-1};
		SMALL_RECT srctClipRect = {dwCursorPosition.X-1, dwCursorPosition.Y, srWindowRight, yStartTyping+lines-1};

		// Scroll up one character.
		ScrollConsoleScreenBuffer(
			hStdout,         // screen buffer handle
			&srctScrollRect, // scrolling rectangle
			&srctClipRect,   // clipping rectangle
			dwCursorPosition,       // top left destination cell
			&chiFill);       // fill character and color
		///////////////////////////////////////

		//determine and scroll 'e' characters
		if((dwCursorPosition.X>1) && (dwCursorPosition.Y<yStartTyping+lines-1))
		{
			SMALL_RECT srctScrollRect = {0, dwCursorPosition.Y+1, dwCursorPosition.X-2, yStartTyping+lines-1};
			SMALL_RECT srctClipRect = {0, dwCursorPosition.Y+1, dwCursorPosition.X-1, yStartTyping+lines-1};
			COORD coordDest = {1, dwCursorPosition.Y+1};

			ScrollConsoleScreenBuffer(
				hStdout,         // screen buffer handle
				&srctScrollRect, // scrolling rectangle
				&srctClipRect,   // clipping rectangle
				coordDest,       // top left destination cell
				&chiFill);       // fill character and color
		}

		//restore 'c' at ends of lines
		if(buffer)
		{
			COORD dwBufferSize = { 1, lines_to_move };
			COORD dwBufferCoord = { 0, 0 };
			SMALL_RECT rcRegion = { 0, dwCursorPosition.Y+1, 1, yStartTyping+lines};

			WriteConsoleOutput( hStdout, (CHAR_INFO *)buffer, dwBufferSize, dwBufferCoord, &rcRegion );

			delete[] buffer;
		}
	}

	/**
	Moves the typed text to the left for one character regarding the cursor position.
	Called after deleting one character at the cursor position.
	@lines - number of lines to shift
	@csbi - information about the console output screen buffer
	*/
	void console_scroll_to_left(const SHORT lines, const CONSOLE_SCREEN_BUFFER_INFO& csbi)
	{
		const COORD& dwCursorPosition = csbi.dwCursorPosition;
		const SHORT& srWindowRight = csbi.srWindow.Right;

		/*example: there are 4 blocks
		b at right is removed
		csbi.dwCursorPosition.X = 2
		>a_bbbbbbbb     >a_bbbbbbbc
		ceebbbbbbbb     eebbbbbbbbc
		ceebbbbbbbb     eebbbbbbbbc
		ceebbb          eebbb
		*/

		// Set the fill character and attributes.
		CHAR_INFO chiFill = {wchFill, csbi.wAttributes};

		CHAR_INFO* buffer = 0;
		SHORT lines_to_move = yStartTyping + lines -1 - dwCursorPosition.Y;
		if(lines_to_move>0)
		{
			//determine and remember 'c' characters
			COORD dwBufferSize = { 1, lines_to_move };
			COORD dwBufferCoord = { 0, 0 };
			SMALL_RECT rcRegion = { 0, dwCursorPosition.Y+1, 1, yStartTyping+lines };

			buffer = new CHAR_INFO[1*lines_to_move];

			ReadConsoleOutput( hStdout, (CHAR_INFO *)buffer, dwBufferSize, dwBufferCoord, &rcRegion );
			////////////////////////////////////////

			//determine and scroll 'e' characters
			if(dwCursorPosition.X>0)
			{
				SMALL_RECT srctScrollRect = {1, dwCursorPosition.Y+1, dwCursorPosition.X, yStartTyping+lines-1};
				SMALL_RECT srctClipRect = {0, dwCursorPosition.Y+1, dwCursorPosition.X, yStartTyping+lines-1};
				COORD coordDest = {0, dwCursorPosition.Y+1};

				ScrollConsoleScreenBuffer(
					hStdout,         // screen buffer handle
					&srctScrollRect, // scrolling rectangle
					&srctClipRect,   // clipping rectangle
					coordDest,       // top left destination cell
					&chiFill);       // fill character and color
			}
		}

		//determine and scroll 'b' characters
		SMALL_RECT srctScrollRect = {dwCursorPosition.X+1, dwCursorPosition.Y, srWindowRight, yStartTyping+lines-1};
		SMALL_RECT srctClipRect = {dwCursorPosition.X, dwCursorPosition.Y, srWindowRight, yStartTyping+lines-1};

		// Scroll up one character.
		ScrollConsoleScreenBuffer(
			hStdout,         // screen buffer handle
			&srctScrollRect, // scrolling rectangle
			&srctClipRect,   // clipping rectangle
			dwCursorPosition,       // top left destination cell
			&chiFill);       // fill character and color
		///////////////////////////////////////

		//restore 'c' at ends of lines
		if(buffer)
		{
			COORD dwBufferSize = { 1, lines_to_move };
			COORD dwBufferCoord = { 0, 0 };
			SMALL_RECT rcRegion = { srWindowRight, dwCursorPosition.Y, srWindowRight+1, yStartTyping+lines-1};

			WriteConsoleOutput( hStdout, (CHAR_INFO *)buffer, dwBufferSize, dwBufferCoord, &rcRegion ); 

			delete[] buffer;
		}
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
			console_clear_typed_text(strTyped);

			if(strTyped.length()>0)
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

			return 0;
		}

		if (ch_in == 0x0009) //Tab
		{
			return 0;
		}

		if (ch_in == 0x0008) //Backspace
		{
			CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
			if(!GetConsoleScreenBufferInfo(hStdout, &csbi))
			{
				wcout << wszErrCsbi << hex << GetLastError()  << endl;
				return -1;
			}

			if(csbi.dwCursorPosition.Y <= yStartTyping && csbi.dwCursorPosition.X <= xStartTyping) return 0;

			SHORT lines = (nTyped + xStartTyping + 1)/csbi.dwMaximumWindowSize.X;//cursor takes one position
			SHORT last_pos = (nTyped + xStartTyping + 1)%csbi.dwMaximumWindowSize.X;//cursor takes one position

			if(last_pos) ++lines;

			if(lines<1) return 0;

			//line changed
			if(csbi.dwCursorPosition.X<1)
			{
				--csbi.dwCursorPosition.Y;
				csbi.dwCursorPosition.X = csbi.srWindow.Right;
			}
			else
				--csbi.dwCursorPosition.X;

			//move the cursor
			SetConsoleCursorPosition( hStdout, csbi.dwCursorPosition );
			nTyped--;

			/*example: there are 4 blocks
			csbi.dwCursorPosition.X = 2
			>aa_bbbbbbb     >a_bbbbbbbc
			ceebbbbbbbb     eebbbbbbbbc
			ceebbbbbbbb     eebbbbbbbbc
			ceebbb          eebbb
			*/

			console_scroll_to_left(lines, csbi);
			return 0;
		}

		CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
		if(!GetConsoleScreenBufferInfo(hStdout, &csbi))
		{
			wcout << wszErrCsbi << hex << GetLastError()  << endl;
			return -1;
		}

		SHORT lines = (nTyped + xStartTyping + 1)/csbi.dwMaximumWindowSize.X;//cursor takes one position
		SHORT last_pos = (nTyped + xStartTyping + 1)%csbi.dwMaximumWindowSize.X;//cursor takes one position

		if(last_pos) ++lines;

		if(lines<1) return 0;

		if(nTyped+xStartTyping >(csbi.dwCursorPosition.Y - yStartTyping)*csbi.dwMaximumWindowSize.X+csbi.dwCursorPosition.X)
		{
			//line changed
			if(csbi.dwCursorPosition.X>csbi.srWindow.Right)
			{
				++csbi.dwCursorPosition.Y;
				csbi.dwCursorPosition.X = 0;
			}
			else
				++csbi.dwCursorPosition.X;

			if(!fInsertMode)
			{
				console_scroll_to_right(lines, csbi);
				++nTyped;
			}
		}
		else
			++nTyped;

		wcout<<ch_in;

		/*
		//If the conversion is not possible in the current locale, wctomb returns –1 and errno is set to EILSEQ.
		char* mbchar = new char[MB_CUR_MAX];
		int size = wctomb(mbchar, ch_in);

		if(size<0 && errno==EILSEQ)
		{
			mbchar[0]=' ';
			size = 1;
		}

		DWORD cWritten = 0;
		BOOL bResult = WriteFile(hStdout, mbchar, size, &cWritten, NULL);

		delete[] mbchar;

		if(size>0)
		{
			if(bResult)
			{
				++nTyped;
			}
			else
			{
				return -1;
			}
		}
		*/

		return 0;
	}

	/**
	Processes a keyboard event in the console input buffer.
	@wVirtualKeyCode - a virtual-key code that identifies the given key in a device-independent manner
	@return - 0 if successful, error code otherwise
	*/
	int console_process_key_event(WORD wVirtualKeyCode)
	{
		switch(wVirtualKeyCode)
		{
			case VK_PRIOR: //(21) PAGE UP key
			break;

			case VK_NEXT: //(22) PAGE DOWN key
			break;

			case VK_END: //(23) END key
				{
					CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
					if(!GetConsoleScreenBufferInfo(hStdout, &csbi))
					{
						wcout << wszErrCsbi << hex << GetLastError()  << endl;
						return -1;
					}

					COORD coordDest = {(xStartTyping+nTyped)%csbi.dwMaximumWindowSize.X, yStartTyping+(nTyped+xStartTyping)/csbi.dwMaximumWindowSize.X};
					//move the cursor
					SetConsoleCursorPosition( hStdout, coordDest );
				}
			break;

			case VK_HOME: //(24) HOME key
				{
					COORD coordDest = {xStartTyping, yStartTyping};
					//move the cursor
					SetConsoleCursorPosition( hStdout, coordDest );
				}
			break;

			case VK_LEFT: //(25) LEFT ARROW key
				{
					CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
					if(!GetConsoleScreenBufferInfo(hStdout, &csbi))
					{
						wcout << wszErrCsbi << hex << GetLastError()  << endl;
						return -1;
					}

					// The destination for the scroll rectangle is one column left.
					COORD coordDest = {csbi.dwCursorPosition.X-1, csbi.dwCursorPosition.Y};

					if(csbi.dwCursorPosition.Y>yStartTyping && yStartTyping>0)
					{
						if(csbi.dwCursorPosition.X<1)
						{
							coordDest.X = csbi.dwMaximumWindowSize.X-1;
							--coordDest.Y;
						}
					}
					else
					{
						if(csbi.dwCursorPosition.X <= xStartTyping) return 0;
						//if(csbi.dwCursorPosition.X<1) return 0;
					}

					//move the cursor
					SetConsoleCursorPosition( hStdout, coordDest );
				}
			break;

			case VK_RIGHT: //(27) RIGHT ARROW key
				{
					CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
					if(!GetConsoleScreenBufferInfo(hStdout, &csbi))
					{
						wcout << wszErrCsbi << hex << GetLastError()  << endl;
						return -1;
					}

					SHORT lines = (nTyped + xStartTyping)/csbi.dwMaximumWindowSize.X;
					SHORT last_pos = (nTyped + xStartTyping)%csbi.dwMaximumWindowSize.X;

					if(csbi.dwCursorPosition.Y<yStartTyping+lines)
					{
						if(csbi.dwCursorPosition.X>=csbi.srWindow.Right)
						{
							++csbi.dwCursorPosition.Y;
							csbi.dwCursorPosition.X = 0;
						}
						else
							++csbi.dwCursorPosition.X;

						//move the cursor
						SetConsoleCursorPosition( hStdout, csbi.dwCursorPosition );
					}
					else
					{
						if(csbi.dwCursorPosition.X<last_pos)
						{
							++csbi.dwCursorPosition.X;

							//move the cursor
							SetConsoleCursorPosition( hStdout, csbi.dwCursorPosition );
						}
					}
				}
			break;

			case VK_UP: //(26) UP ARROW key
				if(posInCommandLines>CommandLines.begin())
				{
					wstring strTyped;
					console_clear_typed_text(strTyped);

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
					wcout<<strCmd;
					nTyped = static_cast<SHORT>(strCmd.length());
				}
			break;

			case VK_DOWN: //(28) DOWN ARROW key
				if(posInCommandLines<CommandLines.end())
				{
					if(posInCommandLines+1<CommandLines.end())
					{
						wstring strTyped;
						console_clear_typed_text(strTyped);

						const wstring& strCmd = *++posInCommandLines;
						wcout<<strCmd;
						nTyped = static_cast<SHORT>(strCmd.length());
					}
				}
			break;

			case VK_INSERT: //(2D) INS key
				fInsertMode=!fInsertMode;
			break;

			case VK_DELETE: //(2E) DEL key
				{
					CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
					if(!GetConsoleScreenBufferInfo(hStdout, &csbi))
					{
						wcout << wszErrCsbi << hex << GetLastError()  << endl;
						return -1;
					}

					SHORT lines = (nTyped+xStartTyping+1)/csbi.dwMaximumWindowSize.X;//cursor takes one position
					SHORT last_pos = (nTyped+xStartTyping+1)%csbi.dwMaximumWindowSize.X;//cursor takes one position

					if(last_pos) ++lines;

					if(lines<1) return 0;

					if(nTyped+xStartTyping < (csbi.dwCursorPosition.Y-yStartTyping)*csbi.dwMaximumWindowSize.X+csbi.dwCursorPosition.X+1)
						return 0;

					nTyped--;

					console_scroll_to_left(lines, csbi);
				}
			break;
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

		WORD wOldTextAttributes = 0;
		SHORT cursor_pos = 0;
		WCHAR* printed = 0;
		SHORT printed_len = 0;

		if(fConsoleOut)
		{
			printed_len = console_clear_typed_text(printed, cursor_pos, wOldTextAttributes);

			WORD wAttributes = selected ? ((0x000F&color)<<4)|((0x00F0&wOldTextAttributes)>>4) : (0x000F&color)|(0x00F0&wOldTextAttributes);

			if(((0x00F0&wAttributes)>>4) == (0x000F&wAttributes))//background and foreground are the same
			{
				//to improve visibility
				if( wAttributes & FOREGROUND_INTENSITY )
					wAttributes&=~FOREGROUND_INTENSITY;
				else
					wAttributes|=FOREGROUND_INTENSITY;
			}

			SetConsoleTextAttribute(hStdout, MAKEWORD(LOBYTE(wAttributes), HIBYTE(wOldTextAttributes)));//keep up background
		}

		int nResult = 0;
		if(format && *format)
		{
			nResult = vwprintf(format, argptr);
			wcout<<endl;
		}

		if(fConsoleOut)
		{
			//restore old attributes
			SetConsoleTextAttribute(hStdout, wOldTextAttributes);

			//print the invitation
			const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();
			if(pacchinfo && pacchinfo->name)
			{
				wcout<<pacchinfo->name;
			}

			wcout<<wchInvite;

			CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
			if(GetConsoleScreenBufferInfo(hStdout, &csbi))
			{
				yStartTyping = csbi.dwCursorPosition.Y;
				xStartTyping = csbi.dwCursorPosition.X;
			}
			else
				wcout << wszErrCsbi << hex << GetLastError() << endl;

			//restore typed text
			if(printed)
			{
				if(printed_len>0)
				{
					nTyped = printed_len;
					wcout<<printed;
					//restore cursor position
					if(cursor_pos>=0 && cursor_pos<printed_len)
					{
						SHORT lines = csbi.dwMaximumWindowSize.X > 0 ? (cursor_pos + xStartTyping)/csbi.dwMaximumWindowSize.X : 0;
						SHORT last_pos = csbi.dwMaximumWindowSize.X > 0 ? (cursor_pos + xStartTyping)%csbi.dwMaximumWindowSize.X : 0;

						COORD sPos = {last_pos, yStartTyping+lines};
						if((nTyped<1) || ((sPos.X < xStartTyping) && (sPos.Y == yStartTyping)))
						{
							_ASSERTE(!"Wrong cursor position!");
							sPos.X = xStartTyping;
						}

						SetConsoleCursorPosition( hStdout, sPos );
					}
				}
				delete[] printed;
			}
		}

		return nResult;
	}

	int console_initialize()
	{
		// Get handles to STDIN and STDOUT.
		hStdin = GetStdHandle(STD_INPUT_HANDLE);
		if(hStdin == INVALID_HANDLE_VALUE)
		{
			wcout<< wszErrInputStream << hex << GetLastError();
			return -1;
		}

		hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hStdout == INVALID_HANDLE_VALUE)
		{
			wcout<< wszErrOutputStream << hex << GetLastError();
			return -1;
		}

		//If an application handles multilingual output that can be redirected,
		//determine whether the output handle is a console handle
		//(one method is to call the GetConsoleMode function and check whether it succeeds)
		DWORD dwOutMode = 0;
		if (GetConsoleMode(hStdout, &dwOutMode))
		{
			// Save the current text colors.
			CONSOLE_SCREEN_BUFFER_INFO csbi = {0};
			if (!GetConsoleScreenBufferInfo(hStdout, &csbi)) 
			{
				wcout<<wszErrCsbi<< hex << GetLastError()<<endl;
				return -1;
			}

			fConsoleOut = true;

			byteTextColor = LOBYTE(csbi.wAttributes);

			yStartTyping = csbi.dwCursorPosition.Y;

			//For normal working with Russian and other languages
			//setlocale( LC_ALL, ".OCP" );//For the console
			// Set the active codepage locale to the stream
			//wcout.imbue(locale(".OCP"));
		}
		else
		{
			//For normal working with Russian and other languages
			//setlocale( LC_ALL, ".ACP" );//For redirected output

			// Set the active codepage locale to the stream
			//wcout.imbue(locale(".ACP"));
		}

		// Turn off the line input and echo input modes
		if (GetConsoleMode(hStdin, &dwOldStdinMode)) 
		{
			fConsoleIn = true;
			dwStdinMode = dwOldStdinMode;

			if(fConsoleOut)
			{
				dwStdinMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
			}
			else
			{
				//do not print anything (echoed characters) to console
				//when output redirected to file
				dwStdinMode &= ~ENABLE_ECHO_INPUT;
			}

			if (!SetConsoleMode(hStdin, dwStdinMode)) 
			{
				dwStdinMode = dwOldStdinMode;
				wcout << wszErrSetMode << hex << GetLastError();
				return -1;
			}

			// Set the active codepage locale to the stream
			wcin.imbue(locale(".OCP"));
		}
		else
		{
			// Set the active codepage locale to the stream
			//wcin.imbue(locale(".ACP"));
		}

		PrintLinesMonitor::Initialize();

		return 0;
	}

	void console_finalize()
	{
		PrintLinesMonitor::Delete();

		if(!fConsoleIn) return;

		// Restore the original console mode.
		SetConsoleMode(hStdin, dwOldStdinMode);
	}

	void console_clear_screen()
	{
		PrintLinesMonitor PRINT_LINES_MONITOR;

		if(fConsoleOut)
		{
			system("cls");//clears the screen
			//system("clear");//clears the screen
			xStartTyping = 0;
			yStartTyping = 0;
			nTyped = 0;

			FlushConsoleInputBuffer(hStdout);
		}

		if(fConsoleIn)
		{
			FlushConsoleInputBuffer(hStdin);

			// Restore the original console mode.
			SetConsoleMode(hStdin, dwStdinMode);
		}

		print_line(NULL);
	}

	void console_generate_event()
	{
		PrintLinesMonitor PRINT_LINES_MONITOR;

		DWORD dwNumberOfEventsWritten = 0;
		INPUT_RECORD recBuffer = {0};
		BOOL bRes = WriteConsoleInput(hStdin, &recBuffer, 1, &dwNumberOfEventsWritten);
		DBG_UNREFERENCED_LOCAL_VARIABLE(bRes);
		_ASSERT(bRes);
	}

	void console_skip_input(int seconds)
	{
		print_line_selected(wszWaitingFor, seconds);

		while (seconds-- > 0)
		{
			Sleep(1000);

			if(!fConsoleIn) continue;

			DWORD cRead = 1;
			INPUT_RECORD recBuffer = {0};
			while (seconds && (cRead>0))
			{
				//pull an input event
				if(!PeekConsoleInput(hStdin, &recBuffer, 1, &cRead))
				{
					seconds = 0;
					break;
				}

				//check whether to stop or continue waiting
				if(cRead<1) continue;

				//remove the event
				if(!ReadConsoleInput(hStdin, &recBuffer, 1, &cRead))
				{
					seconds = 0;
					break;
				}

				if(cRead<1) continue;
				if(KEY_EVENT != recBuffer.EventType) continue;
				if(!recBuffer.Event.KeyEvent.bKeyDown) continue;

				if((recBuffer.Event.KeyEvent.dwControlKeyState&LEFT_CTRL_PRESSED)
					|| (recBuffer.Event.KeyEvent.dwControlKeyState&RIGHT_CTRL_PRESSED))
				{
					if(recBuffer.Event.KeyEvent.wVirtualKeyCode == 0x08//Break
						|| recBuffer.Event.KeyEvent.wVirtualKeyCode == 0x43)//C
					{
						seconds = 0;
						break;
					}
				}
			}
		}

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

		//Read console input
		static DWORD cRead = 0;
		static INPUT_RECORD recBuffer = {0};

		if(!ReadConsoleInput(hStdin, &recBuffer, 1, &cRead)) return -1;

		if(KEY_EVENT != recBuffer.EventType) return 0;

		if(!recBuffer.Event.KeyEvent.bKeyDown) return 0;

		if((recBuffer.Event.KeyEvent.dwControlKeyState&LEFT_CTRL_PRESSED)
			|| (recBuffer.Event.KeyEvent.dwControlKeyState&RIGHT_CTRL_PRESSED))
		{
			if(recBuffer.Event.KeyEvent.wVirtualKeyCode == 0x08//Break
				|| recBuffer.Event.KeyEvent.wVirtualKeyCode == 0x43)//C
			{
				return -2;
			}
		}

		int result = 0;

		PrintLinesMonitor PRINT_LINES_MONITOR;

		//arrow keys, delete etc.
		if(0 == recBuffer.Event.KeyEvent.uChar.UnicodeChar)
		{
			result = console_process_key_event(recBuffer.Event.KeyEvent.wVirtualKeyCode);
		}
		else
		{
			//character key
			for(WORD i=0; i<recBuffer.Event.KeyEvent.wRepeatCount; i++)
			{
				result = console_process_input_char(recBuffer.Event.KeyEvent.uChar.UnicodeChar, strLine);
			}
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

		int nResult = vprint_line(byteTextColor, false, format, argptr);

		va_end(argptr);

		return nResult;
	}

	int print_line_selected(const wchar_t* format, ...)
	{
		va_list argptr;
		va_start(argptr, format);

		int nResult = vprint_line(byteTextColor, true, format, argptr);

		va_end(argptr);

		return nResult;
	}

	void print_bytes(const unsigned char* bytes, unsigned int len)
	{
		wchar_t wszLine[65]={0};
		for(unsigned int i=0; i<len; i++)
		{
			wchar_t* seek = wszLine;
			for(i; (i<len) && (seek+1<wszLine+_ARRAYSIZE(wszLine)); i++)
			{
				if(bytes[i]>0x20 && __isascii(bytes[i]))//__isascii only works normally here
					*seek = bytes[i];
				else
					*seek = L'.';
				++seek;
			}

			// to avoid escape and formatting sequences
			print_line(L"%ls", wszLine);
			memset(wszLine, 0, sizeof(wszLine));
		}

		for(unsigned int i=0; i<len; i++)
		{
			wchar_t* seek = wszLine;
			for(i; (i<len) && (seek+4<wszLine+_ARRAYSIZE(wszLine)); i++)
			{
				swprintf(seek, 4, L"%02X ", bytes[i]);
				seek+=3;
			}

			print_line(wszLine);
			memset(wszLine, 0, sizeof(wszLine));
		}
	}
}
