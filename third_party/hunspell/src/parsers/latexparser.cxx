/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * Copyright (C) 2002-2022 Németh László
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Hunspell is based on MySpell which is Copyright (C) 2002 Kevin Hendricks.
 *
 * Contributor(s): David Einstein, Davide Prina, Giuseppe Modugno,
 * Gianluca Turconi, Simon Brouwer, Noll János, Bíró Árpád,
 * Goldman Eleonóra, Sarlós Tamás, Bencsáth Boldizsár, Halácsy Péter,
 * Dvornik László, Gefferth András, Nagy Viktor, Varga Dániel, Chris Halls,
 * Rene Engelhard, Bram Moolenaar, Dafydd Jones, Harri Pitkänen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctype.h>

#include "../hunspell/csutil.hxx"
#include "latexparser.hxx"

#ifndef W32
using namespace std;
#endif

#define UTF8_APOS "\xe2\x80\x99"
#define APOSTROPHE "'"

static struct {
  const char* pat[2];
  int arg;
} PATTERN[] = {{{"\\(", "\\)"}, 0},
               {{"$$", "$$"}, 0},
               {{"$", "$"}, 0},
               {{"\\begin{math}", "\\end{math}"}, 0},
               {{"\\[", "\\]"}, 0},
               {{"\\begin{displaymath}", "\\end{displaymath}"}, 0},
               {{"\\begin{equation}", "\\end{equation}"}, 0},
               {{"\\begin{equation*}", "\\end{equation*}"}, 0},
               {{"\\begin{align}", "\\end{align}"}, 0},
               {{"\\begin{align*}", "\\end{align*}"}, 0},
               {{"\\begin{lstlisting}", "\\end{lstlisting}"}, 0},
               {{"\\cite", nullptr}, 1},
               {{"\\textcite", nullptr}, 1},
               {{"\\autocite", nullptr}, 1},
               {{"\\nocite", nullptr}, 1},
               {{"\\index", nullptr}, 1},
               {{"\\label", nullptr}, 1},
               {{"\\ref", nullptr}, 1},
               {{"\\pageref", nullptr}, 1},
               {{"\\autoref", nullptr}, 1},
               {{"\\parbox", nullptr}, 1},
               {{"\\begin{verbatim}", "\\end{verbatim}"}, 0},
               {{"\\verb+", "+"}, 0},
               {{"\\verb|", "|"}, 0},
               {{"\\verb#", "#"}, 0},
               {{"\\verb*", "*"}, 0},
               {{"\\documentstyle", "\\begin{document}"}, 0},
               {{"\\documentclass", "\\begin{document}"}, 0},
               // { { "\\documentclass", NULL } , 1 },
               {{"\\usepackage", nullptr}, 1},
               {{"\\includeonly", nullptr}, 1},
               {{"\\include", nullptr}, 1},
               {{"\\input", nullptr}, 1},
               {{"\\vspace", nullptr}, 1},
               {{"\\setlength", nullptr}, 2},
               {{"\\addtolength", nullptr}, 2},
               {{"\\settowidth", nullptr}, 2},
               {{"\\rule", nullptr}, 2},
               {{"\\hspace", nullptr}, 1},
               {{"\\vspace", nullptr}, 1},
               {{"\\\\[", "]"}, 0},
               {{"\\pagebreak[", "]"}, 0},
               {{"\\nopagebreak[", "]"}, 0},
               {{"\\enlargethispage", nullptr}, 1},
               {{"\\begin{tabular}", nullptr}, 1},
               {{"\\addcontentsline", nullptr}, 2},
               {{"\\gls", nullptr}, 1},
               {{"\\glspl", nullptr}, 1},
               {{"\\Gls", nullptr}, 1},
               {{"\\Glspl", nullptr}, 1},
               {{"\\begin{thebibliography}", nullptr}, 1},
               {{"\\bibliography", nullptr}, 1},
               {{"\\bibliographystyle", nullptr}, 1},
               {{"\\bibitem", nullptr}, 1},
               {{"\\begin", nullptr}, 1},
               {{"\\end", nullptr}, 1},
               {{"\\pagestyle", nullptr}, 1},
               {{"\\pagenumbering", nullptr}, 1},
               {{"\\thispagestyle", nullptr}, 1},
               {{"\\newtheorem", nullptr}, 2},
               {{"\\newcommand", nullptr}, 2},
               {{"\\renewcommand", nullptr}, 2},
               {{"\\setcounter", nullptr}, 2},
               {{"\\addtocounter", nullptr}, 1},
               {{"\\stepcounter", nullptr}, 1},
               {{"\\selectlanguage", nullptr}, 1},
               {{"\\inputencoding", nullptr}, 1},
               {{"\\hyphenation", nullptr}, 1},
               {{"\\definecolor", nullptr}, 3},
               {{"\\color", nullptr}, 1},
               {{"\\textcolor", nullptr}, 1},
               {{"\\pagecolor", nullptr}, 1},
               {{"\\colorbox", nullptr}, 2},
               {{"\\fcolorbox", nullptr}, 2},
               {{"\\declaregraphicsextensions", nullptr}, 1},
               {{"\\psfig", nullptr}, 1},
               {{"\\url", nullptr}, 1},
               {{"\\eqref", nullptr}, 1},
               {{"\\cref", nullptr}, 1},
               {{"\\Cref", nullptr}, 1},
               {{"\\vskip", nullptr}, 1},
               {{"\\vglue", nullptr}, 1},
               {{"\'\'", nullptr}, 1}};

#define PATTERN_LEN (sizeof(PATTERN) / sizeof(PATTERN[0]))

LaTeXParser::LaTeXParser(const char* wordchars)
    : TextParser(wordchars)
    , pattern_num(0), depth(0), arg(0), opt(0) {
}

LaTeXParser::LaTeXParser(const w_char* wordchars, int len)
    : TextParser(wordchars, len)
    , pattern_num(0), depth(0), arg(0), opt(0) {
}

LaTeXParser::~LaTeXParser() = default;

int LaTeXParser::look_pattern(int col) {
  for (unsigned int i = 0; i < PATTERN_LEN; i++) {
    const char* j = line[actual].c_str() + head;
    const char* k = PATTERN[i].pat[col];
    if (!k)
      continue;
    while ((*k != '\0') && (tolower(*j) == *k)) {
      j++;
      k++;
    }
    if (*k == '\0')
      return i;
  }
  return -1;
}

/*
 * LaTeXParser
 *
 * state 0: not wordchar
 * state 1: wordchar
 * state 2: comments
 * state 3: commands
 * state 4: commands with arguments
 * state 5: % comment
 *
 */

bool LaTeXParser::next_token(std::string& t) {
  t.clear();
  int i;
  int slash = 0;
  int apostrophe;
  for (;;) {
    // fprintf(stderr,"depth: %d, state: %d, , arg: %d, token:
    // %s\n",depth,state,arg,line[actual]+head);

    switch (state) {
      case 0:  // non word chars
        if ((pattern_num = look_pattern(0)) != -1) {
          if (PATTERN[pattern_num].pat[1]) {
            state = 2;
          } else {
            state = 4;
            depth = 0;
            arg = 0;
            opt = 1;
          }
          head += strlen(PATTERN[pattern_num].pat[0]) - 1;
        } else if (line[actual][head] == '%') {
          state = 5;
        } else if (is_wordchar(line[actual].c_str() + head)) {
          state = 1;
          token = head;
        } else if (line[actual][head] == '\\') {
          if (line[actual][head + 1] == '\\' ||   // \\ (linebreak)
              (line[actual][head + 1] == '$') ||  // \$ (dollar sign)
              (line[actual][head + 1] == '%')) {  // \% (percent)
            head++;
            break;
          }
          state = 3;
        }
        break;
      case 1:  // wordchar
        apostrophe = 0;
        if ((is_wordchar(APOSTROPHE) ||
             (is_utf8() && is_wordchar(UTF8_APOS))) &&
            !line[actual].empty() && line[actual][head] == '\'' &&
            is_wordchar(line[actual].c_str() + head + 1)) {
          head++;
        } else if (is_utf8() &&
                   is_wordchar(APOSTROPHE) &&  // add Unicode apostrophe
                                                      // to the WORDCHARS, if
                                                      // needed
                   strncmp(line[actual].c_str() + head, UTF8_APOS, strlen(UTF8_APOS)) ==
                   0 &&
                   is_wordchar(line[actual].c_str() + head + strlen(UTF8_APOS))) {
          head += strlen(UTF8_APOS) - 1;
        } else if (!is_wordchar(line[actual].c_str() + head) ||
            (line[actual][head] == '\'' && line[actual][head + 1] == '\'' &&
             ++apostrophe)) {
          state = 0;
          bool ok = alloc_token(token, &head, t);
          if (apostrophe)
            head += 2;
          if (ok)
            return true;
        }
        break;
      case 2:  // comment, labels, etc
        if (((i = look_pattern(1)) != -1) &&
            (strcmp(PATTERN[i].pat[1], PATTERN[pattern_num].pat[1]) == 0)) {
          state = 0;
          head += strlen(PATTERN[pattern_num].pat[1]) - 1;
        }
        break;
      case 3:  // command
        if ((tolower(line[actual][head]) < 'a') ||
            (tolower(line[actual][head]) > 'z')) {
          state = 0;
          head--;
        }
        break;
      case 4:  // command with arguments
        if (slash && (line[actual][head] != '\0')) {
          slash = 0;
          head++;
          break;
        } else if (line[actual][head] == '\\') {
          slash = 1;
        } else if ((line[actual][head] == '{') ||
                   ((opt) && (line[actual][head] == '['))) {
          depth++;
          opt = 0;
        } else if (line[actual][head] == '}') {
          depth--;
          if (depth == 0) {
            opt = 1;
            arg++;
          }
          if (((depth == 0) && (arg == PATTERN[pattern_num].arg)) ||
              (depth < 0)) {
            state = 0;  // XXX not handles the last optional arg.
          }
        } else if (line[actual][head] == ']')
          depth--;
    }  // case
    if (next_char(line[actual].c_str(), &head)) {
      if (state == 5)
        state = 0;
      return false;
    }
  }
}
