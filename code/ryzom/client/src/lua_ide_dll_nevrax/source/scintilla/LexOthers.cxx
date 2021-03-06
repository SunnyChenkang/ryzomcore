// Ryzom - MMORPG Framework <http://dev.ryzom.com/projects/ryzom/>
// Copyright (C) 2010  Winch Gate Property Limited
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Scintilla source code edit control
/** @file LexOthers.cxx
 ** Lexers for batch files, diff results, properties files, make files and error lists.
 ** Also lexer for LaTeX documents.
 **/
// Copyright 1998-2001 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "Platform.h"

#include "PropSet.h"
#include "Accessor.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "SciLexer.h"

static inline bool AtEOL(Accessor &styler, unsigned int i) {
	return (styler[i] == '\n') ||
		((styler[i] == '\r') && (styler.SafeGetCharAt(i + 1) != '\n'));
}

static void ColouriseBatchLine(
    char *lineBuffer,
    unsigned int lengthLine,
    unsigned int startLine,
    unsigned int endPos,
    WordList &keywords,
    Accessor &styler) {

	unsigned int i = 0;
	unsigned int state = SCE_BAT_DEFAULT;

	while ((i < lengthLine) && isspacechar(lineBuffer[i])) {	// Skip initial spaces
		i++;
	}
	if (lineBuffer[i] == '@') {	// Hide command (ECHO OFF)
		styler.ColourTo(startLine + i, SCE_BAT_HIDE);
		i++;
		while ((i < lengthLine) && isspacechar(lineBuffer[i])) {	// Skip next spaces
			i++;
		}
	}
	if (lineBuffer[i] == ':') {
		// Label
		if (lineBuffer[i + 1] == ':') {
			// :: is a fake label, similar to REM, see http://content.techweb.com/winmag/columns/explorer/2000/21.htm
			styler.ColourTo(endPos, SCE_BAT_COMMENT);
		} else {	// Real label
			styler.ColourTo(endPos, SCE_BAT_LABEL);
		}
	} else {
		// Check if initial word is a keyword
		char wordBuffer[21];
		unsigned int wbl = 0, offset = i;
		// Copy word in buffer
		for (; offset < lengthLine && wbl < 20 &&
		        !isspacechar(lineBuffer[offset]); wbl++, offset++) {
			wordBuffer[wbl] = static_cast<char>(tolower(lineBuffer[offset]));
		}
		wordBuffer[wbl] = '\0';
		// Check if it is a comment
		if (CompareCaseInsensitive(wordBuffer, "rem") == 0) {
			styler.ColourTo(endPos, SCE_BAT_COMMENT);
			return ;
		}
		// Check if it is in the list
		if (keywords.InList(wordBuffer)) {
			styler.ColourTo(startLine + offset - 1, SCE_BAT_WORD);	// Regular keyword
		} else {
			// Search end of word (can be a long path)
			while (offset < lengthLine &&
			        !isspacechar(lineBuffer[offset])) {
				offset++;
			}
			styler.ColourTo(startLine + offset - 1, SCE_BAT_COMMAND);	// External command / program
		}
		// Remainder of the line: colourise the variables.

		while (offset < lengthLine) {
			if (state == SCE_BAT_DEFAULT && lineBuffer[offset] == '%') {
				styler.ColourTo(startLine + offset - 1, state);
				if (isdigit(lineBuffer[offset + 1])) {
					styler.ColourTo(startLine + offset + 1, SCE_BAT_IDENTIFIER);
					offset += 2;
				} else if (lineBuffer[offset + 1] == '%' &&
				           !isspacechar(lineBuffer[offset + 2])) {
					// Should be safe, as there is CRLF at the end of the line...
					styler.ColourTo(startLine + offset + 2, SCE_BAT_IDENTIFIER);
					offset += 3;
				} else {
					state = SCE_BAT_IDENTIFIER;
				}
			} else if (state == SCE_BAT_IDENTIFIER && lineBuffer[offset] == '%') {
				styler.ColourTo(startLine + offset, state);
				state = SCE_BAT_DEFAULT;
			} else if (state == SCE_BAT_DEFAULT &&
			           (lineBuffer[offset] == '*' ||
			            lineBuffer[offset] == '?' ||
			            lineBuffer[offset] == '=' ||
			            lineBuffer[offset] == '<' ||
			            lineBuffer[offset] == '>' ||
			            lineBuffer[offset] == '|')) {
				styler.ColourTo(startLine + offset - 1, state);
				styler.ColourTo(startLine + offset, SCE_BAT_OPERATOR);
			}
			offset++;
		}
		//		if (endPos > startLine + offset - 1) {
		styler.ColourTo(endPos, SCE_BAT_DEFAULT);		// Remainder of line, currently not lexed
		//		}
	}

}
// ToDo: (not necessarily at beginning of line) GOTO, [IF] NOT, ERRORLEVEL
// IF [NO] (test) (command) -- test is EXIST (filename) | (string1)==(string2) | ERRORLEVEL (number)
// FOR %%(variable) IN (set) DO (command) -- variable is [a-zA-Z] -- eg for %%X in (*.txt) do type %%X
// ToDo: %n (parameters), %EnvironmentVariable% colourising
// ToDo: Colourise = > >> < | "

static void ColouriseBatchDoc(
    unsigned int startPos,
    int length,
    int /*initStyle*/,
    WordList *keywordlists[],
    Accessor &styler) {

	char lineBuffer[1024];
	WordList &keywords = *keywordlists[0];

	styler.StartAt(startPos);
	styler.StartSegment(startPos);
	unsigned int linePos = 0;
	unsigned int startLine = startPos;
	for (unsigned int i = startPos; i < startPos + length; i++) {
		lineBuffer[linePos++] = styler[i];
		if (AtEOL(styler, i) || (linePos >= sizeof(lineBuffer) - 1)) {
			// End of line (or of line buffer) met, colourise it
			lineBuffer[linePos] = '\0';
			ColouriseBatchLine(lineBuffer, linePos, startLine, i, keywords, styler);
			linePos = 0;
			startLine = i + 1;
		}
	}
	if (linePos > 0) {	// Last line does not have ending characters
		ColouriseBatchLine(lineBuffer, linePos, startLine, startPos + length - 1,
		                   keywords, styler);
	}
}

static void ColouriseDiffLine(char *lineBuffer, int endLine, Accessor &styler) {
	// It is needed to remember the current state to recognize starting
	// comment lines before the first "diff " or "--- ". If a real
	// difference starts then each line starting with ' ' is a whitespace
	// otherwise it is considered a comment (Only in..., Binary file...)
	if (0 == strncmp(lineBuffer, "diff ", 3)) {
		styler.ColourTo(endLine, SCE_DIFF_COMMAND);
	} else if (0 == strncmp(lineBuffer, "--- ", 3)) {
		styler.ColourTo(endLine, SCE_DIFF_HEADER);
	} else if (0 == strncmp(lineBuffer, "+++ ", 3)) {
		styler.ColourTo(endLine, SCE_DIFF_HEADER);
	} else if (0 == strncmp(lineBuffer, "***", 3)) {
		styler.ColourTo(endLine, SCE_DIFF_HEADER);
	} else if (lineBuffer[0] == '@') {
		styler.ColourTo(endLine, SCE_DIFF_POSITION);
	} else if (lineBuffer[0] == '-') {
		styler.ColourTo(endLine, SCE_DIFF_DELETED);
	} else if (lineBuffer[0] == '+') {
		styler.ColourTo(endLine, SCE_DIFF_ADDED);
	} else if (lineBuffer[0] != ' ') {
		styler.ColourTo(endLine, SCE_DIFF_COMMENT);
	} else {
		styler.ColourTo(endLine, SCE_DIFF_DEFAULT);
	}
}

static void ColouriseDiffDoc(unsigned int startPos, int length, int, WordList *[], Accessor &styler) {
	char lineBuffer[1024];
	styler.StartAt(startPos);
	styler.StartSegment(startPos);
	unsigned int linePos = 0;
	for (unsigned int i = startPos; i < startPos + length; i++) {
		lineBuffer[linePos++] = styler[i];
		if (AtEOL(styler, i) || (linePos >= sizeof(lineBuffer) - 1)) {
			// End of line (or of line buffer) met, colourise it
			lineBuffer[linePos] = '\0';
			ColouriseDiffLine(lineBuffer, i, styler);
			linePos = 0;
		}
	}
	if (linePos > 0) {	// Last line does not have ending characters
		ColouriseDiffLine(lineBuffer, startPos + length - 1, styler);
	}
}

static void ColourisePropsLine(
    char *lineBuffer,
    unsigned int lengthLine,
    unsigned int startLine,
    unsigned int endPos,
    Accessor &styler) {

	unsigned int i = 0;
	while ((i < lengthLine) && isspacechar(lineBuffer[i]))	// Skip initial spaces
		i++;
	if (i < lengthLine) {
		if (lineBuffer[i] == '#' || lineBuffer[i] == '!' || lineBuffer[i] == ';') {
			styler.ColourTo(endPos, SCE_PROPS_COMMENT);
		} else if (lineBuffer[i] == '[') {
			styler.ColourTo(endPos, SCE_PROPS_SECTION);
		} else if (lineBuffer[i] == '@') {
			styler.ColourTo(startLine + i, SCE_PROPS_DEFVAL);
			if (lineBuffer[++i] == '=')
				styler.ColourTo(startLine + i, SCE_PROPS_ASSIGNMENT);
			styler.ColourTo(endPos, SCE_PROPS_DEFAULT);
		} else {
			// Search for the '=' character
			while ((i < lengthLine) && (lineBuffer[i] != '='))
				i++;
			if ((i < lengthLine) && (lineBuffer[i] == '=')) {
				styler.ColourTo(startLine + i - 1, SCE_PROPS_DEFAULT);
				styler.ColourTo(startLine + i, 3);
				styler.ColourTo(endPos, SCE_PROPS_DEFAULT);
			} else {
				styler.ColourTo(endPos, SCE_PROPS_DEFAULT);
			}
		}
	} else {
		styler.ColourTo(endPos, SCE_PROPS_DEFAULT);
	}
}

static void ColourisePropsDoc(unsigned int startPos, int length, int, WordList *[], Accessor &styler) {
	char lineBuffer[1024];
	styler.StartAt(startPos);
	styler.StartSegment(startPos);
	unsigned int linePos = 0;
	unsigned int startLine = startPos;
	for (unsigned int i = startPos; i < startPos + length; i++) {
		lineBuffer[linePos++] = styler[i];
		if (AtEOL(styler, i) || (linePos >= sizeof(lineBuffer) - 1)) {
			// End of line (or of line buffer) met, colourise it
			lineBuffer[linePos] = '\0';
			ColourisePropsLine(lineBuffer, linePos, startLine, i, styler);
			linePos = 0;
			startLine = i + 1;
		}
	}
	if (linePos > 0) {	// Last line does not have ending characters
		ColourisePropsLine(lineBuffer, linePos, startLine, startPos + length - 1, styler);
	}
}

static void ColouriseMakeLine(
    char *lineBuffer,
    unsigned int lengthLine,
    unsigned int startLine,
    unsigned int endPos,
    Accessor &styler) {

	unsigned int i = 0;
        unsigned int lastNonSpace = 0;
	unsigned int state = SCE_MAKE_DEFAULT;
	bool bSpecial = false;
	// Skip initial spaces
	while ((i < lengthLine) && isspacechar(lineBuffer[i])) {
		i++;
	}
	if (lineBuffer[i] == '#') {	// Comment
		styler.ColourTo(endPos, SCE_MAKE_COMMENT);
		return;
	}
	if (lineBuffer[i] == '!') {	// Special directive
		styler.ColourTo(endPos, SCE_MAKE_PREPROCESSOR);
		return;
	}
	while (i < lengthLine) {
		if (lineBuffer[i] == '$' && lineBuffer[i + 1] == '(') {
			styler.ColourTo(startLine + i - 1, state);
			state = SCE_MAKE_IDENTIFIER;
		} else if (state == SCE_MAKE_IDENTIFIER && lineBuffer[i] == ')') {
			styler.ColourTo(startLine + i, state);
			state = SCE_MAKE_DEFAULT;
		}
		if (!bSpecial) {
			if (lineBuffer[i] == ':') {
				// We should check that no colouring was made since the beginning of the line,
				// to avoid colouring stuff like /OUT:file
				styler.ColourTo(startLine + lastNonSpace, SCE_MAKE_TARGET);
				styler.ColourTo(startLine + i - 1, SCE_MAKE_DEFAULT);
				styler.ColourTo(startLine + i, SCE_MAKE_OPERATOR);
				bSpecial = true;	// Only react to the first ':' of the line
				state = SCE_MAKE_DEFAULT;
			} else if (lineBuffer[i] == '=') {
				styler.ColourTo(startLine + lastNonSpace, SCE_MAKE_IDENTIFIER);
				styler.ColourTo(startLine + i - 1, SCE_MAKE_DEFAULT);
				styler.ColourTo(startLine + i, SCE_MAKE_OPERATOR);
				bSpecial = true;	// Only react to the first '=' of the line
				state = SCE_MAKE_DEFAULT;
			}
		}
		if (!isspacechar(lineBuffer[i])) {
			lastNonSpace = i;
		}
		i++;
	}
	if (state == SCE_MAKE_IDENTIFIER) {
		styler.ColourTo(endPos, SCE_MAKE_IDEOL);	// Error, variable reference not ended
	} else {
		styler.ColourTo(endPos, SCE_MAKE_DEFAULT);
	}
}

static void ColouriseMakeDoc(unsigned int startPos, int length, int, WordList *[], Accessor &styler) {
	char lineBuffer[1024];
	styler.StartAt(startPos);
	styler.StartSegment(startPos);
	unsigned int linePos = 0;
	unsigned int startLine = startPos;
	for (unsigned int i = startPos; i < startPos + length; i++) {
		lineBuffer[linePos++] = styler[i];
		if (AtEOL(styler, i) || (linePos >= sizeof(lineBuffer) - 1)) {
			// End of line (or of line buffer) met, colourise it
			lineBuffer[linePos] = '\0';
			ColouriseMakeLine(lineBuffer, linePos, startLine, i, styler);
			linePos = 0;
			startLine = i + 1;
		}
	}
	if (linePos > 0) {	// Last line does not have ending characters
		ColouriseMakeLine(lineBuffer, linePos, startLine, startPos + length - 1, styler);
	}
}

static void ColouriseErrorListLine(
    char *lineBuffer,
    unsigned int lengthLine,
    //		unsigned int startLine,
    unsigned int endPos,
    Accessor &styler) {
	if (lineBuffer[0] == '>') {
		// Command or return status
		styler.ColourTo(endPos, SCE_ERR_CMD);
	} else if (lineBuffer[0] == '!') {
		styler.ColourTo(endPos, SCE_ERR_DIFF_CHANGED);
	} else if (lineBuffer[0] == '+') {
		styler.ColourTo(endPos, SCE_ERR_DIFF_ADDITION);
	} else if (lineBuffer[0] == '-' && lineBuffer[1] == '-' && lineBuffer[2] == '-') {
		styler.ColourTo(endPos, SCE_ERR_DIFF_MESSAGE);
	} else if (lineBuffer[0] == '-') {
		styler.ColourTo(endPos, SCE_ERR_DIFF_DELETION);
	} else if (strstr(lineBuffer, "File \"") && strstr(lineBuffer, ", line ")) {
		styler.ColourTo(endPos, SCE_ERR_PYTHON);
	} else if (0 == strncmp(lineBuffer, "Error ", strlen("Error "))) {
		// Borland error message
		styler.ColourTo(endPos, SCE_ERR_BORLAND);
	} else if (0 == strncmp(lineBuffer, "Warning ", strlen("Warning "))) {
		// Borland warning message
		styler.ColourTo(endPos, SCE_ERR_BORLAND);
	} else if (strstr(lineBuffer, "at line " ) &&
	           (strstr(lineBuffer, "at line " ) < (lineBuffer + lengthLine)) &&
	           strstr(lineBuffer, "file ") &&
	           (strstr(lineBuffer, "file ") < (lineBuffer + lengthLine))) {
		// Lua error message
		styler.ColourTo(endPos, SCE_ERR_LUA);
	} else if (strstr(lineBuffer, " at " ) &&
	           (strstr(lineBuffer, " at " ) < (lineBuffer + lengthLine)) &&
	           strstr(lineBuffer, " line ") &&
	           (strstr(lineBuffer, " line ") < (lineBuffer + lengthLine))) {
		// perl error message
		styler.ColourTo(endPos, SCE_ERR_PERL);
	} else if ((memcmp(lineBuffer, "   at ", 6) == 0) &&
		strstr(lineBuffer, ":line ")) {
		// A .NET traceback
		styler.ColourTo(endPos, SCE_ERR_NET);
	} else {
		// Look for <filename>:<line>:message
		// Look for <filename>(line)message
		// Look for <filename>(line,pos)message
		int state = 0;
		for (unsigned int i = 0; i < lengthLine; i++) {
			if ((state == 0) && (lineBuffer[i] == ':') && isdigit(lineBuffer[i + 1])) {
				state = 1;
			} else if ((state == 0) && (lineBuffer[i] == '(')) {
				state = 10;
			} else if ((state == 0) && (lineBuffer[i] == '\t')) {
				state = 20;
			} else if ((state == 1) && isdigit(lineBuffer[i])) {
				state = 2;
			} else if ((state == 2) && (lineBuffer[i] == ':')) {
				state = 3;
				break;
			} else if ((state == 2) && !isdigit(lineBuffer[i])) {
				state = 99;
			} else if ((state == 10) && isdigit(lineBuffer[i])) {
				state = 11;
			} else if ((state == 11) && (lineBuffer[i] == ',')) {
				state = 14;
			} else if ((state == 11) && (lineBuffer[i] == ')')) {
				state = 12;
			} else if ((state == 12) && (lineBuffer[i] == ':')) {
				state = 13;
			} else if ((state == 14) && (lineBuffer[i] == ')')) {
				state = 15;
				break;
			} else if (((state == 11) || (state == 14)) && !((lineBuffer[i] == ' ') || isdigit(lineBuffer[i]))) {
				state = 99;
			} else if ((state == 20) && isdigit(lineBuffer[i])) {
				state = 24;
				break;
			} else if ((state == 20) && ((lineBuffer[i] == '/') && (lineBuffer[i+1] == '^'))) {
				state = 21;
			} else if ((state == 21) && ((lineBuffer[i] == '$') && (lineBuffer[i+1] == '/'))) {
				state = 22;
				break;		
			}
		}
		if (state == 3) {
			styler.ColourTo(endPos, SCE_ERR_GCC);
		} else if ((state == 13) || (state == 14) || (state == 15)) {
			styler.ColourTo(endPos, SCE_ERR_MS);
		} else if (((state == 22) || (state == 24)) && (lineBuffer[0] != '\t')) {
			styler.ColourTo(endPos, SCE_ERR_CTAG);	
		} else {
			styler.ColourTo(endPos, SCE_ERR_DEFAULT);
		}
	}
}

static void ColouriseErrorListDoc(unsigned int startPos, int length, int, WordList *[], Accessor &styler) {
	char lineBuffer[1024];
	styler.StartAt(startPos);
	styler.StartSegment(startPos);
	unsigned int linePos = 0;
	for (unsigned int i = startPos; i < startPos + length; i++) {
		lineBuffer[linePos++] = styler[i];
		if (AtEOL(styler, i) || (linePos >= sizeof(lineBuffer) - 1)) {
			// End of line (or of line buffer) met, colourise it
			lineBuffer[linePos] = '\0';
			ColouriseErrorListLine(lineBuffer, linePos, i, styler);
			linePos = 0;
		}
	}
	if (linePos > 0) {	// Last line does not have ending characters
		ColouriseErrorListLine(lineBuffer, linePos, startPos + length - 1, styler);
	}
}

static int isSpecial(char s) {
	return (s == '\\') || (s == ',') || (s == ';') || (s == '\'') || (s == ' ') ||
	       (s == '\"') || (s == '`') || (s == '^') || (s == '~');
}

static int isTag(int start, Accessor &styler) {
	char s[6];
	unsigned int i = 0, e = 1;
	while (i < 5 && e) {
		s[i] = styler[start + i];
		i++;
		e = styler[start + i] != '{';
	}
	s[i] = '\0';
	return (strcmp(s, "begin") == 0) || (strcmp(s, "end") == 0);
}

static void ColouriseLatexDoc(unsigned int startPos, int length, int initStyle,
                              WordList *[], Accessor &styler) {

	styler.StartAt(startPos);

	int state = initStyle;
	char chNext = styler[startPos];
	styler.StartSegment(startPos);
	int lengthDoc = startPos + length;

	for (int i = startPos; i < lengthDoc; i++) {
		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);

		if (styler.IsLeadByte(ch)) {
			chNext = styler.SafeGetCharAt(i + 2);
			i++;
			continue;
		}
		switch (state) {
		case SCE_L_DEFAULT :
			switch (ch) {
			case '\\' :
				styler.ColourTo(i - 1, state);
				if (isSpecial(styler[i + 1])) {
					styler.ColourTo(i + 1, SCE_L_COMMAND);
					i++;
					chNext = styler.SafeGetCharAt(i + 1);
				} else {
					if (isTag(i + 1, styler))
						state = SCE_L_TAG;
					else
						state = SCE_L_COMMAND;
				}
				break;
			case '$' :
				styler.ColourTo(i - 1, state);
				state = SCE_L_MATH;
				if (chNext == '$') {
					i++;
					chNext = styler.SafeGetCharAt(i + 1);
				}
				break;
			case '%' :
				styler.ColourTo(i - 1, state);
				state = SCE_L_COMMENT;
				break;
			}
			break;
		case SCE_L_COMMAND :
			if (chNext == '[' || chNext == '{' || chNext == '}' ||
			        chNext == ' ' || chNext == '\r' || chNext == '\n') {
				styler.ColourTo(i, state);
				state = SCE_L_DEFAULT;
				i++;
				chNext = styler.SafeGetCharAt(i + 1);
			}
			break;
		case SCE_L_TAG :
			if (ch == '}') {
				styler.ColourTo(i, state);
				state = SCE_L_DEFAULT;
			}
			break;
		case SCE_L_MATH :
			if (ch == '$') {
				if (chNext == '$') {
					i++;
					chNext = styler.SafeGetCharAt(i + 1);
				}
				styler.ColourTo(i, state);
				state = SCE_L_DEFAULT;
			}
			break;
		case SCE_L_COMMENT :
			if (ch == '\r' || ch == '\n') {
				styler.ColourTo(i - 1, state);
				state = SCE_L_DEFAULT;
			}
		}
	}
	styler.ColourTo(lengthDoc, state);
}

LexerModule lmBatch(SCLEX_BATCH, ColouriseBatchDoc, "batch");
LexerModule lmDiff(SCLEX_DIFF, ColouriseDiffDoc, "diff");
LexerModule lmProps(SCLEX_PROPERTIES, ColourisePropsDoc, "props");
LexerModule lmMake(SCLEX_MAKEFILE, ColouriseMakeDoc, "makefile");
LexerModule lmErrorList(SCLEX_ERRORLIST, ColouriseErrorListDoc, "errorlist");
LexerModule lmLatex(SCLEX_LATEX, ColouriseLatexDoc, "latex");
