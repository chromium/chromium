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

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <ctime>

#include <algorithm>
#include <chrono>
#include <memory>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

#include "affixmgr.hxx"
#include "affentry.hxx"
#include "hunspelltrace.hxx"
#include "langnum.hxx"

#include "csutil.hxx"

namespace {
  // the distinct records of one analysis, in the order they were first added. a record is a run of
  // text between two breakchar separators
  class DistinctRecords {
   public:
    explicit DistinctRecords(char breakchar) : sep(breakchar) {}

    // add each record of add that is not held already
    void append(const std::string& add) {
      size_t pos = 0;
      while (pos < add.size()) {
        size_t end = add.find(sep, pos);
        size_t len = (end == std::string::npos ? add.size() : end) - pos;
        if (len) {
          std::string record(add, pos, len);
          if (seen.insert(record).second)
            records.push_back(std::move(record));
        }
        if (end == std::string::npos)
          break;
        pos = end + 1;
      }
    }

    // the records joined back into one string, each one terminated by breakchar
    std::string join() const {
      std::string text;
      for (const std::string& record : records) {
        text.append(record);
        text.push_back(sep);
      }
      return text;
    }

   private:
    std::vector<std::string> records;
    // the same records again, to answer "have I seen this one" without a walk over records
    std::unordered_set<std::string> seen;
    char sep;
  };
}

#ifdef HUNSPELL_CHROME_CLIENT
AffixMgr::AffixMgr(hunspell::BDictReader* reader,
                   const std::vector<std::unique_ptr<HashMgr>>& ptr)
  : alldic(ptr)
  , pHMgr(ptr[0].get()) {
  bdict_reader = reader;
#else
AffixMgr::AffixMgr(const char* affpath,
                   const std::vector<std::unique_ptr<HashMgr>>& ptr,
                   const char* key)
  : alldic(ptr)
  , pHMgr(ptr[0].get()) {
#endif

  // register hash manager and load affix data from aff file
  csconv = nullptr;
  utf8 = 0;
  complexprefixes = 0;
  parsedmaptable = false;
  parsedbreaktable = false;
  iconvtable = nullptr;
  oconvtable = nullptr;
  // allow simplified compound forms (see 3rd field of CHECKCOMPOUNDPATTERN)
  simplifiedcpd = 0;
  parsedcheckcpd = false;
  parseddefcpd = false;
  phone = nullptr;
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
  pfxappnd = nullptr;     // previous prefix for counting syllables of the prefix BUG
  sfxappnd = nullptr;     // previous suffix for counting syllables of the suffix BUG
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

  sfx = nullptr;
  pfx = nullptr;

  for (int i = 0; i < SETSIZE; i++) {
    pStart[i] = nullptr;
    sStart[i] = nullptr;
    pFlag[i] = nullptr;
    sFlag[i] = nullptr;
  }

#ifdef HUNSPELL_CHROME_CLIENT
  // Define dummy parameters for parse_file() to avoid changing the parameters
  // of parse_file(). This may make it easier to merge the changes of the
  // original hunspell.
  const char* affpath = NULL;
  const char* key = NULL;
#else
  memset(contclasses, 0, CONTSIZE * sizeof(char));
#endif

  if (parse_file(affpath, key)) {
    fprintf(stderr, "Failure loading aff file %s\n", affpath);
  }

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
    breaktable.emplace_back("-");
    breaktable.emplace_back("^-");
    breaktable.emplace_back("-$");
    parsedbreaktable = true;
  }

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
  // not entirely sure this is invalid, so only for fuzzing for now
  if (iconvtable && !iconvtable->check_against_breaktable(breaktable)) {
      delete iconvtable;
      iconvtable = nullptr;
  }
#endif

  if (cpdmin == -1)
    cpdmin = MINCPDLEN;
}

AffixMgr::~AffixMgr() {
  // pass through linked prefix entries and clean up
  for (int i = 0; i < SETSIZE; i++) {
    pFlag[i] = nullptr;
    PfxEntry* ptr = pStart[i];
    PfxEntry* nptr = nullptr;
    while (ptr) {
      nptr = ptr->getNext();
      delete (ptr);
      ptr = nptr;
    }
  }

  // pass through linked suffix entries and clean up
  for (int j = 0; j < SETSIZE; j++) {
    sFlag[j] = nullptr;
    SfxEntry* ptr = sStart[j];
    SfxEntry* nptr = nullptr;
    while (ptr) {
      nptr = ptr->getNext();
      delete (ptr);
      ptr = nptr;
    }
    sStart[j] = nullptr;
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
  pHMgr = nullptr;
  cpdmin = 0;
  cpdmaxsyllable = 0;
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
  const auto flg = (unsigned char)(ep->getFlag() & 0x00FF);

  // first index by flag which must exist
  ptr = pFlag[flg];
  ep->setFlgNxt(ptr);
  pFlag[flg] = ep;

  // handle the special case of null affix string
  if (*key == '\0') {
    // always inset them at head of list at element 0
    ptr = pStart[0];
    ep->setNext(ptr);
    pStart[0] = ep;
    return 0;
  }

  // now handle the normal case
  ep->setNextEQ(nullptr);
  ep->setNextNE(nullptr);

  unsigned char sp = *((const unsigned char*)key);
  ptr = pStart[sp];

  // handle the first insert
  if (!ptr) {
    pStart[sp] = ep;
    return 0;
  }

  // otherwise use binary tree insertion so that a sorted
  // list can easily be generated later
  pptr = nullptr;
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
  const auto flg = (unsigned char)(ep->getFlag() & 0x00FF);

  // first index by flag which must exist
  ptr = sFlag[flg];
  ep->setFlgNxt(ptr);
  sFlag[flg] = ep;

  // next index by affix string

  // handle the special case of null affix string
  if (*key == '\0') {
    // always inset them at head of list at element 0
    ptr = sStart[0];
    ep->setNext(ptr);
    sStart[0] = ep;
    return 0;
  }

  // now handle the normal case
  ep->setNextEQ(nullptr);
  ep->setNextNE(nullptr);

  unsigned char sp = *((const unsigned char*)key);
  ptr = sStart[sp];

  // handle the first insert
  if (!ptr) {
    sStart[sp] = ep;
    return 0;
  }

  // otherwise use binary tree insertion so that a sorted
  // list can easily be generated later
  pptr = nullptr;
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
    pStart[i] = process_pfx_in_order(pStart[i], nullptr);
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
    sStart[i] = process_sfx_in_order(sStart[i], nullptr);
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

    for (; ptr != nullptr; ptr = ptr->getNext()) {
      PfxEntry* nptr = ptr->getNext();
      for (; nptr != nullptr; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
      }
      ptr->setNextNE(nptr);
      ptr->setNextEQ(nullptr);
      if ((ptr->getNext()) &&
          isSubset(ptr->getKey(), (ptr->getNext())->getKey()))
        ptr->setNextEQ(ptr->getNext());
    }

    // now clean up by adding smart search termination strings:
    // if you are already a superset of the previous prefix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = pStart[i];
    for (; ptr != nullptr; ptr = ptr->getNext()) {
      PfxEntry* nptr = ptr->getNext();
      PfxEntry* mptr = nullptr;
      for (; nptr != nullptr; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
        mptr = nptr;
      }
      if (mptr)
        mptr->setNextNE(nullptr);
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

    for (; ptr != nullptr; ptr = ptr->getNext()) {
      SfxEntry* nptr = ptr->getNext();
      for (; nptr != nullptr; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
      }
      ptr->setNextNE(nptr);
      ptr->setNextEQ(nullptr);
      if ((ptr->getNext()) &&
          isSubset(ptr->getKey(), (ptr->getNext())->getKey()))
        ptr->setNextEQ(ptr->getNext());
    }

    // now clean up by adding smart search termination strings:
    // if you are already a superset of the previous suffix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = sStart[i];
    for (; ptr != nullptr; ptr = ptr->getNext()) {
      SfxEntry* nptr = ptr->getNext();
      SfxEntry* mptr = nullptr;
      for (; nptr != nullptr; nptr = nptr->getNext()) {
        if (!isSubset(ptr->getKey(), nptr->getKey()))
          break;
        mptr = nptr;
      }
      if (mptr)
        mptr->setNextNE(nullptr);
    }
  }
  return 0;
}

// add flags to the result for dictionary debugging
std::string& AffixMgr::debugflag(std::string& result, unsigned short flag) {
  std::string st = encode_flag(flag);
  result.push_back(MSEP_FLD);
  result.append(MORPH_FLAG);
  result.append(st);
  return result;
}

// Count match positions in an .aff condition pattern: each [...] group
// counts as one, each non-grouped codepoint counts as one.
int AffixMgr::condlen(const std::string& s) {
  int l = 0;
  bool group = false;
  size_t i = 0;
  while (i < s.size()) {
    if (s[i] == '[') {
      group = true;
      ++l;
      ++i;
    } else if (s[i] == ']') {
      group = false;
      ++i;
    } else if (group) {
      ++i;
    } else {
      ++l;
      i = utf8 ? utf8_next(s, i) : i + 1;
    }
  }
  return l;
}

int AffixMgr::encodeit(AffEntry& entry, const std::string& cs) {
  if (cs.compare(".") != 0) {
    int n = condlen(cs);
    if (n > std::numeric_limits<unsigned char>::max()) {
      HUNSPELL_WARNING(stderr, "error: condition length %d is over max limit\n", n);
      return 1;
    }
    entry.numconds = (unsigned char)n;
    const size_t cslen = cs.size();
    const size_t short_part = std::min<size_t>(MAXCONDLEN, cslen);
    memcpy(entry.c.conds, cs.data(), short_part);
    if (short_part < MAXCONDLEN) {
      //blank out the remaining space
      memset(entry.c.conds + short_part, 0, MAXCONDLEN - short_part);
    } else if (cs[MAXCONDLEN]) {
      //there is more conditions than fit in fixed space, so its
      //a long condition
      entry.opts |= aeLONGCOND;
      size_t remaining = cs.size() - MAXCONDLEN_1;
      entry.c.l.conds2 = new char[1 + remaining];
      memcpy(entry.c.l.conds2, cs.data() + MAXCONDLEN_1, remaining);
      entry.c.l.conds2[remaining] = 0;
    }
  } else {
    entry.numconds = 0;
    entry.c.conds[0] = '\0';
  }
  return 0;
}

// return 1 if s1 is a leading subset of s2 (dots are for infixes)
inline int AffixMgr::isSubset(const char* s1, const char* s2) {
  while (((*s1 == *s2) || (*s1 == '.')) && (*s1 != '\0') && (*s2 != '\0')) {
    s1++;
    s2++;
  }
  return (*s1 == '\0');
}

// check word for prefixes
struct hentry* AffixMgr::prefix_check(const std::string& word,
                                      int start,
                                      int len,
                                      char in_compound,
                                      AffixScratch& scratch,
                                      const FLAG needflag,
                                      const FLAG avoidflag) {
  struct hentry* rv = nullptr;

  pfx = nullptr;
  pfxappnd = nullptr;
  sfxappnd = nullptr;
  sfxextra = 0;

  TraceCtx* t = trace_on(scratch.trace);
  int candidates = 0;

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
      ++candidates;
      rv = pe->checkword(word, start, len, in_compound, needflag, scratch);
      // Skip a stem with the avoid flag and keep scanning the other prefixes.
      if (rv && avoidflag != FLAG_NULL && TESTAFF(rv->astr, avoidflag, rv->alen)) {
        trace_avoidflag(t, avoidflag, rv);
        rv = nullptr;
      }
      if (rv) {
        pfx = pe;  // BUG: pfx not stateless
        return rv;
      }
    }
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = word[start];
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word.c_str() + start)) {
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
        ++candidates;
        rv = pptr->checkword(word, start, len, in_compound, needflag, scratch);
        if (rv && avoidflag != FLAG_NULL && TESTAFF(rv->astr, avoidflag, rv->alen)) {
          trace_avoidflag(t, avoidflag, rv);
          rv = nullptr;
        }
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

  // a pass that had nothing to try says so, which is different from a pass that
  // did not run
  if (t && candidates == 0)
    trace(*t, "pfx \"%s\" candidates=0", word.substr(start, len).c_str());

  return nullptr;
}

// check word for prefixes and two-level suffixes
struct hentry* AffixMgr::prefix_check_twosfx(const std::string& word,
                                             int start,
                                             int len,
                                             char in_compound,
                                             AffixScratch& scratch,
                                             const FLAG needflag) {
  struct hentry* rv = nullptr;

  pfx = nullptr;
  sfxappnd = nullptr;
  sfxextra = 0;

  // first handle the special case of 0 length prefixes
  PfxEntry* pe = pStart[0];

  while (pe) {
    rv = pe->check_twosfx(word, start, len, in_compound, needflag, scratch);
    if (rv)
      return rv;
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = word[start];
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word.c_str() + start)) {
      rv = pptr->check_twosfx(word, start, len, in_compound, needflag, scratch);
      if (rv) {
        pfx = pptr;
        return rv;
      }
      pptr = pptr->getNextEQ();
    } else {
      pptr = pptr->getNextNE();
    }
  }

  return nullptr;
}

// check word for prefixes and morph
std::string AffixMgr::prefix_check_morph(const std::string& word,
                                         int start,
                                         int len,
                                         char in_compound,
                                         AffixScratch& scratch,
                                         const FLAG needflag) {

  // many prefixes can analyse a word the same way
  DistinctRecords result(MSEP_REC);

  pfx = nullptr;
  sfxappnd = nullptr;
  sfxextra = 0;

  // first handle the special case of 0 length prefixes
  PfxEntry* pe = pStart[0];
  while (pe) {
    std::string st = pe->check_morph(word, start, len, in_compound, needflag, scratch);
    if (!st.empty()) {
      result.append(st);
    }
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = word[start];
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word.c_str() + start)) {
      std::string st = pptr->check_morph(word, start, len, in_compound, needflag, scratch);
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

  return result.join();
}

// check word for prefixes and morph and two-level suffixes
std::string AffixMgr::prefix_check_twosfx_morph(const std::string& word,
                                                int start,
                                                int len,
                                                char in_compound,
                                                AffixScratch& scratch,
                                                const FLAG needflag) {
  // every prefix crosses with every pair of suffixes
  DistinctRecords result(MSEP_REC);

  pfx = nullptr;
  sfxappnd = nullptr;
  sfxextra = 0;

  // first handle the special case of 0 length prefixes
  PfxEntry* pe = pStart[0];
  while (pe) {
    std::string st = pe->check_twosfx_morph(word, start, len, in_compound, needflag, scratch);
    if (!st.empty()) {
      result.append(st);
    }
    pe = pe->getNext();
  }

  // now handle the general case
  unsigned char sp = word[start];
  PfxEntry* pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->getKey(), word.c_str() + start)) {
      std::string st = pptr->check_twosfx_morph(word, start, len, in_compound, needflag, scratch);
      if (!st.empty()) {
        result.append(st);
        pfx = pptr;
      }
      pptr = pptr->getNextEQ();
    } else {
      pptr = pptr->getNextNE();
    }
  }

  return result.join();
}

// Is word a non-compound with a REP substitution (see checkcompoundrep)?
int AffixMgr::cpdrep_check(const std::string& in_word,
                           int wl,
                           AffixScratch& scratch,
                           bool& timelimit_exceeded,
                           std::chrono::steady_clock::time_point clock_time_start) {
  if ((wl < 2) || get_reptable().empty())
    return 0;

  std::string word(in_word, 0, wl);

#ifdef HUNSPELL_CHROME_CLIENT
  std::string_view pattern, pattern2;
  hunspell::ReplacementIterator iterator = bdict_reader->GetReplacementIterator();
  while (iterator.GetNext(&pattern, &pattern2)) {
    size_t r = 0;
    // search every occurence of the pattern in the word
    while ((r = word.find(pattern, r)) != std::string::npos) {
      std::string candidate(word);
      candidate.replace(r, pattern.size(), pattern2);
      if (candidate_check(candidate, scratch)) return 1;
      r++; // search for the next letter
    }
  }

#else
  for (const auto& i : get_reptable()) {
    if (timelimit_exceeded || std::chrono::steady_clock::now() - clock_time_start > TIMELIMIT_MS) {
      timelimit_exceeded = true;
      return 0;
    }
    // use only available mid patterns
    if (!i.outstrings[0].empty()) {
      size_t r = 0;
      const size_t lenp = i.pattern.size();
      // search every occurence of the pattern in the word
      while ((r = word.find(i.pattern, r)) != std::string::npos) {
        std::string candidate(word);
        candidate.replace(r, lenp, i.outstrings[0]);
        if (candidate_check(candidate, scratch))
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
int AffixMgr::cpdwordpair_check(const std::string& word,
                                int wl,
                                AffixScratch& scratch,
                                bool& timelimit_exceeded,
                                std::chrono::steady_clock::time_point clock_time_start) {
  TraceCtx* t = trace_on(scratch.trace);
  int pair_found = 0;
  std::string pair;

  {
    // this check puts a space at every position in turn, so the trace reports the verdict alone
    TraceSuppress no_trace(scratch.trace);

    if (wl > 2) {
      std::string candidate(word, 0, wl);
      for (size_t i = 1; i < candidate.size(); i++) {
        if (timelimit_exceeded || std::chrono::steady_clock::now() - clock_time_start > TIMELIMIT_MS) {
          timelimit_exceeded = true;
          break;
        }
        // go to end of the UTF-8 character
        if (utf8 && is_utf8_cont(candidate[i]))
            continue;
        candidate.insert(i, 1, ' ');
        if (candidate_check(candidate, scratch)) {
          pair_found = 1;
          pair = candidate;
          break;
        }
        candidate.erase(i, 1);
      }
    }
  }

  if (t) {
    if (pair_found)
      trace(*t, "test wordpair pair=\"%s\" -> fail, the parts are a known word pair", pair.c_str());
    else if (!timelimit_exceeded)
      trace(*t, "test wordpair -> pass, no space splits this into a known word pair");
  }

  return pair_found;
}

// a CHECKCOMPOUNDPATTERN half may leave its flag out, and then any word is allowed to stand there
static std::string trace_cond_flag(const AffixMgr* pAMgr, FLAG cond) {
  if (cond == FLAG_NULL)
    return "(any)";
  return trace_flag(pAMgr, cond);
}

// one side of a compound join, as the entry it was found in and the flags that entry carries
static std::string trace_join_word(const AffixMgr* pAMgr, const hentry* entry) {
  if (!entry)
    return "(none)";
  return "\"" + std::string(entry->word, entry->blen) + "\"/" +
         trace_flags(pAMgr, entry->astr, entry->alen);
}

// one side of a compound join carries a flag when the dictionary entry has it, or when a prefix or
// suffix that built that side passes it on through its continuation classes
static bool join_side_has_flag(const hentry* entry, FLAG cond, PfxEntry* p, SfxEntry* s) {
  return (entry->astr && TESTAFF(entry->astr, cond, entry->alen)) ||
         (p && p->getCont() && TESTAFF(p->getCont(), cond, p->getContLen())) ||
         (s && s->getCont() && TESTAFF(s->getCont(), cond, s->getContLen()));
}

// forbid compoundings when there are special patterns at word bound
int AffixMgr::cpdpat_check(const std::string& word,
                           size_t pos,
                           hentry* r1,
                           hentry* r2,
                           const char /*affixed*/,
                           const TraceCtx* t,
                           PfxEntry* p1,
                           SfxEntry* s1,
                           PfxEntry* p2,
                           SfxEntry* s2) {
  for (auto& i : checkcpdtable) {
    size_t len;
    bool right_text_ok = isSubset(i.pattern2.c_str(), word.c_str() + pos);
    bool left_flag_ok = right_text_ok &&
        (!r1 || !i.cond || join_side_has_flag(r1, i.cond, p1, s1));
    bool right_flag_ok = left_flag_ok &&
        (!r2 || !i.cond2 || join_side_has_flag(r2, i.cond2, p2, s2));
    // zero length pattern => only TESTAFF
    // zero pattern (0/flag) => unmodified stem (zero affixes allowed)
    bool left_text_ok = right_flag_ok &&
        (i.pattern.empty() ||
         ((i.pattern[0] == '0' && r1->blen <= pos &&
           strncmp(word.c_str() + pos - r1->blen, r1->word, r1->blen) == 0) ||
          (i.pattern[0] != '0' &&
           ((len = i.pattern.size()) != 0) && len <= pos &&
           strncmp(word.c_str() + pos - len, i.pattern.c_str(), len) == 0)));

    if (t) {
      std::string fields = "left=\"" + i.pattern + "\"/" + trace_cond_flag(this, i.cond) +
                           " right=\"" + i.pattern2 + "\"/" + trace_cond_flag(this, i.cond2) +
                           " first=" + trace_join_word(this, r1) +
                           " second=" + trace_join_word(this, r2);
      if (left_text_ok)
        trace(*t, "test cpdpattern %s -> fail, this pair is forbidden at the join", fields.c_str());
      else if (!right_text_ok)
        trace(*t, "test cpdpattern %s -> pass, the text after the join does not start with \"%s\"",
              fields.c_str(), i.pattern2.c_str());
      else if (!left_flag_ok)
        trace(*t, "test cpdpattern %s -> pass, \"%s\" has no %s", fields.c_str(),
              std::string(r1->word, r1->blen).c_str(), trace_flag(this, i.cond).c_str());
      else if (!right_flag_ok)
        trace(*t, "test cpdpattern %s -> pass, \"%s\" has no %s", fields.c_str(),
              std::string(r2->word, r2->blen).c_str(), trace_flag(this, i.cond2).c_str());
      else
        trace(*t, "test cpdpattern %s -> pass, the text before the join does not end with \"%s\"",
              fields.c_str(), i.pattern.c_str());
    }

    if (left_text_ok)
      return 1;
  }
  return 0;
}

// forbid compounding with neighbouring upper and lower case characters at word
// bounds
int AffixMgr::cpdcase_check(const std::string& word, int pos) {
  if (utf8) {
    const char* p;
    const char* wordp = word.c_str();
    for (p = wordp + pos - 1; p > wordp && is_utf8_cont(*p); p--)
      ;
    std::string pair(p);
    std::vector<w_char> pair_u;
    u8_u16(pair_u, pair);
    unsigned short a = pair_u.size() > 1 ? (unsigned short)pair_u[1] : 0,
                   b = !pair_u.empty() ? (unsigned short)pair_u[0] : 0;
    if (((unicodetoupper(a, langnum) == a && unicodetolower(a, langnum) != a) ||
         (unicodetoupper(b, langnum) == b && unicodetolower(b, langnum) != b)) &&
        (a != '-') && (b != '-'))
      return 1;
  } else {
    const unsigned char a = word[pos - 1], b = word[pos];
    if ((csconv[a].ccase || csconv[b].ccase) && (a != '-') && (b != '-'))
      return 1;
  }
  return 0;
}

struct metachar_data {
  size_t btpp;        // metacharacter (*, ?) position for backtracking
  signed short btwp;  // word position for metacharacters
  int btnum;          // number of matched characters in metacharacter
};

// check compound patterns
int AffixMgr::defcpd_check(hentry*** words,
                           short wnum,
                           short maxwordnum,
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

  if (wnum >= maxwordnum) {
    if (w)
      *words = nullptr;
    return 0;
  }

  std::vector<metachar_data> btinfo(1);

  short bt = 0;

  (*words)[wnum] = rv;

  // has the last word COMPOUNDRULE flag?
  if (rv->alen == 0) {
    (*words)[wnum] = nullptr;
    if (w)
      *words = nullptr;
    return 0;
  }
  int ok = 0;
  for (auto& i : defcpdtable) {
    for (auto& j : i) {
      if (j != '*' && j != '?' &&
          TESTAFF(rv->astr, j, rv->alen)) {
        ok = 1;
        break;
      }
    }
  }
  if (ok == 0) {
    (*words)[wnum] = nullptr;
    if (w)
      *words = nullptr;
    return 0;
  }

  for (auto& i : defcpdtable) {
    size_t pp = 0;  // pattern position
    signed short wp = 0;  // "words" position
    int ok2 = 1;
    ok = 1;
    do {
      while ((pp < i.size()) && (wp <= wnum)) {
        if (((pp + 1) < i.size()) &&
            ((i[pp + 1] == '*') ||
             (i[pp + 1] == '?'))) {
          int wend = (i[pp + 1] == '?') ? wp : wnum;
          ok2 = 1;
          pp += 2;
          btinfo[bt].btpp = pp;
          btinfo[bt].btwp = wp;
          while (wp <= wend) {
            if (!(*words)[wp] ||
                !(*words)[wp]->alen ||
                !TESTAFF((*words)[wp]->astr, i[pp - 2],
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
              !TESTAFF((*words)[wp]->astr, i[pp],
                       (*words)[wp]->alen)) {
            ok = 0;
            break;
          }
          pp++;
          wp++;
          if ((i.size() == pp) && !(wp > wnum))
            ok = 0;
        }
      }
      if (ok && ok2) {
        size_t r = pp;
        while ((i.size() > r) && ((r + 1) < i.size()) &&
               ((i[r + 1] == '*') ||
                (i[r + 1] == '?')))
          r += 2;
        if (i.size() <= r)
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

    if (ok && ok2 && (!all || (i.size() <= pp)))
      return 1;

    // check zero ending
    while (ok && ok2 && (i.size() > pp) &&
           ((pp + 1) < i.size()) &&
           ((i[pp + 1] == '*') ||
            (i[pp + 1] == '?')))
      pp += 2;
    if (ok && ok2 && (i.size() <= pp))
      return 1;
  }
  (*words)[wnum] = nullptr;
  if (w)
    *words = nullptr;
  return 0;
}

inline int AffixMgr::candidate_check(const std::string& word, AffixScratch& scratch) {

  struct hentry* rv = lookup(word.c_str(), word.size());
  if (rv)
    return 1;

  //  rv = prefix_check(word,0,len,1);
  //  if (rv) return 1;

  rv = affix_check(word, 0, word.size(), scratch);
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
    num = (short)std::count_if(word.begin(), word.end(),
          [&](char c) {
            return std::binary_search(cpdvowels.begin(), cpdvowels.end(), c);
          });
  } else if (!cpdvowels_utf16.empty()) {
    std::vector<w_char> w;
    u8_u16(w, word);
    num = (short)std::count_if(w.begin(), w.end(),
          [&](w_char wc) {
            return std::binary_search(cpdvowels_utf16.begin(), cpdvowels_utf16.end(), wc);
          });
  }

  return num;
}

void AffixMgr::setcminmax(size_t* cmin, size_t* cmax, const char* word, size_t len) {
  if (utf8) {
    int i;
    for (*cmin = 0, i = 0; (i < cpdmin) && *cmin < len; i++) {
      for ((*cmin)++; *cmin < len && is_utf8_cont(word[*cmin]); (*cmin)++)
        ;
    }
    for (*cmax = len, i = 0; (i < (cpdmin - 1)) && *cmax > 0; i++) {
      for ((*cmax)--; *cmax > 0 && is_utf8_cont(word[*cmax]); (*cmax)--)
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
                                        hentry** words,
                                        hentry** rwords,
                                        char hu_mov_rule,
                                        char is_sug,
                                        int* info,
                                        AffixScratch& scratch) {
  short oldnumsyllable, oldnumsyllable2, oldwordnum, oldwordnum2;
  hentry *rv = nullptr, *rv_first;
  std::string st;
  char ch = '\0', affixed;
  size_t cmin, cmax;
  int striple = 0, soldi = 0, oldcmin = 0, oldcmax = 0, oldlen = 0, checkedstriple = 0;
  hentry** oldwords = words;
  size_t scpd = 0, len = word.size();

  TraceCtx* t = trace_on(scratch.trace);

  // protect subsequent words[wnum + 1] reads and any recursion
  if (wnum + 1 >= maxwordnum)
    return nullptr;

  int checked_prefix;

  // add a time limit to handle possible
  // combinatorical explosion of the overlapping words

  HUNSPELL_THREAD_LOCAL std::chrono::steady_clock::time_point clock_time_start;
  HUNSPELL_THREAD_LOCAL bool timelimit_exceeded;

  // get the current time
  std::chrono::steady_clock::time_point clock_now = std::chrono::steady_clock::now();

  if (wnum == 0) {
      // set the start time
      clock_time_start = clock_now;
      timelimit_exceeded = false;
  }
  else if (clock_now - clock_time_start > TIMELIMIT_MS)
      timelimit_exceeded = true;

  setcminmax(&cmin, &cmax, word.c_str(), len);

  // with SIMPLIFIEDTRIPLE a second part that starts at a doubled letter is one letter longer
  // than the surface shows, so the split can go one letter past cmax
  size_t cmaxtriple = simplifiedtriple ? (utf8 ? utf8_next(word, cmax) : cmax + 1) : cmax;

  st.assign(word);

  for (size_t i = cmin; i < cmaxtriple; ++i) {
    // go to end of the UTF-8 character
    if (utf8) {
      for (; is_utf8_cont(st[i]); i++)
        ;
      if (i >= cmaxtriple)
        return nullptr;
    }
    if (i >= cmax && !(i > 2 && word[i - 1] == word[i - 2]))
      break;

    words = oldwords;
    int onlycpdrule = (words) ? 1 : 0;

    do {  // onlycpdrule loop

      oldnumsyllable = numsyllable;
      oldwordnum = wordnum;
      checked_prefix = 0;

      do {  // simplified checkcompoundpattern loop

        if (timelimit_exceeded ||
            std::chrono::steady_clock::now() - clock_time_start > TIMELIMIT_MS) {
          if (t && !timelimit_exceeded)
            trace(*t, "test timelimit -> fail, the compound search stops here and gives up");
          timelimit_exceeded = true;
          return nullptr;
        }

        if (scpd > 0) {
          for (; scpd <= checkcpdtable.size() &&
                 (checkcpdtable[scpd - 1].pattern3.empty() ||
                  i > word.size() ||
                  word.compare(i, checkcpdtable[scpd - 1].pattern3.size(), checkcpdtable[scpd - 1].pattern3) != 0);
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

        if (i >= st.size())
          return nullptr;

        ch = st[i];
        if (t) {
          // a non-zero scpd means this split is the retry that one CHECKCOMPOUNDPATTERN entry asks
          // for, with the text at the join replaced by the two sides of that entry
          if (scpd != 0)
            trace(*t, "split at=%d left=\"%s\" right=\"%s\" cpdpattern=%d", (int)i,
                  st.substr(0, i).c_str(), st.substr(i).c_str(), (int)scpd);
          else
            trace(*t, "split at=%d left=\"%s\" right=\"%s\"", (int)i,
                  st.substr(0, i).c_str(), st.substr(i).c_str());
        }
        // everything this split point tries belongs to the split
        TraceScope split_depth(t);
        st[i] = '\0';

        sfx = nullptr;
        pfx = nullptr;

        // FIRST WORD

        affixed = 1;
        rv = lookup(st.c_str(), i);  // perhaps without prefix
        if (t) {
          if (rv)
            trace(*t, "first \"%s\" -> entry \"%s\" flags=%s", st.c_str(),
                  rv->word, trace_flags(this, rv->astr, rv->alen).c_str());
          else
            trace(*t, "first \"%s\" -> miss", st.c_str());
        }

        // forbid dictionary stems with COMPOUNDFORBIDFLAG in
        // compound words, overriding the effect of COMPOUNDPERMITFLAG
        if ((rv) && compoundforbidflag &&
                TESTAFF(rv->astr, compoundforbidflag, rv->alen) && !hu_mov_rule) {
            bool would_continue = !onlycpdrule && simplifiedcpd;
            if (!scpd && would_continue) {
                // given the while conditions that continue jumps to, this situation
                // never ends
                HUNSPELL_WARNING(stderr, "break infinite loop\n");
                break;
            }

            if (scpd > 0 && would_continue) {
                // under these conditions we loop again, but the assumption above
                // appears to be that cmin and cmax are the original values they
                // had in the outside loop
                cmin = oldcmin;
                cmax = oldcmax;
            }
            continue;
        }

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
                     defcpd_check(&words, wnum, maxwordnum, rv, rwords, 0)) ||
                    (words &&
                     defcpd_check(&words, wnum, maxwordnum, rv, rwords, 0))))) ||
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
              !(rv = prefix_check(st, 0, i,
                                  hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                                  scratch, compoundflag))) {
            if (((rv = suffix_check(st, 0, i, 0, nullptr, scratch, FLAG_NULL, compoundflag, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                 (compoundmoresuffixes && (rv = suffix_check_twosfx(st, 0, i, 0, nullptr, scratch, compoundflag)))) &&
                !hu_mov_rule && sfx->getCont() &&
                ((compoundforbidflag && TESTAFF(sfx->getCont(), compoundforbidflag, sfx->getContLen())) ||
                 (compoundend && TESTAFF(sfx->getCont(), compoundend, sfx->getContLen())))) {
              rv = nullptr;
              // the suffix is dropped with the word it built
              sfx = nullptr;
            }
          }

          if (rv ||
              (((wordnum == 0) && compoundbegin &&
                ((rv = suffix_check(st, 0, i, 0, nullptr, scratch, FLAG_NULL, compoundbegin, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                 (compoundmoresuffixes && (rv = suffix_check_twosfx(st, 0, i, 0, nullptr, scratch,
                                                                    compoundbegin))) ||  // twofold suffixes + compound
                 (rv = prefix_check(st, 0, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, scratch, compoundbegin)))) ||
               ((wordnum > 0) && compoundmiddle &&
                ((rv = suffix_check(st, 0, i, 0, nullptr, scratch, FLAG_NULL, compoundmiddle, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
                 (compoundmoresuffixes && (rv = suffix_check_twosfx(st, 0, i, 0, nullptr, scratch,
                                                                    compoundmiddle))) ||  // twofold suffixes + compound
                 (rv = prefix_check(st, 0, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, scratch, compoundmiddle))))))
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
          rv = nullptr;
        }

        // check compoundend flag in suffix and prefix
        if ((rv) && !checked_prefix && compoundend && !hu_mov_rule &&
            ((pfx && pfx->getCont() &&
              TESTAFF(pfx->getCont(), compoundend, pfx->getContLen())) ||
             (sfx && sfx->getCont() &&
              TESTAFF(sfx->getCont(), compoundend, sfx->getContLen())))) {
          rv = nullptr;
        }

        // check compoundmiddle flag in suffix and prefix
        if ((rv) && !checked_prefix && (wordnum == 0) && compoundmiddle &&
            !hu_mov_rule &&
            ((pfx && pfx->getCont() &&
              TESTAFF(pfx->getCont(), compoundmiddle, pfx->getContLen())) ||
             (sfx && sfx->getCont() &&
              TESTAFF(sfx->getCont(), compoundmiddle, sfx->getContLen())))) {
          rv = nullptr;
        }

        // check forbiddenwords
        if ((rv) && (rv->astr) &&
            (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
             TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
             (is_sug && nosuggest && TESTAFF(rv->astr, nosuggest, rv->alen)))) {
          return nullptr;
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
                 join_side_has_flag(rv, checkcpdtable[scpd - 1].cond, pfx, sfx)) &&
             !((checkcompoundtriple && scpd == 0 &&
                !words && i < word.size() && // test triple letters
                (word[i - 1] == word[i]) &&
                (((i > 1) && (word[i - 1] == word[i - 2])) ||
                 ((word[i - 1] == word[i + 1]))  // may be word[i+1] == '\0'
                 )) ||
               (checkcompoundcase && scpd == 0 && !words && i < word.size() &&
                cpdcase_check(word, i))))
            // LANG_hu section: spec. Hungarian rule
            || ((!rv) && (langnum == LANG_hu) && hu_mov_rule &&
                (rv = affix_check(st, 0, i, scratch)) &&
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
          // the affixes that built the first word, so its continuation classes stay reachable once
          // the second word has overwritten pfx and sfx
          PfxEntry* rv_first_pfx = pfx;
          SfxEntry* rv_first_sfx = sfx;
          st[i] = ch;

          do {  // striple loop

            // check simplifiedtriple
            if (simplifiedtriple) {
              if (striple) {
                checkedstriple = 1;
                i--;  // check "fahrt" instead of "ahrt" in "Schiffahrt"
              } else if (i > 2 && i <= word.size() && word[i - 1] == word[i - 2]) {
                striple = 1;
                // past cmax the surface split is too short, so only the form with the letter
                // put back is tried
                if (i >= cmax) {
                  checkedstriple = 1;
                  i--;
                }
              }
            }

            rv = lookup(st.c_str() + i, st.size() - i);  // perhaps without prefix
            if (t) {
              if (rv)
                trace(*t, "second \"%s\" -> entry \"%s\" flags=%s", st.c_str() + i,
                      rv->word, trace_flags(this, rv->astr, rv->alen).c_str());
              else
                trace(*t, "second \"%s\" -> miss", st.c_str() + i);
            }

            // search homonym with compound flag
            while ((rv) && ((needaffix && TESTAFF(rv->astr, needaffix, rv->alen)) ||
                            !((compoundflag && !words && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                              (compoundend && !words && TESTAFF(rv->astr, compoundend, rv->alen)) ||
                              (!defcpdtable.empty() && words && defcpd_check(&words, wnum + 1, maxwordnum, rv, nullptr, 1))) ||
                            (scpd != 0 && checkcpdtable[scpd - 1].cond2 != FLAG_NULL &&
                             !TESTAFF(rv->astr, checkcpdtable[scpd - 1].cond2, rv->alen)))) {
              rv = rv->next_homonym;
            }

            // check FORCEUCASE
            if (rv && forceucase &&
                (TESTAFF(rv->astr, forceucase, rv->alen)) &&
                !(info && *info & SPELL_ORIGCAP))
              rv = nullptr;

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
              return nullptr;

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
                    (i < word.size() && !cpdpat_check(word, i, rv_first, rv, 0, t, rv_first_pfx, rv_first_sfx, nullptr, nullptr))) &&
                ((!checkcompounddup || (rv != rv_first)))
                // test CHECKCOMPOUNDPATTERN conditions
                &&
                (scpd == 0 || checkcpdtable[scpd - 1].cond2 == FLAG_NULL ||
                 TESTAFF(rv->astr, checkcpdtable[scpd - 1].cond2, rv->alen))) {
              // forbid compound word, if it is a non-compound word with typical
              // fault
              if ((checkcompoundrep && cpdrep_check(word, len, scratch, timelimit_exceeded, clock_time_start)) ||
                  cpdwordpair_check(word, len, scratch, timelimit_exceeded, clock_time_start))
                return nullptr;
              return rv_first;
            }

            numsyllable = oldnumsyllable2;
            wordnum = oldwordnum2;

            // perhaps second word has prefix or/and suffix
            sfx = nullptr;
            sfxflag = FLAG_NULL;
            // the affixes that built the second word, kept because affix_check clears the members
            PfxEntry* rv_second_pfx = nullptr;
            SfxEntry* rv_second_sfx = nullptr;
            rv = (compoundflag && !onlycpdrule && i < word.size()) ? affix_check(word, i, word.size() - i, scratch, compoundflag, IN_CPD_END, FLAG_NULL, &rv_second_pfx, &rv_second_sfx)
                                                                   : nullptr;
            if (!rv && compoundend && !onlycpdrule) {
              sfx = nullptr;
              pfx = nullptr;
              if (i < word.size())
                rv = affix_check(word, i, word.size() - i, scratch, compoundend, IN_CPD_END, FLAG_NULL, &rv_second_pfx, &rv_second_sfx);
            }

            if (!rv && !defcpdtable.empty() && words) {
              if (i < word.size())
                rv = affix_check(word, i, word.size() - i, scratch, 0, IN_CPD_END);
              if (rv && defcpd_check(&words, wnum + 1, maxwordnum, rv, nullptr, 1))
                return rv_first;
              rv = nullptr;
            }

            // test CHECKCOMPOUNDPATTERN conditions (allowed forms)
            if (rv &&
                !(scpd == 0 || checkcpdtable[scpd - 1].cond2 == FLAG_NULL ||
                  join_side_has_flag(rv, checkcpdtable[scpd - 1].cond2, rv_second_pfx,
                                     rv_second_sfx)))
              rv = nullptr;

            // test CHECKCOMPOUNDPATTERN conditions (forbidden compounds)
            if (rv && !checkcpdtable.empty() && scpd == 0 &&
                cpdpat_check(word, i, rv_first, rv, affixed, t, rv_first_pfx, rv_first_sfx, rv_second_pfx, rv_second_sfx))
              rv = nullptr;

            // check non_compound flag in suffix and prefix
            if ((rv) && ((pfx && pfx->getCont() &&
                          TESTAFF(pfx->getCont(), compoundforbidflag,
                                  pfx->getContLen())) ||
                         (sfx && sfx->getCont() &&
                          TESTAFF(sfx->getCont(), compoundforbidflag,
                                  sfx->getContLen())))) {
              rv = nullptr;
            }

            // check FORCEUCASE
            if (rv && forceucase &&
                (TESTAFF(rv->astr, forceucase, rv->alen)) &&
                !(info && *info & SPELL_ORIGCAP))
              rv = nullptr;

            // check forbiddenwords
            if ((rv) && (rv->astr) &&
                (TESTAFF(rv->astr, forbiddenword, rv->alen) ||
                 TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
                 (is_sug && nosuggest &&
                  TESTAFF(rv->astr, nosuggest, rv->alen))))
              return nullptr;

            // pfxappnd = prefix of word+i, or NULL
            // calculate syllable number of prefix.
            // hungarian convention: when syllable number of prefix is more,
            // than 1, the prefix+word counts as two words.

            if (langnum == LANG_hu) {
              if (i < word.size()) {
                // calculate syllable number of the word
                numsyllable += get_syllable(word.substr(i));
              }

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
              if ((checkcompoundrep && cpdrep_check(word, len, scratch, timelimit_exceeded, clock_time_start)) ||
                  cpdwordpair_check(word, len, scratch, timelimit_exceeded, clock_time_start))
                return nullptr;
              return rv_first;
            }

            numsyllable = oldnumsyllable2;
            wordnum = oldwordnum2;

            // perhaps second word is a compound word (recursive call)
            // (only if SPELL_COMPOUND_2 is not set and maxwordnum is not exceeded)
            if ((!info || !(*info & SPELL_COMPOUND_2)) && wordnum + 2 < maxwordnum && wnum + 1 < maxwordnum) {
              rv = compound_check(st.substr(i), wordnum + 1,
                                  numsyllable, maxwordnum, wnum + 1, words, rwords, 0,
                                  is_sug, info, scratch);

              if (rv && !checkcpdtable.empty() && i < word.size() &&
                  ((scpd == 0 &&
                    cpdpat_check(word, i, rv_first, rv, affixed, t, rv_first_pfx, rv_first_sfx, nullptr, nullptr)) ||
                   (scpd != 0 &&
                    !cpdpat_check(word, i, rv_first, rv, affixed, t, rv_first_pfx, rv_first_sfx, nullptr, nullptr))))
                rv = nullptr;
            } else {
              rv = nullptr;
            }
            if (rv) {
              // forbid compound word, if it is a non-compound word with typical
              // fault, or a dictionary word pair

              if (cpdwordpair_check(word, len, scratch, timelimit_exceeded, clock_time_start))
                return nullptr;

              if (checkcompoundrep || forbiddenword) {
                if (checkcompoundrep && cpdrep_check(word, len, scratch, timelimit_exceeded, clock_time_start))
                  return nullptr;

                // check first part
                if (i < word.size() && word.compare(i, rv->blen, rv->word, rv->blen) == 0) {
                  char r = st[i + rv->blen];
                  st[i + rv->blen] = '\0';

                  if ((checkcompoundrep && cpdrep_check(st, i + rv->blen, scratch, timelimit_exceeded, clock_time_start)) ||
                      cpdwordpair_check(st, i + rv->blen, scratch, timelimit_exceeded, clock_time_start)) {
                    st[ + i + rv->blen] = r;
                    continue;
                  }

                  if (forbiddenword) {
                    struct hentry* rv2 = lookup(word.c_str(), word.size());
                    if (!rv2 && len <= word.size())
                      rv2 = affix_check(word, 0, len, scratch);
                    if (rv2 && rv2->astr &&
                        TESTAFF(rv2->astr, forbiddenword, rv2->alen) &&
                        (strncmp(rv2->word, st.c_str(), i + rv->blen) == 0)) {
                      return nullptr;
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
        len = oldlen;
        cmin = oldcmin;
        cmax = oldcmax;
      } else
        st[i] = ch;

    } while (!defcpdtable.empty() && oldwordnum == 0 &&
             onlycpdrule++ < 1);  // end of onlycpd loop
  }

  return nullptr;
}

// check if compound word is correctly spelled
// hu_mov_rule = spec. Hungarian rule (XXX)
int AffixMgr::compound_check_morph(const std::string& word,
                                   short wordnum,
                                   short numsyllable,
                                   short maxwordnum,
                                   short wnum,
                                   hentry** words,
                                   hentry** rwords,
                                   char hu_mov_rule,
                                   std::string& result,
                                   const std::string* partresult,
                                   AffixScratch& scratch) {
  short oldnumsyllable, oldnumsyllable2, oldwordnum, oldwordnum2;
  hentry *rv = nullptr, *rv_first;
  std::string st, presult;
  char ch, affixed = 0;
  int checked_prefix, ok = 0;
  size_t cmin, cmax;
  hentry** oldwords = words;
  size_t len = word.size();

  // protect subsequent words[wnum + 1] reads and any recursion
  if (wnum + 1 >= maxwordnum)
    return 0;

  // add a time limit to handle possible
  // combinatorical explosion of the overlapping words

  HUNSPELL_THREAD_LOCAL std::chrono::steady_clock::time_point clock_time_start;
  HUNSPELL_THREAD_LOCAL bool timelimit_exceeded;

  // get the current time
  std::chrono::steady_clock::time_point clock_now = std::chrono::steady_clock::now();

  if (wnum == 0) {
      // set the start time
      clock_time_start = clock_now;
      timelimit_exceeded = false;
  }
  else if (clock_now - clock_time_start > TIMELIMIT_MS)
      timelimit_exceeded = true;

  setcminmax(&cmin, &cmax, word.c_str(), len);

  st.assign(word);

  for (size_t i = cmin; i < cmax; ++i) {
    // go to end of the UTF-8 character
    if (utf8) {
      for (; is_utf8_cont(st[i]); i++)
        ;
      if (i >= cmax)
        return 0;
    }

    words = oldwords;
    int onlycpdrule = (words) ? 1 : 0;

    do {  // onlycpdrule loop

      if (timelimit_exceeded ||
          std::chrono::steady_clock::now() - clock_time_start > TIMELIMIT_MS) {
        timelimit_exceeded = true;
        return 0;
      }

      if (result.size() > MAXMORPHRESULT)
        return 0;

      oldnumsyllable = numsyllable;
      oldwordnum = wordnum;
      checked_prefix = 0;

      if (i >= st.size())
        return 0;

      ch = st[i];
      st[i] = '\0';
      sfx = nullptr;

      // FIRST WORD

      affixed = 1;

      presult.clear();
      if (partresult)
        presult.append(*partresult);

      rv = lookup(st.c_str(), i);  // perhaps without prefix

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
                   defcpd_check(&words, wnum, maxwordnum, rv, rwords, 0)) ||
                  (words &&
                   defcpd_check(&words, wnum, maxwordnum, rv, rwords, 0))))))) {
        rv = rv->next_homonym;
      }


      if (rv)
        affixed = 0;

      if (rv) {
        presult.push_back(MSEP_FLD);
        presult.append(MORPH_PART);
        presult.append(st, 0, i);
        if (!HENTRY_FIND(rv, MORPH_STEM)) {
          presult.push_back(MSEP_FLD);
          presult.append(MORPH_STEM);
          presult.append(st, 0, i);
        }
        if (HENTRY_DATA(rv)) {
          presult.push_back(MSEP_FLD);
          presult.append(HENTRY_DATA2(rv));
        }
      }

      if (!rv) {
        if (compoundflag &&
            !(rv =
                  prefix_check(st, 0, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN,
                               scratch, compoundflag))) {
          if (((rv = suffix_check(st, 0, i, 0, nullptr, scratch, FLAG_NULL, compoundflag, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
               (compoundmoresuffixes && (rv = suffix_check_twosfx(st, 0, i, 0, nullptr, scratch, compoundflag)))) &&
              !hu_mov_rule && sfx->getCont() &&
              ((compoundforbidflag && TESTAFF(sfx->getCont(), compoundforbidflag, sfx->getContLen())) ||
               (compoundend && TESTAFF(sfx->getCont(), compoundend, sfx->getContLen())))) {
            rv = nullptr;
          }
        }

        if (rv ||
            (((wordnum == 0) && compoundbegin &&
              ((rv = suffix_check(st, 0, i, 0, nullptr, scratch, FLAG_NULL, compoundbegin, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
               (compoundmoresuffixes && (rv = suffix_check_twosfx(st, 0, i, 0, nullptr, scratch,
                                                                  compoundbegin))) ||  // twofold suffix+compound
               (rv = prefix_check(st, 0, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, scratch, compoundbegin)))) ||
             ((wordnum > 0) && compoundmiddle &&
              ((rv = suffix_check(st, 0, i, 0, nullptr, scratch, FLAG_NULL, compoundmiddle, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN)) ||
               (compoundmoresuffixes && (rv = suffix_check_twosfx(st, 0, i, 0, nullptr, scratch,
                                                                  compoundmiddle))) ||  // twofold suffix+compound
               (rv = prefix_check(st, 0, i, hu_mov_rule ? IN_CPD_OTHER : IN_CPD_BEGIN, scratch, compoundmiddle)))))) {
          std::string p;
          if (compoundflag)
            p = affix_check_morph(st, 0, i, scratch, compoundflag);
          if (p.empty()) {
            if ((wordnum == 0) && compoundbegin) {
              p = affix_check_morph(st, 0, i, scratch, compoundbegin);
            } else if ((wordnum > 0) && compoundmiddle) {
              p = affix_check_morph(st, 0, i, scratch, compoundmiddle);
            }
          }
          presult.push_back(MSEP_FLD);
          presult.append(MORPH_PART);
          presult.append(st, 0, i);
          if (!p.empty()) {
            line_uniq_app(p, MSEP_REC);
            if (!p.empty() && p[0] != MSEP_FLD)
              presult.push_back(MSEP_FLD);
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
        rv = nullptr;
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
           (checked_prefix || (words && words[wnum]) || (compoundflag && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
            ((oldwordnum == 0) && compoundbegin && TESTAFF(rv->astr, compoundbegin, rv->alen)) ||
            ((oldwordnum > 0) && compoundmiddle && TESTAFF(rv->astr, compoundmiddle, rv->alen))
            // LANG_hu section: spec. Hungarian rule
            || ((langnum == LANG_hu) &&  // hu_mov_rule
                hu_mov_rule &&
                (TESTAFF(rv->astr, 'F', rv->alen) || TESTAFF(rv->astr, 'G', rv->alen) || TESTAFF(rv->astr, 'H', rv->alen)))
            // END of LANG_hu section
            ) &&
           !((checkcompoundtriple && !words &&  // test triple letters
              (word[i - 1] == word[i]) &&
              (((i > 1) && (word[i - 1] == word[i - 2])) || ((word[i - 1] == word[i + 1]))  // may be word[i+1] == '\0'
               )) ||
             (
                 // test CHECKCOMPOUNDPATTERN
                 !checkcpdtable.empty() && !words && cpdpat_check(word, i, rv, nullptr, affixed, nullptr, nullptr, nullptr, nullptr, nullptr)) ||
             (checkcompoundcase && !words && cpdcase_check(word, i))))
          // LANG_hu section: spec. Hungarian rule
          || ((!rv) && (langnum == LANG_hu) && hu_mov_rule && (rv = affix_check(st, 0, i, scratch)) &&
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
        rv = lookup(word.c_str() + i, word.size() - i);  // perhaps without prefix

        // search homonym with compound flag
        while ((rv) && ((needaffix && TESTAFF(rv->astr, needaffix, rv->alen)) ||
                        !((compoundflag && !words && TESTAFF(rv->astr, compoundflag, rv->alen)) ||
                          (compoundend && !words && TESTAFF(rv->astr, compoundend, rv->alen)) ||
                          (!defcpdtable.empty() && words && defcpd_check(&words, wnum + 1, maxwordnum, rv, nullptr, 1))))) {
          rv = rv->next_homonym;
        }

        if (rv && words && words[wnum + 1]) {
          result.append(presult);
          result.push_back(MSEP_FLD);
          result.append(MORPH_PART);
          result.append(word, i, word.size());
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
          result.append(word, i, word.size());

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
        sfx = nullptr;
        sfxflag = FLAG_NULL;

        if (compoundflag && !onlycpdrule)
          rv = affix_check(word, i, word.size() - i, scratch, compoundflag);
        else
          rv = nullptr;

        if (!rv && compoundend && !onlycpdrule) {
          sfx = nullptr;
          pfx = nullptr;
          rv = affix_check(word, i, word.size() - i, scratch, compoundend, IN_CPD_END);
        }

        if (!rv && !defcpdtable.empty() && words) {
          rv = affix_check(word, i, word.size() - i, scratch, 0, IN_CPD_END);
          if (rv && words && defcpd_check(&words, wnum + 1, maxwordnum, rv, nullptr, 1)) {
            std::string m;
            if (compoundflag)
              m = affix_check_morph(word, i, word.size() - i, scratch, compoundflag);
            if (m.empty() && compoundend) {
              m = affix_check_morph(word, i, word.size() - i, scratch, compoundend);
            }
            result.append(presult);
            if (!m.empty()) {
              result.push_back(MSEP_FLD);
              result.append(MORPH_PART);
              result.append(word, i, word.size());
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
          rv = nullptr;
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
          std::string m;
          if (compoundflag)
            m = affix_check_morph(word, i, word.size() - i, scratch, compoundflag, IN_CPD_END);
          if (m.empty() && compoundend) {
            m = affix_check_morph(word, i, word.size() - i, scratch, compoundend, IN_CPD_END);
          }
          result.append(presult);
          if (!m.empty()) {
            result.push_back(MSEP_FLD);
            result.append(MORPH_PART);
            result.append(word, i, word.size());
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
        if ((wordnum + 2 < maxwordnum) && (wnum + 1 < maxwordnum) && (ok == 0)) {
          compound_check_morph(word.substr(i), wordnum + 1,
                               numsyllable, maxwordnum, wnum + 1, words, rwords, 0,
                               result, &presult, scratch);
        } else {
          rv = nullptr;
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

// a circumfix is one affix split in two halves, so the flag has to be on both the prefix and the
// suffix, or on neither of them
bool AffixMgr::circumfix_ok(PfxEntry* pfx, SfxEntry* sfx, const TraceCtx* t) const {
  if (!circumfix)
    return true;
  bool in_prefix = pfx && pfx->getCont() &&
                   TESTAFF(pfx->getCont(), circumfix, pfx->getContLen());
  bool in_suffix = sfx->getCont() &&
                   TESTAFF(sfx->getCont(), circumfix, sfx->getContLen());
  if (t)
    trace_circumfix(*t, this, circumfix, pfx, sfx, in_prefix, in_suffix);
  return in_prefix == in_suffix;
}

// decide whether a suffix entry may be applied at all, before the suffix itself is matched
// against the word
// the stem was found and then dropped again for carrying the flag the caller asked to keep away
// from
void AffixMgr::trace_avoidflag(TraceCtx* t,
                               const FLAG avoidflag,
                               const struct hentry* stem) const {
  if (!t)
    return;
  TraceScope trace_depth(t);
  trace_test(*t, "avoidflag", this, avoidflag, "dic", stem->astr, stem->alen,
             "fail, the caller is skipping stems with this flag");
}

bool AffixMgr::suffix_applicable(PfxEntry* pfx,
                                 SfxEntry* sfx,
                                 const FLAG cclass,
                                 char in_compound,
                                 const TraceCtx* t) const {
  // suffixes are only allowed at the beginning of a compound when they are signed with the
  // compoundpermitflag flag
  if (in_compound == IN_CPD_BEGIN &&
      !(sfx->getCont() && compoundpermitflag &&
        TESTAFF(sfx->getCont(), compoundpermitflag, sfx->getContLen()))) {
    if (t)
      trace_test(*t, "compoundpermit", this, compoundpermitflag, "sfx-cont",
                 sfx->getCont(), sfx->getContLen(),
                 "fail, a suffix at the start of a compound needs this flag");
    return false;
  }

  if (!circumfix_ok(pfx, sfx, t))
    return false;

  // a fogemorpheme is only allowed inside a compound
  if (!in_compound && sfx->getCont() &&
      TESTAFF(sfx->getCont(), onlyincompound, sfx->getContLen())) {
    if (t)
      trace_test(*t, "onlyincompound", this, onlyincompound, "sfx-cont",
                 sfx->getCont(), sfx->getContLen(),
                 "fail, this suffix is only allowed inside a compound");
    return false;
  }

  // a needaffix suffix needs a further affix, so it is allowed either as a second suffix or
  // when a prefix is present that does not itself need one
  if (!cclass && sfx->getCont() &&
      TESTAFF(sfx->getCont(), needaffix, sfx->getContLen()) &&
      !(pfx && !(pfx->getCont() &&
                 TESTAFF(pfx->getCont(), needaffix, pfx->getContLen())))) {
    if (t)
      trace_test(*t, "needaffix", this, needaffix, "sfx-cont", sfx->getCont(),
                 sfx->getContLen(),
                 "fail, this suffix needs a further affix and has none");
    return false;
  }

  return true;
}

// check word for suffixes
struct hentry* AffixMgr::suffix_check(const std::string& word,
                                      int start,
                                      int len,
                                      int sfxopts,
                                      PfxEntry* ppfx,
                                      AffixScratch& scratch,
                                      const FLAG cclass,
                                      const FLAG needflag,
                                      char in_compound,
                                      const FLAG avoidflag) {
  struct hentry* rv = nullptr;

  TraceCtx* t = trace_on(scratch.trace);
  int candidates = 0;
  // a pass that had nothing to try says so, which is different from a pass that
  // did not run
  auto report_empty_pass = [t, &word, start, len, sfxopts, &candidates]() {
    if (t && candidates == 0)
      trace(*t, "sfx \"%s\" candidates=0%s", word.substr(start, len).c_str(),
            (sfxopts & aeXPRODUCT) != 0 ? " xprod=Y" : "");
  };
  // a rule the suffix conditions turned away never reaches checkword, so name
  // the rule here and run the conditions again to say which of them refused it
  auto report_refused = [this, t, ppfx, cclass, in_compound,
                         &candidates](SfxEntry* se) {
    if (!t)
      return;
    ++candidates;
    trace_affix(*t, "sfx", this, *se);
    TraceScope trace_depth(t);
    suffix_applicable(ppfx, se, cclass, in_compound, t);
  };

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];

  while (se) {
    if (!cclass || se->getCont()) {
      if (suffix_applicable(ppfx, se, cclass, in_compound, nullptr)) {
        ++candidates;
        rv = se->checkword(word, start, len, sfxopts, ppfx,
                           (FLAG)cclass, needflag,
                           (in_compound ? 0 : onlyincompound),
                           scratch);
        // Skip a stem with the avoid flag and keep scanning the other suffixes.
        if (rv && avoidflag != FLAG_NULL && TESTAFF(rv->astr, avoidflag, rv->alen)) {
          trace_avoidflag(t, avoidflag, rv);
          rv = nullptr;
        }
        if (rv) {
          sfx = se;  // BUG: sfx not stateless
          return rv;
        }
      } else {
        report_refused(se);
      }
    }
    se = se->getNext();
  }

  // now handle the general case
  if (len == 0) {
    report_empty_pass();
    return nullptr;  // FULLSTRIP
  }
  unsigned char sp = word[start + len - 1];
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word.c_str() + start + len - 1, len)) {
      if (!suffix_applicable(ppfx, sptr, cclass, in_compound, nullptr))
        report_refused(sptr);
      else if (in_compound != IN_CPD_END || ppfx ||
               !(sptr->getCont() &&
                 TESTAFF(sptr->getCont(), onlyincompound, sptr->getContLen()))) {
        ++candidates;
        rv = sptr->checkword(word, start, len, sfxopts, ppfx,
                             cclass, needflag,
                             (in_compound ? 0 : onlyincompound),
                             scratch);
        if (rv && avoidflag != FLAG_NULL && TESTAFF(rv->astr, avoidflag, rv->alen)) {
          trace_avoidflag(t, avoidflag, rv);
          rv = nullptr;
        }
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

  report_empty_pass();

  return nullptr;
}

// check word for two-level suffixes
struct hentry* AffixMgr::suffix_check_twosfx(const std::string& word,
                                             int start,
                                             int len,
                                             int sfxopts,
                                             PfxEntry* ppfx,
                                             AffixScratch& scratch,
                                             const FLAG needflag) {
  struct hentry* rv = nullptr;

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];
  while (se) {
    if (contclasses[se->getFlag()]) {
      rv = se->check_twosfx(word, start, len, sfxopts, ppfx, needflag, scratch);
      if (rv)
        return rv;
    }
    se = se->getNext();
  }

  // now handle the general case
  if (len == 0)
    return nullptr;  // FULLSTRIP
  unsigned char sp = word[start + len - 1];
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word.c_str() + start + len - 1, len)) {
      if (contclasses[sptr->getFlag()]) {
        rv = sptr->check_twosfx(word, start, len, sfxopts, ppfx, needflag, scratch);
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

  return nullptr;
}

// check word for two-level suffixes and morph
std::string AffixMgr::suffix_check_twosfx_morph(const std::string& word,
                                                int start,
                                                int len,
                                                int sfxopts,
                                                PfxEntry* ppfx,
                                                AffixScratch& scratch,
                                                const FLAG needflag) {
  std::string result2;
  std::string result3;
  // the second suffix runs the whole suffix table again for every first suffix
  DistinctRecords result(MSEP_REC);

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];
  while (se) {
    if (contclasses[se->getFlag()]) {
      std::string st = se->check_twosfx_morph(word, start, len, sfxopts, ppfx, needflag, scratch);
      if (!st.empty()) {
        std::string analysis;
        if (ppfx) {
          if (ppfx->getMorph()) {
            analysis.append(ppfx->getMorph());
            analysis.push_back(MSEP_FLD);
          } else
            debugflag(analysis, ppfx->getFlag());
        }
        analysis.append(st);
        if (se->getMorph()) {
          analysis.push_back(MSEP_FLD);
          analysis.append(se->getMorph());
        } else
          debugflag(analysis, se->getFlag());
        result.append(analysis);
      }
    }
    se = se->getNext();
  }

  // now handle the general case
  if (len == 0)
    return { };  // FULLSTRIP
  unsigned char sp = word[start + len - 1];
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word.c_str() + start + len - 1, len)) {
      if (contclasses[sptr->getFlag()]) {
        std::string st = sptr->check_twosfx_morph(word, start, len, sfxopts, ppfx, needflag, scratch);
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
          result.append(result2);
        }
      }
      sptr = sptr->getNextEQ();
    } else {
      sptr = sptr->getNextNE();
    }
  }

  return result.join();
}

std::string AffixMgr::suffix_check_morph(const std::string& word,
                                         int start,
                                         int len,
                                         int sfxopts,
                                         PfxEntry* ppfx,
                                         AffixScratch& scratch,
                                         const FLAG cclass,
                                         const FLAG needflag,
                                         char in_compound) {
  std::string result;

  struct hentry* rv = nullptr;

  // first handle the special case of 0 length suffixes
  SfxEntry* se = sStart[0];
  while (se) {
    if (!cclass || se->getCont()) {
      if (suffix_applicable(ppfx, se, cclass, in_compound, nullptr))
        rv = se->checkword(word, start, len, sfxopts, ppfx, cclass,
                           needflag, FLAG_NULL, scratch);
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
    return { };  // FULLSTRIP
  unsigned char sp = word[start + len - 1];
  SfxEntry* sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->getKey(), word.c_str() + start + len - 1, len)) {
      if (suffix_applicable(ppfx, sptr, cclass, in_compound, nullptr))
        rv = sptr->checkword(word, start, len, sfxopts, ppfx, cclass,
                             needflag, FLAG_NULL, scratch);
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
struct hentry* AffixMgr::affix_check(const std::string& word,
                                     int start,
                                     int len,
                                     AffixScratch& scratch,
                                     const FLAG needflag,
                                     char in_compound,
                                     const FLAG avoidflag,
                                     PfxEntry** found_pfx,
                                     SfxEntry** found_sfx) {

  TraceCtx* t = trace_on(scratch.trace);
  // the affixes that built the word are reported before the members holding them are cleared
  auto report_form = [this, t, &word, start, len, found_pfx, found_sfx](const struct hentry* rv) {
    if (!rv)
      return;
    if (t)
      trace_form(*t, this, word.substr(start, len), rv->word, pfx, sfx);
    if (found_pfx)
      *found_pfx = pfx;
    if (found_sfx)
      *found_sfx = sfx;
  };

  // check all prefixes (also crossed with suffixes if allowed)
  struct hentry* rv = prefix_check(word, start, len, in_compound, scratch, needflag, avoidflag);
  if (rv) {
    report_form(rv);
    return rv;
  }

  // if still not found check all suffixes
  rv = suffix_check(word, start, len, 0, nullptr, scratch, FLAG_NULL, needflag, in_compound, avoidflag);

  if (havecontclass) {
    if (rv)
      report_form(rv);

    sfx = nullptr;
    pfx = nullptr;

    if (rv)
      return rv;
    // if still not found check all two-level suffixes
    rv = suffix_check_twosfx(word, start, len, 0, nullptr, scratch, needflag);

    if (rv) {
      report_form(rv);
      return rv;
    }
    // if still not found check all two-level suffixes
    rv = prefix_check_twosfx(word, start, len, IN_CPD_NOT, scratch, needflag);
  }

  report_form(rv);

  return rv;
}

// check if word with affixes is correctly spelled
std::string AffixMgr::affix_check_morph(const std::string& word,
                                  int start,
                                  int len,
                                  AffixScratch& scratch,
                                  const FLAG needflag,
                                  char in_compound) {
  // the four checks below overlap
  DistinctRecords result(MSEP_REC);

  // check all prefixes (also crossed with suffixes if allowed)
  std::string st = prefix_check_morph(word, start, len, in_compound, scratch);
  if (!st.empty()) {
    result.append(st);
  }

  // if still not found check all suffixes
  st = suffix_check_morph(word, start, len, 0, nullptr, scratch, '\0', needflag, in_compound);
  if (!st.empty()) {
    result.append(st);
  }

  if (havecontclass) {
    sfx = nullptr;
    pfx = nullptr;
    // if still not found check all two-level suffixes
    st = suffix_check_twosfx_morph(word, start, len, 0, nullptr, scratch, needflag);
    if (!st.empty()) {
      result.append(st);
    }

    // if still not found check all two-level suffixes
    st = prefix_check_twosfx_morph(word, start, len, IN_CPD_NOT, scratch, needflag);
    if (!st.empty()) {
      result.append(st);
    }
  }

  return result.join();
}

// morphcmp(): compare MORPH_DERI_SFX, MORPH_INFL_SFX and MORPH_TERM_SFX fields
// in the first line of the inputs
// return 0, if inputs equal
// return 1, if inputs may equal with a secondary suffix
// otherwise return -1
static int morphcmp(const char* s, const char* t) {
  bool se = false, te = false;
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
    olds = nullptr;
  }
  oldt = t;
  tl = strchr(t, '\n');
  t = strstr(t, MORPH_DERI_SFX);
  if (!t || (tl && tl < t))
    t = strstr(oldt, MORPH_INFL_SFX);
  if (!t || (tl && tl < t))
    t = strstr(oldt, MORPH_TERM_SFX);
  while (s && t && (!sl || sl > s) && (!tl || tl > t)) {
    s += MORPH_TAG_LEN;
    t += MORPH_TAG_LEN;
    se = false;
    te = false;
    // both values empty: matched at the shared field terminator.
    // skip the inner loop, which would step past the '\0'.
    if (*s == '\0' && *t == '\0') {
      se = true;
      te = true;
    }
    while ((*s == *t) && !se && !te) {
      s++;
      t++;
      switch (*s) {
        case ' ':
        case '\n':
        case '\t':
        case '\0':
          se = true;
      }
      switch (*t) {
        case ' ':
        case '\n':
        case '\t':
        case '\0':
          te = true;
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
      olds = nullptr;
    }
    oldt = t;
    t = strstr(t, MORPH_DERI_SFX);
    if (!t || (tl && tl < t))
      t = strstr(oldt, MORPH_INFL_SFX);
    if (!t || (tl && tl < t))
      t = strstr(oldt, MORPH_TERM_SFX);
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
                         int level,
                         const FLAG avoidflag) {
  // handle suffixes
  if (!morph)
    return {};

  // check substandard flag
  if (TESTAFF(ap, substandard, al))
    return {};

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
    // A derivational suffix whose own continuation class re-enables the same
    // suffix would otherwise be applied twice here, doubling the ending (for
    // example the Hungarian -szeru adjective suffix producing peldaszeruszeru).
    // Skip the flag that was just applied one level up.
    if (avoidflag != FLAG_NULL && ap[i] == avoidflag)
      continue;
    const auto c = (unsigned char)(ap[i] & 0x00FF);
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
            hentry* check = pHMgr->lookup(newword.c_str(), newword.size());  // XXX extra dic
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
                         sptr->getContLen(), stemmorph, targetmorph, 1,
                         sptr->getFlag());

            if (!newword2.empty()) {
              return newword2;
            }
          }
        }
      }
      sptr = sptr->getFlgNxt();
    }
  }
  return { };
}

namespace {
  // replaces strdup with ansi version
  char* mystrdup(const char* s) {
    char* d = nullptr;
    if (s) {
      size_t sl = strlen(s) + 1;
      d = new char[sl];
      memcpy(d, s, sl);
    }
    return d;
  }
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
    wlst[nh].allow = false;
    wlst[nh].orig = nullptr;
    nh++;
    // add special phonetic version
    if (phon && (nh < maxn)) {
      wlst[nh].word = mystrdup(phon);
      wlst[nh].allow = false;
      wlst[nh].orig = mystrdup(ts);
      nh++;
    }
  }

  // handle suffixes
  for (int i = 0; i < al; i++) {
    const auto c = (unsigned char)(ap[i] & 0x00FF);
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
            wlst[nh].orig = nullptr;
            nh++;
            // add special phonetic version
            if (phon && (nh < maxn)) {
              std::string prefix(phon);
              std::string key(sptr->getKey());
              reverseword(key);
              prefix.append(key);
              wlst[nh].word = mystrdup(prefix.c_str());
              wlst[nh].allow = false;
              wlst[nh].orig = mystrdup(newword.c_str());
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
        const auto c = (unsigned char)(ap[k] & 0x00FF);
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
                wlst[nh].orig = nullptr;
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
    const auto c = (unsigned char)(ap[m] & 0x00FF);
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
            wlst[nh].orig = nullptr;
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
    return nullptr;
  return iconvtable;
}

// return oconv table
RepList* AffixMgr::get_oconvtable() const {
  if (!oconvtable)
    return nullptr;
  return oconvtable;
}

// return replacing table
struct phonetable* AffixMgr::get_phonetable() const {
  if (!phone)
    return nullptr;
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

std::string AffixMgr::encode_flag(unsigned short aflag) const {
  return pHMgr->encode_flag(aflag);
}

// return the preferred ignore string for suggestions
const char* AffixMgr::get_ignore() const {
  if (ignorechars.empty())
    return nullptr;
  return ignorechars.c_str();
}

// return the preferred ignore string for suggestions
const std::vector<w_char>& AffixMgr::get_ignore_utf16() const {
  return ignorechars_utf16;
}

// return the keyboard string for suggestions
const std::string& AffixMgr::get_key_string() {
  if (keystring.empty())
    keystring = SPELL_KEYSTRING;
  return keystring;
}

// return the preferred try string for suggestions
const std::string& AffixMgr::get_try_string() const {
  return trystring;
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

// return the circumfix flag
FLAG AffixMgr::get_circumfix() const {
  return circumfix;
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
struct hentry* AffixMgr::lookup(const char* word, size_t len) {
  struct hentry* he = nullptr;
  for (size_t i = 0; i < alldic.size() && !he; ++i) {
    he = alldic[i]->lookup(word, len);
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

int AffixMgr::get_maxngramsugs() const {
  return maxngramsugs;
}

int AffixMgr::get_maxcpdsugs() const {
  return maxcpdsugs;
}

int AffixMgr::get_maxdiff() const {
  return maxdiff;
}

int AffixMgr::get_onlymaxdiff() const {
  return onlymaxdiff;
}

// return nosplitsugs
int AffixMgr::get_nosplitsugs() const {
  return nosplitsugs;
}

// return sugswithdots
int AffixMgr::get_sugswithdots() const {
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
  *out = pHMgr->decode_flag(s);
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
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
              *rl = nullptr;
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
  std::unique_ptr<phonetable> new_phone;
  int num = -1;
  int i = 0;
  int np = 0;
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
        new_phone = std::make_unique<phonetable>();
        new_phone->utf8 = (char)utf8;
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
    const size_t old_size = new_phone->rules.size();
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
            new_phone->rules.emplace_back(start_piece, iter);
            break;
          }
          case 2: {
            new_phone->rules.emplace_back(start_piece, iter);
            mystrrep(new_phone->rules.back(), "_", "");
            break;
          }
          default:
            break;
        }
        ++i;
      }
      start_piece = mystrsep(nl, iter);
    }
    if (new_phone->rules.size() != old_size + 2) {
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      return false;
    }
  }
  new_phone->rules.emplace_back("");
  new_phone->rules.emplace_back("");
  init_phonet_hash(*new_phone);
  phone = new_phone.release();
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
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
        checkcpdtable.reserve(std::min(numcheckcpd, 16384));
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
    checkcpdtable.emplace_back();
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 20, "CHECKCOMPOUNDPATTERN", 20) != 0) {
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            checkcpdtable.clear();
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
            checkcpdtable.back().cond = pHMgr->decode_flag(chunk);
          }
          break;
        }
        case 2: {
          checkcpdtable.back().pattern2.assign(start_piece, iter);
          size_t slash_pos = checkcpdtable.back().pattern2.find('/');
          if (slash_pos != std::string::npos) {
            std::string chunk(checkcpdtable.back().pattern2, slash_pos + 1);
            checkcpdtable.back().pattern2.resize(slash_pos);
            checkcpdtable.back().cond2 = pHMgr->decode_flag(chunk);
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
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
        defcpdtable.reserve(std::min(numdefcpd, 16384));
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
    defcpdtable.emplace_back();
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
            for (auto k = start_piece; k != iter; ++k) {
              auto chb = k, che = k + 1;
              if (*k == '(') {
                auto parpos = std::find(k, iter, ')');
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
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
        maptable.reserve(std::min(nummap, 16384));
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
    maptable.emplace_back();
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
          for (auto k = start_piece; k != iter; ++k) {
            auto chb = k, che = k + 1;
            if (*k == '(') {
              auto parpos = std::find(k, iter, ')');
              if (parpos != iter) {
                chb = k + 1;
                che = parpos;
                k = parpos;
              }
            } else {
              if (utf8 && (*k & 0xc0) == 0xc0) {
                ++k;
                while (k != iter && is_utf8_cont(*k))
                    ++k;
                che = k;
                --k;
              }
            }
            if (chb == che) {
              HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                              af->getlinenum());
            }

            maptable.back().emplace_back(chb, che);
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
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
        breaktable.reserve(std::min(numbreak, 16384));
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
          breaktable.emplace_back(start_piece, iter);
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
  // iterate backwards; k wraps to npos (SIZE_MAX) when decremented past 0
  for (size_t k = piece.size() - 1; k != std::string::npos; --k) {
    switch (piece[k]) {
      case '[': {
        if (neg)
          piece[k + 1] = '[';
        else
          piece[k] = ']';
        break;
      }
      case ']': {
        piece[k] = '[';
        if (neg)
          piece[k + 1] = '^';
        neg = 0;
        break;
      }
      case '^': {
        if (piece[k + 1] == ']')
          neg = 1;
        else if (neg)
          piece[k + 1] = piece[k];
        break;
      }
      default: {
        if (neg)
          piece[k + 1] = piece[k];
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
    entries.reserve(std::min(numents, 16384));

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

  AffEntry* first_entry() { return entries.empty() ? nullptr : entries[0]; }

  ~entries_container() {
    for (auto& entry : entries) {
      delete entry;
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
  char xprod = 0;
  int headerline = af->getlinenum();
  entries_container affentries(at, this);

  int i = 0;

// checking lines with bad syntax
#ifdef DEBUG
  int basefieldnum = 0;
#endif

  // split affix header line into pieces

  int np = 0;
  auto iter = line.begin(), start_piece = mystrsep(line, iter);
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
        aflag = pHMgr->decode_flag(std::string(start_piece, iter));
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
        xprod = *start_piece;
        if (xprod == 'Y')
          ff = aeXPRODUCT;
        break;
      }

      // piece 4 - is number of affentries
      case 3: {
        np++;
        numents = atoi(std::string(start_piece, iter).c_str());
        if ((numents <= 0) || ((std::numeric_limits<size_t>::max() /
                                sizeof(AffEntry)) < static_cast<size_t>(numents))) {
          std::string err = pHMgr->encode_flag(aflag);
          HUNSPELL_WARNING(stderr, "error: line %d: affix %s: bad entry number\n",
                           af->getlinenum(), err.c_str());
          return false;
        }

        char opts = ff;
        if (utf8)
          opts |= aeUTF8;
        if (pHMgr->is_aliasf())
          opts |= aeALIASF;
        if (pHMgr->is_aliasm())
          opts |= aeALIASM;
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
    std::string err = pHMgr->encode_flag(aflag);
    HUNSPELL_WARNING(stderr, "error: line %d: affix %s: missing data\n",
                     af->getlinenum(), err.c_str());
    return false;
  }

  // now parse numents affentries for this affix
  AffEntry* entry = affentries.first_entry();
  for (int ent = 0; ent < numents; ++ent) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    int ruleline = af->getlinenum();

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
            entry = affentries.add_entry((char)(aeXPRODUCT | aeUTF8 | aeALIASF | aeALIASM));
          break;
        }

        // piece 2 - is affix char
        case 1: {
          np++;
          std::string chunk(start_piece, iter);
          if (pHMgr->decode_flag(chunk) != aflag) {
            std::string err = pHMgr->encode_flag(aflag);
            HUNSPELL_WARNING(stderr,
                             "error: line %d: affix %s is corrupt\n",
                             af->getlinenum(), err.c_str());
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
          entry->morphcode = nullptr;
          entry->contclass = nullptr;
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
                  &(entry->contclass), dash_str, af);
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
              redundant_condition(at, entry->strip, chunk,
                                  af->getlinenum())) {
            chunk = ".";
            entry->opts |= aeREDUNDANTCOND;
          }
          if (at == 'S') {
            reverseword(chunk);
            reverse_condition(chunk);
          }
          if (encodeit(*entry, chunk))
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
      std::string err = pHMgr->encode_flag(aflag);
      HUNSPELL_WARNING(stderr, "error: line %d: affix %s is corrupt\n",
                       af->getlinenum(), err.c_str());
      return false;
    }

    entry->line = ruleline;
    entry->headerline = headerline;
    entry->xprod = xprod;

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
  auto start = affentries.begin(), end = affentries.end();
  for (auto affentry = start; affentry != end; ++affentry) {
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
                                  const std::string& strip,
                                  const std::string& cond,
                                  int linenum) {
  int stripl = strip.size(), condl = cond.size(), i, j, neg, in;
  if (ft == 'P') {  // prefix
    if (strip.compare(0, condl, cond) == 0)
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
                             linenum, cond.c_str());
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
    if ((stripl >= condl) && strip.compare(stripl - condl, std::string::npos, cond) == 0)
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
        } else if (j > 0) {
          in = 0;
          do {
            j--;
            if (strip[i] == cond[j])
              in = 1;
          } while ((j > 0) && (cond[j] != '['));
          if ((j == 0) && (cond[j] != '[')) {
            HUNSPELL_WARNING(stderr,
                             "error: line: %d: missing ] in condition:\n%s\n",
                             linenum, cond.c_str());
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
                               const std::string& root_word) {
  std::vector<std::string> slst;
  AffixScratch scratch;
  short unsigned* start_ptr = suff;
  for (auto ptr : sStart) {
    while (ptr) {
      suff = start_ptr;
      for (int i = 0; i < len; i++) {
        if ((*suff) == ptr->getFlag()) {
          std::string nw(root_word);
          nw.append(ptr->getAffix());
          hentry* ht = ptr->checkword(nw, 0, nw.size(), 0, nullptr, 0, 0, 0, scratch);
          if (ht) {
            slst.push_back(std::move(nw));
          }
        }
        suff++;
      }
      ptr = ptr->getNext();
    }
  }
  return slst;
}
