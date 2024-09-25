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
#include <limits>
#include <sstream>

#include "hashmgr.hxx"
#include "csutil.hxx"
#include "atypes.hxx"
#include "langnum.hxx"

// build a hash table from a munched word list

#ifdef HUNSPELL_CHROME_CLIENT
HashMgr::HashMgr(hunspell::BDictReader* reader)
    : bdict_reader(reader),
#else
HashMgr::HashMgr(const char* tpath, const char* apath, const char* key)
    :
#endif
      tablesize(0),
      tableptr(NULL),
      flag_mode(FLAG_CHAR),
      complexprefixes(0),
      utf8(0),
      forbiddenword(FORBIDDENWORD)  // forbidden word signing flag
      ,
      numaliasf(0),
      aliasf(NULL),
      aliasflen(0),
      numaliasm(0),
      aliasm(NULL) {
  langnum = 0;
  csconv = 0;
#ifdef HUNSPELL_CHROME_CLIENT
  // No tables to load, just the AF lines.
  load_config(NULL, NULL);
  int ec = LoadAFLines();
#else
  load_config(apath, key);
  int ec = load_tables(tpath, key);
#endif
  if (ec) {
    /* error condition - what should we do here */
    HUNSPELL_WARNING(stderr, "Hash Manager Error : %d\n", ec);
    free(tableptr);
    //keep tablesize to 1 to fix possible division with zero
    tablesize = 1;
    tableptr = (struct hentry**)calloc(tablesize, sizeof(struct hentry*));
    if (!tableptr) {
      tablesize = 0;
    }
  }
}

HashMgr::~HashMgr() {
  if (tableptr) {
    // now pass through hash table freeing up everything
    // go through column by column of the table
    for (int i = 0; i < tablesize; i++) {
      struct hentry* pt = tableptr[i];
      struct hentry* nt = NULL;
      while (pt) {
        nt = pt->next;
        if (pt->astr &&
            (!aliasf || TESTAFF(pt->astr, ONLYUPCASEFLAG, pt->alen)))
          free(pt->astr);
        free(pt);
        pt = nt;
      }
    }
    free(tableptr);
  }
  tablesize = 0;

  if (aliasf) {
    for (int j = 0; j < (numaliasf); j++)
      free(aliasf[j]);
    free(aliasf);
    aliasf = NULL;
    if (aliasflen) {
      free(aliasflen);
      aliasflen = NULL;
    }
  }
  if (aliasm) {
    for (int j = 0; j < (numaliasm); j++)
      free(aliasm[j]);
    free(aliasm);
    aliasm = NULL;
  }

#ifndef OPENOFFICEORG
#ifndef MOZILLA_CLIENT
  if (utf8)
    free_utf_tbl();
#endif
#endif

#ifdef HUNSPELL_CHROME_CLIENT
  EmptyHentryCache();
  for (std::vector<std::string*>::iterator it = pointer_to_strings_.begin();
       it != pointer_to_strings_.end(); ++it) {
    delete *it;
  }
#endif
#ifdef MOZILLA_CLIENT
  delete[] csconv;
#endif
}

#ifdef HUNSPELL_CHROME_CLIENT
void HashMgr::EmptyHentryCache() {
  // We need to delete each cache entry, and each additional one in the linked
  // list of homonyms.
  for (HEntryCache::iterator i = hentry_cache.begin();
       i != hentry_cache.end(); ++i) {
    hentry* cur = i->second;
    while (cur) {
      hentry* next = cur->next_homonym;
      DeleteHashEntry(cur);
      cur = next;
    }
  }
  hentry_cache.clear();
}
#endif

// lookup a root word in the hashtable

struct hentry* HashMgr::lookup(const char* word) const {
#ifdef HUNSPELL_CHROME_CLIENT
  int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];
  int affix_count = bdict_reader->FindWord(word, affix_ids);
  if (affix_count == 0) { // look for custom added word
    std::map<std::string_view, int>::const_iterator iter = 
      custom_word_to_affix_id_map_.find(word);
    if (iter != custom_word_to_affix_id_map_.end()) {
      affix_count = 1;
      affix_ids[0] = iter->second;
    }
  }

  static const int kMaxWordLen = 128;
  static char word_buf[kMaxWordLen];
  // To take account of null-termination, we use upto 127.
  strncpy(word_buf, word, kMaxWordLen - 1);

  return AffixIDsToHentry(word_buf, affix_ids, affix_count);
#else
  struct hentry* dp;
  if (tableptr) {
    dp = tableptr[hash(word)];
    if (!dp)
      return NULL;
    for (; dp != NULL; dp = dp->next) {
      if (strcmp(word, dp->word) == 0)
        return dp;
    }
  }
  return NULL;
#endif
}

// add a word to the hash table (private)
int HashMgr::add_word(const std::string& in_word,
                      int wcl,
                      unsigned short* aff,
                      int al,
                      const std::string* in_desc,
                      bool onlyupcase,
                      int captype) {
// TODO: The following 40 lines or so are actually new. Should they be included?
#ifndef HUNSPELL_CHROME_CLIENT
  const std::string* word = &in_word;
  const std::string* desc = in_desc;

  std::string *word_copy = NULL;
  std::string *desc_copy = NULL;
  if ((!ignorechars.empty() && !has_no_ignored_chars(in_word, ignorechars)) || complexprefixes) {
    word_copy = new std::string(in_word);

    if (!ignorechars.empty()) {
      if (utf8) {
        wcl = remove_ignored_chars_utf(*word_copy, ignorechars_utf16);
      } else {
        remove_ignored_chars(*word_copy, ignorechars);
      }
    }

    if (complexprefixes) {
      if (utf8)
        wcl = reverseword_utf(*word_copy);
      else
        reverseword(*word_copy);

      if (in_desc && !aliasm) {
        desc_copy = new std::string(*in_desc);

        if (complexprefixes) {
          if (utf8)
            reverseword_utf(*desc_copy);
          else
            reverseword(*desc_copy);
        }
        desc = desc_copy;
      }
    }

    word = word_copy;
  }

  bool upcasehomonym = false;
  int descl = desc ? (aliasm ? sizeof(char*) : desc->size() + 1) : 0;
  // variable-length hash record with word and optional fields
  struct hentry* hp =
      (struct hentry*)malloc(sizeof(struct hentry) + word->size() + descl);
  if (!hp) {
    delete desc_copy;
    delete word_copy;
    return 1;
  }

  char* hpw = hp->word;
  strcpy(hpw, word->c_str());

  int i = hash(hpw);

  hp->blen = (unsigned char)word->size();
  hp->clen = (unsigned char)wcl;
  hp->alen = (short)al;
  hp->astr = aff;
  hp->next = NULL;
  hp->next_homonym = NULL;
  hp->var = (captype == INITCAP) ? H_OPT_INITCAP : 0;

  // store the description string or its pointer
  if (desc) {
    hp->var += H_OPT;
    if (aliasm) {
      hp->var += H_OPT_ALIASM;
      store_pointer(hpw + word->size() + 1, get_aliasm(atoi(desc->c_str())));
    } else {
      strcpy(hpw + word->size() + 1, desc->c_str());
    }
    if (strstr(HENTRY_DATA(hp), MORPH_PHON)) {
      hp->var += H_OPT_PHON;
      // store ph: fields (pronounciation, misspellings, old orthography etc.)
      // of a morphological description in reptable to use in REP replacements.
      if (reptable.capacity() < (unsigned int)(tablesize/MORPH_PHON_RATIO))
          reptable.reserve(tablesize/MORPH_PHON_RATIO);
      std::string fields = HENTRY_DATA(hp);
      std::string::const_iterator iter = fields.begin();
      std::string::const_iterator start_piece = mystrsep(fields, iter);
      while (start_piece != fields.end()) {
        if (std::string(start_piece, iter).find(MORPH_PHON) == 0) {
          std::string ph = std::string(start_piece, iter).substr(sizeof MORPH_PHON - 1);
          if (ph.size() > 0) {
            std::vector<w_char> w;
            size_t strippatt;
            std::string wordpart;
            // dictionary based REP replacement, separated by "->"
            // for example "pretty ph:prity ph:priti->pretti" to handle
            // both prity -> pretty and pritier -> prettiest suggestions.
            if (((strippatt = ph.find("->")) != std::string::npos) &&
                    (strippatt > 0) && (strippatt < ph.size() - 2)) {
                wordpart = ph.substr(strippatt + 2);
                ph.erase(ph.begin() + strippatt, ph.end());
            } else
                wordpart = in_word;
            // when the ph: field ends with the character *,
            // strip last character of the pattern and the replacement
            // to match in REP suggestions also at character changes,
            // for example, "pretty ph:prity*" results "prit->prett"
            // REP replacement instead of "prity->pretty", to get
            // prity->pretty and pritiest->prettiest suggestions.
            if (ph.at(ph.size()-1) == '*') {
              strippatt = 1;
              size_t stripword = 0;
              if (utf8) {
                while ((strippatt < ph.size()) &&
                  ((ph.at(ph.size()-strippatt-1) & 0xc0) == 0x80))
                     ++strippatt;
                while ((stripword < wordpart.size()) &&
                  ((wordpart.at(wordpart.size()-stripword-1) & 0xc0) == 0x80))
                     ++stripword;
              }
              ++strippatt;
              ++stripword;
              if ((ph.size() > strippatt) && (wordpart.size() > stripword)) {
                ph.erase(ph.size()-strippatt, strippatt);
                wordpart.erase(in_word.size()-stripword, stripword);
              }
            }
            // capitalize lowercase pattern for capitalized words to support
            // good suggestions also for capitalized misspellings, eg.
            // Wednesday ph:wendsay
            // results wendsay -> Wednesday and Wendsay -> Wednesday, too.
            if (captype==INITCAP) {
              std::string ph_capitalized;
              if (utf8) {
                u8_u16(w, ph);
                if (get_captype_utf8(w, langnum) == NOCAP) {
                  mkinitcap_utf(w, langnum);
                  u16_u8(ph_capitalized, w);
                }
              } else if (get_captype(ph, csconv) == NOCAP)
                  mkinitcap(ph_capitalized, csconv);

              if (ph_capitalized.size() > 0) {
                // add also lowercase word in the case of German or
                // Hungarian to support lowercase suggestions lowercased by
                // compound word generation or derivational suffixes
                // (for example by adjectival suffix "-i" of geographical
                // names in Hungarian:
                // Massachusetts ph:messzecsuzec
                // messzecsuzeci -> massachusettsi (adjective)
                // For lowercasing by conditional PFX rules, see
                // tests/germancompounding test example or the
                // Hungarian dictionary.)
                if (langnum == LANG_de || langnum == LANG_hu) {
                  std::string wordpart_lower(wordpart);
                  if (utf8) {
                    u8_u16(w, wordpart_lower);
                    mkallsmall_utf(w, langnum);
                    u16_u8(wordpart_lower, w);
                  } else {
                    mkallsmall(wordpart_lower, csconv);
                  }
                  reptable.push_back(replentry());
                  reptable.back().pattern.assign(ph);
                  reptable.back().outstrings[0].assign(wordpart_lower);
                }
                reptable.push_back(replentry());
                reptable.back().pattern.assign(ph_capitalized);
                reptable.back().outstrings[0].assign(wordpart);
              }
            }
            reptable.push_back(replentry());
            reptable.back().pattern.assign(ph);
            reptable.back().outstrings[0].assign(wordpart);
          }
        }
        start_piece = mystrsep(fields, iter);
      }
    }
  }

  struct hentry* dp = tableptr[i];
  if (!dp) {
    tableptr[i] = hp;
    delete desc_copy;
    delete word_copy;
    return 0;
  }
  while (dp->next != NULL) {
    if ((!dp->next_homonym) && (strcmp(hp->word, dp->word) == 0)) {
      // remove hidden onlyupcase homonym
      if (!onlyupcase) {
        if ((dp->astr) && TESTAFF(dp->astr, ONLYUPCASEFLAG, dp->alen)) {
          free(dp->astr);
          dp->astr = hp->astr;
          dp->alen = hp->alen;
          free(hp);
          delete desc_copy;
          delete word_copy;
          return 0;
        } else {
          dp->next_homonym = hp;
        }
      } else {
        upcasehomonym = true;
      }
    }
    dp = dp->next;
  }
  if (strcmp(hp->word, dp->word) == 0) {
    // remove hidden onlyupcase homonym
    if (!onlyupcase) {
      if ((dp->astr) && TESTAFF(dp->astr, ONLYUPCASEFLAG, dp->alen)) {
        free(dp->astr);
        dp->astr = hp->astr;
        dp->alen = hp->alen;
        free(hp);
        delete desc_copy;
        delete word_copy;
        return 0;
      } else {
        dp->next_homonym = hp;
      }
    } else {
      upcasehomonym = true;
    }
  }
  if (!upcasehomonym) {
    dp->next = hp;
  } else {
    // remove hidden onlyupcase homonym
    if (hp->astr)
      free(hp->astr);
    free(hp);
  }

  delete desc_copy;
  delete word_copy;
#else
  std::map<std::string_view, int>::iterator iter =
      custom_word_to_affix_id_map_.find(in_word);
  if (iter == custom_word_to_affix_id_map_.end()) {  // word needs to be added
    std::string* new_string_word = new std::string(in_word);
    pointer_to_strings_.push_back(new_string_word);
    std::string_view sp(*(new_string_word));
    custom_word_to_affix_id_map_[sp] = 0; // no affixes for custom words
    return 1;
  }
#endif
  return 0;
}

int HashMgr::add_hidden_capitalized_word(const std::string& word,
                                         int wcl,
                                         unsigned short* flags,
                                         int flagslen,
                                         const std::string* dp,
                                         int captype) {
  if (flags == NULL)
    flagslen = 0;

  // add inner capitalized forms to handle the following allcap forms:
  // Mixed caps: OpenOffice.org -> OPENOFFICE.ORG
  // Allcaps with suffixes: CIA's -> CIA'S
  if (((captype == HUHCAP) || (captype == HUHINITCAP) ||
       ((captype == ALLCAP) && (flagslen != 0))) &&
      !((flagslen != 0) && TESTAFF(flags, forbiddenword, flagslen))) {
    unsigned short* flags2 =
        (unsigned short*)malloc(sizeof(unsigned short) * (flagslen + 1));
    if (!flags2)
      return 1;
    if (flagslen)
      memcpy(flags2, flags, flagslen * sizeof(unsigned short));
    flags2[flagslen] = ONLYUPCASEFLAG;
    if (utf8) {
      std::string st;
      std::vector<w_char> w;
      u8_u16(w, word);
      mkallsmall_utf(w, langnum);
      mkinitcap_utf(w, langnum);
      u16_u8(st, w);
      return add_word(st, wcl, flags2, flagslen + 1, dp, true, INITCAP);
    } else {
      std::string new_word(word);
      mkallsmall(new_word, csconv);
      mkinitcap(new_word, csconv);
      int ret = add_word(new_word, wcl, flags2, flagslen + 1, dp, true, INITCAP);
      return ret;
    }
  }
  return 0;
}

// detect captype and modify word length for UTF-8 encoding
int HashMgr::get_clen_and_captype(const std::string& word, int* captype, std::vector<w_char> &workbuf) {
  int len;
  if (utf8) {
    len = u8_u16(workbuf, word);
    *captype = get_captype_utf8(workbuf, langnum);
  } else {
    len = word.size();
    *captype = get_captype(word, csconv);
  }
  return len;
}

int HashMgr::get_clen_and_captype(const std::string& word, int* captype) {
  std::vector<w_char> workbuf;
  return get_clen_and_captype(word, captype, workbuf);
}

// remove word (personal dictionary function for standalone applications)
int HashMgr::remove(const std::string& word) {
#ifdef HUNSPELL_CHROME_CLIENT
  std::map<std::string_view, int>::iterator iter =
      custom_word_to_affix_id_map_.find(word);
  if (iter != custom_word_to_affix_id_map_.end())
      custom_word_to_affix_id_map_.erase(iter);
#else
  struct hentry* dp = lookup(word.c_str());
  while (dp) {
    if (dp->alen == 0 || !TESTAFF(dp->astr, forbiddenword, dp->alen)) {
      unsigned short* flags =
          (unsigned short*)malloc(sizeof(unsigned short) * (dp->alen + 1));
      if (!flags)
        return 1;
      for (int i = 0; i < dp->alen; i++)
        flags[i] = dp->astr[i];
      flags[dp->alen] = forbiddenword;
      free(dp->astr);
      dp->astr = flags;
      dp->alen++;
      std::sort(flags, flags + dp->alen);
    }
    dp = dp->next_homonym;
  }
#endif
  return 0;
}

/* remove forbidden flag to add a personal word to the hash */
int HashMgr::remove_forbidden_flag(const std::string& word) {
  struct hentry* dp = lookup(word.c_str());
  if (!dp)
    return 1;
  while (dp) {
    if (dp->astr && TESTAFF(dp->astr, forbiddenword, dp->alen))
      dp->alen = 0;  // XXX forbidden words of personal dic.
    dp = dp->next_homonym;
  }
  return 0;
}

// add a custom dic. word to the hash table (public)
int HashMgr::add(const std::string& word) {
  if (remove_forbidden_flag(word)) {
    int captype;
    int al = 0;
    unsigned short* flags = NULL;
    int wcl = get_clen_and_captype(word, &captype);
    add_word(word, wcl, flags, al, NULL, false, captype);
    return add_hidden_capitalized_word(word, wcl, flags, al, NULL,
                                       captype);
  }
  return 0;
}

int HashMgr::add_with_affix(const std::string& word, const std::string& example) {
  // detect captype and modify word length for UTF-8 encoding
  struct hentry* dp = lookup(example.c_str());
  remove_forbidden_flag(word);
  if (dp && dp->astr) {
    int captype;
    int wcl = get_clen_and_captype(word, &captype);
    if (aliasf) {
      add_word(word, wcl, dp->astr, dp->alen, NULL, false, captype);
    } else {
      unsigned short* flags =
          (unsigned short*)malloc(dp->alen * sizeof(unsigned short));
      if (flags) {
        memcpy((void*)flags, (void*)dp->astr,
               dp->alen * sizeof(unsigned short));
        add_word(word, wcl, flags, dp->alen, NULL, false, captype);
      } else
        return 1;
    }
    return add_hidden_capitalized_word(word, wcl, dp->astr,
                                       dp->alen, NULL, captype);
  }
  return 1;
}

// walk the hash table entry by entry - null at end
// initialize: col=-1; hp = NULL; hp = walk_hashtable(&col, hp);
struct hentry* HashMgr::walk_hashtable(int& col, struct hentry* hp) const {
#ifdef HUNSPELL_CHROME_CLIENT
  // Return NULL if dictionary is not valid.
  if (!bdict_reader->IsValid())
    return NULL;

  // This function is only ever called by one place and not nested. We can
  // therefore keep static state between calls and use |col| as a "reset" flag
  // to avoid changing the API. It is set to -1 for the first call.
  // Allocate the iterator on the heap to prevent an exit time destructor.
  static hunspell::WordIterator& word_iterator =
      *new hunspell::WordIterator(bdict_reader->GetAllWordIterator());
  if (col < 0) {
    col = 1;
    word_iterator = bdict_reader->GetAllWordIterator();
  }

  int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];
  static const int kMaxWordLen = 128;
  static char word[kMaxWordLen];
  int affix_count = word_iterator.Advance(word, kMaxWordLen, affix_ids);
  if (affix_count == 0)
    return NULL;
  short word_len = static_cast<short>(strlen(word));

  // Since hunspell 1.2.8, an hentry struct becomes a variable-length struct,
  // i.e. a struct which uses its array 'word[1]' as a variable-length array.
  // As noted above, this function is not nested. So, we just use a static
  // struct which consists of an hentry and a char[kMaxWordLen], and initialize
  // the static struct and return it for now.
  // No need to create linked lists for the extra affixes.
  static struct {
    hentry entry;
    char word[kMaxWordLen];
  } hash_entry;

  return InitHashEntry(&hash_entry.entry, sizeof(hash_entry),
                       &word[0], word_len, affix_ids[0]);
#else
  if (hp && hp->next != NULL)
    return hp->next;
  for (col++; col < tablesize; col++) {
    if (tableptr[col])
      return tableptr[col];
  }
  // null at end and reset to start
  col = -1;
  return NULL;
#endif
}

// load a munched word list and build a hash table on the fly
int HashMgr::load_tables(const char* tpath, const char* key) {
#ifndef HUNSPELL_CHROME_CLIENT
  // open dictionary file
  FileMgr* dict = new FileMgr(tpath, key);
  if (dict == NULL)
    return 1;

  // first read the first line of file to get hash table size */
  std::string ts;
  if (!dict->getline(ts)) {
    HUNSPELL_WARNING(stderr, "error: empty dic file %s\n", tpath);
    delete dict;
    return 2;
  }
  mychomp(ts);

  /* remove byte order mark */
  if (ts.compare(0, 3, "\xEF\xBB\xBF", 3) == 0) {
    ts.erase(0, 3);
  }

  tablesize = atoi(ts.c_str());

  int nExtra = 5 + USERWORD;

  if (tablesize <= 0 ||
      (tablesize >= (std::numeric_limits<int>::max() - 1 - nExtra) /
                        int(sizeof(struct hentry*)))) {
    HUNSPELL_WARNING(
        stderr, "error: line 1: missing or bad word count in the dic file\n");
    delete dict;
    return 4;
  }
  tablesize += nExtra;
  if ((tablesize % 2) == 0)
    tablesize++;

  // allocate the hash table
  tableptr = (struct hentry**)calloc(tablesize, sizeof(struct hentry*));
  if (!tableptr) {
    delete dict;
    return 3;
  }

  // loop through all words on much list and add to hash
  // table and create word and affix strings

  std::vector<w_char> workbuf;

  while (dict->getline(ts)) {
    mychomp(ts);
    // split each line into word and morphological description
    size_t dp_pos = 0;
    while ((dp_pos = ts.find(':', dp_pos)) != std::string::npos) {
      if ((dp_pos > 3) && (ts[dp_pos - 3] == ' ' || ts[dp_pos - 3] == '\t')) {
        for (dp_pos -= 3; dp_pos > 0 && (ts[dp_pos-1] == ' ' || ts[dp_pos-1] == '\t'); --dp_pos)
          ;
        if (dp_pos == 0) {  // missing word
          dp_pos = std::string::npos;
        } else {
          ++dp_pos;
        }
        break;
      }
      ++dp_pos;
    }

    // tabulator is the old morphological field separator
    size_t dp2_pos = ts.find('\t');
    if (dp2_pos != std::string::npos && (dp_pos == std::string::npos || dp2_pos < dp_pos)) {
      dp_pos = dp2_pos + 1;
    }

    std::string dp;
    if (dp_pos != std::string::npos) {
      dp.assign(ts.substr(dp_pos));
      ts.resize(dp_pos - 1);
    }

    // split each line into word and affix char strings
    // "\/" signs slash in words (not affix separator)
    // "/" at beginning of the line is word character (not affix separator)
    size_t ap_pos = ts.find('/');
    while (ap_pos != std::string::npos) {
      if (ap_pos == 0) {
        ++ap_pos;
        continue;
      } else if (ts[ap_pos - 1] != '\\')
        break;
      // replace "\/" with "/"
      ts.erase(ap_pos - 1, 1);
      ap_pos = ts.find('/', ap_pos);
    }

    unsigned short* flags;
    int al;
    if (ap_pos != std::string::npos && ap_pos != ts.size()) {
      std::string ap(ts.substr(ap_pos + 1));
      ts.resize(ap_pos);
      if (aliasf) {
        int index = atoi(ap.c_str());
        al = get_aliasf(index, &flags, dict);
        if (!al) {
          HUNSPELL_WARNING(stderr, "error: line %d: bad flag vector alias\n",
                           dict->getlinenum());
        }
      } else {
        al = decode_flags(&flags, ap.c_str(), dict);
        if (al == -1) {
          HUNSPELL_WARNING(stderr, "Can't allocate memory.\n");
          delete dict;
          return 6;
        }
        std::sort(flags, flags + al);
      }
    } else {
      al = 0;
      flags = NULL;
    }

    int captype;
    int wcl = get_clen_and_captype(ts, &captype, workbuf);
    const std::string *dp_str = dp.empty() ? NULL : &dp;
    // add the word and its index plus its capitalized form optionally
    if (add_word(ts, wcl, flags, al, dp_str, false, captype) ||
        add_hidden_capitalized_word(ts, wcl, flags, al, dp_str, captype)) {
      delete dict;
      return 5;
    }
  }

  delete dict;
#endif
  return 0;
}

// the hash function is a simple load and rotate
// algorithm borrowed
int HashMgr::hash(const char* word) const {
#ifdef HUNSPELL_CHROME_CLIENT
    return 0;
#else
  unsigned long hv = 0;
  for (int i = 0; i < 4 && *word != 0; i++)
    hv = (hv << 8) | (*word++);
  while (*word != 0) {
    ROTATE(hv, ROTATE_LEN);
    hv ^= (*word++);
  }
  return (unsigned long)hv % tablesize;
#endif
}

int HashMgr::decode_flags(unsigned short** result, const std::string& flags, FileMgr* af) const {
  int len;
  if (flags.empty()) {
    *result = NULL;
    return 0;
  }
  switch (flag_mode) {
    case FLAG_LONG: {  // two-character flags (1x2yZz -> 1x 2y Zz)
      len = flags.size();
      if (len % 2 == 1)
        HUNSPELL_WARNING(stderr, "error: line %d: bad flagvector\n",
                         af->getlinenum());
      len /= 2;
      *result = (unsigned short*)malloc(len * sizeof(unsigned short));
      if (!*result)
        return -1;
      for (int i = 0; i < len; i++) {
        (*result)[i] = ((unsigned short)((unsigned char)flags[i * 2]) << 8) +
                       (unsigned char)flags[i * 2 + 1];
      }
      break;
    }
    case FLAG_NUM: {  // decimal numbers separated by comma (4521,23,233 -> 4521
                      // 23 233)
      len = 1;
      unsigned short* dest;
      for (size_t i = 0; i < flags.size(); ++i) {
        if (flags[i] == ',')
          len++;
      }
      *result = (unsigned short*)malloc(len * sizeof(unsigned short));
      if (!*result)
        return -1;
      dest = *result;
      const char* src = flags.c_str();
      for (const char* p = src; *p; p++) {
        if (*p == ',') {
          int i = atoi(src);
          if (i >= DEFAULTFLAGS)
            HUNSPELL_WARNING(
                stderr, "error: line %d: flag id %d is too large (max: %d)\n",
                af->getlinenum(), i, DEFAULTFLAGS - 1);
          *dest = (unsigned short)i;
          if (*dest == 0)
            HUNSPELL_WARNING(stderr, "error: line %d: 0 is wrong flag id\n",
                             af->getlinenum());
          src = p + 1;
          dest++;
        }
      }
      int i = atoi(src);
      if (i >= DEFAULTFLAGS)
        HUNSPELL_WARNING(stderr,
                         "error: line %d: flag id %d is too large (max: %d)\n",
                         af->getlinenum(), i, DEFAULTFLAGS - 1);
      *dest = (unsigned short)i;
      if (*dest == 0)
        HUNSPELL_WARNING(stderr, "error: line %d: 0 is wrong flag id\n",
                         af->getlinenum());
      break;
    }
    case FLAG_UNI: {  // UTF-8 characters
      std::vector<w_char> w;
      u8_u16(w, flags);
      len = w.size();
      *result = (unsigned short*)malloc(len * sizeof(unsigned short));
      if (!*result)
        return -1;
      memcpy(*result, &w[0], len * sizeof(short));
      break;
    }
    default: {  // Ispell's one-character flags (erfg -> e r f g)
      unsigned short* dest;
      len = flags.size();
      *result = (unsigned short*)malloc(len * sizeof(unsigned short));
      if (!*result)
        return -1;
      dest = *result;
      for (size_t i = 0; i < flags.size(); ++i) {
        *dest = (unsigned char)flags[i];
        dest++;
      }
    }
  }
  return len;
}

bool HashMgr::decode_flags(std::vector<unsigned short>& result, const std::string& flags, FileMgr* af) const {
  if (flags.empty()) {
    return false;
  }
  switch (flag_mode) {
    case FLAG_LONG: {  // two-character flags (1x2yZz -> 1x 2y Zz)
      size_t len = flags.size();
      if (len % 2 == 1)
        HUNSPELL_WARNING(stderr, "error: line %d: bad flagvector\n",
                         af->getlinenum());
      len /= 2;
      result.reserve(result.size() + len);
      for (size_t i = 0; i < len; ++i) {
        result.push_back(((unsigned short)((unsigned char)flags[i * 2]) << 8) +
                         (unsigned char)flags[i * 2 + 1]);
      }
      break;
    }
    case FLAG_NUM: {  // decimal numbers separated by comma (4521,23,233 -> 4521
                      // 23 233)
      const char* src = flags.c_str();
      for (const char* p = src; *p; p++) {
        if (*p == ',') {
          int i = atoi(src);
          if (i >= DEFAULTFLAGS)
            HUNSPELL_WARNING(
                stderr, "error: line %d: flag id %d is too large (max: %d)\n",
                af->getlinenum(), i, DEFAULTFLAGS - 1);
          result.push_back((unsigned short)i);
          if (result.back() == 0)
            HUNSPELL_WARNING(stderr, "error: line %d: 0 is wrong flag id\n",
                             af->getlinenum());
          src = p + 1;
        }
      }
      int i = atoi(src);
      if (i >= DEFAULTFLAGS)
        HUNSPELL_WARNING(stderr,
                         "error: line %d: flag id %d is too large (max: %d)\n",
                         af->getlinenum(), i, DEFAULTFLAGS - 1);
      result.push_back((unsigned short)i);
      if (result.back() == 0)
        HUNSPELL_WARNING(stderr, "error: line %d: 0 is wrong flag id\n",
                         af->getlinenum());
      break;
    }
    case FLAG_UNI: {  // UTF-8 characters
      std::vector<w_char> w;
      u8_u16(w, flags);
      size_t len = w.size();
      size_t origsize = result.size();
      result.resize(origsize + len);
      memcpy(&result[origsize], &w[0], len * sizeof(short));
      break;
    }
    default: {  // Ispell's one-character flags (erfg -> e r f g)
      result.reserve(flags.size());
      for (size_t i = 0; i < flags.size(); ++i) {
        result.push_back((unsigned char)flags[i]);
      }
    }
  }
  return true;
}

unsigned short HashMgr::decode_flag(const char* f) const {
  unsigned short s = 0;
  int i;
  switch (flag_mode) {
    case FLAG_LONG:
      s = ((unsigned short)((unsigned char)f[0]) << 8) + (unsigned char)f[1];
      break;
    case FLAG_NUM:
      i = atoi(f);
      if (i >= DEFAULTFLAGS)
        HUNSPELL_WARNING(stderr, "error: flag id %d is too large (max: %d)\n",
                         i, DEFAULTFLAGS - 1);
      s = (unsigned short)i;
      break;
    case FLAG_UNI: {
      std::vector<w_char> w;
      u8_u16(w, f);
      if (!w.empty())
          memcpy(&s, &w[0], 1 * sizeof(short));
      break;
    }
    default:
      s = *(unsigned char*)f;
  }
  if (s == 0)
    HUNSPELL_WARNING(stderr, "error: 0 is wrong flag id\n");
  return s;
}

char* HashMgr::encode_flag(unsigned short f) const {
  if (f == 0)
    return mystrdup("(NULL)");
  std::string ch;
  if (flag_mode == FLAG_LONG) {
    ch.push_back((unsigned char)(f >> 8));
    ch.push_back((unsigned char)(f - ((f >> 8) << 8)));
  } else if (flag_mode == FLAG_NUM) {
    std::ostringstream stream;
    stream << f;
    ch = stream.str();
  } else if (flag_mode == FLAG_UNI) {
    const w_char* w_c = (const w_char*)&f;
    std::vector<w_char> w(w_c, w_c + 1);
    u16_u8(ch, w);
  } else {
    ch.push_back((unsigned char)(f));
  }
  return mystrdup(ch.c_str());
}

// read in aff file and set flag mode
int HashMgr::load_config(const char* affpath, const char* key) {
  int firstline = 1;

  // open the affix file
#ifdef HUNSPELL_CHROME_CLIENT
  hunspell::LineIterator iterator = bdict_reader->GetOtherLineIterator();
  FileMgr * afflst = new FileMgr(&iterator);
#else
  FileMgr* afflst = new FileMgr(affpath, key);
#endif
  if (!afflst) {
    HUNSPELL_WARNING(
        stderr, "Error - could not open affix description file %s\n", affpath);
    return 1;
  }

  // read in each line ignoring any that do not
  // start with a known line type indicator

  std::string line;
  while (afflst->getline(line)) {
    mychomp(line);

    /* remove byte order mark */
    if (firstline) {
      firstline = 0;
      if (line.compare(0, 3, "\xEF\xBB\xBF", 3) == 0) {
        line.erase(0, 3);
      }
    }

    /* parse in the try string */
    if ((line.compare(0, 4, "FLAG", 4) == 0) && line.size() > 4 && isspace(line[4])) {
      if (flag_mode != FLAG_CHAR) {
        HUNSPELL_WARNING(stderr,
                         "error: line %d: multiple definitions of the FLAG "
                         "affix file parameter\n",
                         afflst->getlinenum());
      }
      if (line.find("long") != std::string::npos)
        flag_mode = FLAG_LONG;
      if (line.find("num") != std::string::npos)
        flag_mode = FLAG_NUM;
      if (line.find("UTF-8") != std::string::npos)
        flag_mode = FLAG_UNI;
      if (flag_mode == FLAG_CHAR) {
        HUNSPELL_WARNING(
            stderr,
            "error: line %d: FLAG needs `num', `long' or `UTF-8' parameter\n",
            afflst->getlinenum());
      }
    }

    if (line.compare(0, 13, "FORBIDDENWORD", 13) == 0) {
      std::string st;
      if (!parse_string(line, st, afflst->getlinenum())) {
        delete afflst;
        return 1;
      }
      forbiddenword = decode_flag(st.c_str());
    }

    if (line.compare(0, 3, "SET", 3) == 0) {
      if (!parse_string(line, enc, afflst->getlinenum())) {
        delete afflst;
        return 1;
      }
      if (enc == "UTF-8") {
        utf8 = 1;
#ifndef OPENOFFICEORG
#ifndef MOZILLA_CLIENT
        initialize_utf_tbl();
#endif
#endif
      } else
        csconv = get_current_cs(enc);
    }

    if (line.compare(0, 4, "LANG", 4) == 0) {
      if (!parse_string(line, lang, afflst->getlinenum())) {
        delete afflst;
        return 1;
      }
      langnum = get_lang_num(lang);
    }

    /* parse in the ignored characters (for example, Arabic optional diacritics
     * characters */
    if (line.compare(0, 6, "IGNORE", 6) == 0) {
      if (!parse_array(line, ignorechars, ignorechars_utf16,
                       utf8, afflst->getlinenum())) {
        delete afflst;
        return 1;
      }
    }

    if ((line.compare(0, 2, "AF", 2) == 0) && line.size() > 2 && isspace(line[2])) {
      if (!parse_aliasf(line, afflst)) {
        delete afflst;
        return 1;
      }
    }

    if ((line.compare(0, 2, "AM", 2) == 0) && line.size() > 2 && isspace(line[2])) {
      if (!parse_aliasm(line, afflst)) {
        delete afflst;
        return 1;
      }
    }

    if (line.compare(0, 15, "COMPLEXPREFIXES", 15) == 0)
      complexprefixes = 1;

    /* parse in the typical fault correcting table */
    if (line.compare(0, 3, "REP", 3) == 0) {
      if (!parse_reptable(line, afflst)) {
        delete afflst;
        return 1;
      }
    }

    // don't check the full affix file, yet
    if (((line.compare(0, 3, "SFX", 3) == 0) ||
         (line.compare(0, 3, "PFX", 3) == 0)) &&
            line.size() > 3 && isspace(line[3]) &&
            !reptable.empty()) // (REP table is in the end of Afrikaans aff file)
      break;
  }

  if (csconv == NULL)
    csconv = get_current_cs(SPELL_ENCODING);
  delete afflst;
  return 0;
}

/* parse in the ALIAS table */
bool HashMgr::parse_aliasf(const std::string& line, FileMgr* af) {
  if (numaliasf != 0) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
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
        numaliasf = atoi(std::string(start_piece, iter).c_str());
        if (numaliasf < 1) {
          numaliasf = 0;
          aliasf = NULL;
          aliasflen = NULL;
          HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                           af->getlinenum());
          return false;
        }
        aliasf =
            (unsigned short**)malloc(numaliasf * sizeof(unsigned short*));
        aliasflen =
            (unsigned short*)malloc(numaliasf * sizeof(unsigned short));
        if (!aliasf || !aliasflen) {
          numaliasf = 0;
          if (aliasf)
            free(aliasf);
          if (aliasflen)
            free(aliasflen);
          aliasf = NULL;
          aliasflen = NULL;
          return false;
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
  if (np != 2) {
    numaliasf = 0;
    free(aliasf);
    free(aliasflen);
    aliasf = NULL;
    aliasflen = NULL;
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the numaliasf lines to read in the remainder of the table */
  for (int j = 0; j < numaliasf; j++) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    i = 0;
    aliasf[j] = NULL;
    aliasflen[j] = 0;
    iter = nl.begin();
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 2, "AF", 2) != 0) {
            numaliasf = 0;
            free(aliasf);
            free(aliasflen);
            aliasf = NULL;
            aliasflen = NULL;
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            return false;
          }
          break;
        }
        case 1: {
          std::string piece(start_piece, iter);
          aliasflen[j] =
              (unsigned short)decode_flags(&(aliasf[j]), piece, af);
          std::sort(aliasf[j], aliasf[j] + aliasflen[j]);
          break;
        }
        default:
          break;
      }
      ++i;
      start_piece = mystrsep(nl, iter);
    }
    if (!aliasf[j]) {
      free(aliasf);
      free(aliasflen);
      aliasf = NULL;
      aliasflen = NULL;
      numaliasf = 0;
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      return false;
    }
  }
  return true;
}

#ifdef HUNSPELL_CHROME_CLIENT
int HashMgr::LoadAFLines()
{
  utf8 = 1;  // We always use UTF-8.

  // Read in all the AF lines which tell us the rules for each affix group ID.
  hunspell::LineIterator iterator = bdict_reader->GetAfLineIterator();
  FileMgr afflst(&iterator);
  std::string line;
  while (afflst.getline(line)) {
    int rv = parse_aliasf(line, &afflst);
    if (rv)
      return rv;
  }

  return 0;
}

hentry* HashMgr::InitHashEntry(hentry* entry,
                               size_t item_size,
                               const char* word,
                               int word_length,
                               int affix_index) const {
  // Return if the given buffer doesn't have enough space for a hentry struct
  // or the given word is too long.
  // Our BDICT cannot handle words longer than (128 - 1) bytes. So, it is
  // better to return an error if the given word is too long and prevent
  // an unexpected result caused by a long word.
  const int kMaxWordLen = 128;
  if (item_size < sizeof(hentry) + word_length + 1 ||
      word_length >= kMaxWordLen)
    return NULL;

  // Initialize a hentry struct with the given parameters, and
  // append the given string at the end of this hentry struct.
  memset(entry, 0, item_size);

  // `get_aliasf` only uses the dictionary file in the case of an error.
  // Should that occur, `FileMgr::getlinenum` is called. But for a BDICT file,
  // the line number doesn't help identify the place which caused the parser
  // error. As a result, `getlinenum` always returns 0. Since we don't actually
  // need the dictionary file, and since `InitHashEntry` is called for every
  // dictionary entry, eliminate the overhead of creating the `FileMgr` object
  // every time.
  static FileMgr dummyDictionaryFile(NULL);
  entry->alen = static_cast<short>(const_cast<HashMgr*>(this)->get_aliasf(
      affix_index, &entry->astr, &dummyDictionaryFile));
  entry->blen = static_cast<unsigned char>(word_length);
  std::vector<w_char> w;
  entry->clen = u8_u16(w, word);
  memcpy(&entry->word, word, word_length);

  return entry;
}

hentry* HashMgr::CreateHashEntry(const char* word,
                                 int word_length,
                                 int affix_index) const {
  // Return if the given word is too long.
  // (See the comment in HashMgr::InitHashEntry().)
  const int kMaxWordLen = 128;
  if (word_length >= kMaxWordLen)
    return NULL;

  const size_t kEntrySize = sizeof(hentry) + word_length + 1;
  struct hentry* entry = reinterpret_cast<hentry*>(malloc(kEntrySize));
  if (entry)
    InitHashEntry(entry, kEntrySize, word, word_length, affix_index);

  return entry;
}

void HashMgr::DeleteHashEntry(hentry* entry) const {
  free(entry);
}

hentry* HashMgr::AffixIDsToHentry(char* word,
                                  int* affix_ids, 
                                  int affix_count) const
{
  if (affix_count == 0)
    return NULL;

  HEntryCache& cache = const_cast<HashMgr*>(this)->hentry_cache;
  std::string std_word(word);
  HEntryCache::iterator found = cache.find(std_word);
  if (found != cache.end()) {
    // We must return an existing hentry for the same word if we've previously
    // handed one out. Hunspell will compare pointers in some cases to see if
    // two words it has found are the same.
    return found->second;
  }

  short word_len = static_cast<short>(strlen(word));

  // We can get a number of prefixes per word. There will normally be only one,
  // but if not, there will be a linked list of "hentry"s for the "homonym"s 
  // for the word.
  struct hentry* first_he = NULL;
  struct hentry* prev_he = NULL;  // For making linked list.
  for (int i = 0; i < affix_count; i++) {
    struct hentry* he = CreateHashEntry(word, word_len, affix_ids[i]);
    if (!he)
      break;
    if (i == 0)
      first_he = he;
    if (prev_he)
      prev_he->next_homonym = he;
    prev_he = he;
  }

  cache[std_word] = first_he;  // Save this word in the cache for later.
  return first_he;
}

hentry* HashMgr::GetHentryFromHEntryCache(char* word) {
  HEntryCache& cache = const_cast<HashMgr*>(this)->hentry_cache;
  std::string std_word(word);
  HEntryCache::iterator found = cache.find(std_word);
  if (found != cache.end())
    return found->second;
  else
    return NULL;
}
#endif

int HashMgr::is_aliasf() const {
  return (aliasf != NULL);
}

int HashMgr::get_aliasf(int index, unsigned short** fvec, FileMgr* af) const {
  if ((index > 0) && (index <= numaliasf)) {
    *fvec = aliasf[index - 1];
    return aliasflen[index - 1];
  }
  HUNSPELL_WARNING(stderr, "error: line %d: bad flag alias index: %d\n",
                   af->getlinenum(), index);
  *fvec = NULL;
  return 0;
}

/* parse morph alias definitions */
bool HashMgr::parse_aliasm(const std::string& line, FileMgr* af) {
  if (numaliasm != 0) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
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
        numaliasm = atoi(std::string(start_piece, iter).c_str());
        if (numaliasm < 1) {
          HUNSPELL_WARNING(stderr, "error: line %d: bad entry number\n",
                           af->getlinenum());
          return false;
        }
        aliasm = (char**)malloc(numaliasm * sizeof(char*));
        if (!aliasm) {
          numaliasm = 0;
          return false;
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
  if (np != 2) {
    numaliasm = 0;
    free(aliasm);
    aliasm = NULL;
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n",
                     af->getlinenum());
    return false;
  }

  /* now parse the numaliasm lines to read in the remainder of the table */
  for (int j = 0; j < numaliasm; j++) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    aliasm[j] = NULL;
    iter = nl.begin();
    i = 0;
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 2, "AM", 2) != 0) {
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            numaliasm = 0;
            free(aliasm);
            aliasm = NULL;
            return false;
          }
          break;
        }
        case 1: {
          // add the remaining of the line
          std::string::const_iterator end = nl.end();
          std::string chunk(start_piece, end);
          if (complexprefixes) {
            if (utf8)
              reverseword_utf(chunk);
            else
              reverseword(chunk);
          }
          aliasm[j] = mystrdup(chunk.c_str());
          break;
        }
        default:
          break;
      }
      ++i;
      start_piece = mystrsep(nl, iter);
    }
    if (!aliasm[j]) {
      numaliasm = 0;
      free(aliasm);
      aliasm = NULL;
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      return false;
    }
  }
  return true;
}

int HashMgr::is_aliasm() const {
  return (aliasm != NULL);
}

char* HashMgr::get_aliasm(int index) const {
  if ((index > 0) && (index <= numaliasm))
    return aliasm[index - 1];
  HUNSPELL_WARNING(stderr, "error: bad morph. alias index: %d\n", index);
  return NULL;
}

/* parse in the typical fault correcting table */
bool HashMgr::parse_reptable(const std::string& line, FileMgr* af) {
  if (!reptable.empty()) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple table definitions\n",
                     af->getlinenum());
    return false;
  }
  int numrep = -1;
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
        numrep = atoi(std::string(start_piece, iter).c_str());
        if (numrep < 1) {
          HUNSPELL_WARNING(stderr, "error: line %d: incorrect entry number\n",
                           af->getlinenum());
          return false;
        }
        reptable.reserve(numrep);
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

  /* now parse the numrep lines to read in the remainder of the table */
  for (int j = 0; j < numrep; ++j) {
    std::string nl;
    if (!af->getline(nl))
      return false;
    mychomp(nl);
    reptable.push_back(replentry());
    iter = nl.begin();
    i = 0;
    int type = 0;
    start_piece = mystrsep(nl, iter);
    while (start_piece != nl.end()) {
      switch (i) {
        case 0: {
          if (nl.compare(start_piece - nl.begin(), 3, "REP", 3) != 0) {
            HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                             af->getlinenum());
            reptable.clear();
            return false;
          }
          break;
        }
        case 1: {
          if (*start_piece == '^')
            type = 1;
          reptable.back().pattern.assign(start_piece + type, iter);
          mystrrep(reptable.back().pattern, "_", " ");
          if (!reptable.back().pattern.empty() && reptable.back().pattern[reptable.back().pattern.size() - 1] == '$') {
            type += 2;
            reptable.back().pattern.resize(reptable.back().pattern.size() - 1);
          }
          break;
        }
        case 2: {
          reptable.back().outstrings[type].assign(start_piece, iter);
          mystrrep(reptable.back().outstrings[type], "_", " ");
          break;
        }
        default:
          break;
      }
      ++i;
      start_piece = mystrsep(nl, iter);
    }
    if (reptable.back().pattern.empty() || reptable.back().outstrings[type].empty()) {
      HUNSPELL_WARNING(stderr, "error: line %d: table is corrupt\n",
                       af->getlinenum());
      reptable.clear();
      return false;
    }
  }
  return true;
}

// return replacing table
const std::vector<replentry>& HashMgr::get_reptable() const {
  return reptable;
}
