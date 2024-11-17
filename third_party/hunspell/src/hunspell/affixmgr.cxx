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
/*
 * Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada
 * And Contributors.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All modifications to the source code must be clearly marked as
 *    such.  Binary redistributions based on modified source code
 *    must be clearly marked as modified versions in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY KEVIN B. HENDRICKS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * KEVIN B. HENDRICKS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "affixmgr.hxx"
#include "affentry.hxx"
#include "langnum.hxx"

#include "csutil.hxx"

#ifdef HUNSPELL_CHROME_CLIENT
AffixMgr::AffixMgr(hunspell::BDictReader* reader,
                   const std::vector<HashMgr*>& ptr)
  : alldic(ptr)
  , pHMgr(ptr[0]) {
  bdict_reader = reader;
#else
AffixMgr::AffixMgr(const char* affpath,
                   const std::vector<HashMgr*>& ptr,
                   const char* key)
  : alldic(ptr)
  , pHMgr(ptr[0]) {
#endif

  // register hash manager and load affix data from aff file
  csconv = NULL;
  utf8 = 0;
  complexprefixes = 0;
  parsedmaptable = false;
  parsedbreaktable = false;
  iconvtable = NULL;
  oconvtable = NULL;
  // allow simplified compound forms (see 3rd field of CHECKCOMPOUNDPATTERN)
  simplifiedcpd = 0;
  parsedcheckcpd = false;
  parseddefcpd = false;
  phone = NULL;
  compoundflag = FLAG_NULL;        // permits word in compound forms
  compoundbegin = FLAG_NULL;       // may be first word in compound forms
  compoundmiddle = FLAG_NULL;      // may be middle word in compound forms
  compoundend = FLAG_NULL;         // may be last word in compound forms
  compoundroot = FLAG_NULL;        // compound word signing flag
  compoundpermitflag = FLAG_NULL;  // compound permitting flag for suffixed word
  compoundforbidflag = FLAG_NULL;  // compound fordidden flag for suffixed word
  compoundmoresuffixes = 0;        // allow more suffixes within compound words
  checkcompounddup = 0;            // forbid double words in compounds
  checkcompoundrep = 0;  // forbid bad compounds (may be non-compound word with
                         // a REP substitution)
  checkcompoundcase =
      0;  // forbid upper and lowercase combinations at word bounds
  checkcompoundtriple = 0;  // forbid compounds with triple letters
  simplifiedtriple = 0;     // allow simplified triple letters in compounds
                            // (Schiff+fahrt -> Schiffahrt)
  forbiddenword = FORBIDDENWORD;  // forbidden word signing flag
  nosuggest = FLAG_NULL;  // don't suggest words signed with NOSUGGEST flag
  nongramsuggest = FLAG_NULL;
  langnum = 0;  // language code (see http://l10n.openoffice.org/languages.html)
  needaffix = FLAG_NULL;  // forbidden root, allowed only with suffixes
  cpdwordmax = -1;        // default: unlimited wordcount in compound words
  cpdmin = -1;            // undefined
  cpdmaxsyllable = 0;     // default: unlimited syllablecount in compound words
  pfxappnd = NULL;  // previous prefix for counting syllables of the prefix BUG
  sfxappnd = NULL;  // previous suffix for counting syllables of the suffix BUG
  sfxextra = 0;     // modifier for syllable count of sfxappnd BUG
  checknum = 0;               // checking numbers, and word with numbers
  havecontclass = 0;  // flags of possible continuing classes (double affix)
  // LEMMA_PRESENT: not put root into the morphological output. Lemma presents
  // in morhological description in dictionary file. It's often combined with
  // PSEUDOROOT.
  lemma_present = FLAG_NULL;
  circumfix = FLAG_NULL;
  onlyincompound = FLAG_NULL;
  maxngramsugs = -1;  // undefined
  maxdiff = -1;       // undefined
  onlymaxdiff = 0;
  maxcpdsugs = -1;  // undefined
  nosplitsugs = 0;
  sugswithdots = 0;
  keepcase = 0;
  forceucase = 0;
  warn = 0;
  forbidwarn = 0;
  checksharps = 0;
  substandard = FLAG_NULL;
  fullstrip = 0;

  sfx = NULL;
  pfx = NULL;

  for (int i = 0; i < SETSIZE; i++) {
    pStart[i] = NULL;
    sStart[i] = NULL;
    pFlag[i] = NULL;
    sFlag[i] = NULL;
  }

#ifdef HUNSPELL_CHROME_CLIENT
  // Define dummy parameters for parse_file() to avoid changing the parameters
  // of parse_file(). This may make it easier to merge the changes of the
  // original hunspell.
  const char* affpath = NULL;
  const char* key = NULL;
#else
  for (int j = 0; j < CONTSIZE; j++) {
    contclasses[j] = 0;
  }
#endif

  if (parse_file(affpath, key)) {
    HUNSPELL_WARNING(stderr, "Failure loading aff file %s\n", affpath);
  }

  if (cpdmin == -1)
    cpdmin = MINCPDLEN;
}

AffixMgr::~AffixMgr() {
  // pass through linked prefix entries and clean up
  for (int i = 0; i < SETSIZE; i++) {
    pFlag[i] = NULL;
    PfxEntry* ptr = pStart[i];
    PfxEntry* nptr = NULL;
    while (ptr) {
      nptr = ptr->getNext();
      delete (ptr);
      ptr = nptr;
      nptr = NULL;
    }
  }

  // pass through linked suffix entries and clean up
  for (int j = 0; j < SETSIZE; j++) {
    sFlag[j] = NULL;
    SfxEntry* ptr = sStart[j];
    SfxEntry* nptr = NULL;
    while (ptr) {
      nptr = ptr->getNext();
      delete (ptr);
      ptr = nptr;
      nptr = NULL;
    }
    sStart[j] = NULL;
  }

  delete iconvtable;
  delete oconvtable;
  delete phone;

  FREE_FLAG(compoundflag);
  FREE_FLAG(compoundbegin);
  FREE_FLAG(compoundmiddle);
  FREE_FLAG(compoundend);
  FREE_FLAG(compoundpermitflag);
  FREE_FLAG(compoundforbidflag);
  FREE_FLAG(compoundroot);
  FREE_FLAG(forbiddenword);
  FREE_FLAG(nosuggest);
  FREE_FLAG(nongramsuggest);
  FREE_FLAG(needaffix);
  FREE_FLAG(lemma_present);
  FREE_FLAG(circumfix);
  FREE_FLAG(onlyincompound);

  cpdwordmax = 0;
  pHMgr = NULL;
  cpdmin = 0;
  cpdmaxsyllable = 0;
  free_utf_tbl();
  checknum = 0;
#ifdef MOZILLA_CLIENT
  delete[] csconv;
#endif
}

void AffixMgr::finishFileMgr(FileMgr* afflst) {
  delete afflst;

  // convert affix trees to sorted list
  process_pfx_tree_to_list();
  process_sfx_tree_to_list();
}

// read in aff file and build up prefix and suffix entry objects
int AffixMgr::parse_file(const char* affpath, const char* key) {
  std::string line;
#ifdef HUNSPELL_CHROME_CLIENT
  // open the affix file
  // We're always UTF-8
  utf8 = 1;

  // A BDICT file stores PFX and SFX lines in a special section and it provides
  // a special line iterator for reading PFX and SFX lines.
  // We create a FileMgr object from this iterator and parse PFX and SFX lines
  // before parsing other lines.
  hunspell::LineIterator affix_iterator = bdict_reader->GetAffixLineIterator();
  FileMgr* iterator = new FileMgr(&affix_iterator);
  if (!iterator) {
    HUNSPELL_WARNING(stderr,
        "error: could not create a FileMgr from an affix line iterator.\n");
    return 1;
  }

  while (iterator->getline(line)) {
    char ft = ' ';
    if (line.compare(0, 3, "PFX") == 0) ft = complexprefixes ? 'S' : 'P';
    if (line.compare(0, 3, "SFX") == 0) ft = complexprefixes ? 'P' : 'S';
    if (ft != ' ')
      parse_affix(line, ft, iterator, NULL);
  }
  delete iterator;

  // Create a FileMgr object for reading lines except PFX and SFX lines.
  // We don't need to change the loop below since our FileMgr emulates the
  // original one.
  hunspell::LineIterator other_iterator = bdict_reader->GetOtherLineIterator();
  FileMgr * afflst = new FileMgr(&other_iterator);
  if (!afflst) {
    HUNSPELL_WARNING(stderr,
        "error: could not create a FileMgr from an other line iterator.\n");
    return 1;
  }
#else
  // checking flag duplication
  char dupflags[CONTSIZE];
  char dupflags_ini = 1;

  // first line indicator for removing byte order mark
  int firstline = 1;

  // open the affix file
  FileMgr* afflst = new FileMgr(affpath, key);
  if (!afflst) {
    HUNSPELL_WARNING(
        stderr, "error: could not open affix description file %s\n", affpath);
    return 1;
  }
#endif

  // step one is to parse the affix file building up the internal
  // affix data structures

  // read in each line ignoring any that do not
  // start with a known line type indicator
  while (afflst->getline(line)) {
    mychomp(line);

#ifndef HUNSPELL_CHROME_CLIENT
    /* remove byte order mark */
    if (firstline) {
      firstline = 0;
      // Affix file begins with byte order mark: possible incompatibility with
      // old Hunspell versions
      if (line.compare(0, 3, "\xEF\xBB\xBF", 3) == 0) {
        line.erase(0, 3);
      }
    }
#endif

    /* parse in the keyboard string */
    if (line.compare(0, 3, "KEY", 3) == 0) {
      if (!parse_string(line, keystring, afflst->getlinenum())) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the try string */
    if (line.compare(0, 3, "TRY", 3) == 0) {
      if (!parse_string(line, trystring, afflst->getlinenum())) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the name of the character set used by the .dict and .aff */
    if (line.compare(0, 3, "SET", 3) == 0) {
      if (!parse_string(line, encoding, afflst->getlinenum())) {
        finishFileMgr(afflst);
        return 1;
      }
      if (encoding == "UTF-8") {
        utf8 = 1;
#ifndef OPENOFFICEORG
#ifndef MOZILLA_CLIENT
        initialize_utf_tbl();
#endif
#endif
      }
    }

    /* parse COMPLEXPREFIXES for agglutinative languages with right-to-left
     * writing system */
    if (line.compare(0, 15, "COMPLEXPREFIXES", 15) == 0)
      complexprefixes = 1;

    /* parse in the flag used by the controlled compound words */
    if (line.compare(0, 12, "COMPOUNDFLAG", 12) == 0) {
      if (!parse_flag(line, &compoundflag, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by compound words */
    if (line.compare(0, 13, "COMPOUNDBEGIN", 13) == 0) {
      if (complexprefixes) {
        if (!parse_flag(line, &compoundend, afflst)) {
          finishFileMgr(afflst);
          return 1;
        }
      } else {
        if (!parse_flag(line, &compoundbegin, afflst)) {
          finishFileMgr(afflst);
          return 1;
        }
      }
    }

    /* parse in the flag used by compound words */
    if (line.compare(0, 14, "COMPOUNDMIDDLE", 14) == 0) {
      if (!parse_flag(line, &compoundmiddle, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by compound words */
    if (line.compare(0, 11, "COMPOUNDEND", 11) == 0) {
      if (complexprefixes) {
        if (!parse_flag(line, &compoundbegin, afflst)) {
          finishFileMgr(afflst);
          return 1;
        }
      } else {
        if (!parse_flag(line, &compoundend, afflst)) {
          finishFileMgr(afflst);
          return 1;
        }
      }
    }

    /* parse in the data used by compound_check() method */
    if (line.compare(0, 15, "COMPOUNDWORDMAX", 15) == 0) {
      if (!parse_num(line, &cpdwordmax, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag sign compounds in dictionary */
    if (line.compare(0, 12, "COMPOUNDROOT", 12) == 0) {
      if (!parse_flag(line, &compoundroot, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by compound_check() method */
    if (line.compare(0, 18, "COMPOUNDPERMITFLAG", 18) == 0) {
      if (!parse_flag(line, &compoundpermitflag, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by compound_check() method */
    if (line.compare(0, 18, "COMPOUNDFORBIDFLAG", 18) == 0) {
      if (!parse_flag(line, &compoundforbidflag, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    if (line.compare(0, 20, "COMPOUNDMORESUFFIXES", 20) == 0) {
      compoundmoresuffixes = 1;
    }

    if (line.compare(0, 16, "CHECKCOMPOUNDDUP", 16) == 0) {
      checkcompounddup = 1;
    }

    if (line.compare(0, 16, "CHECKCOMPOUNDREP", 16) == 0) {
      checkcompoundrep = 1;
    }

    if (line.compare(0, 19, "CHECKCOMPOUNDTRIPLE", 19) == 0) {
      checkcompoundtriple = 1;
    }

    if (line.compare(0, 16, "SIMPLIFIEDTRIPLE", 16) == 0) {
      simplifiedtriple = 1;
    }

    if (line.compare(0, 17, "CHECKCOMPOUNDCASE", 17) == 0) {
      checkcompoundcase = 1;
    }

    if (line.compare(0, 9, "NOSUGGEST", 9) == 0) {
      if (!parse_flag(line, &nosuggest, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    if (line.compare(0, 14, "NONGRAMSUGGEST", 14) == 0) {
      if (!parse_flag(line, &nongramsuggest, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by forbidden words */
    if (line.compare(0, 13, "FORBIDDENWORD", 13) == 0) {
      if (!parse_flag(line, &forbiddenword, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by forbidden words (is deprecated) */
    if (line.compare(0, 13, "LEMMA_PRESENT", 13) == 0) {
      if (!parse_flag(line, &lemma_present, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by circumfixes */
    if (line.compare(0, 9, "CIRCUMFIX", 9) == 0) {
      if (!parse_flag(line, &circumfix, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by fogemorphemes */
    if (line.compare(0, 14, "ONLYINCOMPOUND", 14) == 0) {
      if (!parse_flag(line, &onlyincompound, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by `needaffixs' (is deprecated) */
    if (line.compare(0, 10, "PSEUDOROOT", 10) == 0) {
      if (!parse_flag(line, &needaffix, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by `needaffixs' */
    if (line.compare(0, 9, "NEEDAFFIX", 9) == 0) {
      if (!parse_flag(line, &needaffix, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the minimal length for words in compounds */
    if (line.compare(0, 11, "COMPOUNDMIN", 11) == 0) {
      if (!parse_num(line, &cpdmin, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
      if (cpdmin < 1)
        cpdmin = 1;
    }

    /* parse in the max. words and syllables in compounds */
    if (line.compare(0, 16, "COMPOUNDSYLLABLE", 16) == 0) {
      if (!parse_cpdsyllable(line, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by compound_check() method */
    if (line.compare(0, 11, "SYLLABLENUM", 11) == 0) {
      if (!parse_string(line, cpdsyllablenum, afflst->getlinenum())) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by the controlled compound words */
    if (line.compare(0, 8, "CHECKNUM", 8) == 0) {
      checknum = 1;
    }

    /* parse in the extra word characters */
    if (line.compare(0, 9, "WORDCHARS", 9) == 0) {
      if (!parse_array(line, wordchars, wordchars_utf16,
                       utf8, afflst->getlinenum())) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the ignored characters (for example, Arabic optional diacretics
     * charachters */
    if (line.compare(0, 6, "IGNORE", 6) == 0) {
      if (!parse_array(line, ignorechars, ignorechars_utf16,
                       utf8, afflst->getlinenum())) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the input conversion table */
    if (line.compare(0, 5, "ICONV", 5) == 0) {
      if (!parse_convtable(line, afflst, &iconvtable, "ICONV")) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the output conversion table */
    if (line.compare(0, 5, "OCONV", 5) == 0) {
      if (!parse_convtable(line, afflst, &oconvtable, "OCONV")) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the phonetic translation table */
    if (line.compare(0, 5, "PHONE", 5) == 0) {
      if (!parse_phonetable(line, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the checkcompoundpattern table */
    if (line.compare(0, 20, "CHECKCOMPOUNDPATTERN", 20) == 0) {
      if (!parse_checkcpdtable(line, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the defcompound table */
    if (line.compare(0, 12, "COMPOUNDRULE", 12) == 0) {
      if (!parse_defcpdtable(line, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the related character map table */
    if (line.compare(0, 3, "MAP", 3) == 0) {
      if (!parse_maptable(line, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the word breakpoints table */
    if (line.compare(0, 5, "BREAK", 5) == 0) {
      if (!parse_breaktable(line, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the language for language specific codes */
    if (line.compare(0, 4, "LANG", 4) == 0) {
      if (!parse_string(line, lang, afflst->getlinenum())) {
        finishFileMgr(afflst);
        return 1;
      }
      langnum = get_lang_num(lang);
    }

    if (line.compare(0, 7, "VERSION", 7) == 0) {
      size_t startpos = line.find_first_not_of(" \t", 7);
      if (startpos != std::string::npos) {
          version = line.substr(startpos);
      }
    }

    if (line.compare(0, 12, "MAXNGRAMSUGS", 12) == 0) {
      if (!parse_num(line, &maxngramsugs, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    if (line.compare(0, 11, "ONLYMAXDIFF", 11) == 0)
      onlymaxdiff = 1;

    if (line.compare(0, 7, "MAXDIFF", 7) == 0) {
      if (!parse_num(line, &maxdiff, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    if (line.compare(0, 10, "MAXCPDSUGS", 10) == 0) {
      if (!parse_num(line, &maxcpdsugs, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    if (line.compare(0, 11, "NOSPLITSUGS", 11) == 0) {
      nosplitsugs = 1;
    }

    if (line.compare(0, 9, "FULLSTRIP", 9) == 0) {
      fullstrip = 1;
    }

    if (line.compare(0, 12, "SUGSWITHDOTS", 12) == 0) {
      sugswithdots = 1;
    }

    /* parse in the flag used by forbidden words */
    if (line.compare(0, 8, "KEEPCASE", 8) == 0) {
      if (!parse_flag(line, &keepcase, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by `forceucase' */
    if (line.compare(0, 10, "FORCEUCASE", 10) == 0) {
      if (!parse_flag(line, &forceucase, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    /* parse in the flag used by `warn' */
    if (line.compare(0, 4, "WARN", 4) == 0) {
      if (!parse_flag(line, &warn, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    if (line.compare(0, 10, "FORBIDWARN", 10) == 0) {
      forbidwarn = 1;
    }

    /* parse in the flag used by the affix generator */
    if (line.compare(0, 11, "SUBSTANDARD", 11) == 0) {
      if (!parse_flag(line, &substandard, afflst)) {
        finishFileMgr(afflst);
        return 1;
      }
    }

    if (line.compare(0, 11, "CHECKSHARPS", 11) == 0) {
      checksharps = 1;
    }

#ifndef HUNSPELL_CHROME_CLIENT
    /* parse this affix: P - prefix, S - suffix */
    // affix type
    char ft = ' ';
    if (line.compare(0, 3, "PFX", 3) == 0)
      ft = complexprefixes ? 'S' : 'P';
    if (line.compare(0, 3, "SFX", 3) == 0)
      ft = complexprefixes ? 'P' : 'S';
    if (ft != ' ') {
      if (dupflags_ini) {
        memset(dupflags, 0, sizeof(dupflags));
        dupflags_ini = 0;
      }
      if (!parse_affix(line, ft, afflst, dupflags)) {
        finishFileMgr(afflst);
        return 1;
      }
    }
#endif
  }

  finishFileMgr(afflst);
  // affix trees are sorted now

  // now we can speed up performance greatly taking advantage of the
  // relationship between the affixes and the idea of "subsets".

  // View each prefix as a potential leading subset of another and view
  // each suffix (reversed) as a potential trailing subset of another.

  // To illustrate this relationship if we know the prefix "ab" is found in the
  // word to examine, only prefixes that "ab" is a leading subset of need be
  // examined.
  // Furthermore is "ab" is not present then none of the prefixes that "ab" is
  // is a subset need be examined.
  // The same argument goes for suffix string that are reversed.

  // Then to top this off why not examine the first char of the word to quickly
  // limit the set of prefixes to examine (i.e. the prefixes to examine must
  // be leading supersets of the first character of the word (if they exist)

  // To take advantage of this "subset" relationship, we need to add two links
  // from entry.  One to take next if the current prefix is found (call it
  // nexteq)
  // and one to take next if the current prefix is not found (call it nextne).

  // Since we have built ordered lists, all that remains is to properly
  // initialize
  // the nextne and nexteq pointers that relate them

  process_pfx_order();
  process_sfx_order();

  /* get encoding for CHECKCOMPOUNDCASE */
  if (!utf8) {
    csconv = get_current_cs(get_encoding());
    for (int i = 0; i <= 255; i++) {
      if ((csconv[i].cupper != csconv[i].clower) &&
          (wordchars.find((char)i) == std::string::npos)) {
        wordchars.push_back((char)i);
      }
    }

  }

  // default BREAK definition
  if (!parsedbreaktable) {
    breaktable.push_back("-");
    breaktable.push_back("^-");
    breaktable.push_back("-$");
    parsedbreaktable = true;
  }
  return 0;
}

// we want to be able to quickly access prefix information
// both by prefix flag, and sorted by prefix string itself
// so we need to set up two indexes

int AffixMgr::build_pfxtree(PfxEntry* pfxptr) {
  PfxEntry* ptr;
  PfxEntry* pptr;
  PfxEntry* ep = pfxptr;

  // get the right starting points
  const char* key = ep->getKey();
  const unsigned char flg = (unsigned char)(ep->getFlag() & 0x00FF);

  // first index by flag which must exist
  ptr = pFlag[flg];
  ep->setFlgNxt(ptr);
  pFlag[flg] = ep;

  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
    ptr = pStart[0];
    ep->setNext(ptr);
    pStart[0] = ep;
    return 0;
  }

  // now handle the normal case
  ep->setNextEQ(NULL);
  ep->setNextNE(NULL);

  unsigned char sp = *((const unsigned char*)key);
  ptr = pStart[sp];

  // handle the first insert
  if (!ptr) {
    pStart[sp] = ep;
    return 0;
  }

  // otherwise use binary tree insertion so that a sorted
  // list can easily be generated later
  pptr = NULL;
  for (;;) {
    pptr = ptr;
    if (strcmp(ep->getKey(), ptr->getKey()) <= 0) {
      ptr = ptr->getNextEQ();
      if (!ptr) {
        pptr->setNextEQ(ep);
        break;
      }
    } else {
      ptr = ptr->getNextNE();
      if (!ptr) {
        pptr->setNextNE(ep);
        break;
      }
    }
  }
  return 0;
}

// we want to be able to quickly access suffix information
// both by suffix flag, and sorted by the reverse of the
// suffix string itself; so we need to set up two indexes
int AffixMgr::build_sfxtree(SfxEntry* sfxptr) {

  sfxptr->initReverseWord();

  SfxEntry* ptr;
  SfxEntry* pptr;
  SfxEntry* ep = sfxptr;

  /* get the right starting point */
  const char* key = ep->getKey();
  const unsigned char flg = (unsigned char)(ep->getFlag() & 0x00FF);

  // first index by flag which must exist
  ptr = sFlag[flg];
  ep->setFlgNxt(ptr);
  sFlag[flg] = ep;

  // next index by affix string

  // handle the special case of null affix string
  if (strlen(key) == 0) {
    // always inset them at head of list at element 0
    ptr = sStart[0];
    ep->setNext(ptr);
    sStart[0] = ep;
    return 0;
  }

  // now handle the normal case
  ep->setNextEQ(NULL);
  ep->setNextNE(NULL);

  unsigned char sp = *((const unsigned char*)key);
  ptr = sStart[sp];

  // handle the first insert
  if (!ptr) {
    sStart[sp] = ep;
    return 0;
  }

  // otherwise use binary tree insertion so that a sorted
  // list can easily be generated later
  pptr = NULL;
  for (;;) {
    pptr = ptr;
    if (strcmp(ep->getKey(), ptr->getKey()) <= 0) {
      ptr = ptr->getNextEQ();
      if (!ptr) {
        pptr->setNextEQ(ep);
        break;
      }
    } else {
      ptr = ptr->getNextNE();
      if (!ptr) {
        pptr->setNextNE(ep);
        break;
      }
    }
  }
  return 0;
}

// convert from binary tree to sorted list
int AffixMgr::process_pfx_tree_to_list() {
  for (int i = 1; i < SETSIZE; i++) {
    pStart[i] = process_pfx_in_order(pStart[i], NULL);
  }
  return 0;
}

PfxEntry* AffixMgr::process_pfx_in_order(PfxEntry* ptr, PfxEntry* nptr) {
  if (ptr) {
    nptr = process_pfx_in_order(ptr->getNextNE(), nptr);
    ptr->setNext(nptr);
    nptr = process_pfx_in_order(ptr->getNextEQ(), ptr);
  }
  return nptr;
}

// convert from binary tree to sorted list
int AffixMgr::process_sfx_tree_to_list() {
  for (int i = 1; i < SETSIZE; i++) {
    sStart[i] = process_sfx_in_order(sStart[i], NULL);
  }
  return 0;
}

SfxEntry* AffixMgr::process_sfx_in_order(SfxEntry* ptr, SfxEntry* nptr) {
  if (ptr) {
    nptr = process_sfx_in_order(ptr->getNextNE(), nptr);
    ptr->setNext(nptr);
    nptr = process_sfx_in_order(ptr->getNextEQ(), ptr);
  }
  return nptr;
}

// reinitialize the PfxEntry links NextEQ and NextNE to speed searching
// using the idea of leading subsets this time
int AffixMgr::process_pfx_order() {
  PfxEntry* ptr;

  // loop through each prefix list starting point
  for (int i = 1; i < SETSIZE; i++) {
    ptr = pStart[i];

    // look through the remainder of the list
    //  and find next entry with affix that
    // the current one is not a subset of
    // mark that as destination for NextNE
    // use next in list that you are a subset
    // of as NextEQ

    for (; ptr != NULL; ptr = ptr->getNext()) {
      PfxEntry* nptr = ptr->getNext();
      for (; nptr != NULL; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
      }
      ptr->setNextNE(nptr);
      ptr->setNextEQ(NULL);
      if ((ptr->getNext()) &&
          isSubset(ptr->getKey(), (ptr->getNext())->getKey()))
        ptr->setNextEQ(ptr->getNext());
    }

    // now clean up by adding smart search termination strings:
    // if you are already a superset of the previous prefix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = pStart[i];
    for (; ptr != NULL; ptr = ptr->getNext()) {
      PfxEntry* nptr = ptr->getNext();
      PfxEntry* mptr = NULL;
      for (; nptr != NULL; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
        mptr = nptr;
      }
      if (mptr)
        mptr->setNextNE(NULL);
    }
  }
  return 0;
}

// initialize the SfxEntry links NextEQ and NextNE to speed searching
// using the idea of leading subsets this time
int AffixMgr::process_sfx_order() {
  SfxEntry* ptr;

  // loop through each prefix list starting point
  for (int i = 1; i < SETSIZE; i++) {
    ptr = sStart[i];

    // look through the remainder of the list
    //  and find next entry with affix that
    // the current one is not a subset of
    // mark that as destination for NextNE
    // use next in list that you are a subset
    // of as NextEQ

    for (; ptr != NULL; ptr = ptr->getNext()) {
      SfxEntry* nptr = ptr->getNext();
      for (; nptr != NULL; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
      }
      ptr->setNextNE(nptr);
      ptr->setNextEQ(NULL);
      if ((ptr->getNext()) &&
          isSubset(ptr->getKey(), (ptr->getNext())->getKey()))
        ptr->setNextEQ(ptr->getNext());
    }

    // now clean up by adding smart search termination strings:
    // if you are already a superset of the previous suffix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = sStart[i];
    for (; ptr != NULL; ptr = ptr->getNext()) {
      SfxEntry* nptr = ptr->getNext();
      SfxEntry* mptr = NULL;
      for (; nptr != NULL; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
        mptr = nptr;
      }
      if (mptr)
        mptr->setNextNE(NULL);
    }
  }
  return 0;
}

// add flags to the result for dictionary debugging
std::string& AffixMgr::debugflag(std::string& result, unsigned short flag) {
  char* st = encode_flag(flag);
  result.push_back(MSEP_FLD);
  result.append(MORPH_FLAG);
  if (st) {
    result.append(st);
    free(st);
  }
  return result;
}

// calculate the character length of the condition
int AffixMgr::condlen(const char* st) {
  int l = 0;
  bool group = false;
  for (; *st; st++) {
    if (*st == '[') {
      group = true;
      l++;
    } else if (*st == ']')
      group = false;
    else if (!group && (!utf8 || (!(*st & 0x80) || ((*st & 0xc0) == 0x80))))
      l++;
  }
  return l;
}

int AffixMgr::encodeit(AffEntry& entry, const char* cs) {
  if (strcmp(cs, ".") != 0) {
    entry.numconds = (char)condlen(cs);
    const size_t cslen = strlen(cs);
    const size_t short_part = std::min<size_t>(MAXCONDLEN, cslen);
    memcpy(entry.c.conds, cs, short_part);
    if (short_part < MAXCONDLEN) {
      //blank out the remaining space
      memset(entry.c.conds + short_part, 0, MAXCONDLEN - short_part);
    } else if (cs[MAXCONDLEN]) {
      //there is more conditions than fit in fixed space, so its
      //a long condition
      entry.opts += aeLONGCOND;
      entry.c.l.conds2 = mystrdup(cs + MAXCONDLEN_1);
      if (!entry.c.l.conds2)
        return 1;
    }
  } else {
    entry.numconds = 0;
    entry.c.conds[0] = '\0';
  }
  return 0;
}

// return 1 if s1 is a leading subset of s2 (dots are for infixes)
inline int AffixMgr::isSubset(const char* s1, const char* s2) {
  while (((*s1 == *s2) || (*s1 == '.')) && (*s1 != '\0')) {
    s1++;
    s2++;
  }
  return (*s1 == '\0');
}

// check word for prefixes
struct hentry* AffixMgr::prefix_check(const char* word,
                                      int len,
                                      char in_compound,
                                      const FLAG needflag) {
  struct hentry* rv = NULL;

  pfx = NULL;
  pfxappnd = NULL;
  sfxappnd = NULL;
  sfxextra = 0;

  // first handle the special case of 0 length prefixes
  PfxEntry* pe = pStart[0];
  while (pe) {
    if (
        // fogemorpheme
        ((in_compound != IN_CPD_NOT) ||
         !(pe->getCont() &&
           (TESTAFF(pe->getCont(), onlyincompound, pe->getContLen())))) &&
        // permit prefixes in compounds
        ((in_compound != IN_CPD_END) ||
         (pe->getCont() &&
          (TESTAFF(pe->getCont(), compoundpermitflag, pe->getContLen()))))) {
      // check prefix
      rv = pe->checkword(word, len, in_compound, needflag);
      if (rv) {
        pfx = pe;  // BUG: pfx not stateless
        return rv;
      }
    }
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = *((const unsigned char*)word);
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word)) {
      if (
          // fogemorpheme
          ((in_compound != IN_CPD_NOT) ||
           !(pptr->getCont() &&
             (TESTAFF(pptr->getCont(), onlyincompound, pptr->getContLen())))) &&
          // permit prefixes in compounds
          ((in_compound != IN_CPD_END) ||
           (pptr->getCont() && (TESTAFF(pptr->getCont(), compoundpermitflag,
                                        pptr->getContLen()))))) {
        // check prefix
        rv = pptr->checkword(word, len, in_compound, needflag);
        if (rv) {
          pfx = pptr;  // BUG: pfx not stateless
          return rv;
        }
      }
      pptr = pptr->getNextEQ();
    } else {
      pptr = pptr->getNextNE();
    }
  }

  return NULL;
}

// check word for prefixes and two-level suffixes
struct hentry* AffixMgr::prefix_check_twosfx(const char* word,
                                             int len,
                                             char in_compound,
                                             const FLAG needflag) {
  struct hentry* rv = NULL;

  pfx = NULL;
  sfxappnd = NULL;
  sfxextra = 0;

  // first handle the special case of 0 length prefixes
  PfxEntry* pe = pStart[0];

  while (pe) {
    rv = pe->check_twosfx(word, len, in_compound, needflag);
    if (rv)
      return rv;
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = *((const unsigned char*)word);
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word)) {
      rv = pptr->check_twosfx(word, len, in_compound, needflag);
      if (rv) {
        pfx = pptr;
        return rv;
      }
      pptr = pptr->getNextEQ();
    } else {
      pptr = pptr->getNextNE();
    }
  }

  return NULL;
}

// check word for prefixes and morph
std::string AffixMgr::prefix_check_morph(const char* word,
                                         int len,
                                         char in_compound,
                                         const FLAG needflag) {

  std::string result;

  pfx = NULL;
  sfxappnd = NULL;
  sfxextra = 0;

  // first handle the special case of 0 length prefixes
  PfxEntry* pe = pStart[0];
  while (pe) {
    std::string st = pe->check_morph(word, len, in_compound, needflag);
    if (!st.empty()) {
      result.append(st);
    }
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = *((const unsigned char*)word);
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word)) {
      std::string st = pptr->check_morph(word, len, in_compound, needflag);
      if (!st.empty()) {
        // fogemorpheme
        if ((in_compound != IN_CPD_NOT) ||
            !((pptr->getCont() && (TESTAFF(pptr->getCont(), onlyincompound,
                                           pptr->getContLen()))))) {
          result.append(st);
          pfx = pptr;
        }
      }
      pptr = pptr->getNextEQ();
    } else {
      pptr = pptr->getNextNE();
    }
  }

  return result;
}

// check word for prefixes and morph and two-level suffixes
std::string AffixMgr::prefix_check_twosfx_morph(const char* word,
                                                int len,
                                                char in_compound,
                                                const FLAG needflag) {
  std::string result;

  pfx = NULL;
  sfxappnd = NULL;
  sfxextra = 0;

  // first handle the special case of 0 length prefixes
  PfxEntry* pe = pStart[0];
  while (pe) {
    std::string st = pe->check_twosfx_morph(word, len, in_compound, needflag);
    if (!st.empty()) {
      result.append(st);
    }
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = *((const unsigned char*)word);
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word)) {
      std::string st = pptr->check_twosfx_morph(word, len, in_compound, needflag);
      if (!st.empty()) {
        result.append(st);
        pfx = pptr;
      }
      pptr = pptr->getNextEQ();
    } else {
      pptr = pptr->getNextNE();
    }
  }

  return result;
}

// Is word a non-compound with a REP substitution (see checkcompoundrep)?
int AffixMgr::cpdrep_check(const char* word, int wl) {

#ifdef HUNSPELL_CHROME_CLIENT
  const char *pattern, *pattern2;
  hunspell::ReplacementIterator iterator = bdict_reader->GetReplacementIterator();
  while (iterator.GetNext(&pattern, &pattern2)) {
    const char* r = word;
    const size_t lenr = strlen(pattern2);
    const size_t lenp = strlen(pattern);

    // search every occurence of the pattern in the word
    while ((r=strstr(r, pattern)) != NULL) {
      std::string candidate(word);
      candidate.replace(r-word, lenp, pattern2);
      if (candidate_check(candidate.c_str(), candidate.size())) return 1;
      r++; // search for the next letter
    }
  }

#else
  if ((wl < 2) || get_reptable().empty())
    return 0;

  for (size_t i = 0; i < get_reptable().size(); ++i) {
    // use only available mid patterns
    if (!get_reptable()[i].outstrings[0].empty()) {
      const char* r = word;
      const size_t lenp = get_reptable()[i].pattern.size();
      // search every occurence of the pattern in the word
      while ((r = strstr(r, get_reptable()[i].pattern.c_str())) != NULL) {
        std::string candidate(word);
        candidate.replace(r - word, lenp, get_reptable()[i].outstrings[0]);
        if (candidate_check(candidate.c_str(), candidate.size()))
          return 1;
        ++r;  // search for the next letter
      }
    }
  }
#endif

 return 0;
}

// forbid compound words, if they are in the dictionary as a
// word pair separated by space
int AffixMgr::cpdwordpair_check(const char * word, int wl) {
  if (wl > 2) {
    std::string candidate(word);
    for (size_t i = 1; i < candidate.size(); i++) {
      // go to end of the UTF-8 character
      if (utf8 && ((word[i] & 0xc0) == 0x80))
          continue;
      candidate.insert(i, 1, ' ');
      if (candidate_check(candidate.c_str(), candidate.size()))
        return 1;
      candidate.erase(i, 1);
    }
  }

  return 0;
}

// forbid compoundings when there are special patterns at word bound
int AffixMgr::cpdpat_check(const char* word,
                           int pos,
                           hentry* r1,
                           hentry* r2,
                           const char /*affixed*/) {
  for (size_t i = 0; i < checkcpdtable.size(); ++i) {
    size_t len;
    if (isSubset(checkcpdtable[i].pattern2.c_str(), word + pos) &&
        (!r1 || !checkcpdtable[i].cond ||
         (r1->astr && TESTAFF(r1->astr, checkcpdtable[i].cond, r1->alen))) &&
        (!r2 || !checkcpdtable[i].cond2 ||
         (r2->astr && TESTAFF(r2->astr, checkcpdtable[i].cond2, r2->alen))) &&
        // zero length pattern => only TESTAFF
        // zero pattern (0/flag) => unmodified stem (zero affixes allowed)
        (checkcpdtable[i].pattern.empty() ||
         ((checkcpdtable[i].pattern[0] == '0' && r1->blen <= pos &&
           strncmp(word + pos - r1->blen, r1->word, r1->blen) == 0) ||
          (checkcpdtable[i].pattern[0] != '0' &&
           ((len = checkcpdtable[i].pattern.size()) != 0) &&
           strncmp(word + pos - len, checkcpdtable[i].pattern.c_str(), len) == 0)))) {
      return 1;
    }
  }
  return 0;
}

// forbid compounding with neighbouring upper and lower case characters at word
// bounds
int AffixMgr::cpdcase_check(const char* word, int pos) {
  if (utf8) {
    const char* p;
    for (p = word + pos - 1; (*p & 0xc0) == 0x80; p--)
      ;
    std::string pair(p);
    std::vector<w_char> pair_u;
    u8_u16(pair_u, pair);
    unsigned short a = pair_u.size() > 1 ? ((pair_u[1].h << 8) + pair_u[1].l) : 0;
    unsigned short b = !pair_u.empty() ? ((pair_u[0].h << 8) + pair_u[0].l) : 0;
    if (((unicodetoupper(a, langnum) == a) ||
         (unicodetoupper(b, langnum) == b)) &&
        (a != '-') && (b != '-'))
      return 1;
  } else {
    unsigned char a = *(word + pos - 1);
    unsigned char b = *(word + pos);
    if ((csconv[a].ccase || csconv[b].ccase) && (a != '-') && (b != '-'))
      return 1;
  }
  return 0;
}

struct metachar_data {
  signed short btpp;  // metacharacter (*, ?) position for backtracking
  signed short btwp;  // word position for metacharacters
  int btnum;          // number of matched characters in metacharacter
};

// check compound patterns
int AffixMgr::defcpd_check(hentry*** words,
                           short wnum,
                           hentry* rv,
                           hentry** def,
                           char all) {
  int w = 0;

  if (!*words) {
    w = 1;
    *words = def;
  }

  if (!*words) {
    return 0;
  }

  std::vector<metachar_data> btinfo(1);

  short bt = 0;

  (*words)[wnum] = rv;

  // has the last word COMPOUNDRULE flag?
  if (rv->alen == 0) {
    (*words)[wnum] = NULL;
    if (w)
      *words = NULL;
    return 0;
  }
  int ok = 0;
  for (size_t i = 0; i < defcpdtable.size(); ++i) {
    for (size_t j = 0; j < defcpdtable[i].size(); ++j) {
      if (defcpdtable[i][j] != '*' && defcpdtable[i][j] != '?' &&
          TESTAFF(rv->astr, defcpdtable[i][j], rv->alen)) {
        ok = 1;
        break;
      }
    }
  }
  if (ok == 0) {
    (*words)[wnum] = NULL;
    if (w)
      *words = NULL;
    return 0;
  }

  for (size_t i = 0; i < defcpdtable.size(); ++i) {
    size_t pp = 0;  // pattern position
    signed short wp = 0;  // "words" position
    int ok2;
    ok = 1;
    ok2 = 1;
    do {
      while ((pp < defcpdtable[i].size()) && (wp <= wnum)) {
        if (((pp + 1) < defcpdtable[i].size()) &&
            ((defcpdtable[i][pp + 1] == '*') ||
             (defcpdtable[i][pp + 1] == '?'))) {
          int wend = (defcpdtable[i][pp + 1] == '?') ? wp : wnum;
          ok2 = 1;
          pp += 2;
          btinfo[bt].btpp = pp;
          btinfo[bt].btwp = wp;
          while (wp <= wend) {
            if (!(*words)[wp]->alen ||
                !TESTAFF((*words)[wp]->astr, defcpdtable[i][pp - 2],
                         (*words)[wp]->alen)) {
              ok2 = 0;
              break;
            }
            wp++;
          }
          if (wp <= wnum)
            ok2 = 0;
          btinfo[bt].btnum = wp - btinfo[bt].btwp;
          if (btinfo[bt].btnum > 0) {
            ++bt;
            btinfo.resize(bt+1);
          }
          if (ok2)
            break;
        } else {
          ok2 = 1;
          if (!(*words)[wp] || !(*words)[wp]->alen ||
              !TESTAFF((*words)[wp]->astr, defcpdtable[i][pp],
                       (*words)[wp]->alen)) {
            ok = 0;
            break;
          }
          pp++;
          wp++;
          if ((defcpdtable[i].size() == pp) && !(wp > wnum))
            ok = 0;
        }
      }
      if (ok && ok2) {
        size_t r = pp;
        while ((defcpdtable[i].size() > r) && ((r + 1) < defcpdtable[i].size()) &&
               ((defcpdtable[i][r + 1] == '*') ||
                (defcpdtable[i][r + 1] == '?')))
          r += 2;
        if (defcpdtable[i].size() <= r)
          return 1;
      }
      // backtrack
      if (bt)
        do {
          ok = 1;
          btinfo[bt - 1].btnum--;
          pp = btinfo[bt - 1].btpp;
          wp = btinfo[bt - 1].btwp + (signed short)btinfo[bt - 1].btnum;
        } while ((btinfo[bt - 1].btnum < 0) && --bt);
    } while (bt);

    if (ok && ok2 && (!all || (defcpdtable[i].size() <= pp)))
      return 1;

    // check zero ending
    while (ok && ok2 && (defcpdtable[i].size() > pp) &&
           ((pp + 1) < defcpdtable[i].size()) &&
           ((defcpdtable[i][pp + 1] == '*') ||
            (defcpdtable[i][pp + 1] == '?')))
      pp += 2;
    if (ok && ok2 && (defcpdtable[i].size() <= pp))
      return 1;
  }
  (*words)[wnum] = NULL;
  if (w)
    *words = NULL;
  return 0;
}

inline int AffixMgr::candidate_check(const char* word, int len) {

  struct hentry* rv = lookup(word);
  if (rv)
    return 1;

  //  rv = prefix_check(word,len,1);
  //  if (rv) return 1;

  rv = affix_check(word, len);
  if (rv)
    return 1;
  return 0;
}

// calculate number of syllable for compound-checking
short AffixMgr::get_syllable(const std::string& word) {
  if (cpdmaxsyllable == 0)
    return 0;

  short num = 0;

  if (!utf8) {
    for (size_t i = 0; i < word.size(); ++i) {
      if (std::binary_search(cpdvowels.begin(), cpdvowels.end(),
                             word[i])) {
        ++num;
      }
    }
  } else if (!cpdvowels_utf16.empty()) {
    std::vector<w_char> w;
    u8_u16(w, word);
    for (size_t i = 0; i < w.size(); ++i) {
      if (std::binary_search(cpdvowels_utf16.begin(),
                             cpdvowels_utf16.end(),
                             w[i])) {
        ++num;
      }
    }
  }

  return num;
}

void AffixMgr::setcminmax(int* cmin, int* cmax, const char* word, int len) {
  if (utf8) {
    int i;
    for (*cmin = 0, i = 0; (i < cpdmin) && *cmin < len; i++) {
      for ((*cmin)++; *cmin < len && (word[*cmin] & 0xc0) == 0x80; (*cmin)++)
        ;
    }
    for (*cmax = len, i = 0; (i < (cpdmin - 1)) && *cmax >= 0; i++) {
      for ((*cmax)--; *cmax >= 0 && (word[*cmax] & 0xc0) == 0x80; (*cmax)--)
        ;
    }
  } else {
    *cmin = cpdmin;
    *cmax = len - cpdmin + 1;
  }
}

// check if compound word is correctly spelled
// hu_mov_rule = spec. Hungarian rule (XXX)
struct hentry* AffixMgr::compound_check(const std::string& word,
                                        short wordnum,
                                        short numsyllable,
                                        short maxwordnum,
                                        short wnum,
                                        hentry** words = NULL,
                                        hentry** rwords = NULL,
                                        char hu_mov_rule = 0,
                                        char is_sug = 0,
                                        int* info = NULL) {
  int i;
  short oldnumsyllable, oldnumsyllable2, oldwordnum, oldwordnum2;
  struct hentry* rv = NULL;
  struct hentry* rv_first;
  std::string st;
  char ch = '\0';
  int cmin;
  int cmax;
  int striple = 0;
  size_t scpd = 0;
  int soldi = 0;
  int oldcmin = 0;
  int oldcmax = 0;
  int oldlen = 0;
  int checkedstriple = 0;
  char affixed = 0;
  hentry** oldwords = words;
  size_t len = word.size();

  int checked_prefix;

  // add a time limit to handle possible
  // combinatorical explosion of the overlapping words

  HUNSPELL_THREAD_LOCAL clock_t timelimit;

  if (wordnum == 0)
      timelimit = clock();
  else if (timelimit != 0 && (clock() > timelimit + TIMELIMIT)) {
      timelimit = 0;
  }

  setcminmax(&cmin, &cmax, word.c_str(), len);

  st.assign(word);

  for (i = cmin; i < cmax; i++) {
    // go to end of the UTF-8 character
    if (utf8) {
      for (; (st[i] & 0xc0) == 0x80; i++)
        ;
      if (i >= cmax)
        return NULL;
    }

    words = oldwords;
    int onlycpdrule = (words) ? 1 : 0;

    do {  // onlycpdrule loop

      oldnumsyllable = numsyllable;
      oldwordnum = wordnum;
      checked_prefix = 0;

      do {  // simplified checkcompoundpattern loop

        if (timelimit == 0)
          return 0;

        if (scpd > 0) {
          for (; scpd <= checkcpdtable.size() &&
                 (checkcpdtable[scpd - 1].pattern3.empty() ||
                  strncmp(word.c_str() + i, checkcpdtable[scpd - 1].pattern3.c_str(),
                          checkcpdtable[scpd - 1].pattern3.size()) != 0);
               scpd++)
            ;

          if (scpd > checkcpdtable.size())
            break;  // break simplified checkcompoundpattern loop
          st.replace(i, std::string::npos, checkcpdtable[scpd - 1].pattern);
          soldi = i;
          i += checkcpdtable[scpd - 1].pattern.size();
          st.replace(i, std::string::npos, checkcpdtable[scpd - 1].pattern2);
          st.replace(i + checkcpdtable[scpd - 1].pattern2.size(), std::string::npos,
                 word.substr(soldi + checkcpdtable[scpd - 1].pattern3.size()));

          oldlen = len;
          len += checkcpdtable[scpd - 1].pattern.size() +
                 checkcpdtable[scpd - 1].pattern2.size() -
                 checkcpdtable[scpd - 1].pattern3.size();
          oldcmin = cmin;
          oldcmax = cmax;
          setcminmax(&cmin, &cmax, st.c_str(), len);

          cmax = len - cpdmin + 1;
        }

        ch = st[i];
        st[i] = '\0';

        sfx = NULL;
        pfx = NULL;

        // FIRST WORD

        affixed = 1;
        rv = lookup(st.c_str());  // perhaps without prefix

        // forbid dictionary stems with COMPOUNDFORBIDFLAG in
        // compound words, overriding the effect of COMPOUNDPERMITFLAG
        if ((rv) && compoundforbidflag &&
                TESTAFF(rv->astr, compoundforbidflag, rv->alen) && !hu_mov_rule)
            continue;

        // search homonym with compound flag
        while ((rv) && !hu_mov_rule &&
               ((needaffix && TESTAFF(rv->astr, needaffix, rv->alen)) ||
                !((compoundflag && !words && !onlycpdrule &&
                   TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                  (compoundbegin && !wordnum && !onlycpdrule &&
                   TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
                  (compoundmiddle && wordnum && !words && !onlycpdrule &&
                   TESTAFF(rv->astr, compoundmiddle, rv->alen)) ||
                  (!defcpdtable.empty() && onlycpdrule &&
                   ((!words && !wordnum &&
                     defcpd_check(&words, wnum, rv, rwords, 0)) ||
                    (words &&
                     defcpd_check(&words, wnum, rv, rwords, 0))))) ||
                (scpd != 0 && checkcpdtable[scpd - 1].cond != FLAG_NULL &&
                 !TESTAFF(rv->astr, checkcpdtable[scpd - 1].cond, rv->alen)))) {
          rv = rv->next_homonym;
        }

        if (rv)
          affixed = 0;

        if (!rv) {
          if (onlycpdrule)
            break;
          if (compoundflag &&
              !(rv = prefix_check(st.c_str(), i,
                                  hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                                  compoundflag))) {
            if (((rv = suffix_check(
                      st.c_str(), i, 0, NULL, FLAG_NULL, compoundflag,
                      hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                 (compoundmoresuffixes &&
                  (rv = suffix_check_twosfx(st.c_str(), i, 0, NULL, compoundflag)))) &&
                !hu_mov_rule && sfx->getCont() &&
                ((compoundforbidflag &&
                  TESTAFF(sfx->getCont(), compoundforbidflag,
                          sfx->getContLen())) ||
                 (compoundend &&
                  TESTAFF(sfx->getCont(), compoundend, sfx->getContLen())))) {
              rv = NULL;
            }
          }

          if (rv ||
              (((wordnum == 0) && compoundbegin &&
                ((rv = suffix_check(
                      st.c_str(), i, 0, NULL, FLAG_NULL, compoundbegin,
                      hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                 (compoundmoresuffixes &&
                  (rv = suffix_check_twosfx(
                       st.c_str(), i, 0, NULL,
                       compoundbegin))) ||  // twofold suffixes + compound
                 (rv = prefix_check(st.c_str(), i,
                                    hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                                    compoundbegin)))) ||
               ((wordnum > 0) && compoundmiddle &&
                ((rv = suffix_check(
                      st.c_str(), i, 0, NULL, FLAG_NULL, compoundmiddle,
                      hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                 (compoundmoresuffixes &&
                  (rv = suffix_check_twosfx(
                       st.c_str(), i, 0, NULL,
                       compoundmiddle))) ||  // twofold suffixes + compound
                 (rv = prefix_check(st.c_str(), i,
                                    hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                                    compoundmiddle))))))
            checked_prefix = 1;
          // else check forbiddenwords and needaffix
        } else if (rv->astr && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
                                TESTAFF(rv->astr, needaffix, rv->alen) ||
                                TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
                                (is_sug && nosuggest &&
                                 TESTAFF(rv->astr, nosuggest, rv->alen)))) {
          st[i] = ch;
          // continue;
          break;
        }

        // check non_compound flag in suffix and prefix
        if ((rv) && !hu_mov_rule &&
            ((pfx && pfx->getCont() &&
              TESTAFF(pfx->getCont(), compoundforbidflag, pfx->getContLen())) ||
             (sfx && sfx->getCont() &&
              TESTAFF(sfx->getCont(), compoundforbidflag,
                      sfx->getContLen())))) {
          rv = NULL;
        }

        // check compoundend flag in suffix and prefix
        if ((rv) && !checked_prefix && compoundend && !hu_mov_rule &&
            ((pfx && pfx->getCont() &&
              TESTAFF(pfx->getCont(), compoundend, pfx->getContLen())) ||
             (sfx && sfx->getCont() &&
              TESTAFF(sfx->getCont(), compoundend, sfx->getContLen())))) {
          rv = NULL;
        }

        // check compoundmiddle flag in suffix and prefix
        if ((rv) && !checked_prefix && (wordnum == 0) && compoundmiddle &&
            !hu_mov_rule &&
            ((pfx && pfx->getCont() &&
              TESTAFF(pfx->getCont(), compoundmiddle, pfx->getContLen())) ||
             (sfx && sfx->getCont() &&
              TESTAFF(sfx->getCont(), compoundmiddle, sfx->getContLen())))) {
          rv = NULL;
        }

        // check forbiddenwords
        if ((rv) && (rv->astr) &&
            (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
             TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
             (is_sug && nosuggest && TESTAFF(rv->astr, nosuggest, rv->alen)))) {
          return NULL;
        }

        // increment word number, if the second root has a compoundroot flag
        if ((rv) && compoundroot &&
            (TESTAFF(rv->astr, compoundroot, rv->alen))) {
          wordnum++;
        }

        // first word is acceptable in compound words?
        if (((rv) &&
             (checked_prefix || (words && words[wnum]) ||
              (compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
              ((oldwordnum == 0) && compoundbegin &&
               TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
              ((oldwordnum > 0) && compoundmiddle &&
               TESTAFF(rv->astr, compoundmiddle, rv->alen))

              // LANG_hu section: spec. Hungarian rule
              || ((langnum == LANG_hu) && hu_mov_rule &&
                  (TESTAFF(
                       rv->astr, 'F',
                       rv->alen) ||  // XXX hardwired Hungarian dictionary codes
                   TESTAFF(rv->astr, 'G', rv->alen) ||
                   TESTAFF(rv->astr, 'H', rv->alen)))
              // END of LANG_hu section
              ) &&
             (
                 // test CHECKCOMPOUNDPATTERN conditions
                 scpd == 0 || checkcpdtable[scpd - 1].cond == FLAG_NULL ||
                 TESTAFF(rv->astr, checkcpdtable[scpd - 1].cond, rv->alen)) &&
             !((checkcompoundtriple && scpd == 0 &&
                !words &&  // test triple letters
                (word[i - 1] == word[i]) &&
                (((i > 1) && (word[i - 1] == word[i - 2])) ||
                 ((word[i - 1] == word[i + 1]))  // may be word[i+1] == '\0'
                 )) ||
               (checkcompoundcase && scpd == 0 && !words &&
                cpdcase_check(word.c_str(), i))))
            // LANG_hu section: spec. Hungarian rule
            || ((!rv) && (langnum == LANG_hu) && hu_mov_rule &&
                (rv = affix_check(st.c_str(), i)) &&
                (sfx && sfx->getCont() &&
                 (  // XXX hardwired Hungarian dic. codes
                     TESTAFF(sfx->getCont(), (unsigned short)'x',
                             sfx->getContLen()) ||
                     TESTAFF(
                         sfx->getCont(), (unsigned short)'%',
                         sfx->getContLen()))))) {  // first word is ok condition

          // LANG_hu section: spec. Hungarian rule
          if (langnum == LANG_hu) {
            // calculate syllable number of the word
            numsyllable += get_syllable(st.substr(0, i));
            // + 1 word, if syllable number of the prefix > 1 (hungarian
            // convention)
            if (pfx && (get_syllable(pfx->getKey()) > 1))
              wordnum++;
          }
          // END of LANG_hu section

          // NEXT WORD(S)
          rv_first = rv;
          st[i] = ch;

          do {  // striple loop

            // check simplifiedtriple
            if (simplifiedtriple) {
              if (striple) {
                checkedstriple = 1;
                i--;  // check "fahrt" instead of "ahrt" in "Schiffahrt"
              } else if (i > 2 && word[i - 1] == word[i - 2])
                striple = 1;
            }

            rv = lookup(st.c_str() + i);  // perhaps without prefix

            // search homonym with compound flag
            while ((rv) &&
                   ((needaffix && TESTAFF(rv->astr, needaffix, rv->alen)) ||
                    !((compoundflag && !words &&
                       TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                      (compoundend && !words &&
                       TESTAFF(rv->astr, compoundend, rv->alen)) ||
                      (!defcpdtable.empty() && words &&
                       defcpd_check(&words, wnum + 1, rv, NULL, 1))) ||
                    (scpd != 0 && checkcpdtable[scpd - 1].cond2 != FLAG_NULL &&
                     !TESTAFF(rv->astr, checkcpdtable[scpd - 1].cond2,
                              rv->alen)))) {
              rv = rv->next_homonym;
            }

            // check FORCEUCASE
            if (rv && forceucase && (rv) &&
                (TESTAFF(rv->astr, forceucase, rv->alen)) &&
                !(info && *info & SPELL_ORIGCAP))
              rv = NULL;

            if (rv && words && words[wnum + 1])
              return rv_first;

            oldnumsyllable2 = numsyllable;
            oldwordnum2 = wordnum;

            // LANG_hu section: spec. Hungarian rule, XXX hardwired dictionary
            // code
            if ((rv) && (langnum == LANG_hu) &&
                (TESTAFF(rv->astr, 'I', rv->alen)) &&
                !(TESTAFF(rv->astr, 'J', rv->alen))) {
              numsyllable--;
            }
            // END of LANG_hu section

            // increment word number, if the second root has a compoundroot flag
            if ((rv) && (compoundroot) &&
                (TESTAFF(rv->astr, compoundroot, rv->alen))) {
              wordnum++;
            }

            // check forbiddenwords
            if ((rv) && (rv->astr) &&
                (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
                 TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
                 (is_sug && nosuggest &&
                  TESTAFF(rv->astr, nosuggest, rv->alen))))
              return NULL;

            // second word is acceptable, as a root?
            // hungarian conventions: compounding is acceptable,
            // when compound forms consist of 2 words, or if more,
            // then the syllable number of root words must be 6, or lesser.

            if ((rv) &&
                ((compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                 (compoundend && TESTAFF(rv->astr, compoundend, rv->alen))) &&
                (((cpdwordmax == -1) || (wordnum + 1 < cpdwordmax)) ||
                 ((cpdmaxsyllable != 0) &&
                  (numsyllable + get_syllable(std::string(HENTRY_WORD(rv), rv->blen)) <=
                   cpdmaxsyllable))) &&
                (
                    // test CHECKCOMPOUNDPATTERN
                    checkcpdtable.empty() || scpd != 0 ||
                    !cpdpat_check(word.c_str(), i, rv_first, rv, 0)) &&
                ((!checkcompounddup || (rv != rv_first)))
                // test CHECKCOMPOUNDPATTERN conditions
                &&
                (scpd == 0 || checkcpdtable[scpd - 1].cond2 == FLAG_NULL ||
                 TESTAFF(rv->astr, checkcpdtable[scpd - 1].cond2, rv->alen))) {
              // forbid compound word, if it is a non-compound word with typical
              // fault
              if ((checkcompoundrep && cpdrep_check(word.c_str(), len)) ||
                      cpdwordpair_check(word.c_str(), len))
                return NULL;
              return rv_first;
            }

            numsyllable = oldnumsyllable2;
            wordnum = oldwordnum2;

            // perhaps second word has prefix or/and suffix
            sfx = NULL;
            sfxflag = FLAG_NULL;
            rv = (compoundflag && !onlycpdrule)
                     ? affix_check((word.c_str() + i), strlen(word.c_str() + i), compoundflag,
                                   IN_CPD_END)
                     : NULL;
            if (!rv && compoundend && !onlycpdrule) {
              sfx = NULL;
              pfx = NULL;
              rv = affix_check((word.c_str() + i), strlen(word.c_str() + i), compoundend,
                               IN_CPD_END);
            }

            if (!rv && !defcpdtable.empty() && words) {
              rv = affix_check((word.c_str() + i), strlen(word.c_str() + i), 0, IN_CPD_END);
              if (rv && defcpd_check(&words, wnum + 1, rv, NULL, 1))
                return rv_first;
              rv = NULL;
            }

            // test CHECKCOMPOUNDPATTERN conditions (allowed forms)
            if (rv &&
                !(scpd == 0 || checkcpdtable[scpd - 1].cond2 == FLAG_NULL ||
                  TESTAFF(rv->astr, checkcpdtable[scpd - 1].cond2, rv->alen)))
              rv = NULL;

            // test CHECKCOMPOUNDPATTERN conditions (forbidden compounds)
            if (rv && !checkcpdtable.empty() && scpd == 0 &&
                cpdpat_check(word.c_str(), i, rv_first, rv, affixed))
              rv = NULL;

            // check non_compound flag in suffix and prefix
            if ((rv) && ((pfx && pfx->getCont() &&
                          TESTAFF(pfx->getCont(), compoundforbidflag,
                                  pfx->getContLen())) ||
                         (sfx && sfx->getCont() &&
                          TESTAFF(sfx->getCont(), compoundforbidflag,
                                  sfx->getContLen())))) {
              rv = NULL;
            }

            // check FORCEUCASE
            if (rv && forceucase && (rv) &&
                (TESTAFF(rv->astr, forceucase, rv->alen)) &&
                !(info && *info & SPELL_ORIGCAP))
              rv = NULL;

            // check forbiddenwords
            if ((rv) && (rv->astr) &&
                (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
                 TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
                 (is_sug && nosuggest &&
                  TESTAFF(rv->astr, nosuggest, rv->alen))))
              return NULL;

            // pfxappnd = prefix of word+i, or NULL
            // calculate syllable number of prefix.
            // hungarian convention: when syllable number of prefix is more,
            // than 1, the prefix+word counts as two words.

            if (langnum == LANG_hu) {
              // calculate syllable number of the word
              numsyllable += get_syllable(word.c_str() + i);

              // - affix syllable num.
              // XXX only second suffix (inflections, not derivations)
              if (sfxappnd) {
                std::string tmp(sfxappnd);
                reverseword(tmp);
                numsyllable -= short(get_syllable(tmp) + sfxextra);
              } else {
                numsyllable -= short(sfxextra);
              }

              // + 1 word, if syllable number of the prefix > 1 (hungarian
              // convention)
              if (pfx && (get_syllable(pfx->getKey()) > 1))
                wordnum++;

              // increment syllable num, if last word has a SYLLABLENUM flag
              // and the suffix is beginning `s'

              if (!cpdsyllablenum.empty()) {
                switch (sfxflag) {
                  case 'c': {
                    numsyllable += 2;
                    break;
                  }
                  case 'J': {
                    numsyllable += 1;
                    break;
                  }
                  case 'I': {
                    if (rv && TESTAFF(rv->astr, 'J', rv->alen))
                      numsyllable += 1;
                    break;
                  }
                }
              }
            }

            // increment word number, if the second word has a compoundroot flag
            if ((rv) && (compoundroot) &&
                (TESTAFF(rv->astr, compoundroot, rv->alen))) {
              wordnum++;
            }
            // second word is acceptable, as a word with prefix or/and suffix?
            // hungarian conventions: compounding is acceptable,
            // when compound forms consist 2 word, otherwise
            // the syllable number of root words is 6, or lesser.
            if ((rv) &&
                (((cpdwordmax == -1) || (wordnum + 1 < cpdwordmax)) ||
                 ((cpdmaxsyllable != 0) && (numsyllable <= cpdmaxsyllable))) &&
                ((!checkcompounddup || (rv != rv_first)))) {
              // forbid compound word, if it is a non-compound word with typical
              // fault
              if ((checkcompoundrep && cpdrep_check(word.c_str(), len)) ||
                      cpdwordpair_check(word.c_str(), len))
                return NULL;
              return rv_first;
            }

            numsyllable = oldnumsyllable2;
            wordnum = oldwordnum2;

            // perhaps second word is a compound word (recursive call)
            if (wordnum + 2 < maxwordnum) {
              rv = compound_check(st.substr(i), wordnum + 1,
                                  numsyllable, maxwordnum, wnum + 1, words, rwords, 0,
                                  is_sug, info);

              if (rv && !checkcpdtable.empty() &&
                  ((scpd == 0 &&
                    cpdpat_check(word.c_str(), i, rv_first, rv, affixed)) ||
                   (scpd != 0 &&
                    !cpdpat_check(word.c_str(), i, rv_first, rv, affixed))))
                rv = NULL;
            } else {
              rv = NULL;
            }
            if (rv) {
              // forbid compound word, if it is a non-compound word with typical
              // fault, or a dictionary word pair

              if (cpdwordpair_check(word.c_str(), len))
                  return NULL;

              if (checkcompoundrep || forbiddenword) {

                if (checkcompoundrep && cpdrep_check(word.c_str(), len))
                  return NULL;

                // check first part
                if (strncmp(rv->word, word.c_str() + i, rv->blen) == 0) {
                  char r = st[i + rv->blen];
                  st[i + rv->blen] = '\0';

                  if ((checkcompoundrep && cpdrep_check(st.c_str(), i + rv->blen)) ||
                      cpdwordpair_check(st.c_str(), i + rv->blen)) {
                    st[ + i + rv->blen] = r;
                    continue;
                  }

                  if (forbiddenword) {
                    struct hentry* rv2 = lookup(word.c_str());
                    if (!rv2)
                      rv2 = affix_check(word.c_str(), len);
                    if (rv2 && rv2->astr &&
                        TESTAFF(rv2->astr, forbiddenword, rv2->alen) &&
                        (strncmp(rv2->word, st.c_str(), i + rv->blen) == 0)) {
                      return NULL;
                    }
                  }
                  st[i + rv->blen] = r;
                }
              }
              return rv_first;
            }
          } while (striple && !checkedstriple);  // end of striple loop

          if (checkedstriple) {
            i++;
            checkedstriple = 0;
            striple = 0;
          }

        }  // first word is ok condition

        if (soldi != 0) {
          i = soldi;
          soldi = 0;
          len = oldlen;
          cmin = oldcmin;
          cmax = oldcmax;
        }
        scpd++;

      } while (!onlycpdrule && simplifiedcpd &&
               scpd <= checkcpdtable.size());  // end of simplifiedcpd loop

      scpd = 0;
      wordnum = oldwordnum;
      numsyllable = oldnumsyllable;

      if (soldi != 0) {
        i = soldi;
        st.assign(word);  // XXX add more optim.
        soldi = 0;
      } else
        st[i] = ch;

    } while (!defcpdtable.empty() && oldwordnum == 0 &&
             onlycpdrule++ < 1);  // end of onlycpd loop
  }

  return NULL;
}

// check if compound word is correctly spelled
// hu_mov_rule = spec. Hungarian rule (XXX)
int AffixMgr::compound_check_morph(const char* word,
                                   int len,
                                   short wordnum,
                                   short numsyllable,
                                   short maxwordnum,
                                   short wnum,
                                   hentry** words,
                                   hentry** rwords,
                                   char hu_mov_rule,
                                   std::string& result,
                                   const std::string* partresult) {
  int i;
  short oldnumsyllable, oldnumsyllable2, oldwordnum, oldwordnum2;
  int ok = 0;

  struct hentry* rv = NULL;
  struct hentry* rv_first;
  std::string st;
  char ch;

  int checked_prefix;
  std::string presult;

  int cmin;
  int cmax;

  char affixed = 0;
  hentry** oldwords = words;

  // add a time limit to handle possible
  // combinatorical explosion of the overlapping words

  HUNSPELL_THREAD_LOCAL clock_t timelimit;

  if (wordnum == 0)
      timelimit = clock();
  else if (timelimit != 0 && (clock() > timelimit + TIMELIMIT)) {
      timelimit = 0;
  }

  setcminmax(&cmin, &cmax, word, len);

  st.assign(word);

  for (i = cmin; i < cmax; i++) {
    // go to end of the UTF-8 character
    if (utf8) {
      for (; (st[i] & 0xc0) == 0x80; i++)
        ;
      if (i >= cmax)
        return 0;
    }

    words = oldwords;
    int onlycpdrule = (words) ? 1 : 0;

    do {  // onlycpdrule loop

      if (timelimit == 0)
        return 0;

      oldnumsyllable = numsyllable;
      oldwordnum = wordnum;
      checked_prefix = 0;

      ch = st[i];
      st[i] = '\0';
      sfx = NULL;

      // FIRST WORD

      affixed = 1;

      presult.clear();
      if (partresult)
        presult.append(*partresult);

      rv = lookup(st.c_str());  // perhaps without prefix

      // forbid dictionary stems with COMPOUNDFORBIDFLAG in
      // compound words, overriding the effect of COMPOUNDPERMITFLAG
      if ((rv) && compoundforbidflag &&
              TESTAFF(rv->astr, compoundforbidflag, rv->alen) && !hu_mov_rule)
          continue;

      // search homonym with compound flag
      while ((rv) && !hu_mov_rule &&
             ((needaffix && TESTAFF(rv->astr, needaffix, rv->alen)) ||
              !((compoundflag && !words && !onlycpdrule &&
                 TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                (compoundbegin && !wordnum && !onlycpdrule &&
                 TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
                (compoundmiddle && wordnum && !words && !onlycpdrule &&
                 TESTAFF(rv->astr, compoundmiddle, rv->alen)) ||
                (!defcpdtable.empty() && onlycpdrule &&
                 ((!words && !wordnum &&
                   defcpd_check(&words, wnum, rv, rwords, 0)) ||
                  (words &&
                   defcpd_check(&words, wnum, rv, rwords, 0))))))) {
        rv = rv->next_homonym;
      }

      if (timelimit == 0)
        return 0;

      if (rv)
        affixed = 0;

      if (rv) {
        presult.push_back(MSEP_FLD);
        presult.append(MORPH_PART);
        presult.append(st.c_str());
        if (!HENTRY_FIND(rv, MORPH_STEM)) {
          presult.push_back(MSEP_FLD);
          presult.append(MORPH_STEM);
          presult.append(st.c_str());
        }
        if (HENTRY_DATA(rv)) {
          presult.push_back(MSEP_FLD);
          presult.append(HENTRY_DATA2(rv));
        }
      }

      if (!rv) {
        if (compoundflag &&
            !(rv =
                  prefix_check(st.c_str(), i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                               compoundflag))) {
          if (((rv = suffix_check(st.c_str(), i, 0, NULL, FLAG_NULL,
                                  compoundflag,
                                  hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
               (compoundmoresuffixes &&
                (rv = suffix_check_twosfx(st.c_str(), i, 0, NULL, compoundflag)))) &&
              !hu_mov_rule && sfx->getCont() &&
              ((compoundforbidflag &&
                TESTAFF(sfx->getCont(), compoundforbidflag,
                        sfx->getContLen())) ||
               (compoundend &&
                TESTAFF(sfx->getCont(), compoundend, sfx->getContLen())))) {
            rv = NULL;
          }
        }

        if (rv ||
            (((wordnum == 0) && compoundbegin &&
              ((rv = suffix_check(st.c_str(), i, 0, NULL, FLAG_NULL,
                                  compoundbegin,
                                  hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
               (compoundmoresuffixes &&
                (rv = suffix_check_twosfx(
                     st.c_str(), i, 0, NULL,
                     compoundbegin))) ||  // twofold suffix+compound
               (rv = prefix_check(st.c_str(), i,
                                  hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                                  compoundbegin)))) ||
             ((wordnum > 0) && compoundmiddle &&
              ((rv = suffix_check(st.c_str(), i, 0, NULL, FLAG_NULL,
                                  compoundmiddle,
                                  hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
               (compoundmoresuffixes &&
                (rv = suffix_check_twosfx(
                     st.c_str(), i, 0, NULL,
                     compoundmiddle))) ||  // twofold suffix+compound
               (rv = prefix_check(st.c_str(), i,
                                  hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                                  compoundmiddle)))))) {
          std::string p;
          if (compoundflag)
            p = affix_check_morph(st.c_str(), i, compoundflag);
          if (p.empty()) {
            if ((wordnum == 0) && compoundbegin) {
              p = affix_check_morph(st.c_str(), i, compoundbegin);
            } else if ((wordnum > 0) && compoundmiddle) {
              p = affix_check_morph(st.c_str(), i, compoundmiddle);
            }
          }
          if (!p.empty()) {
            presult.push_back(MSEP_FLD);
            presult.append(MORPH_PART);
            presult.append(st.c_str());
            line_uniq_app(p, MSEP_REC);
            presult.append(p);
          }
          checked_prefix = 1;
        }
        // else check forbiddenwords
      } else if (rv->astr && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
                              TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
                              TESTAFF(rv->astr, needaffix, rv->alen))) {
        st[i] = ch;
        continue;
      }

      // check non_compound flag in suffix and prefix
      if ((rv) && !hu_mov_rule &&
          ((pfx && pfx->getCont() &&
            TESTAFF(pfx->getCont(), compoundforbidflag, pfx->getContLen())) ||
           (sfx && sfx->getCont() &&
            TESTAFF(sfx->getCont(), compoundforbidflag, sfx->getContLen())))) {
        continue;
      }

      // check compoundend flag in suffix and prefix
      if ((rv) && !checked_prefix && compoundend && !hu_mov_rule &&
          ((pfx && pfx->getCont() &&
            TESTAFF(pfx->getCont(), compoundend, pfx->getContLen())) ||
           (sfx && sfx->getCont() &&
            TESTAFF(sfx->getCont(), compoundend, sfx->getContLen())))) {
        continue;
      }

      // check compoundmiddle flag in suffix and prefix
      if ((rv) && !checked_prefix && (wordnum == 0) && compoundmiddle &&
          !hu_mov_rule &&
          ((pfx && pfx->getCont() &&
            TESTAFF(pfx->getCont(), compoundmiddle, pfx->getContLen())) ||
           (sfx && sfx->getCont() &&
            TESTAFF(sfx->getCont(), compoundmiddle, sfx->getContLen())))) {
        rv = NULL;
      }

      // check forbiddenwords
      if ((rv) && (rv->astr) && (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
                                 TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen)))
        continue;

      // increment word number, if the second root has a compoundroot flag
      if ((rv) && (compoundroot) &&
          (TESTAFF(rv->astr, compoundroot, rv->alen))) {
        wordnum++;
      }

      // first word is acceptable in compound words?
      if (((rv) &&
           (checked_prefix || (words && words[wnum]) ||
            (compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
            ((oldwordnum == 0) && compoundbegin &&
             TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
            ((oldwordnum > 0) && compoundmiddle &&
             TESTAFF(rv->astr, compoundmiddle, rv->alen))
            // LANG_hu section: spec. Hungarian rule
            || ((langnum == LANG_hu) &&  // hu_mov_rule
                hu_mov_rule && (TESTAFF(rv->astr, 'F', rv->alen) ||
                                TESTAFF(rv->astr, 'G', rv->alen) ||
                                TESTAFF(rv->astr, 'H', rv->alen)))
            // END of LANG_hu section
            ) &&
           !((checkcompoundtriple && !words &&  // test triple letters
              (word[i - 1] == word[i]) &&
              (((i > 1) && (word[i - 1] == word[i - 2])) ||
               ((word[i - 1] == word[i + 1]))  // may be word[i+1] == '\0'
               )) ||
             (
                 // test CHECKCOMPOUNDPATTERN
                 !checkcpdtable.empty() && !words &&
                 cpdpat_check(word, i, rv, NULL, affixed)) ||
             (checkcompoundcase && !words && cpdcase_check(word, i))))
          // LANG_hu section: spec. Hungarian rule
          ||
          ((!rv) && (langnum == LANG_hu) && hu_mov_rule &&
           (rv = affix_check(st.c_str(), i)) &&
           (sfx && sfx->getCont() &&
            (TESTAFF(sfx->getCont(), (unsigned short)'x', sfx->getContLen()) ||
             TESTAFF(sfx->getCont(), (unsigned short)'%', sfx->getContLen()))))
          // END of LANG_hu section
          ) {
        // LANG_hu section: spec. Hungarian rule
        if (langnum == LANG_hu) {
          // calculate syllable number of the word
          numsyllable += get_syllable(st.substr(0, i));

          // + 1 word, if syllable number of the prefix > 1 (hungarian
          // convention)
          if (pfx && (get_syllable(pfx->getKey()) > 1))
            wordnum++;
        }
        // END of LANG_hu section

        // NEXT WORD(S)
        rv_first = rv;
        rv = lookup((word + i));  // perhaps without prefix

        // search homonym with compound flag
        while ((rv) && ((needaffix && TESTAFF(rv->astr, needaffix, rv->alen)) ||
                        !((compoundflag && !words &&
                           TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                          (compoundend && !words &&
                           TESTAFF(rv->astr, compoundend, rv->alen)) ||
                          (!defcpdtable.empty() && words &&
                           defcpd_check(&words, wnum + 1, rv, NULL, 1))))) {
          rv = rv->next_homonym;
        }

        if (rv && words && words[wnum + 1]) {
          result.append(presult);
          result.push_back(MSEP_FLD);
          result.append(MORPH_PART);
          result.append(word + i);
          if (complexprefixes && HENTRY_DATA(rv))
            result.append(HENTRY_DATA2(rv));
          if (!HENTRY_FIND(rv, MORPH_STEM)) {
            result.push_back(MSEP_FLD);
            result.append(MORPH_STEM);
            result.append(HENTRY_WORD(rv));
          }
          // store the pointer of the hash entry
          if (!complexprefixes && HENTRY_DATA(rv)) {
            result.push_back(MSEP_FLD);
            result.append(HENTRY_DATA2(rv));
          }
          result.push_back(MSEP_REC);
          return 0;
        }

        oldnumsyllable2 = numsyllable;
        oldwordnum2 = wordnum;

        // LANG_hu section: spec. Hungarian rule
        if ((rv) && (langnum == LANG_hu) &&
            (TESTAFF(rv->astr, 'I', rv->alen)) &&
            !(TESTAFF(rv->astr, 'J', rv->alen))) {
          numsyllable--;
        }
        // END of LANG_hu section
        // increment word number, if the second root has a compoundroot flag
        if ((rv) && (compoundroot) &&
            (TESTAFF(rv->astr, compoundroot, rv->alen))) {
          wordnum++;
        }

        // check forbiddenwords
        if ((rv) && (rv->astr) &&
            (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
             TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen))) {
          st[i] = ch;
          continue;
        }

        // second word is acceptable, as a root?
        // hungarian conventions: compounding is acceptable,
        // when compound forms consist of 2 words, or if more,
        // then the syllable number of root words must be 6, or lesser.
        if ((rv) &&
            ((compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
             (compoundend && TESTAFF(rv->astr, compoundend, rv->alen))) &&
            (((cpdwordmax == -1) || (wordnum + 1 < cpdwordmax)) ||
             ((cpdmaxsyllable != 0) &&
              (numsyllable + get_syllable(std::string(HENTRY_WORD(rv), rv->blen)) <=
               cpdmaxsyllable))) &&
            ((!checkcompounddup || (rv != rv_first)))) {
          // bad compound word
          result.append(presult);
          result.push_back(MSEP_FLD);
          result.append(MORPH_PART);
          result.append(word + i);

          if (HENTRY_DATA(rv)) {
            if (complexprefixes)
              result.append(HENTRY_DATA2(rv));
            if (!HENTRY_FIND(rv, MORPH_STEM)) {
              result.push_back(MSEP_FLD);
              result.append(MORPH_STEM);
              result.append(HENTRY_WORD(rv));
            }
            // store the pointer of the hash entry
            if (!complexprefixes) {
              result.push_back(MSEP_FLD);
              result.append(HENTRY_DATA2(rv));
            }
          }
          result.push_back(MSEP_REC);
          ok = 1;
        }

        numsyllable = oldnumsyllable2;
        wordnum = oldwordnum2;

        // perhaps second word has prefix or/and suffix
        sfx = NULL;
        sfxflag = FLAG_NULL;

        if (compoundflag && !onlycpdrule)
          rv = affix_check((word + i), strlen(word + i), compoundflag);
        else
          rv = NULL;

        if (!rv && compoundend && !onlycpdrule) {
          sfx = NULL;
          pfx = NULL;
          rv = affix_check((word + i), strlen(word + i), compoundend);
        }

        if (!rv && !defcpdtable.empty() && words) {
          rv = affix_check((word + i), strlen(word + i), 0, IN_CPD_END);
          if (rv && words && defcpd_check(&words, wnum + 1, rv, NULL, 1)) {
            std::string m;
            if (compoundflag)
              m = affix_check_morph((word + i), strlen(word + i), compoundflag);
            if (m.empty() && compoundend) {
              m = affix_check_morph((word + i), strlen(word + i), compoundend);
            }
            result.append(presult);
            if (!m.empty()) {
              result.push_back(MSEP_FLD);
              result.append(MORPH_PART);
              result.append(word + i);
              line_uniq_app(m, MSEP_REC);
              result.append(m);
            }
            result.push_back(MSEP_REC);
            ok = 1;
          }
        }

        // check non_compound flag in suffix and prefix
        if ((rv) &&
            ((pfx && pfx->getCont() &&
              TESTAFF(pfx->getCont(), compoundforbidflag, pfx->getContLen())) ||
             (sfx && sfx->getCont() &&
              TESTAFF(sfx->getCont(), compoundforbidflag,
                      sfx->getContLen())))) {
          rv = NULL;
        }

        // check forbiddenwords
        if ((rv) && (rv->astr) &&
            (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
             TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen)) &&
            (!TESTAFF(rv->astr, needaffix, rv->alen))) {
          st[i] = ch;
          continue;
        }

        if (langnum == LANG_hu) {
          // calculate syllable number of the word
          numsyllable += get_syllable(word + i);

          // - affix syllable num.
          // XXX only second suffix (inflections, not derivations)
          if (sfxappnd) {
            std::string tmp(sfxappnd);
            reverseword(tmp);
            numsyllable -= short(get_syllable(tmp) + sfxextra);
          } else {
            numsyllable -= short(sfxextra);
          }

          // + 1 word, if syllable number of the prefix > 1 (hungarian
          // convention)
          if (pfx && (get_syllable(pfx->getKey()) > 1))
            wordnum++;

          // increment syllable num, if last word has a SYLLABLENUM flag
          // and the suffix is beginning `s'

          if (!cpdsyllablenum.empty()) {
            switch (sfxflag) {
              case 'c': {
                numsyllable += 2;
                break;
              }
              case 'J': {
                numsyllable += 1;
                break;
              }
              case 'I': {
                if (rv && TESTAFF(rv->astr, 'J', rv->alen))
                  numsyllable += 1;
                break;
              }
            }
          }
        }

        // increment word number, if the second word has a compoundroot flag
        if ((rv) && (compoundroot) &&
            (TESTAFF(rv->astr, compoundroot, rv->alen))) {
          wordnum++;
        }
        // second word is acceptable, as a word with prefix or/and suffix?
        // hungarian conventions: compounding is acceptable,
        // when compound forms consist 2 word, otherwise
        // the syllable number of root words is 6, or lesser.
        if ((rv) &&
            (((cpdwordmax == -1) || (wordnum + 1 < cpdwordmax)) ||
             ((cpdmaxsyllable != 0) && (numsyllable <= cpdmaxsyllable))) &&
            ((!checkcompounddup || (rv != rv_first)))) {
          std::string m;
          if (compoundflag)
            m = affix_check_morph((word + i), strlen(word + i), compoundflag);
          if (m.empty() && compoundend) {
            m = affix_check_morph((word + i), strlen(word + i), compoundend);
          }
          result.append(presult);
          if (!m.empty()) {
            result.push_back(MSEP_FLD);
            result.append(MORPH_PART);
            result.append(word + i);
            line_uniq_app(m, MSEP_REC);
            result.push_back(MSEP_FLD);
            result.append(m);
          }
          result.push_back(MSEP_REC);
          ok = 1;
        }

        numsyllable = oldnumsyllable2;
        wordnum = oldwordnum2;

        // perhaps second word is a compound word (recursive call)
        if ((wordnum + 2 < maxwordnum) && (ok == 0)) {
          compound_check_morph((word + i), strlen(word + i), wordnum + 1,
                               numsyllable, maxwordnum, wnum + 1, words, rwords, 0,
                               result, &presult);
        } else {
          rv = NULL;
        }
      }
      st[i] = ch;
      wordnum = oldwordnum;
      numsyllable = oldnumsyllable;

    } while (!defcpdtable.empty() && oldwordnum == 0 &&
             onlycpdrule++ < 1);  // end of onlycpd loop
  }
  return 0;
}


inline int AffixMgr::isRevSubset(const char* s1,
                                 const char* end_of_s2,
                                 int len) {
  while ((len > 0) && (*s1 != '\0') && ((*s1 == *end_of_s2) || (*s1 == '.'))) {
    s1++;
    end_of_s2--;
    len--;
  }
  return (*s1 == '\0');
}

// check word for suffixes
struct hentry* AffixMgr::suffix_check(const char* word,
                                      int len,
                                      int sfxopts,
                                      PfxEntry* ppfx,
                                      const FLAG cclass,
                                      const FLAG needflag,
                                      char in_compound) {
  struct hentry* rv = NULL;
  PfxEntry* ep = ppfx;

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];

  while (se) {
    if (!cclass || se->getCont()) {
      // suffixes are not allowed in beginning of compounds
      if ((((in_compound != IN_CPD_BEGIN)) ||  // && !cclass
           // except when signed with compoundpermitflag flag
           (se->getCont() && compoundpermitflag &&
            TESTAFF(se->getCont(), compoundpermitflag, se->getContLen()))) &&
          (!circumfix ||
           // no circumfix flag in prefix and suffix
           ((!ppfx || !(ep->getCont()) ||
             !TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
            (!se->getCont() ||
             !(TESTAFF(se->getCont(), circumfix, se->getContLen())))) ||
           // circumfix flag in prefix AND suffix
           ((ppfx && (ep->getCont()) &&
             TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
            (se->getCont() &&
             (TESTAFF(se->getCont(), circumfix, se->getContLen()))))) &&
          // fogemorpheme
          (in_compound ||
           !(se->getCont() &&
             (TESTAFF(se->getCont(), onlyincompound, se->getContLen())))) &&
          // needaffix on prefix or first suffix
          (cclass ||
           !(se->getCont() &&
             TESTAFF(se->getCont(), needaffix, se->getContLen())) ||
           (ppfx &&
            !((ep->getCont()) &&
              TESTAFF(ep->getCont(), needaffix, ep->getContLen()))))) {
        rv = se->checkword(word, len, sfxopts, ppfx,
                           (FLAG)cclass, needflag,
                           (in_compound ? 0 : onlyincompound));
        if (rv) {
          sfx = se;  // BUG: sfx not stateless
          return rv;
        }
      }
    }
    se = se->getNext();
  }

  // now handle the general case
  if (len == 0)
    return NULL;  // FULLSTRIP
  unsigned char sp = *((const unsigned char*)(word + len - 1));
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word + len - 1, len)) {
      // suffixes are not allowed in beginning of compounds
      if ((((in_compound != IN_CPD_BEGIN)) ||  // && !cclass
           // except when signed with compoundpermitflag flag
           (sptr->getCont() && compoundpermitflag &&
            TESTAFF(sptr->getCont(), compoundpermitflag,
                    sptr->getContLen()))) &&
          (!circumfix ||
           // no circumfix flag in prefix and suffix
           ((!ppfx || !(ep->getCont()) ||
             !TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
            (!sptr->getCont() ||
             !(TESTAFF(sptr->getCont(), circumfix, sptr->getContLen())))) ||
           // circumfix flag in prefix AND suffix
           ((ppfx && (ep->getCont()) &&
             TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
            (sptr->getCont() &&
             (TESTAFF(sptr->getCont(), circumfix, sptr->getContLen()))))) &&
          // fogemorpheme
          (in_compound ||
           !((sptr->getCont() && (TESTAFF(sptr->getCont(), onlyincompound,
                                          sptr->getContLen()))))) &&
          // needaffix on prefix or first suffix
          (cclass ||
           !(sptr->getCont() &&
             TESTAFF(sptr->getCont(), needaffix, sptr->getContLen())) ||
           (ppfx &&
            !((ep->getCont()) &&
              TESTAFF(ep->getCont(), needaffix, ep->getContLen())))))
        if (in_compound != IN_CPD_END || ppfx ||
            !(sptr->getCont() &&
              TESTAFF(sptr->getCont(), onlyincompound, sptr->getContLen()))) {
          rv = sptr->checkword(word, len, sfxopts, ppfx,
                               cclass, needflag,
                               (in_compound ? 0 : onlyincompound));
          if (rv) {
            sfx = sptr;                 // BUG: sfx not stateless
            sfxflag = sptr->getFlag();  // BUG: sfxflag not stateless
            if (!sptr->getCont())
              sfxappnd = sptr->getKey();  // BUG: sfxappnd not stateless
            // LANG_hu section: spec. Hungarian rule
            else if (langnum == LANG_hu && sptr->getKeyLen() &&
                     sptr->getKey()[0] == 'i' && sptr->getKey()[1] != 'y' &&
                     sptr->getKey()[1] != 't') {
              sfxextra = 1;
            }
            // END of LANG_hu section
            return rv;
          }
        }
      sptr = sptr->getNextEQ();
    } else {
      sptr = sptr->getNextNE();
    }
  }

  return NULL;
}

// check word for two-level suffixes
struct hentry* AffixMgr::suffix_check_twosfx(const char* word,
                                             int len,
                                             int sfxopts,
                                             PfxEntry* ppfx,
                                             const FLAG needflag) {
  struct hentry* rv = NULL;

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];
  while (se) {
    if (contclasses[se->getFlag()]) {
      rv = se->check_twosfx(word, len, sfxopts, ppfx, needflag);
      if (rv)
        return rv;
    }
    se = se->getNext();
  }

  // now handle the general case
  if (len == 0)
    return NULL;  // FULLSTRIP
  unsigned char sp = *((const unsigned char*)(word + len - 1));
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word + len - 1, len)) {
      if (contclasses[sptr->getFlag()]) {
        rv = sptr->check_twosfx(word, len, sfxopts, ppfx, needflag);
        if (rv) {
          sfxflag = sptr->getFlag();  // BUG: sfxflag not stateless
          if (!sptr->getCont())
            sfxappnd = sptr->getKey();  // BUG: sfxappnd not stateless
          return rv;
        }
      }
      sptr = sptr->getNextEQ();
    } else {
      sptr = sptr->getNextNE();
    }
  }

  return NULL;
}

// check word for two-level suffixes and morph
std::string AffixMgr::suffix_check_twosfx_morph(const char* word,
                                                int len,
                                                int sfxopts,
                                                PfxEntry* ppfx,
                                                const FLAG needflag) {
  std::string result;
  std::string result2;
  std::string result3;

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];
  while (se) {
    if (contclasses[se->getFlag()]) {
      std::string st = se->check_twosfx_morph(word, len, sfxopts, ppfx, needflag);
      if (!st.empty()) {
        if (ppfx) {
          if (ppfx->getMorph()) {
            result.append(ppfx->getMorph());
            result.push_back(MSEP_FLD);
          } else
            debugflag(result, ppfx->getFlag());
        }
        result.append(st);
        if (se->getMorph()) {
          result.push_back(MSEP_FLD);
          result.append(se->getMorph());
        } else
          debugflag(result, se->getFlag());
        result.push_back(MSEP_REC);
      }
    }
    se = se->getNext();
  }

  // now handle the general case
  if (len == 0)
    return std::string();  // FULLSTRIP
  unsigned char sp = *((const unsigned char*)(word + len - 1));
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word + len - 1, len)) {
      if (contclasses[sptr->getFlag()]) {
        std::string st = sptr->check_twosfx_morph(word, len, sfxopts, ppfx, needflag);
        if (!st.empty()) {
          sfxflag = sptr->getFlag();  // BUG: sfxflag not stateless
          if (!sptr->getCont())
            sfxappnd = sptr->getKey();  // BUG: sfxappnd not stateless
          result2.assign(st);

          result3.clear();

          if (sptr->getMorph()) {
            result3.push_back(MSEP_FLD);
            result3.append(sptr->getMorph());
          } else
            debugflag(result3, sptr->getFlag());
          strlinecat(result2, result3);
          result2.push_back(MSEP_REC);
          result.append(result2);
        }
      }
      sptr = sptr->getNextEQ();
    } else {
      sptr = sptr->getNextNE();
    }
  }

  return result;
}

std::string AffixMgr::suffix_check_morph(const char* word,
                                         int len,
                                         int sfxopts,
                                         PfxEntry* ppfx,
                                         const FLAG cclass,
                                         const FLAG needflag,
                                         char in_compound) {
  std::string result;

  struct hentry* rv = NULL;

  PfxEntry* ep = ppfx;

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];
  while (se) {
    if (!cclass || se->getCont()) {
      // suffixes are not allowed in beginning of compounds
      if (((((in_compound != IN_CPD_BEGIN)) ||  // && !cclass
            // except when signed with compoundpermitflag flag
            (se->getCont() && compoundpermitflag &&
             TESTAFF(se->getCont(), compoundpermitflag, se->getContLen()))) &&
           (!circumfix ||
            // no circumfix flag in prefix and suffix
            ((!ppfx || !(ep->getCont()) ||
              !TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
             (!se->getCont() ||
              !(TESTAFF(se->getCont(), circumfix, se->getContLen())))) ||
            // circumfix flag in prefix AND suffix
            ((ppfx && (ep->getCont()) &&
              TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
             (se->getCont() &&
              (TESTAFF(se->getCont(), circumfix, se->getContLen()))))) &&
           // fogemorpheme
           (in_compound ||
            !((se->getCont() &&
               (TESTAFF(se->getCont(), onlyincompound, se->getContLen()))))) &&
           // needaffix on prefix or first suffix
           (cclass ||
            !(se->getCont() &&
              TESTAFF(se->getCont(), needaffix, se->getContLen())) ||
            (ppfx &&
             !((ep->getCont()) &&
               TESTAFF(ep->getCont(), needaffix, ep->getContLen()))))))
        rv = se->checkword(word, len, sfxopts, ppfx, cclass,
                           needflag, FLAG_NULL);
      while (rv) {
        if (ppfx) {
          if (ppfx->getMorph()) {
            result.append(ppfx->getMorph());
            result.push_back(MSEP_FLD);
          } else
            debugflag(result, ppfx->getFlag());
        }
        if (complexprefixes && HENTRY_DATA(rv))
          result.append(HENTRY_DATA2(rv));
        if (!HENTRY_FIND(rv, MORPH_STEM)) {
          result.push_back(MSEP_FLD);
          result.append(MORPH_STEM);
          result.append(HENTRY_WORD(rv));
        }

        if (!complexprefixes && HENTRY_DATA(rv)) {
          result.push_back(MSEP_FLD);
          result.append(HENTRY_DATA2(rv));
        }
        if (se->getMorph()) {
          result.push_back(MSEP_FLD);
          result.append(se->getMorph());
        } else
          debugflag(result, se->getFlag());
        result.push_back(MSEP_REC);
        rv = se->get_next_homonym(rv, sfxopts, ppfx, cclass, needflag);
      }
    }
    se = se->getNext();
  }

  // now handle the general case
  if (len == 0)
    return std::string();  // FULLSTRIP
  unsigned char sp = *((const unsigned char*)(word + len - 1));
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word + len - 1, len)) {
      // suffixes are not allowed in beginning of compounds
      if (((((in_compound != IN_CPD_BEGIN)) ||  // && !cclass
            // except when signed with compoundpermitflag flag
            (sptr->getCont() && compoundpermitflag &&
             TESTAFF(sptr->getCont(), compoundpermitflag,
                     sptr->getContLen()))) &&
           (!circumfix ||
            // no circumfix flag in prefix and suffix
            ((!ppfx || !(ep->getCont()) ||
              !TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
             (!sptr->getCont() ||
              !(TESTAFF(sptr->getCont(), circumfix, sptr->getContLen())))) ||
            // circumfix flag in prefix AND suffix
            ((ppfx && (ep->getCont()) &&
              TESTAFF(ep->getCont(), circumfix, ep->getContLen())) &&
             (sptr->getCont() &&
              (TESTAFF(sptr->getCont(), circumfix, sptr->getContLen()))))) &&
           // fogemorpheme
           (in_compound ||
            !((sptr->getCont() && (TESTAFF(sptr->getCont(), onlyincompound,
                                           sptr->getContLen()))))) &&
           // needaffix on first suffix
           (cclass ||
            !(sptr->getCont() &&
              TESTAFF(sptr->getCont(), needaffix, sptr->getContLen())))))
        rv = sptr->checkword(word, len, sfxopts, ppfx, cclass,
                             needflag, FLAG_NULL);
      while (rv) {
        if (ppfx) {
          if (ppfx->getMorph()) {
            result.append(ppfx->getMorph());
            result.push_back(MSEP_FLD);
          } else
            debugflag(result, ppfx->getFlag());
        }
        if (complexprefixes && HENTRY_DATA(rv))
          result.append(HENTRY_DATA2(rv));
        if (!HENTRY_FIND(rv, MORPH_STEM)) {
          result.push_back(MSEP_FLD);
          result.append(MORPH_STEM);
          result.append(HENTRY_WORD(rv));
        }

        if (!complexprefixes && HENTRY_DATA(rv)) {
          result.push_back(MSEP_FLD);
          result.append(HENTRY_DATA2(rv));
        }

        if (sptr->getMorph()) {
          result.push_back(MSEP_FLD);
          result.append(sptr->getMorph());
        } else
          debugflag(result, sptr->getFlag());
        result.push_back(MSEP_REC);
        rv = sptr->get_next_homonym(rv, sfxopts, ppfx, cclass, needflag);
      }
      sptr = sptr->getNextEQ();
    } else {
      sptr = sptr->getNextNE();
    }
  }

  return result;
}

// check if word with affixes is correctly spelled
struct hentry* AffixMgr::affix_check(const char* word,
                                     int len,
                                     const FLAG needflag,
                                     char in_compound) {

  // check all prefixes (also crossed with suffixes if allowed)
  struct hentry* rv = prefix_check(word, len, in_compound, needflag);
  if (rv)
    return rv;

  // if still not found check all suffixes
  rv = suffix_check(word, len, 0, NULL, FLAG_NULL, needflag, in_compound);

  if (havecontclass) {
    sfx = NULL;
    pfx = NULL;

    if (rv)
      return rv;
    // if still not found check all two-level suffixes
    rv = suffix_check_twosfx(word, len, 0, NULL, needflag);

    if (rv)
      return rv;
    // if still not found check all two-level suffixes
    rv = prefix_check_twosfx(word, len, IN_CPD_NOT, needflag);
  }

  return rv;
}

// check if word with affixes is correctly spelled
std::string AffixMgr::affix_check_morph(const char* word,
                                  int len,
                                  const FLAG needflag,
                                  char in_compound) {
  std::string result;

  // check all prefixes (also crossed with suffixes if allowed)
  std::string st = prefix_check_morph(word, len, in_compound);
  if (!st.empty()) {
    result.append(st);
  }

  // if still not found check all suffixes
  st = suffix_check_morph(word, len, 0, NULL, '\0', needflag, in_compound);
  if (!st.empty()) {
    result.append(st);
  }

  if (havecontclass) {
    sfx = NULL;
    pfx = NULL;
    // if still not found check all two-level suffixes
    st = suffix_check_twosfx_morph(word, len, 0, NULL, needflag);
    if (!st.empty()) {
      result.append(st);
    }

    // if still not found check all two-level suffixes
    st = prefix_check_twosfx_morph(word, len, IN_CPD_NOT, needflag);
    if (!st.empty()) {
      result.append(st);
    }
  }

  return result;
}

// morphcmp(): compare MORPH_DERI_SFX, MORPH_INFL_SFX and MORPH_TERM_SFX fields
// in the first line of the inputs
// return 0, if inputs equal
// return 1, if inputs may equal with a secondary suffix
// otherwise return -1
static int morphcmp(const char* s, const char* t) {
  int se = 0;
  int te = 0;
  const char* sl;
  const char* tl;
  const char* olds;
  const char* oldt;
  if (!s || !t)
    return 1;
  olds = s;
  sl = strchr(s, '\n');
  s = strstr(s, MORPH_DERI_SFX);
  if (!s || (sl && sl < s))
    s = strstr(olds, MORPH_INFL_SFX);
  if (!s || (sl && sl < s)) {
    s = strstr(olds, MORPH_TERM_SFX);
    olds = NULL;
  }
  oldt = t;
  tl = strchr(t, '\n');
  t = strstr(t, MORPH_DERI_SFX);
  if (!t || (tl && tl < t))
    t = strstr(oldt, MORPH_INFL_SFX);
  if (!t || (tl && tl < t)) {
    t = strstr(oldt, MORPH_TERM_SFX);
    oldt = NULL;
  }
  while (s && t && (!sl || sl > s) && (!tl || tl > t)) {
    s += MORPH_TAG_LEN;
    t += MORPH_TAG_LEN;
    se = 0;
    te = 0;
    while ((*s == *t) && !se && !te) {
      s++;
      t++;
      switch (*s) {
        case ' ':
        case '\n':
        case '\t':
        case '\0':
          se = 1;
      }
      switch (*t) {
        case ' ':
        case '\n':
        case '\t':
        case '\0':
          te = 1;
      }
    }
    if (!se || !te) {
      // not terminal suffix difference
      if (olds)
        return -1;
      return 1;
    }
    olds = s;
    s = strstr(s, MORPH_DERI_SFX);
    if (!s || (sl && sl < s))
      s = strstr(olds, MORPH_INFL_SFX);
    if (!s || (sl && sl < s)) {
      s = strstr(olds, MORPH_TERM_SFX);
      olds = NULL;
    }
    oldt = t;
    t = strstr(t, MORPH_DERI_SFX);
    if (!t || (tl && tl < t))
      t = strstr(oldt, MORPH_INFL_SFX);
    if (!t || (tl && tl < t)) {
      t = strstr(oldt, MORPH_TERM_SFX);
      oldt = NULL;
    }
  }
  if (!s && !t && se && te)
    return 0;
  return 1;
}

std::string AffixMgr::morphgen(const char* ts,
                               int wl,
                               const unsigned short* ap,
                               unsigned short al,
                               const char* morph,
                               const char* targetmorph,
                         int level) {
  // handle suffixes
  if (!morph)
    return std::string();

  // check substandard flag
  if (TESTAFF(ap, substandard, al))
    return std::string();

  if (morphcmp(morph, targetmorph) == 0)
    return ts;

  size_t stemmorphcatpos;
  std::string mymorph;

  // use input suffix fields, if exist
  if (strstr(morph, MORPH_INFL_SFX) || strstr(morph, MORPH_DERI_SFX)) {
    mymorph.assign(morph);
    mymorph.push_back(MSEP_FLD);
    stemmorphcatpos = mymorph.size();
  } else {
    stemmorphcatpos = std::string::npos;
  }

  for (int i = 0; i < al; i++) {
    const unsigned char c = (unsigned char)(ap[i] & 0x00FF);
    SfxEntry* sptr = sFlag[c];
    while (sptr) {
      if (sptr->getFlag() == ap[i] && sptr->getMorph() &&
          ((sptr->getContLen() == 0) ||
           // don't generate forms with substandard affixes
           !TESTAFF(sptr->getCont(), substandard, sptr->getContLen()))) {
        const char* stemmorph;
        if (stemmorphcatpos != std::string::npos) {
          mymorph.replace(stemmorphcatpos, std::string::npos, sptr->getMorph());
          stemmorph = mymorph.c_str();
        } else {
          stemmorph = sptr->getMorph();
        }

        int cmp = morphcmp(stemmorph, targetmorph);

        if (cmp == 0) {
          std::string newword = sptr->add(ts, wl);
          if (!newword.empty()) {
            hentry* check = pHMgr->lookup(newword.c_str());  // XXX extra dic
            if (!check || !check->astr ||
                !(TESTAFF(check->astr, forbiddenword, check->alen) ||
                  TESTAFF(check->astr, ONLYUPCASEFLAG, check->alen))) {
              return newword;
            }
          }
        }

        // recursive call for secondary suffixes
        if ((level == 0) && (cmp == 1) && (sptr->getContLen() > 0) &&
            !TESTAFF(sptr->getCont(), substandard, sptr->getContLen())) {
          std::string newword = sptr->add(ts, wl);
          if (!newword.empty()) {
            std::string newword2 =
                morphgen(newword.c_str(), newword.size(), sptr->getCont(),
                         sptr->getContLen(), stemmorph, targetmorph, 1);

            if (!newword2.empty()) {
              return newword2;
            }
          }
        }
      }
      sptr = sptr->getFlgNxt();
    }
  }
  return std::string();
}

int AffixMgr::expand_rootword(struct guessword* wlst,
                              int maxn,
                              const char* ts,
                              int wl,
                              const unsigned short* ap,
                              unsigned short al,
                              const char* bad,
                              int badl,
                              const char* phon) {
  int nh = 0;
  // first add root word to list
  if ((nh < maxn) &&
      !(al && ((needaffix && TESTAFF(ap, needaffix, al)) ||
               (onlyincompound && TESTAFF(ap, onlyincompound, al))))) {
    wlst[nh].word = mystrdup(ts);
    if (!wlst[nh].word)
      return 0;
    wlst[nh].allow = false;
    wlst[nh].orig = NULL;
    nh++;
    // add special phonetic version
    if (phon && (nh < maxn)) {
      wlst[nh].word = mystrdup(phon);
      if (!wlst[nh].word)
        return nh - 1;
      wlst[nh].allow = false;
      wlst[nh].orig = mystrdup(ts);
      if (!wlst[nh].orig)
        return nh - 1;
      nh++;
    }
  }

  // handle suffixes
  for (int i = 0; i < al; i++) {
    const unsigned char c = (unsigned char)(ap[i] & 0x00FF);
    SfxEntry* sptr = sFlag[c];
    while (sptr) {
      if ((sptr->getFlag() == ap[i]) &&
          (!sptr->getKeyLen() ||
           ((badl > sptr->getKeyLen()) &&
            (strcmp(sptr->getAffix(), bad + badl - sptr->getKeyLen()) == 0))) &&
          // check needaffix flag
          !(sptr->getCont() &&
            ((needaffix &&
              TESTAFF(sptr->getCont(), needaffix, sptr->getContLen())) ||
             (circumfix &&
              TESTAFF(sptr->getCont(), circumfix, sptr->getContLen())) ||
             (onlyincompound &&
              TESTAFF(sptr->getCont(), onlyincompound, sptr->getContLen()))))) {
        std::string newword = sptr->add(ts, wl);
        if (!newword.empty()) {
          if (nh < maxn) {
            wlst[nh].word = mystrdup(newword.c_str());
            wlst[nh].allow = sptr->allowCross();
            wlst[nh].orig = NULL;
            nh++;
            // add special phonetic version
            if (phon && (nh < maxn)) {
              std::string prefix(phon);
              std::string key(sptr->getKey());
              reverseword(key);
              prefix.append(key);
              wlst[nh].word = mystrdup(prefix.c_str());
              if (!wlst[nh].word)
                return nh - 1;
              wlst[nh].allow = false;
              wlst[nh].orig = mystrdup(newword.c_str());
              if (!wlst[nh].orig)
                return nh - 1;
              nh++;
            }
          }
        }
      }
      sptr = sptr->getFlgNxt();
    }
  }

  int n = nh;

  // handle cross products of prefixes and suffixes
  for (int j = 1; j < n; j++)
    if (wlst[j].allow) {
      for (int k = 0; k < al; k++) {
        const unsigned char c = (unsigned char)(ap[k] & 0x00FF);
        PfxEntry* cptr = pFlag[c];
        while (cptr) {
          if ((cptr->getFlag() == ap[k]) && cptr->allowCross() &&
              (!cptr->getKeyLen() ||
               ((badl > cptr->getKeyLen()) &&
                (strncmp(cptr->getKey(), bad, cptr->getKeyLen()) == 0)))) {
            int l1 = strlen(wlst[j].word);
            std::string newword = cptr->add(wlst[j].word, l1);
            if (!newword.empty()) {
              if (nh < maxn) {
                wlst[nh].word = mystrdup(newword.c_str());
                wlst[nh].allow = cptr->allowCross();
                wlst[nh].orig = NULL;
                nh++;
              }
            }
          }
          cptr = cptr->getFlgNxt();
        }
      }
    }

  // now handle pure prefixes
  for (int m = 0; m < al; m++) {
    const unsigned char c = (unsigned char)(ap[m] & 0x00FF);
    PfxEntry* ptr = pFlag[c];
    while (ptr) {
      if ((ptr->getFlag() == ap[m]) &&
          (!ptr->getKeyLen() ||
           ((badl > ptr->getKeyLen()) &&
            (strncmp(ptr->getKey(), bad, ptr->getKeyLen()) == 0))) &&
          // check needaffix flag
          !(ptr->getCont() &&
            ((needaffix &&
              TESTAFF(ptr->getCont(), needaffix, ptr->getContLen())) ||
             (circumfix &&
              TESTAFF(ptr->getCont(), circumfix, ptr->getContLen())) ||
             (onlyincompound &&
              TESTAFF(ptr->getCont(), onlyincompound, ptr->getContLen()))))) {
        std::string newword = ptr->add(ts, wl);
        if (!newword.empty()) {
          if (nh < maxn) {
            wlst[nh].word = mystrdup(newword.c_str());
            wlst[nh].allow = ptr->allowCross();
            wlst[nh].orig = NULL;
            nh++;
          }
        }
      }
      ptr = ptr->getFlgNxt();
    }
  }

  return nh;
}

// return replacing table
const std::vector<replentry>& AffixMgr::get_reptable() const {
  return pHMgr->get_reptable();
}

// return iconv table
RepList* AffixMgr::get_iconvtable() const {
  if (!iconvtable)
    return NULL;
  return iconvtable;
}

// return oconv table
RepList* AffixMgr::get_oconvtable() const {
  if (!oconvtable)
    return NULL;
  return oconvtable;
}

// return replacing table
struct phonetable* AffixMgr::get_phonetable() const {
  if (!phone)
    return NULL;
  return phone;
}

// return character map table
const std::vector<mapentry>& AffixMgr::get_maptable() const {
  return maptable;
}

// return character map table
const std::vector<std::string>& AffixMgr::get_breaktable() const {
  return breaktable;
}

// return text encoding of dictionary
const std::string& AffixMgr::get_encoding() {
  if (encoding.empty())
    encoding = SPELL_ENCODING;
  return encoding;
}

// return text encoding of dictionary
int AffixMgr::get_langnum() const {
  return langnum;
}

// return double prefix option
int AffixMgr::get_complexprefixes() const {
  return complexprefixes;
}

// return FULLSTRIP option
int AffixMgr::get_fullstrip() const {
  return fullstrip;
}

FLAG AffixMgr::get_keepcase() const {
  return keepcase;
}

FLAG AffixMgr::get_forceucase() const {
  return forceucase;
}

FLAG AffixMgr::get_warn() const {
  return warn;
}

int AffixMgr::get_forbidwarn() const {
  return forbidwarn;
}

int AffixMgr::get_checksharps() const {
  return checksharps;
}

char* AffixMgr::encode_flag(unsigned short aflag) const {
  return pHMgr->encode_flag(aflag);
}

// return the preferred ignore string for suggestions
const char* AffixMgr::get_ignore() const {
  if (ignorechars.empty())
    return NULL;
  return ignorechars.c_str();
}

// return the preferred ignore string for suggestions
const std::vector<w_char>& AffixMgr::get_ignore_utf16() const {
  return ignorechars_utf16;
}

// return the keyboard string for suggestions
char* AffixMgr::get_key_string() {
  if (keystring.empty())
    keystring = SPELL_KEYSTRING;
  return mystrdup(keystring.c_str());
}

// return the preferred try string for suggestions
char* AffixMgr::get_try_string() const {
  if (trystring.empty())
    return NULL;
  return mystrdup(trystring.c_str());
}

// return the preferred try string for suggestions
const std::string& AffixMgr::get_wordchars() const {
  return wordchars;
}

const std::vector<w_char>& AffixMgr::get_wordchars_utf16() const {
  return wordchars_utf16;
}

// is there compounding?
int AffixMgr::get_compound() const {
  return compoundflag || compoundbegin || !defcpdtable.empty();
}

// return the compound words control flag
FLAG AffixMgr::get_compoundflag() const {
  return compoundflag;
}

// return the forbidden words control flag
FLAG AffixMgr::get_forbiddenword() const {
  return forbiddenword;
}

// return the forbidden words control flag
FLAG AffixMgr::get_nosuggest() const {
  return nosuggest;
}

// return the forbidden words control flag
FLAG AffixMgr::get_nongramsuggest() const {
  return nongramsuggest;
}

// return the substandard root/affix control flag
FLAG AffixMgr::get_substandard() const {
  return substandard;
}

// return the forbidden words flag modify flag
FLAG AffixMgr::get_needaffix() const {
  return needaffix;
}

// return the onlyincompound flag
FLAG AffixMgr::get_onlyincompound() const {
  return onlyincompound;
}

// return the value of suffix
const std::string& AffixMgr::get_version() const {
  return version;
}

// utility method to look up root words in hash table
struct hentry* AffixMgr::lookup(const char* word) {
  struct hentry* he = NULL;
  for (size_t i = 0; i < alldic.size() && !he; ++i) {
    he = alldic[i]->lookup(word);
  }
  return he;
}

// return the value of suffix
int AffixMgr::have_contclass() const {
  return havecontclass;
}

// return utf8
int AffixMgr::get_utf8() const {
  return utf8;
}

int AffixMgr::get_maxngramsugs(void) const {
  return maxngramsugs;
}

int AffixMgr::get_maxcpdsugs(void) const {
  return maxcpdsugs;
}

int AffixMgr::get_maxdiff(void) const {
  return maxdiff;
}

int AffixMgr::get_onlymaxdiff(void) const {
  return onlymaxdiff;
}

// return nosplitsugs
int AffixMgr::get_nosplitsugs(void) const {
  return nosplitsugs;
}

// return sugswithdots
int AffixMgr::get_sugswithdots(void) const {
  return sugswithdots;
}

/* parse flag */
bool AffixMgr::parse_flag(const std::string& line, unsigned short* out, FileMgr* af) {
  if (*out != FLAG_NULL && !(*out >= DEFAULTFLAGS)) {
    HUNSPELL_WARNING(
        stderr,
        "error: line %d: multiple definitions of an affix file parameter\n",
        af->getlinenum());
    return false;
  }
  std::string s;
  if (!parse_string(line, s, af->getlinenum()))
    return false;
  *out = pHMgr->decode_flag(s.c_str());
  return true;
}

/* parse num */
bool AffixMgr::parse_num(const std::string& line, int* out, FileMgr* af) {
  if (*out != -1) {
    HUNSPELL_WARNING(
        stderr,
        "error: line %d: multiple definitions of an affix file parameter\n",
        af->getlinenum());
    return false;
  }
  std::string s;
  if (!parse_string(line, s, af->getlinenum()))
    return false;
  *out = atoi(s.c_str());
  return true;
}

/* parse in the max syllablecount of compound words and  */
bool AffixMgr::parse_cpdsyllable(const std::string& line, FileMgr* af) {
  int i = 0;
  int np = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      case 0: {
        np++;
        break;
      }
      case 1: {
        cpdmaxsyllable = atoi(std::string(start_piece, iter).c_str());
        np++;
        break;
      }
      case 2: {
        if (!utf8) {
          cpdvowels.assign(start_piece, iter);
          std::sort(cpdvowels.begin(), cpdvowels.end());
        } else {
          std::string piece(start_piece, iter);
          u8_u16(cpdvowels_utf16, piece);
          std::sort(cpdvowels_utf16.begin(), cpdvowels_utf16.end());
        }
        np++;
        break;
      }
      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  if (np < 2) {
    HUNSPELL_WARNING(stderr,
                     "error: line %d: missing compoundsyllable information\n",
                     af->getlinenum());
    return false;
  }
  if (np == 2)
    cpdvowels = "AEIOUaeiou";
  return true;
}

bool AffixMgr::parse_convtable(const std::string& line,
                              FileMgr* af,
                              RepList** rl,
                              const std::string& keyword) {
  if (*rl) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
  int i = 0;
  int np = 0;
  int numrl = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      case 0: {
        np++;
        break;
      }
      case 1: {
        numrl = atoi(std::string(start_piece, iter).c_str());
        if (numrl < 1) {
          HUNSPELL_WARNING(stderr, "error: line %d: incorrect entry number\n",
                           af->getlinenum());
          return false;
        }
        *rl = new RepList(numrl);
        if (!*rl)
          return false;
        np++;
        break;
      }
      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  if (np != 2) {
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the num lines to read in the remainder of the table */
  for (int j = 0; j < numrl; j++) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    i = 0;
    std::string pattern;
    std::string pattern2;
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      {
        switch (i) {
          case 0: {
            if (nl.compare(start_piece - nl.begin(), keyword.size(), keyword, 0, keyword.size()) != 0) {
              HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                               af->getlinenum());
              delete *rl;
              *rl = NULL;
              return false;
            }
            break;
          }
          case 1: {
            pattern.assign(start_piece, iter);
            break;
          }
          case 2: {
            pattern2.assign(start_piece, iter);
            break;
          }
          default:
            break;
        }
        ++i;
      }
      start_piece = mystrsep(nl, iter);
    }
    if (pattern.empty() || pattern2.empty()) {
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      return false;
    }
    (*rl)->add(pattern, pattern2);
  }
  return true;
}

/* parse in the typical fault correcting table */
bool AffixMgr::parse_phonetable(const std::string& line, FileMgr* af) {
  if (phone) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
  int num = -1;
  int i = 0;
  int np = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      case 0: {
        np++;
        break;
      }
      case 1: {
        num = atoi(std::string(start_piece, iter).c_str());
        if (num < 1) {
          HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                           af->getlinenum());
          return false;
        }
        phone = new phonetable;
        phone->utf8 = (char)utf8;
        np++;
        break;
      }
      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  if (np != 2) {
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the phone->num lines to read in the remainder of the table */
  for (int j = 0; j < num; ++j) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    i = 0;
    const size_t old_size = phone->rules.size();
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      {
        switch (i) {
          case 0: {
            if (nl.compare(start_piece - nl.begin(), 5, "PHONE", 5) != 0) {
              HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                               af->getlinenum());
              return false;
            }
            break;
          }
          case 1: {
            phone->rules.push_back(std::string(start_piece, iter));
            break;
          }
          case 2: {
            phone->rules.push_back(std::string(start_piece, iter));
            mystrrep(phone->rules.back(), "_", "");
            break;
          }
          default:
            break;
        }
        ++i;
      }
      start_piece = mystrsep(nl, iter);
    }
    if (phone->rules.size() != old_size + 2) {
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      phone->rules.clear();
      return false;
    }
  }
  phone->rules.push_back("");
  phone->rules.push_back("");
  init_phonet_hash(*phone);
  return true;
}

/* parse in the checkcompoundpattern table */
bool AffixMgr::parse_checkcpdtable(const std::string& line, FileMgr* af) {
  if (parsedcheckcpd) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
  parsedcheckcpd = true;
  int numcheckcpd = -1;
  int i = 0;
  int np = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      case 0: {
        np++;
        break;
      }
      case 1: {
        numcheckcpd = atoi(std::string(start_piece, iter).c_str());
        if (numcheckcpd < 1) {
          HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                           af->getlinenum());
          return false;
        }
        checkcpdtable.reserve(numcheckcpd);
        np++;
        break;
      }
      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  if (np != 2) {
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the numcheckcpd lines to read in the remainder of the table */
  for (int j = 0; j < numcheckcpd; ++j) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    i = 0;
    checkcpdtable.push_back(patentry());
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 20, "CHECKCOMPOUNDPATTERN", 20) != 0) {
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            return false;
          }
          break;
        }
        case 1: {
          checkcpdtable.back().pattern.assign(start_piece, iter);
          size_t slash_pos = checkcpdtable.back().pattern.find('/');
          if (slash_pos != std::string::npos) {
            std::string chunk(checkcpdtable.back().pattern, slash_pos + 1);
            checkcpdtable.back().pattern.resize(slash_pos);
            checkcpdtable.back().cond = pHMgr->decode_flag(chunk.c_str());
          }
          break;
        }
        case 2: {
          checkcpdtable.back().pattern2.assign(start_piece, iter);
          size_t slash_pos = checkcpdtable.back().pattern2.find('/');
          if (slash_pos != std::string::npos) {
            std::string chunk(checkcpdtable.back().pattern2, slash_pos + 1);
            checkcpdtable.back().pattern2.resize(slash_pos);
            checkcpdtable.back().cond2 = pHMgr->decode_flag(chunk.c_str());
          }
          break;
        }
        case 3: {
          checkcpdtable.back().pattern3.assign(start_piece, iter);
          simplifiedcpd = 1;
          break;
        }
        default:
          break;
      }
      i++;
      start_piece = mystrsep(nl, iter);
    }
  }
  return true;
}

/* parse in the compound rule table */
bool AffixMgr::parse_defcpdtable(const std::string& line, FileMgr* af) {
  if (parseddefcpd) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
  parseddefcpd = true;
  int numdefcpd = -1;
  int i = 0;
  int np = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      case 0: {
        np++;
        break;
      }
      case 1: {
        numdefcpd = atoi(std::string(start_piece, iter).c_str());
        if (numdefcpd < 1) {
          HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                           af->getlinenum());
          return false;
        }
        defcpdtable.reserve(numdefcpd);
        np++;
        break;
      }
      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  if (np != 2) {
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the numdefcpd lines to read in the remainder of the table */
  for (int j = 0; j < numdefcpd; ++j) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    i = 0;
    defcpdtable.push_back(flagentry());
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 12, "COMPOUNDRULE", 12) != 0) {
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            numdefcpd = 0;
            return false;
          }
          break;
        }
        case 1: {  // handle parenthesized flags
          if (std::find(start_piece, iter, '(') != iter) {
            for (std::string::const_iterator k = start_piece; k != iter; ++k) {
              std::string::const_iterator chb = k;
              std::string::const_iterator che = k + 1;
              if (*k == '(') {
                std::string::const_iterator parpos = std::find(k, iter, ')');
                if (parpos != iter) {
                  chb = k + 1;
                  che = parpos;
                  k = parpos;
                }
              }

              if (*chb == '*' || *chb == '?') {
                defcpdtable.back().push_back((FLAG)*chb);
              } else {
                pHMgr->decode_flags(defcpdtable.back(), std::string(chb, che), af);
              }
            }
          } else {
            pHMgr->decode_flags(defcpdtable.back(), std::string(start_piece, iter), af);
          }
          break;
        }
        default:
          break;
      }
      ++i;
      start_piece = mystrsep(nl, iter);
    }
    if (defcpdtable.back().empty()) {
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      return false;
    }
  }
  return true;
}

/* parse in the character map table */
bool AffixMgr::parse_maptable(const std::string& line, FileMgr* af) {
  if (parsedmaptable) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
  parsedmaptable = true;
  int nummap = -1;
  int i = 0;
  int np = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      case 0: {
        np++;
        break;
      }
      case 1: {
        nummap = atoi(std::string(start_piece, iter).c_str());
        if (nummap < 1) {
          HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                           af->getlinenum());
          return false;
        }
        maptable.reserve(nummap);
        np++;
        break;
      }
      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  if (np != 2) {
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the nummap lines to read in the remainder of the table */
  for (int j = 0; j < nummap; ++j) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    i = 0;
    maptable.push_back(mapentry());
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 3, "MAP", 3) != 0) {
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            nummap = 0;
            return false;
          }
          break;
        }
        case 1: {
          for (std::string::const_iterator k = start_piece; k != iter; ++k) {
            std::string::const_iterator chb = k;
            std::string::const_iterator che = k + 1;
            if (*k == '(') {
              std::string::const_iterator parpos = std::find(k, iter, ')');
              if (parpos != iter) {
                chb = k + 1;
                che = parpos;
                k = parpos;
              }
            } else {
              if (utf8 && (*k & 0xc0) == 0xc0) {
                ++k;
                while (k != iter && (*k & 0xc0) == 0x80)
                    ++k;
                che = k;
                --k;
              }
            }
            maptable.back().push_back(std::string(chb, che));
          }
          break;
        }
        default:
          break;
      }
      ++i;
      start_piece = mystrsep(nl, iter);
    }
    if (maptable.back().empty()) {
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      return false;
    }
  }
  return true;
}

/* parse in the word breakpoint table */
bool AffixMgr::parse_breaktable(const std::string& line, FileMgr* af) {
  if (parsedbreaktable) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
  parsedbreaktable = true;
  int numbreak = -1;
  int i = 0;
  int np = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      case 0: {
        np++;
        break;
      }
      case 1: {
        numbreak = atoi(std::string(start_piece, iter).c_str());
        if (numbreak < 0) {
          HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                           af->getlinenum());
          return false;
        }
        if (numbreak == 0)
          return true;
        breaktable.reserve(numbreak);
        np++;
        break;
      }
      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  if (np != 2) {
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the numbreak lines to read in the remainder of the table */
  for (int j = 0; j < numbreak; ++j) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    i = 0;
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 5, "BREAK", 5) != 0) {
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            numbreak = 0;
            return false;
          }
          break;
        }
        case 1: {
          breaktable.push_back(std::string(start_piece, iter));
          break;
        }
        default:
          break;
      }
      ++i;
      start_piece = mystrsep(nl, iter);
    }
  }

  if (breaktable.size() != static_cast<size_t>(numbreak)) {
    HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                     af->getlinenum());
    return false;
  }

  return true;
}

void AffixMgr::reverse_condition(std::string& piece) {
  if (piece.empty())
      return;

  int neg = 0;
  for (std::string::reverse_iterator k = piece.rbegin(); k != piece.rend(); ++k) {
    switch (*k) {
      case '[': {
        if (neg)
          *(k - 1) = '[';
        else
          *k = ']';
        break;
      }
      case ']': {
        *k = '[';
        if (neg)
          *(k - 1) = '^';
        neg = 0;
        break;
      }
      case '^': {
        if (*(k - 1) == ']')
          neg = 1;
        else if (neg)
          *(k - 1) = *k;
        break;
      }
      default: {
        if (neg)
          *(k - 1) = *k;
      }
    }
  }
}

class entries_container {
  std::vector<AffEntry*> entries;
  AffixMgr* m_mgr;
  char m_at;
public:
  entries_container(char at, AffixMgr* mgr)
    : m_mgr(mgr)
    , m_at(at) {
  }
  void release() {
    entries.clear();
  }
  void initialize(int numents,
                  char opts, unsigned short aflag) {
    entries.reserve(numents);

    if (m_at == 'P') {
      entries.push_back(new PfxEntry(m_mgr));
    } else {
      entries.push_back(new SfxEntry(m_mgr));
    }

    entries.back()->opts = opts;
    entries.back()->aflag = aflag;
  }

  AffEntry* add_entry(char opts) {
    if (m_at == 'P') {
      entries.push_back(new PfxEntry(m_mgr));
    } else {
      entries.push_back(new SfxEntry(m_mgr));
    }
    AffEntry* ret = entries.back();
    ret->opts = entries[0]->opts & opts;
    return ret;
  }

  AffEntry* first_entry() {
    return entries.empty() ? NULL : entries[0];
  }

  ~entries_container() {
    for (size_t i = 0; i < entries.size(); ++i) {
        delete entries[i];
    }
  }

  std::vector<AffEntry*>::iterator begin() { return entries.begin(); }
  std::vector<AffEntry*>::iterator end() { return entries.end(); }
};

bool AffixMgr::parse_affix(const std::string& line,
                          const char at,
                          FileMgr* af,
                          char* dupflags) {
  int numents = 0;  // number of AffEntry structures to parse

  unsigned short aflag = 0;  // affix char identifier

  char ff = 0;
  entries_container affentries(at, this);

  int i = 0;

// checking lines with bad syntax
#ifdef DEBUG
  int basefieldnum = 0;
#endif

  // split affix header line into pieces

  int np = 0;
  std::string::const_iterator iter = line.begin();
  std::string::const_iterator start_piece = mystrsep(line, iter);
  while (start_piece != line.end()) {
    switch (i) {
      // piece 1 - is type of affix
      case 0: {
        np++;
        break;
      }

      // piece 2 - is affix char
      case 1: {
        np++;
        aflag = pHMgr->decode_flag(std::string(start_piece, iter).c_str());
#ifndef HUNSPELL_CHROME_CLIENT // We don't check for duplicates.
        if (((at == 'S') && (dupflags[aflag] & dupSFX)) ||
            ((at == 'P') && (dupflags[aflag] & dupPFX))) {
          HUNSPELL_WARNING(
              stderr,
              "error: line %d: multiple definitions of an affix flag\n",
              af->getlinenum());
        }
        dupflags[aflag] += (char)((at == 'S') ? dupSFX : dupPFX);
#endif
        break;
      }
      // piece 3 - is cross product indicator
      case 2: {
        np++;
        if (*start_piece == 'Y')
          ff = aeXPRODUCT;
        break;
      }

      // piece 4 - is number of affentries
      case 3: {
        np++;
        numents = atoi(std::string(start_piece, iter).c_str());
        if ((numents <= 0) || ((std::numeric_limits<size_t>::max() /
                                sizeof(AffEntry)) < static_cast<size_t>(numents))) {
          char* err = pHMgr->encode_flag(aflag);
          if (err) {
            HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                             af->getlinenum());
            free(err);
          }
          return false;
        }

        char opts = ff;
        if (utf8)
          opts += aeUTF8;
        if (pHMgr->is_aliasf())
          opts += aeALIASF;
        if (pHMgr->is_aliasm())
          opts += aeALIASM;
        affentries.initialize(numents, opts, aflag);
      }

      default:
        break;
    }
    ++i;
    start_piece = mystrsep(line, iter);
  }
  // check to make sure we parsed enough pieces
  if (np != 4) {
    char* err = pHMgr->encode_flag(aflag);
    if (err) {
      HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                       af->getlinenum());
      free(err);
    }
    return false;
  }

  // now parse numents affentries for this affix
  AffEntry* entry = affentries.first_entry();
  for (int ent = 0; ent < numents; ++ent) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);

    iter = nl.begin();
    i = 0;
    np = 0;

    // split line into pieces
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        // piece 1 - is type
        case 0: {
          np++;
          if (ent != 0)
            entry = affentries.add_entry((char)(aeXPRODUCT + aeUTF8 + aeALIASF + aeALIASM));
          break;
        }

        // piece 2 - is affix char
        case 1: {
          np++;
          std::string chunk(start_piece, iter);
          if (pHMgr->decode_flag(chunk.c_str()) != aflag) {
            char* err = pHMgr->encode_flag(aflag);
            if (err) {
              HUNSPELL_WARNING(stderr,
                               "error: line %d: affix %s is corrupt\n",
                               af->getlinenum(), err);
              free(err);
            }
            return false;
          }

          if (ent != 0) {
            AffEntry* start_entry = affentries.first_entry();
            entry->aflag = start_entry->aflag;
          }
          break;
        }

        // piece 3 - is string to strip or 0 for null
        case 2: {
          np++;
          entry->strip = std::string(start_piece, iter);
          if (complexprefixes) {
            if (utf8)
              reverseword_utf(entry->strip);
            else
              reverseword(entry->strip);
          }
          if (entry->strip.compare("0") == 0) {
            entry->strip.clear();
          }
          break;
        }

        // piece 4 - is affix string or 0 for null
        case 3: {
          entry->morphcode = NULL;
          entry->contclass = NULL;
          entry->contclasslen = 0;
          np++;
          std::string::const_iterator dash = std::find(start_piece, iter, '/');
          if (dash != iter) {
            entry->appnd = std::string(start_piece, dash);
            std::string dash_str(dash + 1, iter);

            if (!ignorechars.empty() && !has_no_ignored_chars(entry->appnd, ignorechars)) {
              if (utf8) {
                remove_ignored_chars_utf(entry->appnd, ignorechars_utf16);
              } else {
                remove_ignored_chars(entry->appnd, ignorechars);
              }
            }

            if (complexprefixes) {
              if (utf8)
                reverseword_utf(entry->appnd);
              else
                reverseword(entry->appnd);
            }

            if (pHMgr->is_aliasf()) {
              int index = atoi(dash_str.c_str());
              entry->contclasslen = (unsigned short)pHMgr->get_aliasf(
                  index, &(entry->contclass), af);
              if (!entry->contclasslen)
                HUNSPELL_WARNING(stderr,
                                 "error: bad affix flag alias: \"%s\"\n",
                                 dash_str.c_str());
            } else {
              entry->contclasslen = (unsigned short)pHMgr->decode_flags(
                  &(entry->contclass), dash_str.c_str(), af);
              std::sort(entry->contclass, entry->contclass + entry->contclasslen);
            }

            havecontclass = 1;
            for (unsigned short _i = 0; _i < entry->contclasslen; _i++) {
              contclasses[(entry->contclass)[_i]] = 1;
            }
          } else {
            entry->appnd = std::string(start_piece, iter);

            if (!ignorechars.empty() && !has_no_ignored_chars(entry->appnd, ignorechars)) {
              if (utf8) {
                remove_ignored_chars_utf(entry->appnd, ignorechars_utf16);
              } else {
                remove_ignored_chars(entry->appnd, ignorechars);
              }
            }

            if (complexprefixes) {
              if (utf8)
                reverseword_utf(entry->appnd);
              else
                reverseword(entry->appnd);
            }
          }

          if (entry->appnd.compare("0") == 0) {
            entry->appnd.clear();
          }
          break;
        }

        // piece 5 - is the conditions descriptions
        case 4: {
          std::string chunk(start_piece, iter);
          np++;
          if (complexprefixes) {
            if (utf8)
              reverseword_utf(chunk);
            else
              reverseword(chunk);
            reverse_condition(chunk);
          }
          if (!entry->strip.empty() && chunk != "." &&
              redundant_condition(at, entry->strip.c_str(), entry->strip.size(), chunk.c_str(),
                                  af->getlinenum()))
            chunk = ".";
          if (at == 'S') {
            reverseword(chunk);
            reverse_condition(chunk);
          }
          if (encodeit(*entry, chunk.c_str()))
            return false;
          break;
        }

        case 5: {
          std::string chunk(start_piece, iter);
          np++;
          if (pHMgr->is_aliasm()) {
            int index = atoi(chunk.c_str());
            entry->morphcode = pHMgr->get_aliasm(index);
          } else {
            if (complexprefixes) {  // XXX - fix me for morph. gen.
              if (utf8)
                reverseword_utf(chunk);
              else
                reverseword(chunk);
            }
            // add the remaining of the line
            std::string::const_iterator end = nl.end();
            if (iter != end) {
              chunk.append(iter, end);
            }
            entry->morphcode = mystrdup(chunk.c_str());
            if (!entry->morphcode)
              return false;
          }
          break;
        }
        default:
          break;
      }
      i++;
      start_piece = mystrsep(nl, iter);
    }
    // check to make sure we parsed enough pieces
    if (np < 4) {
      char* err = pHMgr->encode_flag(aflag);
      if (err) {
        HUNSPELL_WARNING(stderr, "error: line %d: affix %s is corrupt\n",
                         af->getlinenum(), err);
        free(err);
      }
      return false;
    }

#ifdef DEBUG
    // detect unnecessary fields, excepting comments
    if (basefieldnum) {
      int fieldnum =
          !(entry->morphcode) ? 5 : ((*(entry->morphcode) == '#') ? 5 : 6);
      if (fieldnum != basefieldnum)
        HUNSPELL_WARNING(stderr, "warning: line %d: bad field number\n",
                         af->getlinenum());
    } else {
      basefieldnum =
          !(entry->morphcode) ? 5 : ((*(entry->morphcode) == '#') ? 5 : 6);
    }
#endif
  }

  // now create SfxEntry or PfxEntry objects and use links to
  // build an ordered (sorted by affix string) list
  std::vector<AffEntry*>::iterator start = affentries.begin();
  std::vector<AffEntry*>::iterator end = affentries.end();
  for (std::vector<AffEntry*>::iterator affentry = start; affentry != end; ++affentry) {
    if (at == 'P') {
      build_pfxtree(static_cast<PfxEntry*>(*affentry));
    } else {
      build_sfxtree(static_cast<SfxEntry*>(*affentry));
    }
  }

  //contents belong to AffixMgr now
  affentries.release();

  return true;
}

int AffixMgr::redundant_condition(char ft,
                                  const char* strip,
                                  int stripl,
                                  const char* cond,
                                  int linenum) {
  int condl = strlen(cond);
  int i;
  int j;
  int neg;
  int in;
  if (ft == 'P') {  // prefix
    if (strncmp(strip, cond, condl) == 0)
      return 1;
    if (utf8) {
    } else {
      for (i = 0, j = 0; (i < stripl) && (j < condl); i++, j++) {
        if (cond[j] != '[') {
          if (cond[j] != strip[i]) {
            HUNSPELL_WARNING(stderr,
                             "warning: line %d: incompatible stripping "
                             "characters and condition\n",
                             linenum);
            return 0;
          }
        } else {
          neg = (cond[j + 1] == '^') ? 1 : 0;
          in = 0;
          do {
            j++;
            if (strip[i] == cond[j])
              in = 1;
          } while ((j < (condl - 1)) && (cond[j] != ']'));
          if (j == (condl - 1) && (cond[j] != ']')) {
            HUNSPELL_WARNING(stderr,
                             "error: line %d: missing ] in condition:\n%s\n",
                             linenum, cond);
            return 0;
          }
          if ((!neg && !in) || (neg && in)) {
            HUNSPELL_WARNING(stderr,
                             "warning: line %d: incompatible stripping "
                             "characters and condition\n",
                             linenum);
            return 0;
          }
        }
      }
      if (j >= condl)
        return 1;
    }
  } else {  // suffix
    if ((stripl >= condl) && strcmp(strip + stripl - condl, cond) == 0)
      return 1;
    if (utf8) {
    } else {
      for (i = stripl - 1, j = condl - 1; (i >= 0) && (j >= 0); i--, j--) {
        if (cond[j] != ']') {
          if (cond[j] != strip[i]) {
            HUNSPELL_WARNING(stderr,
                             "warning: line %d: incompatible stripping "
                             "characters and condition\n",
                             linenum);
            return 0;
          }
        } else {
          in = 0;
          do {
            j--;
            if (strip[i] == cond[j])
              in = 1;
          } while ((j > 0) && (cond[j] != '['));
          if ((j == 0) && (cond[j] != '[')) {
            HUNSPELL_WARNING(stderr,
                             "error: line: %d: missing ] in condition:\n%s\n",
                             linenum, cond);
            return 0;
          }
          neg = (cond[j + 1] == '^') ? 1 : 0;
          if ((!neg && !in) || (neg && in)) {
            HUNSPELL_WARNING(stderr,
                             "warning: line %d: incompatible stripping "
                             "characters and condition\n",
                             linenum);
            return 0;
          }
        }
      }
      if (j < 0)
        return 1;
    }
  }
  return 0;
}

std::vector<std::string> AffixMgr::get_suffix_words(short unsigned* suff,
                               int len,
                               const char* root_word) {
  std::vector<std::string> slst;
  short unsigned* start_ptr = suff;
  for (int j = 0; j < SETSIZE; j++) {
    SfxEntry* ptr = sStart[j];
    while (ptr) {
      suff = start_ptr;
      for (int i = 0; i < len; i++) {
        if ((*suff) == ptr->getFlag()) {
          std::string nw(root_word);
          nw.append(ptr->getAffix());
          hentry* ht = ptr->checkword(nw.c_str(), nw.size(), 0, NULL, 0, 0, 0);
          if (ht) {
            slst.push_back(nw);
          }
        }
        suff++;
      }
      ptr = ptr->getNext();
    }
  }
  return slst;
}
