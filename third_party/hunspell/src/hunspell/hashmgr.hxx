/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Hunspell, based on MySpell.
 *
 * The Initial Developers of the Original Code are
 * Kevin Hendricks (MySpell) and Németh László (Hunspell).
 * Portions created by the Initial Developers are Copyright (C) 2002-2005
 * the Initial Developers. All Rights Reserved.
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

#ifndef HASHMGR_HXX_
#define HASHMGR_HXX_

#include <stdio.h>
#include <string>
#include <vector>

#include "htypes.hxx"
#include "filemgr.hxx"
#include "w_char.hxx"

#ifdef HUNSPELL_CHROME_CLIENT
#include <map>

#include "base/strings/string_piece.h"
#include "third_party/hunspell/google/bdict_reader.h"
#endif

enum flag { FLAG_CHAR, FLAG_LONG, FLAG_NUM, FLAG_UNI };

class HashMgr {
#ifdef HUNSPELL_CHROME_CLIENT
  // Not owned by this class, owned by the Hunspell object.
  hunspell::BDictReader* bdict_reader;
  std::map<base::StringPiece, int> custom_word_to_affix_id_map_;
  std::vector<std::string*> pointer_to_strings_;
#endif
  int tablesize;
  struct hentry** tableptr;
  flag flag_mode;
  int complexprefixes;
  int utf8;
  unsigned short forbiddenword;
  int langnum;
  std::string enc;
  std::string lang;
  struct cs_info* csconv;
  std::string ignorechars;
  std::vector<w_char> ignorechars_utf16;
  int numaliasf;  // flag vector `compression' with aliases
  unsigned short** aliasf;
  unsigned short* aliasflen;
  int numaliasm;  // morphological desciption `compression' with aliases
  char** aliasm;

 public:
#ifdef HUNSPELL_CHROME_CLIENT
  HashMgr(hunspell::BDictReader* reader);

  // Return the hentry corresponding to the given word. Returns NULL if the
  // word is not there in the cache.
  hentry* GetHentryFromHEntryCache(char* word);

  // Called before we do a new operation. This will empty the cache of pointers
  // to hentries that we have cached. In Chrome, we make these on-demand, but
  // they must live as long as the single spellcheck operation that they're part
  // of since Hunspell will save pointers to various ones as it works.
  //
  // This function allows that cache to be emptied and not grow infinitely.
  void EmptyHentryCache();
#else
  HashMgr(const char* tpath, const char* apath, const char* key = NULL);
#endif
  ~HashMgr();

  struct hentry* lookup(const char*) const;
  int hash(const char*) const;
  struct hentry* walk_hashtable(int& col, struct hentry* hp) const;

  int add(const std::string& word);
  int add_with_affix(const std::string& word, const std::string& pattern);
  int remove(const std::string& word);
  int decode_flags(unsigned short** result, const std::string& flags, FileMgr* af) const;
  bool decode_flags(std::vector<unsigned short>& result, const std::string& flags, FileMgr* af) const;
  unsigned short decode_flag(const char* flag) const;
  char* encode_flag(unsigned short flag) const;
  int is_aliasf() const;
  int get_aliasf(int index, unsigned short** fvec, FileMgr* af) const;
  int is_aliasm() const;
  char* get_aliasm(int index) const;

 private:
  int get_clen_and_captype(const std::string& word, int* captype);
  int load_tables(const char* tpath, const char* key);
  int add_word(const std::string& word,
               int wcl,
               unsigned short* ap,
               int al,
               const std::string* desc,
               bool onlyupcase);
  int load_config(const char* affpath, const char* key);
  bool parse_aliasf(const std::string& line, FileMgr* af);

#ifdef HUNSPELL_CHROME_CLIENT
  // Loads the AF lines from a BDICT.
  // A BDICT file compresses its AF lines to save memory.
  // This function decompresses each AF line and call parse_aliasf().
  int LoadAFLines();

  // Helper functions that create a new hentry struct, initialize it, and
  // delete it.
  // These functions encapsulate non-trivial operations in creating and
  // initializing a hentry struct from BDICT data to avoid changing code so much
  // even when a hentry struct is changed.
  hentry* InitHashEntry(hentry* entry,
                        size_t item_size,
                        const char* word,
                        int word_length,
                        int affix_index) const;
  hentry* CreateHashEntry(const char* word,
                          int word_length,
                          int affix_index) const;
  void DeleteHashEntry(hentry* entry) const;

  // Converts the list of affix IDs to a linked list of hentry structures. The
  // hentry structures will point to the given word. The returned pointer will
  // be a statically allocated variable that will change for the next call. The
  // |word| buffer must be the same.
  hentry* AffixIDsToHentry(char* word, int* affix_ids, int affix_count) const;

  // See EmptyHentryCache above. Note that each one is actually a linked list
  // followed by the homonym pointer.
  typedef std::map<std::string, hentry*> HEntryCache;
  HEntryCache hentry_cache;
#endif

  int add_hidden_capitalized_word(const std::string& word,
                                  int wcl,
                                  unsigned short* flags,
                                  int al,
                                  const std::string* dp,
                                  int captype);
  bool parse_aliasm(const std::string& line, FileMgr* af);
  int remove_forbidden_flag(const std::string& word);
};

#endif
