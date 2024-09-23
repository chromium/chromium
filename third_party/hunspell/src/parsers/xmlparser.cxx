/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * Copyright (C) 2002-2017 Németh László
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
#include "xmlparser.hxx"

#ifndef W32
using namespace std;
#endif

enum { ST_NON_WORD, ST_WORD, ST_TAG, ST_CHAR_ENTITY, ST_OTHER_TAG, ST_ATTRIB };

static const char* __PATTERN__[][2] = {{"<!--", "-->"},
                                       {"<[cdata[", "]]>"},  // XML comment
                                       {"<", ">"}};

#define __PATTERN_LEN__ (sizeof(__PATTERN__) / (sizeof(char*) * 2))

static const char* (*__PATTERN2__)[2] = NULL;

#define __PATTERN_LEN2__ 0

#define ENTITY_APOS "&apos;"
#define UTF8_APOS "\xe2\x80\x99"
#define APOSTROPHE "'"

XMLParser::XMLParser(const char* wordchars)
    : TextParser(wordchars)
    , pattern_num(0), pattern2_num(0), prevstate(0), checkattr(0), quotmark(0) {
}

XMLParser::XMLParser(const w_char* wordchars, int len)
    : TextParser(wordchars, len)
    , pattern_num(0), pattern2_num(0), prevstate(0), checkattr(0), quotmark(0) {
}

XMLParser::~XMLParser() {}

int XMLParser::look_pattern(const char* p[][2], unsigned int len, int column) {
  for (unsigned int i = 0; i < len; i++) {
    const char* j = line[actual].c_str() + head;
    const char* k = p[i][column];
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
 * XML parser
 *
 */

bool XMLParser::next_token(const char* PATTERN[][2],
                           unsigned int PATTERN_LEN,
                           const char* PATTERN2[][2],
                           unsigned int PATTERN_LEN2,
                           std::string& t) {
  t.clear();
  const char* latin1;

  for (;;) {
    switch (state) {
      case ST_NON_WORD:  // non word chars
        prevstate = ST_NON_WORD;
        if ((pattern_num = look_pattern(PATTERN, PATTERN_LEN, 0)) != -1) {
          checkattr = 0;
          if ((pattern2_num = look_pattern(PATTERN2, PATTERN_LEN2, 0)) != -1) {
            checkattr = 1;
          }
          state = ST_TAG;
        } else if (is_wordchar(line[actual].c_str() + head)) {
          state = ST_WORD;
          token = head;
        } else if ((latin1 = get_latin1(line[actual].c_str() + head))) {
          state = ST_WORD;
          token = head;
          head += strlen(latin1);
        } else if (line[actual][head] == '&') {
          state = ST_CHAR_ENTITY;
        }
        break;
      case ST_WORD:  // wordchar
        if ((latin1 = get_latin1(line[actual].c_str() + head))) {
          head += strlen(latin1);
        } else if ((is_wordchar((char*)APOSTROPHE) ||
                    (is_utf8() && is_wordchar((char*)UTF8_APOS))) &&
                   strncmp(line[actual].c_str() + head, ENTITY_APOS,
                           strlen(ENTITY_APOS)) == 0 &&
                   is_wordchar(line[actual].c_str() + head + strlen(ENTITY_APOS))) {
          head += strlen(ENTITY_APOS) - 1;
        } else if (is_utf8() &&
                   is_wordchar((char*)APOSTROPHE) &&  // add Unicode apostrophe
                                                      // to the WORDCHARS, if
                                                      // needed
                   strncmp(line[actual].c_str() + head, UTF8_APOS, strlen(UTF8_APOS)) ==
                       0 &&
                   is_wordchar(line[actual].c_str() + head + strlen(UTF8_APOS))) {
          head += strlen(UTF8_APOS) - 1;
        } else if (!is_wordchar(line[actual].c_str() + head)) {
          state = prevstate;
          if (alloc_token(token, &head, t))
            return true;
        }
        break;
      case ST_TAG:  // comment, labels, etc
        int i;
        if ((checkattr == 1) &&
            ((i = look_pattern(PATTERN2, PATTERN_LEN2, 1)) != -1) &&
            (strcmp(PATTERN2[i][0], PATTERN2[pattern2_num][0]) == 0)) {
          checkattr = 2;
        } else if ((checkattr > 0) && (line[actual][head] == '>')) {
          state = ST_NON_WORD;
        } else if (((i = look_pattern(PATTERN, PATTERN_LEN, 1)) != -1) &&
                   (strcmp(PATTERN[i][1], PATTERN[pattern_num][1]) == 0)) {
          state = ST_NON_WORD;
          head += strlen(PATTERN[pattern_num][1]) - 1;
        } else if ((strcmp(PATTERN[pattern_num][0], "<") == 0) &&
                   ((line[actual][head] == '"') ||
                    (line[actual][head] == '\''))) {
          quotmark = line[actual][head];
          state = ST_ATTRIB;
        }
        break;
      case ST_ATTRIB:  // non word chars
        prevstate = ST_ATTRIB;
        if (line[actual][head] == quotmark) {
          state = ST_TAG;
          if (checkattr == 2)
            checkattr = 1;
          // for IMG ALT
        } else if (is_wordchar(line[actual].c_str() + head) && (checkattr == 2)) {
          state = ST_WORD;
          token = head;
        } else if (line[actual][head] == '&') {
          state = ST_CHAR_ENTITY;
        }
        break;
      case ST_CHAR_ENTITY:  // SGML element
        if ((tolower(line[actual][head]) == ';')) {
          state = prevstate;
          head--;
        }
    }
    if (next_char(line[actual].c_str(), &head))
      return false;
  }
  //FIXME No return, in function returning non-void
}

bool XMLParser::next_token(std::string& t) {
  return next_token(__PATTERN__, __PATTERN_LEN__, __PATTERN2__,
                    __PATTERN_LEN2__, t);
}

int XMLParser::change_token(const char* word) {
  if (strstr(word, APOSTROPHE) != NULL || strchr(word, '"') != NULL ||
      strchr(word, '&') != NULL || strchr(word, '<') != NULL ||
      strchr(word, '>') != NULL) {
    std::string r(word);
    mystrrep(r, "&", "__namp;__");
    mystrrep(r, "__namp;__", "&amp;");
    mystrrep(r, APOSTROPHE, ENTITY_APOS);
    mystrrep(r, "\"", "&quot;");
    mystrrep(r, ">", "&gt;");
    mystrrep(r, "<", "&lt;");
    return TextParser::change_token(r.c_str());
  }
  return TextParser::change_token(word);
}
