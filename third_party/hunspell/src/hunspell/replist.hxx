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

/* string replacement list class */
#ifndef REPLIST_HXX_
#define REPLIST_HXX_

#ifdef HUNSPELL_CHROME_CLIENT
// Compilation issues in spellchecker.cc think near is a macro, therefore
// removing it here solves that problem.
#undef near
#endif

#include "w_char.hxx"

#include <string>
#include <vector>

#include <limits>
#include <cstdint>

// ============================================================================
// Transcoding Outcomes
// ============================================================================
enum class TranscodeResult {
    None,       // All bytes were copied verbatim; no substitutions were found.
    Partial,    // A mix of valid substitutions and verbatim copies occurred.
    All         // Every byte of the input was successfully mapped and substituted.
};

// ============================================================================
// Configurable Trie Template Class
// ============================================================================
template <typename IndexType, bool UseNibbles>
class Trie {
public:
    // A trie node stores relative offsets to its children.
    // By avoiding absolute pointers, we compress the tree and allow the underlying
    // vector to reallocate without invalidating links.
    // Size math:
    // - UseNibbles=true : 16 * sizeof(IndexType) -> Power of two
    // - UseNibbles=false: 256 * sizeof(IndexType) -> Power of two
    struct Node {
        static constexpr size_t ALPHABET_SIZE = UseNibbles ? 16 : 256;
        
        // Zero-initialization via brace-enclosed list. 
        // This avoids the overhead of a manual for-loop during node construction.
        // A '0' offset mathematically represents a null/missing child, as any 
        // valid child added to the arena will have an offset of at least 1.
        IndexType children[ALPHABET_SIZE] = {0};
    };

private:
    // ALGORITHM: Arena Allocation
    // Instead of using `new Node()` which fragments the heap and destroys cache locality,
    // we use flat std::vectors. Traversing the trie simply means jumping forward 
    // in this contiguous block of memory.
    std::vector<Node> nodes;
    
    // Parallel arena storing substitution data. We keep this OUT of the Node struct 
    // to preserve the power-of-two size requirement for fast node array lookups.
    // Format: 24 bits for the char_arena offset, 8 bits for the length.
    std::vector<uint32_t> sub_info_arena;
    
    // Contiguous byte buffer storing all the actual string substitution values.
    std::vector<char> char_arena;

    // Failsafe state flag (avoids C++ exceptions)
    bool status_ok;

    // ALGORITHM: Advance or Create (Trie Insertion)
    // 1. Check if the transition for the given symbol exists.
    // 2. If yes, return the absolute index of the child.
    // 3. If no, calculate the distance from the current node to the END of the arena.
    // 4. Ensure this distance fits within the user-configured IndexType.
    // 5. Store this relative distance, emplace a new empty node, and return its index.
    size_t advance_or_create(size_t curr_idx, uint8_t symbol) {
        IndexType rel_idx = nodes[curr_idx].children[symbol];
        if (rel_idx == 0) {
            size_t new_idx = nodes.size();
            size_t diff = new_idx - curr_idx;
            
            // Diagnostics: Halt execution if the trie outgrows the integer type
            if (diff > std::numeric_limits<IndexType>::max()) {
                status_ok = false;
                return 0;
            }
            
            nodes[curr_idx].children[symbol] = static_cast<IndexType>(diff);
            
            // emplace_back constructs the object directly in the vector's memory
            nodes.emplace_back(); 
            sub_info_arena.push_back(0xFFFFFFFF); // 0xFFFFFFFF = No substitution mapped here
            return new_idx;
        }
        return curr_idx + rel_idx;
    }

public:
    Trie() : status_ok(true) {
        // Initialize the root node (Index 0)
        nodes.emplace_back();
        sub_info_arena.push_back(0xFFFFFFFF);
    }

    void add(const std::string& key, const std::string& value) {
        if (!status_ok) return;

        // Size rules diagnostics for our 32-bit (24/8) packing format
        if (value.size() > 255) { status_ok = false; return; }
        size_t offset = char_arena.size();
        if (offset > 0xFFFFFF) { status_ok = false; return; }

        // Append to the flat char arena
        char_arena.insert(char_arena.end(), value.begin(), value.end());
        uint32_t sub_val = (static_cast<uint32_t>(offset) << 8) | static_cast<uint8_t>(value.size());

        size_t curr_idx = 0;
        
        // Traverse the key, adding nodes as necessary
        for (size_t i = 0; i < key.size(); ++i) {
            uint8_t byte = static_cast<uint8_t>(key[i]);
            
            if (UseNibbles) {
                // Loop unrolling: Process the High Nibble, then the Low Nibble
                curr_idx = advance_or_create(curr_idx, (byte >> 4) & 0x0F);
                if (!status_ok) return;
                curr_idx = advance_or_create(curr_idx, byte & 0x0F);
                if (!status_ok) return;
            } else {
                // Process as a single 8-bit lookup
                curr_idx = advance_or_create(curr_idx, byte);
                if (!status_ok) return;
            }
        }
        // Mark the leaf node with the packed substitution metadata
        sub_info_arena[curr_idx] = sub_val;
    }

    // ALGORITHM: Longest-Prefix Match Transcoding with 3-State Outcome Tracking
    // Scans the source string and attempts to dive as deep as possible into the trie
    // for each character. Tracks whether we successfully applied substitutions, 
    // fell back to verbatim copies, or a mix of both.
    TranscodeResult transcode(const std::string& src, std::string& dst, size_t maxlen) const {
        // Halt and return None if the trie is in an invalid memory state
        if (!status_ok || src.empty()) return TranscodeResult::None;
        
        dst.clear();
        dst.reserve(src.size()); 
        
        // State trackers for the 3-way outcome
        bool any_substituted = false;
        bool any_copied = false;
        
        size_t i = 0;
        
        while (i < src.size() && dst.size() <= maxlen) {
            size_t curr_idx = 0;
            size_t match_len = 0;
            uint32_t best_sub = 0xFFFFFFFF;

            size_t j = i;
            
            // Greedily traverse the trie to find the longest matching sequence
            while (j < src.size()) {
                uint8_t byte = static_cast<uint8_t>(src[j]);
                
                if (UseNibbles) {
                    IndexType r1 = nodes[curr_idx].children[(byte >> 4) & 0x0F];
                    if (r1 == 0) break;
                    curr_idx += r1;

                    IndexType r2 = nodes[curr_idx].children[byte & 0x0F];
                    if (r2 == 0) break;
                    curr_idx += r2;
                } else {
                    IndexType r = nodes[curr_idx].children[byte];
                    if (r == 0) break;
                    curr_idx += r;
                }
                
                ++j;
                
                // Track the deepest valid match found during traversal
                if (sub_info_arena[curr_idx] != 0xFFFFFFFF) {
                    best_sub = sub_info_arena[curr_idx];
                    match_len = j - i;
                }
            }

            if (best_sub != 0xFFFFFFFF) {
                // Decode the 24-bit offset and 8-bit size, append directly from arena
                dst.append(&char_arena[best_sub >> 8], best_sub & 0xFF);
                i += match_len;
                any_substituted = true; // State update: Substitution occurred
            } else {
                // Fallback: copy unmapped character directly to output
                dst.push_back(src[i]);
                ++i;
                any_copied = true; // State update: Verbatim copy occurred
            }
        }
        
        // ALGORITHM: Outcome Evaluation
        if (any_substituted && any_copied) {
            return TranscodeResult::Partial;
        } else if (any_substituted) {
            return TranscodeResult::All;
        }
        
        return TranscodeResult::None;
    }

    bool get_status() const { return status_ok; }
};

class RepList {
 private:
  std::vector<replentry*> dat;
  Trie<uint16_t, true> trie;
  bool can_use_trie;
 public:
  explicit RepList(int n);
  RepList(const RepList&) = delete;
  RepList& operator=(const RepList&) = delete;
  ~RepList();

  bool check_against_breaktable(const std::vector<std::string>& breaktable) const;

  int add(const std::string& pat1, const std::string& pat2);
  int find(const char* word, size_t max_len);
  std::string replace(const size_t wordlen, int n, bool atstart);
  // dest can end just over maxlen, with the rest of the word left unconverted
  bool conv(const std::string& word, std::string& dest, size_t maxlen = std::string::npos);
};
#endif
