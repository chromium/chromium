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

#include "suggestmgr.hxx"
#include "htypes.hxx"
#include "csutil.hxx"

const w_char W_VLINE = {'\0', '|'};

#define MAX_CHAR_DISTANCE 4

#ifdef HUNSPELL_CHROME_CLIENT
namespace {
// A simple class which creates temporary hentry objects which are available
// only in a scope. To conceal memory operations from SuggestMgr functions,
// this object automatically deletes all hentry objects created through
// CreateScopedHashEntry() calls in its destructor. So, the following snippet
// raises a memory error.
//
//   hentry* bad_copy = NULL;
//   {
//     ScopedHashEntryFactory factory;
//     hentry* scoped_copy = factory.CreateScopedHashEntry(0, source);
//     ...
//     bad_copy = scoped_copy;
//   }
//   if (bad_copy->word[0])  // memory for scoped_copy has been deleted!
//
// As listed in the above snippet, it is simple to use this class.
// 1. Declare an instance of this ScopedHashEntryFactory, and;
// 2. Call its CreateHashEntry() member instead of using 'new hentry' or
//    'operator='.
//
class ScopedHashEntryFactory {
 public:
  ScopedHashEntryFactory();
  ~ScopedHashEntryFactory();

  // Creates a temporary copy of the given hentry struct.
  // The returned copy is available only while this object is available.
  // NOTE: this function just calls memcpy() in creating a copy of the given
  // hentry struct, i.e. it does NOT copy objects referred by pointers of the
  // given hentry struct.
  hentry* CreateScopedHashEntry(int index, const hentry* source);

 private:
  // A struct which encapsulates the new hentry struct introduced in hunspell
  // 1.2.8. For a pointer to an hentry struct 'h', hunspell 1.2.8 stores a word
  // (including a NUL character) into 'h->word[0]',...,'h->word[h->blen]' even
  // though arraysize(h->word[]) is 1. Also, it changed 'astr' to a pointer so
  // it can store affix flags into 'h->astr[0]',...,'h->astr[alen-1]'. To handle
  // this new hentry struct, we define a struct which combines three values: an
  // hentry struct 'hentry'; a char array 'word[kMaxWordLen]', and; an unsigned
  // short array 'astr' so a hentry struct 'h' returned from
  // CreateScopedHashEntry() satisfies the following equations:
  //   hentry* h = factory.CreateScopedHashEntry(0, source);
  //   h->word[0] == ((HashEntryItem*)h)->entry.word[0].
  //   h->word[1] == ((HashEntryItem*)h)->word[0].
  //   ...
  //   h->word[h->blen] == ((HashEntryItem*)h)->word[h->blen-1].
  //   h->astr[0] == ((HashEntryItem*)h)->astr[0].
  //   h->astr[1] == ((HashEntryItem*)h)->astr[1].
  //   ...
  //   h->astr[h->alen-1] == ((HashEntryItem*)h)->astr[h->alen-1].
  enum {
    kMaxWordLen = 128,
    kMaxAffixLen = 8,
  };
  struct HashEntryItem {
    hentry entry;
    char word[kMaxWordLen];
    unsigned short astr[kMaxAffixLen];
  };

  HashEntryItem hash_items_[MAX_ROOTS];
};

ScopedHashEntryFactory::ScopedHashEntryFactory() {
  memset(&hash_items_[0], 0, sizeof(hash_items_));
}

ScopedHashEntryFactory::~ScopedHashEntryFactory() {
}

hentry* ScopedHashEntryFactory::CreateScopedHashEntry(int index,
                                                      const hentry* source) {
  if (index >= MAX_ROOTS || source->blen >= kMaxWordLen)
    return NULL;

  // Retrieve a HashEntryItem struct from our spool, initialize it, and
  // returns the address of its 'hentry' member.
  size_t source_size = sizeof(hentry) + source->blen + 1;
  HashEntryItem* hash_item = &hash_items_[index];
  memcpy(&hash_item->entry, source, source_size);
  if (source->astr) {
    hash_item->entry.alen = source->alen;
    if (hash_item->entry.alen > kMaxAffixLen)
      hash_item->entry.alen = kMaxAffixLen;
    memcpy(hash_item->astr, source->astr, hash_item->entry.alen * sizeof(hash_item->astr[0]));
    hash_item->entry.astr = &hash_item->astr[0];
  }
  return &hash_item->entry;
}

}  // namespace
#endif


#ifdef HUNSPELL_CHROME_CLIENT
SuggestMgr::SuggestMgr(hunspell::BDictReader* reader,
                       const char * tryme, int maxn, 
                       AffixMgr * aptr)
{
  bdict_reader = reader;
#else
SuggestMgr::SuggestMgr(const char* tryme, unsigned int maxn, AffixMgr* aptr) {
#endif
  // register affix manager and check in string of chars to
  // try when building candidate suggestions
  pAMgr = aptr;

  csconv = NULL;

  ckeyl = 0;
  ckey = NULL;

  ctryl = 0;
  ctry = NULL;

  utf8 = 0;
  langnum = 0;
  complexprefixes = 0;

  maxSug = maxn;
  nosplitsugs = 0;
  maxngramsugs = MAXNGRAMSUGS;
  maxcpdsugs = MAXCOMPOUNDSUGS;

  if (pAMgr) {
    langnum = pAMgr->get_langnum();
    ckey = pAMgr->get_key_string();
    nosplitsugs = pAMgr->get_nosplitsugs();
    if (pAMgr->get_maxngramsugs() >= 0)
      maxngramsugs = pAMgr->get_maxngramsugs();
    utf8 = pAMgr->get_utf8();
    if (pAMgr->get_maxcpdsugs() >= 0)
      maxcpdsugs = pAMgr->get_maxcpdsugs();
    if (!utf8) {
      csconv = get_current_cs(pAMgr->get_encoding());
    }
    complexprefixes = pAMgr->get_complexprefixes();
  }

  if (ckey) {
    if (utf8) {
      ckeyl = u8_u16(ckey_utf, ckey);
    } else {
      ckeyl = strlen(ckey);
    }
  }

  if (tryme) {
    ctry = mystrdup(tryme);
    if (ctry)
      ctryl = strlen(ctry);
    if (ctry && utf8) {
      ctryl = u8_u16(ctry_utf, tryme);
    }
  }

  // language with possible dash usage
  // (latin letters or dash in TRY characters)
  lang_with_dash_usage = (ctry &&
      ((strchr(ctry, '-') != NULL) || (strchr(ctry, 'a') != NULL)));
}

SuggestMgr::~SuggestMgr() {
  pAMgr = NULL;
  if (ckey)
    free(ckey);
  ckey = NULL;
  ckeyl = 0;
  if (ctry)
    free(ctry);
  ctry = NULL;
  ctryl = 0;
  maxSug = 0;
#ifdef MOZILLA_CLIENT
  delete[] csconv;
#endif
}

void SuggestMgr::testsug(std::vector<std::string>& wlst,
                        const std::string& candidate,
                        int cpdsuggest,
                        int* timer,
                        clock_t* timelimit) {
  int cwrd = 1;
  if (wlst.size() == maxSug)
    return;
  for (size_t k = 0; k < wlst.size(); ++k) {
    if (wlst[k] == candidate) {
      cwrd = 0;
      break;
    }
  }
  if ((cwrd) && checkword(candidate, cpdsuggest, timer, timelimit)) {
    wlst.push_back(candidate);
  }
}

/* generate suggestions for a misspelled word
 *    pass in address of array of char * pointers
 * onlycompoundsug: probably bad suggestions (need for ngram sugs, too)
 * return value: true, if there is a good suggestion
 * (REP, ph: or a dictionary word pair)
 */
bool SuggestMgr::suggest(std::vector<std::string>& slst,
                        const char* w,
                        int* onlycompoundsug) {
  int nocompoundtwowords = 0;
  std::vector<w_char> word_utf;
  int wl = 0;
  size_t nsugorig = slst.size();
  std::string w2;
  const char* word = w;
  size_t oldSug = 0;
  bool good_suggestion = false;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    w2.assign(w);
    if (utf8)
      reverseword_utf(w2);
    else
      reverseword(w2);
    word = w2.c_str();
  }

  if (utf8) {
    wl = u8_u16(word_utf, word);
    if (wl == -1) {
      return false;
    }
  }

  for (int cpdsuggest = 0; (cpdsuggest < 2) && (nocompoundtwowords == 0) && !good_suggestion;
       cpdsuggest++) {

    clock_t timelimit;
    // initialize both in non-compound and compound cycles
    timelimit = clock();

    // limit compound suggestion
    if (cpdsuggest > 0)
      oldSug = slst.size();

    // suggestions for an uppercase word (html -> HTML)
    if (slst.size() < maxSug) {
      size_t i = slst.size();
      if (utf8)
        capchars_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        capchars(slst, word, cpdsuggest);
      if (slst.size() > i)
        good_suggestion = true;
    }

    // perhaps we made a typical fault of spelling
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      size_t i = slst.size();
      replchars(slst, word, cpdsuggest);
      if (slst.size() > i)
        good_suggestion = true;
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // perhaps we made chose the wrong char from a related set
    if ((slst.size() < maxSug) &&
        (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      mapchars(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // only suggest compound words when no other suggestion
    if ((cpdsuggest == 0) && (slst.size() > nsugorig))
      nocompoundtwowords = 1;

    // did we swap the order of chars by mistake
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        swapchar_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        swapchar(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // did we swap the order of non adjacent chars by mistake
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        longswapchar_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        longswapchar(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // did we just hit the wrong key in place of a good char (case and keyboard)
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        badcharkey_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        badcharkey(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // did we add a char that should not be there
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        extrachar_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        extrachar(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // did we forgot a char
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        forgotchar_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        forgotchar(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // did we move a char
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        movechar_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        movechar(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // did we just hit the wrong key in place of a good char
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        badchar_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        badchar(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // did we double two characters
    if ((slst.size() < maxSug) && (!cpdsuggest || (slst.size() < oldSug + maxcpdsugs))) {
      if (utf8)
        doubletwochars_utf(slst, &word_utf[0], wl, cpdsuggest);
      else
        doubletwochars(slst, word, cpdsuggest);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

    // perhaps we forgot to hit space and two words ran together
    // (dictionary word pairs have top priority here, so
    // we always suggest them, in despite of nosplitsugs, and
    // drop compound word and other suggestions)
    if (!cpdsuggest || (!nosplitsugs && slst.size() < oldSug + maxcpdsugs)) {
      good_suggestion = twowords(slst, word, cpdsuggest, good_suggestion);
    }
    if (clock() > timelimit + TIMELIMIT_SUGGESTION)
      return good_suggestion;

  }  // repeating ``for'' statement compounding support

  if (!nocompoundtwowords && (!slst.empty()) && onlycompoundsug)
    *onlycompoundsug = 1;

  return good_suggestion;
}

// suggestions for an uppercase word (html -> HTML)
void SuggestMgr::capchars_utf(std::vector<std::string>& wlst,
                              const w_char* word,
                              int wl,
                              int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  mkallcap_utf(candidate_utf, langnum);
  std::string candidate;
  u16_u8(candidate, candidate_utf);
  testsug(wlst, candidate, cpdsuggest, NULL, NULL);
}

// suggestions for an uppercase word (html -> HTML)
void SuggestMgr::capchars(std::vector<std::string>& wlst,
                          const char* word,
                          int cpdsuggest) {
  std::string candidate(word);
  mkallcap(candidate, csconv);
  testsug(wlst, candidate, cpdsuggest, NULL, NULL);
}

// suggestions for when chose the wrong char out of a related set
int SuggestMgr::mapchars(std::vector<std::string>& wlst,
                         const char* word,
                         int cpdsuggest) {
  std::string candidate;
  clock_t timelimit;
  int timer;

  int wl = strlen(word);
  if (wl < 2 || !pAMgr)
    return wlst.size();

  const std::vector<mapentry>& maptable = pAMgr->get_maptable();
  if (maptable.empty())
    return wlst.size();

  timelimit = clock();
  timer = MINTIMER;
  return map_related(word, candidate, 0, wlst, cpdsuggest,
                     maptable, &timer, &timelimit);
}

int SuggestMgr::map_related(const char* word,
                            std::string& candidate,
                            int wn,
                            std::vector<std::string>& wlst,
                            int cpdsuggest,
                            const std::vector<mapentry>& maptable,
                            int* timer,
                            clock_t* timelimit) {
  if (*(word + wn) == '\0') {
    int cwrd = 1;
    for (size_t m = 0; m < wlst.size(); ++m) {
      if (wlst[m] == candidate) {
        cwrd = 0;
        break;
      }
    }
    if ((cwrd) && checkword(candidate, cpdsuggest, timer, timelimit)) {
      if (wlst.size() < maxSug) {
        wlst.push_back(candidate);
      }
    }
    return wlst.size();
  }
  int in_map = 0;
  for (size_t j = 0; j < maptable.size(); ++j) {
    for (size_t k = 0; k < maptable[j].size(); ++k) {
      size_t len = maptable[j][k].size();
      if (strncmp(maptable[j][k].c_str(), word + wn, len) == 0) {
        in_map = 1;
        size_t cn = candidate.size();
        for (size_t l = 0; l < maptable[j].size(); ++l) {
          candidate.resize(cn);
          candidate.append(maptable[j][l]);
          map_related(word, candidate, wn + len, wlst,
                           cpdsuggest, maptable, timer, timelimit);
          if (!(*timer))
            return wlst.size();
        }
      }
    }
  }
  if (!in_map) {
    candidate.push_back(*(word + wn));
    map_related(word, candidate, wn + 1, wlst, cpdsuggest,
                maptable, timer, timelimit);
  }
  return wlst.size();
}

// suggestions for a typical fault of spelling, that
// differs with more, than 1 letter from the right form.
int SuggestMgr::replchars(std::vector<std::string>& wlst,
                          const char* word,
                          int cpdsuggest) {
  std::string candidate;
  int wl = strlen(word);
  if (wl < 2 || !pAMgr)
    return wlst.size();

// TODO: wrong, 'ns' doesn't exist any more
#ifdef HUNSPELL_CHROME_CLIENT
  const char *pattern, *pattern2;
  hunspell::ReplacementIterator iterator = bdict_reader->GetReplacementIterator();
  while (iterator.GetNext(&pattern, &pattern2)) {
    const char* r = word;
    size_t lenr = strlen(pattern2);
    size_t lenp = strlen(pattern);

    // search every occurence of the pattern in the word
    while ((r=strstr(r, pattern)) != NULL) {
      candidate = word;
      candidate.replace(r-word, lenp, pattern2);
#else
  const std::vector<replentry>& reptable = pAMgr->get_reptable();
  for (size_t i = 0; i < reptable.size(); ++i) {
    const char* r = word;
    // search every occurence of the pattern in the word
    while ((r = strstr(r, reptable[i].pattern.c_str())) != NULL) {
      int type = (r == word) ? 1 : 0;
      if (r - word + reptable[i].pattern.size() == strlen(word))
        type += 2;
      while (type && reptable[i].outstrings[type].empty())
        type = (type == 2 && r != word) ? 0 : type - 1;
      const std::string&out = reptable[i].outstrings[type];
      if (out.empty()) {
        ++r;
        continue;
      }
      candidate.assign(word);
      candidate.resize(r - word);
      candidate.append(reptable[i].outstrings[type]);
      candidate.append(r + reptable[i].pattern.size());
#endif
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
      // check REP suggestions with space
      size_t sp = candidate.find(' ');
      if (sp != std::string::npos) {
        size_t prev = 0;
        while (sp != std::string::npos) {
          std::string prev_chunk = candidate.substr(prev, sp - prev);
          if (checkword(prev_chunk, 0, NULL, NULL)) {
            size_t oldns = wlst.size();
            std::string post_chunk = candidate.substr(sp + 1);
            testsug(wlst, post_chunk, cpdsuggest, NULL, NULL);
            if (oldns < wlst.size()) {
              wlst[wlst.size() - 1] = candidate;
            }
          }
          prev = sp + 1;
          sp = candidate.find(' ', prev);
        }
      }
      r++;  // search for the next letter
    }
  }
  return wlst.size();
}

// perhaps we doubled two characters
// (for example vacation -> vacacation)
// The recognized pattern with regex back-references:
// "(.)(.)\1\2\1" or "..(.)(.)\1\2"

int SuggestMgr::doubletwochars(std::vector<std::string>& wlst,
                               const char* word,
                               int cpdsuggest) {
  int state = 0;
  int wl = strlen(word);
  if (wl < 5 || !pAMgr)
    return wlst.size();
  for (int i = 2; i < wl; i++) {
    if (word[i] == word[i - 2]) {
      state++;
      if (state == 3 || (state == 2 && i >= 4)) {
        std::string candidate(word, word + i - 1);
        candidate.insert(candidate.end(), word + i + 1, word + wl);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
        state = 0;
      }
    } else {
      state = 0;
    }
  }
  return wlst.size();
}

// perhaps we doubled two characters
// (for example vacation -> vacacation)
// The recognized pattern with regex back-references:
// "(.)(.)\1\2\1" or "..(.)(.)\1\2"

int SuggestMgr::doubletwochars_utf(std::vector<std::string>& wlst,
                                   const w_char* word,
                                   int wl,
                                   int cpdsuggest) {
  int state = 0;
  if (wl < 5 || !pAMgr)
    return wlst.size();
  for (int i = 2; i < wl; i++) {
    if (word[i] == word[i - 2]) {
      state++;
      if (state == 3 || (state == 2 && i >= 4)) {
        std::vector<w_char> candidate_utf(word, word + i - 1);
        candidate_utf.insert(candidate_utf.end(), word + i + 1, word + wl);
        std::string candidate;
        u16_u8(candidate, candidate_utf);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
        state = 0;
      }
    } else {
      state = 0;
    }
  }
  return wlst.size();
}

// error is wrong char in place of correct one (case and keyboard related
// version)
int SuggestMgr::badcharkey(std::vector<std::string>& wlst,
                           const char* word,
                           int cpdsuggest) {
  std::string candidate(word);

  // swap out each char one by one and try uppercase and neighbor
  // keyboard chars in its place to see if that makes a good word
  for (size_t i = 0; i < candidate.size(); ++i) {
    char tmpc = candidate[i];
    // check with uppercase letters
    candidate[i] = csconv[((unsigned char)tmpc)].cupper;
    if (tmpc != candidate[i]) {
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
      candidate[i] = tmpc;
    }
    // check neighbor characters in keyboard string
    if (!ckey)
      continue;
    char* loc = strchr(ckey, tmpc);
    while (loc) {
      if ((loc > ckey) && (*(loc - 1) != '|')) {
        candidate[i] = *(loc - 1);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
      }
      if ((*(loc + 1) != '|') && (*(loc + 1) != '\0')) {
        candidate[i] = *(loc + 1);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
      }
      loc = strchr(loc + 1, tmpc);
    }
    candidate[i] = tmpc;
  }
  return wlst.size();
}

// error is wrong char in place of correct one (case and keyboard related
// version)
int SuggestMgr::badcharkey_utf(std::vector<std::string>& wlst,
                               const w_char* word,
                               int wl,
                               int cpdsuggest) {
  std::string candidate;
  std::vector<w_char> candidate_utf(word, word + wl);
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (int i = 0; i < wl; i++) {
    w_char tmpc = candidate_utf[i];
    // check with uppercase letters
    candidate_utf[i] = upper_utf(candidate_utf[i], 1);
    if (tmpc != candidate_utf[i]) {
      u16_u8(candidate, candidate_utf);
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
      candidate_utf[i] = tmpc;
    }
    // check neighbor characters in keyboard string
    if (!ckey)
      continue;
    size_t loc = 0;
    while ((loc < ckeyl) && ckey_utf[loc] != tmpc)
      ++loc;
    while (loc < ckeyl) {
      if ((loc > 0) && ckey_utf[loc - 1] != W_VLINE) {
        candidate_utf[i] = ckey_utf[loc - 1];
        u16_u8(candidate, candidate_utf);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
      }
      if (((loc + 1) < ckeyl) && (ckey_utf[loc + 1] != W_VLINE)) {
        candidate_utf[i] = ckey_utf[loc + 1];
        u16_u8(candidate, candidate_utf);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
      }
      do {
        loc++;
      } while ((loc < ckeyl) && ckey_utf[loc] != tmpc);
    }
    candidate_utf[i] = tmpc;
  }
  return wlst.size();
}

// error is wrong char in place of correct one
int SuggestMgr::badchar(std::vector<std::string>& wlst, const char* word, int cpdsuggest) {
  std::string candidate(word);
  clock_t timelimit = clock();
  int timer = MINTIMER;
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (size_t j = 0; j < ctryl; ++j) {
    for (std::string::reverse_iterator aI = candidate.rbegin(), aEnd = candidate.rend(); aI != aEnd; ++aI) {
      char tmpc = *aI;
      if (ctry[j] == tmpc)
        continue;
      *aI = ctry[j];
      testsug(wlst, candidate, cpdsuggest, &timer, &timelimit);
      if (!timer)
        return wlst.size();
      *aI = tmpc;
    }
  }
  return wlst.size();
}

// error is wrong char in place of correct one
int SuggestMgr::badchar_utf(std::vector<std::string>& wlst,
                            const w_char* word,
                            int wl,
                            int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  std::string candidate;
  clock_t timelimit = clock();
  int timer = MINTIMER;
  // swap out each char one by one and try all the tryme
  // chars in its place to see if that makes a good word
  for (size_t j = 0; j < ctryl; ++j) {
    for (int i = wl - 1; i >= 0; i--) {
      w_char tmpc = candidate_utf[i];
      if (tmpc == ctry_utf[j])
        continue;
      candidate_utf[i] = ctry_utf[j];
      u16_u8(candidate, candidate_utf);
      testsug(wlst, candidate, cpdsuggest, &timer, &timelimit);
      if (!timer)
        return wlst.size();
      candidate_utf[i] = tmpc;
    }
  }
  return wlst.size();
}

// error is word has an extra letter it does not need
int SuggestMgr::extrachar_utf(std::vector<std::string>& wlst,
                              const w_char* word,
                              int wl,
                              int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  if (candidate_utf.size() < 2)
    return wlst.size();
  // try omitting one char of word at a time
  for (size_t i = 0; i < candidate_utf.size(); ++i) {
    size_t index = candidate_utf.size() - 1 - i;
    w_char tmpc = candidate_utf[index];
    candidate_utf.erase(candidate_utf.begin() + index);
    std::string candidate;
    u16_u8(candidate, candidate_utf);
    testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    candidate_utf.insert(candidate_utf.begin() + index, tmpc);
  }
  return wlst.size();
}

// error is word has an extra letter it does not need
int SuggestMgr::extrachar(std::vector<std::string>& wlst,
                          const char* word,
                          int cpdsuggest) {
  std::string candidate(word);
  if (candidate.size() < 2)
    return wlst.size();
  // try omitting one char of word at a time
  for (size_t i = 0; i < candidate.size(); ++i) {
    size_t index = candidate.size() - 1 - i;
    char tmpc = candidate[index];
    candidate.erase(candidate.begin() + index);
    testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    candidate.insert(candidate.begin() + index, tmpc);
  }
  return wlst.size();
}

// error is missing a letter it needs
int SuggestMgr::forgotchar(std::vector<std::string>& wlst,
                           const char* word,
                           int cpdsuggest) {
  std::string candidate(word);
  clock_t timelimit = clock();
  int timer = MINTIMER;

  // try inserting a tryme character before every letter (and the null
  // terminator)
  for (size_t k = 0; k < ctryl; ++k) {
    for (size_t i = 0; i <= candidate.size(); ++i) {
      size_t index = candidate.size() - i;
      candidate.insert(candidate.begin() + index, ctry[k]);
      testsug(wlst, candidate, cpdsuggest, &timer, &timelimit);
      if (!timer)
        return wlst.size();
      candidate.erase(candidate.begin() + index);
    }
  }
  return wlst.size();
}

// error is missing a letter it needs
int SuggestMgr::forgotchar_utf(std::vector<std::string>& wlst,
                               const w_char* word,
                               int wl,
                               int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  clock_t timelimit = clock();
  int timer = MINTIMER;

  // try inserting a tryme character at the end of the word and before every
  // letter
  for (size_t k = 0; k < ctryl; ++k) {
    for (size_t i = 0; i <= candidate_utf.size(); ++i) {
      size_t index = candidate_utf.size() - i;
      candidate_utf.insert(candidate_utf.begin() + index, ctry_utf[k]);
      std::string candidate;
      u16_u8(candidate, candidate_utf);
      testsug(wlst, candidate, cpdsuggest, &timer, &timelimit);
      if (!timer)
        return wlst.size();
      candidate_utf.erase(candidate_utf.begin() + index);
    }
  }
  return wlst.size();
}

/* error is should have been two words
 * return value is true, if there is a dictionary word pair,
 * or there was already a good suggestion before calling
 * this function.
 */
bool SuggestMgr::twowords(std::vector<std::string>& wlst,
                         const char* word,
                         int cpdsuggest,
                         bool good) {
  int c2;
  int forbidden = 0;
  int cwrd;

  int wl = strlen(word);
  if (wl < 3)
    return false;

  if (langnum == LANG_hu)
    forbidden = check_forbidden(word, wl);

  char* candidate = (char*)malloc(wl + 2);
  strcpy(candidate + 1, word);

  // split the string into two pieces after every char
  // if both pieces are good words make them a suggestion
  for (char* p = candidate + 1; p[1] != '\0'; p++) {
    p[-1] = *p;
    // go to end of the UTF-8 character
    while (utf8 && ((p[1] & 0xc0) == 0x80)) {
      *p = p[1];
      p++;
    }
    if (utf8 && p[1] == '\0')
      break;  // last UTF-8 character

    // Suggest only word pairs, if they are listed in the dictionary.
    // For example, adding "a lot" to the English dic file will
    // result only "alot" -> "a lot" suggestion instead of
    // "alto, slot, alt, lot, allot, aloft, aloe, clot, plot, blot, a lot".
    // Note: using "ph:alot" keeps the other suggestions:
    // a lot ph:alot
    // alot -> a lot, alto, slot...
    *p = ' ';
    if (!cpdsuggest && checkword(candidate, cpdsuggest, NULL, NULL)) {
      // remove not word pair suggestions
      if (!good) {
        good = true;
        wlst.clear();
      }
      wlst.insert(wlst.begin(), candidate);
    }

    // word pairs with dash?
    if (lang_with_dash_usage) {
      *p = '-';

      if (!cpdsuggest && checkword(candidate, cpdsuggest, NULL, NULL)) {
        // remove not word pair suggestions
        if (!good) {
          good = true;
          wlst.clear();
        }
        wlst.insert(wlst.begin(), candidate);
      }
    }

    if (wlst.size() < maxSug && !nosplitsugs && !good) {
      *p = '\0';
      int c1 = checkword(candidate, cpdsuggest, NULL, NULL);
      if (c1) {
        c2 = checkword((p + 1), cpdsuggest, NULL, NULL);
        if (c2) {
          // spec. Hungarian code (TODO need a better compound word support)
          if ((langnum == LANG_hu) && !forbidden &&
              // if 3 repeating letter, use - instead of space
              (((p[-1] == p[1]) &&
              (((p > candidate + 1) && (p[-1] == p[-2])) || (p[-1] == p[2]))) ||
              // or multiple compounding, with more, than 6 syllables
              ((c1 == 3) && (c2 >= 2))))
            *p = '-';
          else
            *p = ' ';

          cwrd = 1;
          for (size_t k = 0; k < wlst.size(); ++k) {
            if (wlst[k] == candidate) {
              cwrd = 0;
              break;
            }
          }

          if (cwrd && (wlst.size() < maxSug))
              wlst.push_back(candidate);

          // add two word suggestion with dash, depending on the language
          // Note that cwrd doesn't modified for REP twoword sugg.
          if ( !nosplitsugs && lang_with_dash_usage &&
              mystrlen(p + 1) > 1 && mystrlen(candidate) - mystrlen(p) > 1) {
            *p = '-';
            for (size_t k = 0; k < wlst.size(); ++k) {
              if (wlst[k] == candidate) {
                cwrd = 0;
                break;
              }
            }

            if ((wlst.size() < maxSug) && cwrd)
              wlst.push_back(candidate);
          }
        }
      }
    }
  }
  free(candidate);
  return good;
}

// error is adjacent letter were swapped
int SuggestMgr::swapchar(std::vector<std::string>& wlst,
                         const char* word,
                         int cpdsuggest) {
  std::string candidate(word);
  if (candidate.size() < 2)
    return wlst.size();

  // try swapping adjacent chars one by one
  for (size_t i = 0; i < candidate.size() - 1; ++i) {
    std::swap(candidate[i], candidate[i+1]);
    testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    std::swap(candidate[i], candidate[i+1]);
  }

  // try double swaps for short words
  // ahev -> have, owudl -> would
  if (candidate.size() == 4 || candidate.size() == 5) {
    candidate[0] = word[1];
    candidate[1] = word[0];
    candidate[2] = word[2];
    candidate[candidate.size() - 2] = word[candidate.size() - 1];
    candidate[candidate.size() - 1] = word[candidate.size() - 2];
    testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    if (candidate.size() == 5) {
      candidate[0] = word[0];
      candidate[1] = word[2];
      candidate[2] = word[1];
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    }
  }

  return wlst.size();
}

// error is adjacent letter were swapped
int SuggestMgr::swapchar_utf(std::vector<std::string>& wlst,
                             const w_char* word,
                             int wl,
                             int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  if (candidate_utf.size() < 2)
    return wlst.size();

  std::string candidate;
  // try swapping adjacent chars one by one
  for (size_t i = 0; i < candidate_utf.size() - 1; ++i) {
    std::swap(candidate_utf[i], candidate_utf[i+1]);
    u16_u8(candidate, candidate_utf);
    testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    std::swap(candidate_utf[i], candidate_utf[i+1]);
  }

  // try double swaps for short words
  // ahev -> have, owudl -> would, suodn -> sound
  if (candidate_utf.size() == 4 || candidate_utf.size() == 5) {
    candidate_utf[0] = word[1];
    candidate_utf[1] = word[0];
    candidate_utf[2] = word[2];
    candidate_utf[candidate_utf.size() - 2] = word[candidate_utf.size() - 1];
    candidate_utf[candidate_utf.size() - 1] = word[candidate_utf.size() - 2];
    u16_u8(candidate, candidate_utf);
    testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    if (candidate_utf.size() == 5) {
      candidate_utf[0] = word[0];
      candidate_utf[1] = word[2];
      candidate_utf[2] = word[1];
      u16_u8(candidate, candidate_utf);
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    }
  }
  return wlst.size();
}

// error is not adjacent letter were swapped
int SuggestMgr::longswapchar(std::vector<std::string>& wlst,
                             const char* word,
                             int cpdsuggest) {
  std::string candidate(word);
  // try swapping not adjacent chars one by one
  for (std::string::iterator p = candidate.begin(); p < candidate.end(); ++p) {
    for (std::string::iterator q = candidate.begin(); q < candidate.end(); ++q) {
      size_t distance = std::abs(std::distance(q, p));
      if (distance > 1 && distance <= MAX_CHAR_DISTANCE) {
        std::swap(*p, *q);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
        std::swap(*p, *q);
      }
    }
  }
  return wlst.size();
}

// error is adjacent letter were swapped
int SuggestMgr::longswapchar_utf(std::vector<std::string>& wlst,
                                 const w_char* word,
                                 int wl,
                                 int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  // try swapping not adjacent chars
  for (std::vector<w_char>::iterator p = candidate_utf.begin(); p < candidate_utf.end(); ++p) {
    for (std::vector<w_char>::iterator q = candidate_utf.begin(); q < candidate_utf.end(); ++q) {
      size_t distance = std::abs(std::distance(q, p));
      if (distance > 1 && distance <= MAX_CHAR_DISTANCE) {
        std::swap(*p, *q);
        std::string candidate;
        u16_u8(candidate, candidate_utf);
        testsug(wlst, candidate, cpdsuggest, NULL, NULL);
        std::swap(*p, *q);
      }
    }
  }
  return wlst.size();
}

// error is a letter was moved
int SuggestMgr::movechar(std::vector<std::string>& wlst,
                         const char* word,
                         int cpdsuggest) {
  std::string candidate(word);
  if (candidate.size() < 2)
    return wlst.size();

  // try moving a char
  for (std::string::iterator p = candidate.begin(); p < candidate.end(); ++p) {
    for (std::string::iterator q = p + 1; q < candidate.end() && std::distance(p, q) <= MAX_CHAR_DISTANCE; ++q) {
      std::swap(*q, *(q - 1));
      if (std::distance(p, q) < 2)
        continue;  // omit swap char
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    }
    std::copy(word, word + candidate.size(), candidate.begin());
  }

  for (std::string::reverse_iterator p = candidate.rbegin(), pEnd = candidate.rend() - 1; p != pEnd; ++p) {
    for (std::string::reverse_iterator q = p + 1, qEnd = candidate.rend(); q != qEnd && std::distance(p, q) <= MAX_CHAR_DISTANCE; ++q) {
      std::swap(*q, *(q - 1));
      if (std::distance(p, q) < 2)
        continue;  // omit swap char
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    }
    std::copy(word, word + candidate.size(), candidate.begin());
  }

  return wlst.size();
}

// error is a letter was moved
int SuggestMgr::movechar_utf(std::vector<std::string>& wlst,
                             const w_char* word,
                             int wl,
                             int cpdsuggest) {
  std::vector<w_char> candidate_utf(word, word + wl);
  if (candidate_utf.size() < 2)
    return wlst.size();

  // try moving a char
  for (std::vector<w_char>::iterator p = candidate_utf.begin(); p < candidate_utf.end(); ++p) {
    for (std::vector<w_char>::iterator q = p + 1; q < candidate_utf.end() && std::distance(p, q) <= MAX_CHAR_DISTANCE; ++q) {
      std::swap(*q, *(q - 1));
      if (std::distance(p, q) < 2)
        continue;  // omit swap char
      std::string candidate;
      u16_u8(candidate, candidate_utf);
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    }
    std::copy(word, word + candidate_utf.size(), candidate_utf.begin());
  }

  for (std::vector<w_char>::reverse_iterator p = candidate_utf.rbegin(); p < candidate_utf.rend(); ++p) {
    for (std::vector<w_char>::reverse_iterator q = p + 1; q < candidate_utf.rend() && std::distance(p, q) <= MAX_CHAR_DISTANCE; ++q) {
      std::swap(*q, *(q - 1));
      if (std::distance(p, q) < 2)
        continue;  // omit swap char
      std::string candidate;
      u16_u8(candidate, candidate_utf);
      testsug(wlst, candidate, cpdsuggest, NULL, NULL);
    }
    std::copy(word, word + candidate_utf.size(), candidate_utf.begin());
  }

  return wlst.size();
}

// generate a set of suggestions for very poorly spelled words
void SuggestMgr::ngsuggest(std::vector<std::string>& wlst,
                          const char* w,
                          const std::vector<HashMgr*>& rHMgr,
                          int captype) {
  int lval;
  int sc;
  int lp, lpphon;
  int nonbmp = 0;

  // exhaustively search through all root words
  // keeping track of the MAX_ROOTS most similar root words
  struct hentry* roots[MAX_ROOTS];
  char* rootsphon[MAX_ROOTS];
  int scores[MAX_ROOTS];
  int scoresphon[MAX_ROOTS];
  for (int i = 0; i < MAX_ROOTS; i++) {
    roots[i] = NULL;
    scores[i] = -100 * i;
    rootsphon[i] = NULL;
    scoresphon[i] = -100 * i;
  }
  lp = MAX_ROOTS - 1;
  lpphon = MAX_ROOTS - 1;
  int low = NGRAM_LOWERING;

  std::string w2;
  const char* word = w;

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    w2.assign(w);
    if (utf8)
      reverseword_utf(w2);
    else
      reverseword(w2);
    word = w2.c_str();
  }

  std::vector<w_char> u8;
  int nc = strlen(word);
  int n = (utf8) ? u8_u16(u8, word) : nc;

  // set character based ngram suggestion for words with non-BMP Unicode
  // characters
  if (n == -1) {
    utf8 = 0;  // XXX not state-free
    n = nc;
    nonbmp = 1;
    low = 0;
  }

  struct hentry* hp = NULL;
  int col = -1;
#ifdef HUNSPELL_CHROME_CLIENT
  ScopedHashEntryFactory hash_entry_factory;
#endif
  phonetable* ph = (pAMgr) ? pAMgr->get_phonetable() : NULL;
  std::string target;
  std::string candidate;
  std::vector<w_char> w_candidate;
  if (ph) {
    if (utf8) {
      u8_u16(w_candidate, word);
      mkallcap_utf(w_candidate, langnum);
      u16_u8(candidate, w_candidate);
    } else {
      candidate.assign(word);
      if (!nonbmp)
        mkallcap(candidate, csconv);
    }
    target = phonet(candidate, *ph);  // XXX phonet() is 8-bit (nc, not n)
  }

  FLAG forbiddenword = pAMgr ? pAMgr->get_forbiddenword() : FLAG_NULL;
  FLAG nosuggest = pAMgr ? pAMgr->get_nosuggest() : FLAG_NULL;
  FLAG nongramsuggest = pAMgr ? pAMgr->get_nongramsuggest() : FLAG_NULL;
  FLAG onlyincompound = pAMgr ? pAMgr->get_onlyincompound() : FLAG_NULL;

  std::vector<w_char> w_word, w_target;
  if (utf8) {
    u8_u16(w_word, word);
    u8_u16(w_target, target);
  }

  std::string f;
  std::vector<w_char> w_f;

  for (size_t i = 0; i < rHMgr.size(); ++i) {
    while (0 != (hp = rHMgr[i]->walk_hashtable(col, hp))) {
      // skip exceptions
      if (
           // skip it, if the word length different by 5 or
           // more characters (to avoid strange suggestions)
           // (except Unicode characters over BMP)
           (((abs(n - hp->clen) > 4) && !nonbmp)) ||
           // don't suggest capitalized dictionary words for
           // lower case misspellings in ngram suggestions, except
           // - PHONE usage, or
           // - in the case of German, where not only proper
           //   nouns are capitalized, or
           // - the capitalized word has special pronunciation
           ((captype == NOCAP) && (hp->var & H_OPT_INITCAP) &&
              !ph && (langnum != LANG_de) && !(hp->var & H_OPT_PHON)) ||
           // or it has one of the following special flags
           ((hp->astr) && (pAMgr) &&
             (TESTAFF(hp->astr, forbiddenword, hp->alen) ||
             TESTAFF(hp->astr, ONLYUPCASEFLAG, hp->alen) ||
             TESTAFF(hp->astr, nosuggest, hp->alen) ||
             TESTAFF(hp->astr, nongramsuggest, hp->alen) ||
             TESTAFF(hp->astr, onlyincompound, hp->alen)))
         )
        continue;

      if (utf8) {
        u8_u16(w_f, HENTRY_WORD(hp));

        int leftcommon = leftcommonsubstring(w_word, w_f);
        if (low) {
          // lowering dictionary word
          mkallsmall_utf(w_f, langnum);
        }
        sc = ngram(3, w_word, w_f, NGRAM_LONGER_WORSE) + leftcommon;
      } else {
        f.assign(HENTRY_WORD(hp));

        int leftcommon = leftcommonsubstring(word, f.c_str());
        if (low) {
          // lowering dictionary word
          mkallsmall(f, csconv);
        }
        sc = ngram(3, word, f, NGRAM_LONGER_WORSE) + leftcommon;
      }

      // check special pronunciation
      f.clear();
      if ((hp->var & H_OPT_PHON) &&
          copy_field(f, HENTRY_DATA(hp), MORPH_PHON)) {
        int sc2;
        if (utf8) {
          u8_u16(w_f, f);

          int leftcommon = leftcommonsubstring(w_word, w_f);
          if (low) {
            // lowering dictionary word
            mkallsmall_utf(w_f, langnum);
          }
          sc2 = ngram(3, w_word, w_f, NGRAM_LONGER_WORSE) + leftcommon;
        } else {
          int leftcommon = leftcommonsubstring(word, f.c_str());
          if (low) {
            // lowering dictionary word
            mkallsmall(f, csconv);
          }
          sc2 = ngram(3, word, f, NGRAM_LONGER_WORSE) + leftcommon;
        }
        if (sc2 > sc)
          sc = sc2;
      }

      int scphon = -20000;
      if (ph && (sc > 2) && (abs(n - (int)hp->clen) <= 3)) {
        if (utf8) {
          u8_u16(w_candidate, HENTRY_WORD(hp));
          mkallcap_utf(w_candidate, langnum);
          u16_u8(candidate, w_candidate);
        } else {
          candidate = HENTRY_WORD(hp);
          mkallcap(candidate, csconv);
        }
        f = phonet(candidate, *ph);
        if (utf8) {
          u8_u16(w_f, f);
          scphon = 2 * ngram(3, w_target, w_f,
                             NGRAM_LONGER_WORSE);
        } else {
          scphon = 2 * ngram(3, target, f,
                             NGRAM_LONGER_WORSE);
        }
      }

      if (sc > scores[lp]) {
        scores[lp] = sc;
#ifdef HUNSPELL_CHROME_CLIENT
        roots[lp] = hash_entry_factory.CreateScopedHashEntry(lp, hp);
#else
        roots[lp] = hp;
#endif
        lval = sc;
        for (int j = 0; j < MAX_ROOTS; j++)
          if (scores[j] < lval) {
            lp = j;
            lval = scores[j];
          }
      }

      if (scphon > scoresphon[lpphon]) {
        scoresphon[lpphon] = scphon;
        rootsphon[lpphon] = HENTRY_WORD(hp);
        lval = scphon;
        for (int j = 0; j < MAX_ROOTS; j++)
          if (scoresphon[j] < lval) {
            lpphon = j;
            lval = scoresphon[j];
          }
      }
    }
  }

  // find minimum threshold for a passable suggestion
  // mangle original word three differnt ways
  // and score them to generate a minimum acceptable score
  std::vector<w_char> w_mw;
  int thresh = 0;
  for (int sp = 1; sp < 4; sp++) {
    if (utf8) {
      w_mw = w_word;
      for (int k = sp; k < n; k += 4) {
        w_mw[k].l = '*';
        w_mw[k].h = 0;
      }

      if (low) {
        // lowering dictionary word
        mkallsmall_utf(w_mw, langnum);
      }

      thresh += ngram(n, w_word, w_mw, NGRAM_ANY_MISMATCH);
    } else {
      std::string mw = word;
      for (int k = sp; k < n; k += 4)
        mw[k] = '*';

      if (low) {
        // lowering dictionary word
        mkallsmall(mw, csconv);
      }

      thresh += ngram(n, word, mw, NGRAM_ANY_MISMATCH);
    }
  }
  thresh = thresh / 3;
  thresh--;

  // now expand affixes on each of these root words and
  // and use length adjusted ngram scores to select
  // possible suggestions
  char* guess[MAX_GUESS];
  char* guessorig[MAX_GUESS];
  int gscore[MAX_GUESS];
  for (int i = 0; i < MAX_GUESS; i++) {
    guess[i] = NULL;
    guessorig[i] = NULL;
    gscore[i] = -100 * i;
  }

  lp = MAX_GUESS - 1;

  struct guessword* glst;
  glst = (struct guessword*)calloc(MAX_WORDS, sizeof(struct guessword));
  if (!glst) {
    if (nonbmp)
      utf8 = 1;
    return;
  }

  for (int i = 0; i < MAX_ROOTS; i++) {
    if (roots[i]) {
      struct hentry* rp = roots[i];

      f.clear();
      const char *field = NULL;
      if ((rp->var & H_OPT_PHON) && copy_field(f, HENTRY_DATA(rp), MORPH_PHON))
          field = f.c_str();
      int nw = pAMgr->expand_rootword(
          glst, MAX_WORDS, HENTRY_WORD(rp), rp->blen, rp->astr, rp->alen, word,
          nc, field);

      for (int k = 0; k < nw; k++) {
        if (utf8) {
          u8_u16(w_f, glst[k].word);

          int leftcommon = leftcommonsubstring(w_word, w_f);
          if (low) {
            // lowering dictionary word
            mkallsmall_utf(w_f, langnum);
          }

          sc = ngram(n, w_word, w_f, NGRAM_ANY_MISMATCH) + leftcommon;
        } else {
          f = glst[k].word;

          int leftcommon = leftcommonsubstring(word, f.c_str());
          if (low) {
            // lowering dictionary word
            mkallsmall(f, csconv);
          }

          sc = ngram(n, word, f, NGRAM_ANY_MISMATCH) + leftcommon;
        }

        if (sc > thresh) {
          if (sc > gscore[lp]) {
            if (guess[lp]) {
              free(guess[lp]);
              if (guessorig[lp]) {
                free(guessorig[lp]);
                guessorig[lp] = NULL;
              }
            }
            gscore[lp] = sc;
            guess[lp] = glst[k].word;
            guessorig[lp] = glst[k].orig;
            lval = sc;
            for (int j = 0; j < MAX_GUESS; j++)
              if (gscore[j] < lval) {
                lp = j;
                lval = gscore[j];
              }
          } else {
            free(glst[k].word);
            if (glst[k].orig)
              free(glst[k].orig);
          }
        } else {
          free(glst[k].word);
          if (glst[k].orig)
            free(glst[k].orig);
        }
      }
    }
  }
  free(glst);

  // now we are done generating guesses
  // sort in order of decreasing score

  bubblesort(&guess[0], &guessorig[0], &gscore[0], MAX_GUESS);
  if (ph)
    bubblesort(&rootsphon[0], NULL, &scoresphon[0], MAX_ROOTS);

  // weight suggestions with a similarity index, based on
  // the longest common subsequent algorithm and resort

  int is_swap = 0;
  int re = 0;
  double fact = 1.0;
  if (pAMgr) {
    int maxd = pAMgr->get_maxdiff();
    if (maxd >= 0)
      fact = (10.0 - maxd) / 5.0;
  }

  std::vector<w_char> w_gl;
  for (int i = 0; i < MAX_GUESS; i++) {
    if (guess[i]) {
      // lowering guess[i]
      std::string gl;
      int len;
      if (utf8) {
        len = u8_u16(w_gl, guess[i]);
        mkallsmall_utf(w_gl, langnum);
        u16_u8(gl, w_gl);
      } else {
        gl.assign(guess[i]);
        if (!nonbmp)
          mkallsmall(gl, csconv);
        len = strlen(guess[i]);
      }

      int _lcs = lcslen(word, gl.c_str());

      // same characters with different casing
      if ((n == len) && (n == _lcs)) {
        gscore[i] += 2000;
        break;
      }
      // using 2-gram instead of 3, and other weightening

      if (utf8) {
        u8_u16(w_gl, gl);
        //w_gl is lowercase already at this point
        re = ngram(2, w_word, w_gl, NGRAM_ANY_MISMATCH + NGRAM_WEIGHTED);
        if (low) {
          w_f = w_word;
          // lowering dictionary word
          mkallsmall_utf(w_f, langnum);
          re += ngram(2, w_gl, w_f, NGRAM_ANY_MISMATCH + NGRAM_WEIGHTED);
        } else {
          re += ngram(2, w_gl, w_word, NGRAM_ANY_MISMATCH + NGRAM_WEIGHTED);
        }
      } else {
        //gl is lowercase already at this point
        re = ngram(2, word, gl, NGRAM_ANY_MISMATCH + NGRAM_WEIGHTED);
        if (low) {
          f = word;
          // lowering dictionary word
          mkallsmall(f, csconv);
          re += ngram(2, gl, f, NGRAM_ANY_MISMATCH + NGRAM_WEIGHTED);
        } else {
          re += ngram(2, gl, word, NGRAM_ANY_MISMATCH + NGRAM_WEIGHTED);
        }
      }

      int ngram_score, leftcommon_score;
      if (utf8) {
        //w_gl is lowercase already at this point
        ngram_score = ngram(4, w_word, w_gl, NGRAM_ANY_MISMATCH);
        leftcommon_score = leftcommonsubstring(w_word, w_gl);
      } else {
        //gl is lowercase already at this point
        ngram_score = ngram(4, word, gl, NGRAM_ANY_MISMATCH);
        leftcommon_score = leftcommonsubstring(word, gl.c_str());
      }
      gscore[i] =
          // length of longest common subsequent minus length difference
          2 * _lcs - abs((int)(n - len)) +
          // weight length of the left common substring
          leftcommon_score +
          // weight equal character positions
          (!nonbmp && commoncharacterpositions(word, gl.c_str(), &is_swap)
               ? 1
               : 0) +
          // swap character (not neighboring)
          ((is_swap) ? 10 : 0) +
          // ngram
          ngram_score +
          // weighted ngrams
          re +
          // different limit for dictionaries with PHONE rules
          (ph ? (re < len * fact ? -1000 : 0)
              : (re < (n + len) * fact ? -1000 : 0));
    }
  }

  bubblesort(&guess[0], &guessorig[0], &gscore[0], MAX_GUESS);

  // phonetic version
  if (ph)
    for (int i = 0; i < MAX_ROOTS; i++) {
      if (rootsphon[i]) {
        // lowering rootphon[i]
        std::string gl;
        int len;
        if (utf8) {
          len = u8_u16(w_gl, rootsphon[i]);
          mkallsmall_utf(w_gl, langnum);
          u16_u8(gl, w_gl);
        } else {
          gl.assign(rootsphon[i]);
          if (!nonbmp)
            mkallsmall(gl, csconv);
          len = strlen(rootsphon[i]);
        }

        // weight length of the left common substring
        int leftcommon_score;
        if (utf8)
          leftcommon_score = leftcommonsubstring(w_word, w_gl);
        else
          leftcommon_score = leftcommonsubstring(word, gl.c_str());
        // heuristic weigthing of ngram scores
        scoresphon[i] += 2 * lcslen(word, gl) - abs((int)(n - len)) +
                         leftcommon_score;
      }
    }

  if (ph)
    bubblesort(&rootsphon[0], NULL, &scoresphon[0], MAX_ROOTS);

  // copy over
  size_t oldns = wlst.size();

  int same = 0;
  for (int i = 0; i < MAX_GUESS; i++) {
    if (guess[i]) {
      if ((wlst.size() < oldns + maxngramsugs) && (wlst.size() < maxSug) &&
          (!same || (gscore[i] > 1000))) {
        int unique = 1;
        // leave only excellent suggestions, if exists
        if (gscore[i] > 1000)
          same = 1;
        else if (gscore[i] < -100) {
          same = 1;
          // keep the best ngram suggestions, unless in ONLYMAXDIFF mode
          if (wlst.size() > oldns || (pAMgr && pAMgr->get_onlymaxdiff())) {
            free(guess[i]);
            if (guessorig[i])
              free(guessorig[i]);
            continue;
          }
        }
        for (size_t j = 0; j < wlst.size(); ++j) {
          // don't suggest previous suggestions or a previous suggestion with
          // prefixes or affixes
          if ((!guessorig[i] && strstr(guess[i], wlst[j].c_str())) ||
              (guessorig[i] && strstr(guessorig[i], wlst[j].c_str())) ||
              // check forbidden words
              !checkword(guess[i], 0, NULL, NULL)) {
            unique = 0;
            break;
          }
        }
        if (unique) {
          if (guessorig[i]) {
            wlst.push_back(guessorig[i]);
          } else {
            wlst.push_back(guess[i]);
          }
        }
        free(guess[i]);
        if (guessorig[i])
          free(guessorig[i]);
      } else {
        free(guess[i]);
        if (guessorig[i])
          free(guessorig[i]);
      }
    }
  }

  oldns = wlst.size();
  if (ph)
    for (int i = 0; i < MAX_ROOTS; i++) {
      if (rootsphon[i]) {
        if ((wlst.size() < oldns + MAXPHONSUGS) && (wlst.size() < maxSug)) {
          int unique = 1;
          for (size_t j = 0; j < wlst.size(); ++j) {
            // don't suggest previous suggestions or a previous suggestion with
            // prefixes or affixes
            if (strstr(rootsphon[i], wlst[j].c_str()) ||
                // check forbidden words
                !checkword(rootsphon[i], 0, NULL, NULL)) {
              unique = 0;
              break;
            }
          }
          if (unique) {
            wlst.push_back(rootsphon[i]);
          }
        }
      }
    }

  if (nonbmp)
    utf8 = 1;
}

// see if a candidate suggestion is spelled correctly
// needs to check both root words and words with affixes

// obsolote MySpell-HU modifications:
// return value 2 and 3 marks compounding with hyphen (-)
// `3' marks roots without suffix
int SuggestMgr::checkword(const std::string& word,
                          int cpdsuggest,
                          int* timer,
                          clock_t* timelimit) {
  // check time limit
  if (timer) {
    (*timer)--;
    if (!(*timer) && timelimit) {
      if ((clock() - *timelimit) > TIMELIMIT)
        return 0;
      *timer = MAXPLUSTIMER;
    }
  }

  if (pAMgr) {
    struct hentry* rv = NULL;
    int nosuffix = 0;

    if (cpdsuggest == 1) {
      if (pAMgr->get_compound()) {
        struct hentry* rv2 = NULL;
        struct hentry* rwords[100];  // buffer for COMPOUND pattern checking
        rv = pAMgr->compound_check(word, 0, 0, 100, 0, NULL, (hentry**)&rwords, 0, 1, 0);  // EXT
        if (rv &&
            (!(rv2 = pAMgr->lookup(word.c_str())) || !rv2->astr ||
             !(TESTAFF(rv2->astr, pAMgr->get_forbiddenword(), rv2->alen) ||
               TESTAFF(rv2->astr, pAMgr->get_nosuggest(), rv2->alen))))
          return 3;  // XXX obsolote categorisation + only ICONV needs affix
                     // flag check?
      }
      return 0;
    }

    rv = pAMgr->lookup(word.c_str());

    if (rv) {
      if ((rv->astr) &&
          (TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen) ||
           TESTAFF(rv->astr, pAMgr->get_nosuggest(), rv->alen) ||
           TESTAFF(rv->astr, pAMgr->get_substandard(), rv->alen)))
        return 0;
      while (rv) {
        if (rv->astr &&
            (TESTAFF(rv->astr, pAMgr->get_needaffix(), rv->alen) ||
             TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
             TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen))) {
          rv = rv->next_homonym;
        } else
          break;
      }
    } else
      rv = pAMgr->prefix_check(word.c_str(), word.size(),
                               0);  // only prefix, and prefix + suffix XXX

    if (rv) {
      nosuffix = 1;
    } else {
      rv = pAMgr->suffix_check(word.c_str(), word.size(), 0, NULL,
                               FLAG_NULL, FLAG_NULL, IN_CPD_NOT);  // only suffix
    }

    if (!rv && pAMgr->have_contclass()) {
      rv = pAMgr->suffix_check_twosfx(word.c_str(), word.size(), 0, NULL, FLAG_NULL);
      if (!rv)
        rv = pAMgr->prefix_check_twosfx(word.c_str(), word.size(), 0, FLAG_NULL);
    }

    // check forbidden words
    if ((rv) && (rv->astr) &&
        (TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen) ||
         TESTAFF(rv->astr, ONLYUPCASEFLAG, rv->alen) ||
         TESTAFF(rv->astr, pAMgr->get_nosuggest(), rv->alen) ||
         TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen)))
      return 0;

    if (rv) {  // XXX obsolote
      if ((pAMgr->get_compoundflag()) &&
          TESTAFF(rv->astr, pAMgr->get_compoundflag(), rv->alen))
        return 2 + nosuffix;
      return 1;
    }
  }
  return 0;
}

int SuggestMgr::check_forbidden(const char* word, int len) {
  if (pAMgr) {
    struct hentry* rv = pAMgr->lookup(word);
    if (rv && rv->astr &&
        (TESTAFF(rv->astr, pAMgr->get_needaffix(), rv->alen) ||
         TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen)))
      rv = NULL;
    if (!(pAMgr->prefix_check(word, len, 1)))
      rv = pAMgr->suffix_check(word, len, 0, NULL,
                               FLAG_NULL, FLAG_NULL, IN_CPD_NOT);  // prefix+suffix, suffix
    // check forbidden words
    if ((rv) && (rv->astr) &&
        TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen))
      return 1;
  }
  return 0;
}

std::string SuggestMgr::suggest_morph(const std::string& in_w) {
  std::string result;

  struct hentry* rv = NULL;

  if (!pAMgr)
    return std::string();

  std::string w(in_w);

  // word reversing wrapper for complex prefixes
  if (complexprefixes) {
    if (utf8)
      reverseword_utf(w);
    else
      reverseword(w);
  }

  rv = pAMgr->lookup(w.c_str());

  while (rv) {
    if ((!rv->astr) ||
        !(TESTAFF(rv->astr, pAMgr->get_forbiddenword(), rv->alen) ||
          TESTAFF(rv->astr, pAMgr->get_needaffix(), rv->alen) ||
          TESTAFF(rv->astr, pAMgr->get_onlyincompound(), rv->alen))) {
      if (!HENTRY_FIND(rv, MORPH_STEM)) {
        result.push_back(MSEP_FLD);
        result.append(MORPH_STEM);
        result.append(w);
      }
      if (HENTRY_DATA(rv)) {
        result.push_back(MSEP_FLD);
        result.append(HENTRY_DATA2(rv));
      }
      result.push_back(MSEP_REC);
    }
    rv = rv->next_homonym;
  }

  std::string st = pAMgr->affix_check_morph(w.c_str(), w.size());
  if (!st.empty()) {
    result.append(st);
  }

  if (pAMgr->get_compound() && result.empty()) {
    struct hentry* rwords[100];  // buffer for COMPOUND pattern checking
    pAMgr->compound_check_morph(w.c_str(), w.size(), 0, 0, 100, 0, NULL, (hentry**)&rwords, 0, result,
                                NULL);
  }

  line_uniq(result, MSEP_REC);

  return result;
}

static int get_sfxcount(const char* morph) {
  if (!morph || !*morph)
    return 0;
  int n = 0;
  const char* old = morph;
  morph = strstr(morph, MORPH_DERI_SFX);
  if (!morph)
    morph = strstr(old, MORPH_INFL_SFX);
  if (!morph)
    morph = strstr(old, MORPH_TERM_SFX);
  while (morph) {
    n++;
    old = morph;
    morph = strstr(morph + 1, MORPH_DERI_SFX);
    if (!morph)
      morph = strstr(old + 1, MORPH_INFL_SFX);
    if (!morph)
      morph = strstr(old + 1, MORPH_TERM_SFX);
  }
  return n;
}

/* affixation */
std::string SuggestMgr::suggest_hentry_gen(hentry* rv, const char* pattern) {
  std::string result;
  int sfxcount = get_sfxcount(pattern);

  if (get_sfxcount(HENTRY_DATA(rv)) > sfxcount)
    return result;

  if (HENTRY_DATA(rv)) {
    std::string aff = pAMgr->morphgen(HENTRY_WORD(rv), rv->blen, rv->astr, rv->alen,
                                      HENTRY_DATA(rv), pattern, 0);
    if (!aff.empty()) {
      result.append(aff);
      result.push_back(MSEP_REC);
    }
  }

  // check all allomorphs
  char* p = NULL;
  if (HENTRY_DATA(rv))
    p = (char*)strstr(HENTRY_DATA2(rv), MORPH_ALLOMORPH);
  while (p) {
    p += MORPH_TAG_LEN;
    int plen = fieldlen(p);
    std::string allomorph(p, plen);
    struct hentry* rv2 = pAMgr->lookup(allomorph.c_str());
    while (rv2) {
      //            if (HENTRY_DATA(rv2) && get_sfxcount(HENTRY_DATA(rv2)) <=
      //            sfxcount) {
      if (HENTRY_DATA(rv2)) {
        char* st = (char*)strstr(HENTRY_DATA2(rv2), MORPH_STEM);
        if (st && (strncmp(st + MORPH_TAG_LEN, HENTRY_WORD(rv),
                           fieldlen(st + MORPH_TAG_LEN)) == 0)) {
          std::string aff = pAMgr->morphgen(HENTRY_WORD(rv2), rv2->blen, rv2->astr,
                                            rv2->alen, HENTRY_DATA(rv2), pattern, 0);
          if (!aff.empty()) {
            result.append(aff);
            result.push_back(MSEP_REC);
          }
        }
      }
      rv2 = rv2->next_homonym;
    }
    p = strstr(p + plen, MORPH_ALLOMORPH);
  }

  return result;
}

std::string SuggestMgr::suggest_gen(const std::vector<std::string>& desc, const std::string& in_pattern) {
  if (desc.empty() || !pAMgr)
    return std::string();

  const char* pattern = in_pattern.c_str();
  std::string result2;
  std::string newpattern;
  struct hentry* rv = NULL;

  // search affixed forms with and without derivational suffixes
  while (1) {
    for (size_t k = 0; k < desc.size(); ++k) {
      std::string result;

      // add compound word parts (except the last one)
      const char* s = desc[k].c_str();
      const char* part = strstr(s, MORPH_PART);
      if (part) {
        const char* nextpart = strstr(part + 1, MORPH_PART);
        while (nextpart) {
          std::string field;
          copy_field(field, part, MORPH_PART);
          result.append(field);
          part = nextpart;
          nextpart = strstr(part + 1, MORPH_PART);
        }
        s = part;
      }

      std::string tok(s);
      size_t pos = tok.find(" | ");
      while (pos != std::string::npos) {
        tok[pos + 1] = MSEP_ALT;
        pos = tok.find(" | ", pos);
      }
      std::vector<std::string> pl = line_tok(tok, MSEP_ALT);
      for (size_t i = 0; i < pl.size(); ++i) {
        // remove inflectional and terminal suffixes
        size_t is = pl[i].find(MORPH_INFL_SFX);
        if (is != std::string::npos)
          pl[i].resize(is);
        size_t ts = pl[i].find(MORPH_TERM_SFX);
        while (ts != std::string::npos) {
          pl[i][ts] = '_';
          ts = pl[i].find(MORPH_TERM_SFX);
        }
        const char* st = strstr(s, MORPH_STEM);
        if (st) {
          copy_field(tok, st, MORPH_STEM);
          rv = pAMgr->lookup(tok.c_str());
          while (rv) {
            std::string newpat(pl[i]);
            newpat.append(pattern);
            std::string sg = suggest_hentry_gen(rv, newpat.c_str());
            if (sg.empty())
              sg = suggest_hentry_gen(rv, pattern);
            if (!sg.empty()) {
              std::vector<std::string> gen = line_tok(sg, MSEP_REC);
              for (size_t j = 0; j < gen.size(); ++j) {
                result2.push_back(MSEP_REC);
                result2.append(result);
                if (pl[i].find(MORPH_SURF_PFX) != std::string::npos) {
                  std::string field;
                  copy_field(field, pl[i], MORPH_SURF_PFX);
                  result2.append(field);
                }
                result2.append(gen[j]);
              }
            }
            rv = rv->next_homonym;
          }
        }
      }
    }

    if (!result2.empty() || !strstr(pattern, MORPH_DERI_SFX))
      break;

    newpattern.assign(pattern);
    mystrrep(newpattern, MORPH_DERI_SFX, MORPH_TERM_SFX);
    pattern = newpattern.c_str();
  }
  return result2;
}

// generate an n-gram score comparing s1 and s2, UTF16 version
int SuggestMgr::ngram(int n,
                      const std::vector<w_char>& su1,
                      const std::vector<w_char>& su2,
                      int opt) {
  int nscore = 0;
  int ns;
  int l1;
  int l2;
  int test = 0;

  l1 = su1.size();
  l2 = su2.size();
  if (l2 == 0)
    return 0;
  for (int j = 1; j <= n; j++) {
    ns = 0;
    for (int i = 0; i <= (l1 - j); i++) {
      int k = 0;
      for (int l = 0; l <= (l2 - j); l++) {
        for (k = 0; k < j; k++) {
          const w_char& c1 = su1[i + k];
          const w_char& c2 = su2[l + k];
          if ((c1.l != c2.l) || (c1.h != c2.h))
            break;
        }
        if (k == j) {
          ns++;
          break;
        }
      }
      if (k != j && opt & NGRAM_WEIGHTED) {
        ns--;
        test++;
        if (i == 0 || i == l1 - j)
          ns--;  // side weight
      }
    }
    nscore = nscore + ns;
    if (ns < 2 && !(opt & NGRAM_WEIGHTED))
      break;
  }

  ns = 0;
  if (opt & NGRAM_LONGER_WORSE)
    ns = (l2 - l1) - 2;
  if (opt & NGRAM_ANY_MISMATCH)
    ns = abs(l2 - l1) - 2;
  ns = (nscore - ((ns > 0) ? ns : 0));
  return ns;
}

// generate an n-gram score comparing s1 and s2, non-UTF16 version
int SuggestMgr::ngram(int n,
                      const std::string& s1,
                      const std::string& s2,
                      int opt) {
  int nscore = 0;
  int ns;
  int l1;
  int l2;
  int test = 0;

  l2 = s2.size();
  if (l2 == 0)
    return 0;
  l1 = s1.size();
  for (int j = 1; j <= n; j++) {
    ns = 0;
    for (int i = 0; i <= (l1 - j); i++) {
      //s2 is haystack, s1[i..i+j) is needle
      if (s2.find(s1.c_str()+i, 0, j) != std::string::npos) {
        ns++;
      } else if (opt & NGRAM_WEIGHTED) {
        ns--;
        test++;
        if (i == 0 || i == l1 - j)
          ns--;  // side weight
      }
    }
    nscore = nscore + ns;
    if (ns < 2 && !(opt & NGRAM_WEIGHTED))
      break;
  }

  ns = 0;
  if (opt & NGRAM_LONGER_WORSE)
    ns = (l2 - l1) - 2;
  if (opt & NGRAM_ANY_MISMATCH)
    ns = abs(l2 - l1) - 2;
  ns = (nscore - ((ns > 0) ? ns : 0));
  return ns;
}

// length of the left common substring of s1 and (decapitalised) s2, UTF version
int SuggestMgr::leftcommonsubstring(
    const std::vector<w_char>& su1,
    const std::vector<w_char>& su2) {
  int l1 = su1.size();
  int l2 = su2.size();
  // decapitalize dictionary word
  if (complexprefixes) {
    if (su1[l1 - 1] == su2[l2 - 1])
      return 1;
  } else {
    unsigned short idx = su2.empty() ? 0 : (su2[0].h << 8) + su2[0].l;
    unsigned short otheridx = su1.empty() ? 0 : (su1[0].h << 8) + su1[0].l;
    if (otheridx != idx && (otheridx != unicodetolower(idx, langnum)))
      return 0;
    int i;
    for (i = 1; (i < l1) && (i < l2) && (su1[i].l == su2[i].l) &&
                (su1[i].h == su2[i].h);
         i++)
      ;
    return i;
  }
  return 0;
}

// length of the left common substring of s1 and (decapitalised) s2, non-UTF
int SuggestMgr::leftcommonsubstring(
    const char* s1,
    const char* s2) {
  if (complexprefixes) {
    int l1 = strlen(s1);
    int l2 = strlen(s2);
    if (l1 <= l2 && s2[l1 - 1] == s2[l2 - 1])
      return 1;
  } else if (csconv) {
    const char* olds = s1;
    // decapitalise dictionary word
    if ((*s1 != *s2) && (*s1 != csconv[((unsigned char)*s2)].clower))
      return 0;
    do {
      s1++;
      s2++;
    } while ((*s1 == *s2) && (*s1 != '\0'));
    return (int)(s1 - olds);
  }
  return 0;
}

int SuggestMgr::commoncharacterpositions(const char* s1,
                                         const char* s2,
                                         int* is_swap) {
  int num = 0;
  int diff = 0;
  int diffpos[2];
  *is_swap = 0;
  if (utf8) {
    std::vector<w_char> su1;
    std::vector<w_char> su2;
    int l1 = u8_u16(su1, s1);
    int l2 = u8_u16(su2, s2);

    if (l1 <= 0 || l2 <= 0)
      return 0;

    // decapitalize dictionary word
    if (complexprefixes) {
      su2[l2 - 1] = lower_utf(su2[l2 - 1], langnum);
    } else {
      su2[0] = lower_utf(su2[0], langnum);
    }
    for (int i = 0; (i < l1) && (i < l2); i++) {
      if (su1[i] == su2[i]) {
        num++;
      } else {
        if (diff < 2)
          diffpos[diff] = i;
        diff++;
      }
    }
    if ((diff == 2) && (l1 == l2) &&
        (su1[diffpos[0]] == su2[diffpos[1]]) &&
        (su1[diffpos[1]] == su2[diffpos[0]]))
      *is_swap = 1;
  } else {
    size_t i;
    std::string t(s2);
    // decapitalize dictionary word
    if (complexprefixes) {
      size_t l2 = t.size();
      t[l2 - 1] = csconv[(unsigned char)t[l2 - 1]].clower;
    } else {
      mkallsmall(t, csconv);
    }
    for (i = 0; i < t.size() && (*(s1 + i) != 0); ++i) {
      if (*(s1 + i) == t[i]) {
        num++;
      } else {
        if (diff < 2)
          diffpos[diff] = i;
        diff++;
      }
    }
    if ((diff == 2) && (*(s1 + i) == 0) && i == t.size() &&
        (*(s1 + diffpos[0]) == t[diffpos[1]]) &&
        (*(s1 + diffpos[1]) == t[diffpos[0]]))
      *is_swap = 1;
  }
  return num;
}

int SuggestMgr::mystrlen(const char* word) {
  if (utf8) {
    std::vector<w_char> w;
    return u8_u16(w, word);
  } else
    return strlen(word);
}

// sort in decreasing order of score
void SuggestMgr::bubblesort(char** rword, char** rword2, int* rsc, int n) {
  int m = 1;
  while (m < n) {
    int j = m;
    while (j > 0) {
      if (rsc[j - 1] < rsc[j]) {
        int sctmp = rsc[j - 1];
        char* wdtmp = rword[j - 1];
        rsc[j - 1] = rsc[j];
        rword[j - 1] = rword[j];
        rsc[j] = sctmp;
        rword[j] = wdtmp;
        if (rword2) {
          wdtmp = rword2[j - 1];
          rword2[j - 1] = rword2[j];
          rword2[j] = wdtmp;
        }
        j--;
      } else
        break;
    }
    m++;
  }
  return;
}

// longest common subsequence
void SuggestMgr::lcs(const char* s,
                     const char* s2,
                     int* l1,
                     int* l2,
                     char** result) {
  int n, m;
  std::vector<w_char> su;
  std::vector<w_char> su2;
  char* b;
  char* c;
  int i;
  int j;
  if (utf8) {
    m = u8_u16(su, s);
    n = u8_u16(su2, s2);
  } else {
    m = strlen(s);
    n = strlen(s2);
  }
  c = (char *) calloc(m + 1, n + 1);
  b = (char *) calloc(m + 1, n + 1);
  if (!c || !b) {
    if (c)
      free(c);
    if (b)
      free(b);
    *result = NULL;
    return;
  }
  for (i = 1; i <= m; i++) {
    for (j = 1; j <= n; j++) {
      if (((utf8) && (su[i - 1] == su2[j - 1])) ||
          ((!utf8) && (s[i - 1] == s2[j - 1]))) {
        c[i * (n + 1) + j] = c[(i - 1) * (n + 1) + j - 1] + 1;
        b[i * (n + 1) + j] = LCS_UPLEFT;
      } else if (c[(i - 1) * (n + 1) + j] >= c[i * (n + 1) + j - 1]) {
        c[i * (n + 1) + j] = c[(i - 1) * (n + 1) + j];
        b[i * (n + 1) + j] = LCS_UP;
      } else {
        c[i * (n + 1) + j] = c[i * (n + 1) + j - 1];
        b[i * (n + 1) + j] = LCS_LEFT;
      }
    }
  }
  *result = b;
  free(c);
  *l1 = m;
  *l2 = n;
}

int SuggestMgr::lcslen(const char* s, const char* s2) {
  int m;
  int n;
  int i;
  int j;
  char* result;
  int len = 0;
  lcs(s, s2, &m, &n, &result);
  if (!result)
    return 0;
  i = m;
  j = n;
  while ((i != 0) && (j != 0)) {
    if (result[i * (n + 1) + j] == LCS_UPLEFT) {
      len++;
      i--;
      j--;
    } else if (result[i * (n + 1) + j] == LCS_UP) {
      i--;
    } else
      j--;
  }
  free(result);
  return len;
}

int SuggestMgr::lcslen(const std::string& s, const std::string& s2) {
  return lcslen(s.c_str(), s2.c_str());
}
