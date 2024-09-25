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

#ifndef TEXTPARSER_HXX_
#define TEXTPARSER_HXX_

// set sum of actual and previous lines
#define MAXPREVLINE 4

#ifndef MAXLNLEN
#define MAXLNLEN 8192
#endif

#include "../hunspell/w_char.hxx"

#include <vector>

/*
 * Base Text Parser
 *
 */

class TextParser {
 protected:
  std::vector<int> wordcharacters;// for detection of the word boundaries
  std::string line[MAXPREVLINE];  // parsed and previous lines
  std::vector<bool> urlline;      // mask for url detection
  int checkurl;
  int actual;  // actual line
  size_t head; // head position
  size_t token;// begin of token
  int state;   // state of automata
  int utf8;    // UTF-8 character encoding
  int next_char(const char* line, size_t* pos);
  const w_char* wordchars_utf16;
  int wclen;

 public:
  TextParser(const w_char* wordchars, int len);
  explicit TextParser(const char* wc);
  virtual ~TextParser();

  void put_line(const char* line);
  std::string get_line() const;
  std::string get_prevline(int n) const;
  virtual bool next_token(std::string&);
  virtual std::string get_word(const std::string &tok);
  virtual int change_token(const char* word);
  void set_url_checking(int check);

  size_t get_tokenpos();
  int is_wordchar(const char* w);
  inline int is_utf8() { return utf8; }
  const char* get_latin1(const char* s);
  char* next_char();
  int tokenize_urls();
  void check_urls();
  int get_url(size_t token_pos, size_t* head);
  bool alloc_token(size_t token, size_t* head, std::string& out);
private:
  void init(const char*);
  void init(const w_char* wordchars, int len);
};

#endif
