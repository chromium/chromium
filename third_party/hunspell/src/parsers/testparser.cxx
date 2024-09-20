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

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "textparser.hxx"
#include "htmlparser.hxx"
#include "latexparser.hxx"
#include "xmlparser.hxx"

#ifndef W32
using namespace std;
#endif

int main(int argc, char** argv) {
  FILE* f;
  /* first parse the command line options */

  if (argc < 2) {
    fprintf(stderr, "correct syntax is:\n");
    fprintf(stderr, "testparser file\n");
    fprintf(stderr, "example: testparser /dev/stdin\n");
    exit(1);
  }

  /* open the words to check list */
  f = fopen(argv[1], "r");
  if (!f) {
    fprintf(stderr, "Error - could not open file of words to check\n");
    exit(1);
  }

  TextParser* p = new TextParser(
      "qwertzuiopasdfghjklyxcvbnmQWERTZUIOPASDFGHJKLYXCVBNM");

  char buf[MAXLNLEN];

  while (fgets(buf, MAXLNLEN, f)) {
    p->put_line(buf);
    p->set_url_checking(1);
    std::string next;
    while (p->next_token(next)) {
      fprintf(stdout, "token: %s\n", next.c_str());
    }
  }

  delete p;
  fclose(f);
  return 0;
}
