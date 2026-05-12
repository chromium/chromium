// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include "builder.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "filesystem.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

#ifdef ENABLE_NFKC_COMPILE
#include <unicode/errorcode.h>
#include <unicode/locid.h>
#include <unicode/normlzr.h>
#include <unicode/numfmt.h>
#include <unicode/rbnf.h>
#include <unicode/utypes.h>
#endif  // ENABLE_NFKC_COMPILE

#include <set>

#include "normalizer.h"
#include "third_party/darts_clone/darts.h"
#include "util.h"

#ifndef DISABLE_EMBEDDED_DATA
#include "normalization_rule.h"
#endif

namespace sentencepiece {
namespace normalizer {
namespace {

constexpr int kMaxUnicode = 0x10FFFF;

static constexpr absl::string_view kDefaultNormalizerName = "nfkc";

#ifndef ENABLE_NFKC_COMPILE
static constexpr absl::string_view kCompileError =
    "NFK compile is not enabled. rebuild with -DSPM_ENABLE_NFKC_COMPILE=ON";
#endif

#ifdef ENABLE_NFKC_COMPILE
// Normalize `input` with ICU's normalizer with `mode`.
Builder::Chars UnicodeNormalize(UNormalizationMode mode,
                                const Builder::Chars &input) {
  const std::string utf8 = string_util::UnicodeTextToUTF8(input);
  ABSL_CHECK(!utf8.empty());

  icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(utf8.c_str());

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString dst;
  icu::Normalizer::normalize(ustr, mode, 0, dst, status);
  ABSL_CHECK(U_SUCCESS(status));
  std::string normalized;
  normalized.reserve(dst.length() * 3);
  dst.toUTF8String(normalized);
  return string_util::UTF8ToUnicodeText(normalized);
}

Builder::Chars ToNFKD(const Builder::Chars &input) {
  return UnicodeNormalize(UNORM_NFKD, input);
}

Builder::Chars ToNFKC(const Builder::Chars &input) {
  return UnicodeNormalize(UNORM_NFKC, input);
}

Builder::Chars ToNFC(const Builder::Chars &input) {
  return UnicodeNormalize(UNORM_NFC, input);
}

Builder::Chars ToNFD(const Builder::Chars &input) {
  return UnicodeNormalize(UNORM_NFD, input);
}

// Given an NFKD-normalized string, returns a set of all strings which are
// normalized into the same `nfkd`. `norm2orig` is the normalized to
// un-normalized character mapping.
std::vector<Builder::Chars> ExpandUnnormalized(
    const Builder::Chars &nfkd,
    const std::map<char32, std::set<char32>> &norm2orig) {
  ABSL_CHECK(!nfkd.empty());
  std::vector<Builder::Chars> results;
  for (const auto c : port::FindOrDie(norm2orig, nfkd[0])) {
    results.push_back({c});
  }
  for (size_t i = 1; i < nfkd.size(); ++i) {
    const auto &orig = port::FindOrDie(norm2orig, nfkd[i]);
    std::vector<Builder::Chars> new_results;
    for (const auto &r : results) {
      for (const auto c : orig) {
        new_results.emplace_back(r);
        new_results.back().push_back(c);
      }
    }
    results = std::move(new_results);
  }
  ABSL_CHECK_EQ(nfkd.size(), results[0].size());
  return results;
}
#endif  // ENABLE_NFKC_COMPILE

// Normalizes `src` with `chars_map` and returns normalized Chars.
// `max_len` specifies the maximum length of the key in `chars_map`.
Builder::Chars Normalize(const Builder::CharsMap &chars_map,
                         const Builder::Chars &src, int max_len) {
  ABSL_CHECK_GE(max_len, 1);
  Builder::Chars normalized;

  for (size_t i = 0; i < src.size();) {
    Builder::CharsMap::const_iterator it = chars_map.end();
    const size_t slice = std::min<size_t>(i + max_len, src.size());
    // starts with the longest prefix.
    Builder::Chars key(src.begin() + i, src.begin() + slice);
    while (!key.empty()) {
      it = chars_map.find(key);
      if (it != chars_map.end()) {
        break;
      }
      key.pop_back();  // remove the last character.
    }

    // Consumes one character when no rule is found.
    if (it == chars_map.end()) {
      normalized.push_back(src[i]);
      ++i;
    } else {
      std::copy(it->second.begin(), it->second.end(),
                std::back_inserter(normalized));
      i += it->first.size();
    }
  }

  return normalized;
}

util::Status IsValidNormalizerData(absl::string_view blob_data) {
  NormalizerSpec spec;
  spec.set_precompiled_charsmap(blob_data.data(), blob_data.size());
  const Normalizer normalizer(spec);
  return normalizer.status();
}

}  // namespace

// static
util::Status Builder::CompileCharsMap(const CharsMap &chars_map,
                                      std::string *output) {
  CHECK_OR_RETURN(output);
  CHECK_OR_RETURN(!chars_map.empty());

  ABSL_LOG(INFO) << "Loading CharsMap of size=" << chars_map.size();

  // Aggregates the same target strings to save footprint.
  std::map<Chars, int> normalized2pos;
  for (const auto &p : chars_map) {
    normalized2pos[p.second] = 0;
  }

  std::string normalized;
  for (auto &p : normalized2pos) {
    p.second = normalized.size();  // stores the pointer (position).
    const std::string utf8_out = string_util::UnicodeTextToUTF8(p.first);
    CHECK_OR_RETURN(string_util::IsStructurallyValid(utf8_out));
    normalized += utf8_out;
    normalized += '\0';
  }

  std::vector<std::pair<std::string, int>> kv;  // key-value of Trie.
  for (const auto &p : chars_map) {
    // The value of Trie stores the pointer to the normalized string.
    const std::string utf8_in = string_util::UnicodeTextToUTF8(p.first);
    CHECK_OR_RETURN(!utf8_in.empty());
    CHECK_OR_RETURN(string_util::IsStructurallyValid(utf8_in));
    kv.emplace_back(utf8_in, port::FindOrDie(normalized2pos, p.second));
  }

  std::sort(kv.begin(), kv.end());
  std::vector<const char *> key(kv.size());
  std::vector<int> value(kv.size());
  for (size_t i = 0; i < kv.size(); ++i) {
    key[i] = kv[i].first.c_str();
    value[i] = kv[i].second;
  }

  Darts::DoubleArray trie;
  CHECK_EQ_OR_RETURN(0, trie.build(key.size(), const_cast<char **>(&key[0]),
                                   nullptr, &value[0]))
      << "cannot build double-array";

  int max_nodes_size = 0;
  std::vector<Darts::DoubleArray::result_pair_type> results(
      2 * Normalizer::kMaxTrieResultsSize);
  for (const char *str : key) {
    const int num_nodes = trie.commonPrefixSearch(str, results.data(),
                                                  results.size(), strlen(str));
    max_nodes_size = std::max(num_nodes, max_nodes_size);
  }
  CHECK_LT_OR_RETURN(max_nodes_size, Normalizer::kMaxTrieResultsSize)
      << "This charmaps contain many shared prefix. "
      << "The number of shared prefix must be less than "
      << Normalizer::kMaxTrieResultsSize;

  absl::string_view trie_blob(static_cast<const char *>(trie.array()),
                              trie.size() * trie.unit_size());
  *output = Normalizer::EncodePrecompiledCharsMap(trie_blob, normalized);
  RETURN_IF_ERROR(IsValidNormalizerData(*output));

  ABSL_LOG(INFO) << "Generated normalizer blob. size=" << output->size();

  return util::OkStatus();
}

// static
util::Status Builder::DecompileCharsMap(absl::string_view blob,
                                        Builder::CharsMap *chars_map) {
  CHECK_OR_RETURN(chars_map);
  chars_map->clear();

  absl::string_view trie_blob, normalized;
  std::string buf;
  RETURN_IF_ERROR(Normalizer::DecodePrecompiledCharsMap(blob, &trie_blob,
                                                        &normalized, &buf));

  Darts::DoubleArray trie;
  trie.set_array(const_cast<char *>(trie_blob.data()),
                 trie_blob.size() / trie.unit_size());

  std::string key;
  std::function<void(size_t, size_t)> traverse;

  // Given a Trie node at `node_pos` and the key position at `key_position`,
  // Expands children nodes from `node_pos`.
  // When leaf nodes are found, stores them into `chars_map`.
  traverse = [&traverse, &key, &trie, &normalized, &chars_map](
                 size_t node_pos, size_t key_pos) -> void {
    for (int c = 0; c <= 255; ++c) {
      key.push_back(static_cast<char>(c));
      size_t copied_node_pos = node_pos;
      size_t copied_key_pos = key_pos;
      // Note: `copied_(node|key)_pos` are non-const references.
      // They store the new positions after node traversal.
      const Darts::DoubleArray::result_type result = trie.traverse(
          key.data(), copied_node_pos, copied_key_pos, key.size());
      if (result >= -1) {   // node exists.
        if (result >= 0) {  // has a value after transition.
          const absl::string_view value = normalized.data() + result;
          Chars key_chars, value_chars;
          for (const auto c : string_util::UTF8ToUnicodeText(key))
            key_chars.push_back(c);
          for (const auto c : string_util::UTF8ToUnicodeText(value))
            value_chars.push_back(c);
          (*chars_map)[key_chars] = value_chars;
        }
        // Recursively traverse.
        traverse(copied_node_pos, copied_key_pos);
      }
      key.pop_back();
    }
  };

  traverse(0, 0);

  return util::OkStatus();
}

// static
util::Status Builder::GetPrecompiledCharsMap(absl::string_view name,
                                             std::string *output) {
  CHECK_OR_RETURN(output);

  if (name == "identity") {
    output->clear();
    return util::OkStatus();
  }

  if (!std::all_of(name.begin(), name.end(), [](auto c) {
        return (c >= 'a' && c <= 'z') || c == '_' || c == '-';
      })) {
    return util::StatusBuilder(util::StatusCode::kInvalidArgument, GTL_LOC)
           << "Invalid charsmap name " << name;
  }

  std::string result;

#ifndef DISABLE_EMBEDDED_DATA
  for (size_t i = 0; i < kNormalizationRules_size; ++i) {
    const auto *blob = &kNormalizationRules_blob[i];
    if (blob->name == name) {
      output->assign(blob->data, blob->size);
      return IsValidNormalizerData(*output);
    }
  }
#else   // DISABLE_EMBEDDED_DATA
  {
    const std::string filename =
        absl::StrCat(util::JoinPath(GetDataDir(), name), ".bin");
    auto input = filesystem::NewReadableFile(filename, true /* is binary */);
    if (input->status().ok()) {
      input->ReadAll(output);
      return IsValidNormalizerData(*output);
    }
  }
#endif  // DISABLE_EMBEDDED_DATA

  return util::StatusBuilder(util::StatusCode::kNotFound, GTL_LOC)
         << "No precompiled charsmap is found: " << name << " in "
         << GetDataDir();
}

#ifdef ENABLE_NFKC_COMPILE
namespace {
util::Status BuildMapInternal(
    Builder::CharsMap *chars_map,
    std::function<Builder::Chars(const Builder::Chars &)> composer,
    std::function<Builder::Chars(const Builder::Chars &)> decomposer) {
#ifdef ENABLE_NFKC_COMPILE
  // Set of fully NFKD decomposed characters.
  std::set<Builder::Chars> nfkd_decomposed;

  // Fully normalized one character to unnormalized one character map.
  std::map<char32, std::set<char32>> norm2orig;

  Builder::CharsMap nfkc_map;  // The final NFKC mapping.

  constexpr int kMaxUnicode = 0x10FFFF;
  for (char32 cp = 1; cp <= kMaxUnicode; ++cp) {
    if (!U_IS_UNICODE_CHAR(cp)) {
      continue;
    }
    // Aggregates single character to fully NFKC normalized characters.
    const auto nfkc = composer({cp});
    if (nfkc.size() >= 2 || (nfkc.size() == 1 && nfkc[0] != cp)) {
      nfkc_map[{cp}] = nfkc;
    }
    const auto nfkd = decomposer({cp});
    if (nfkd.size() == 1) {
      // Aggregates reverse mapping from normalized to unnormalized character.
      norm2orig[nfkd[0]].insert(cp);
    } else {
      // One character is decomposed into multiple characters.
      nfkd_decomposed.insert(nfkd);
    }
  }

  for (const auto &nfkd : nfkd_decomposed) {
    const auto nfkc = composer(nfkd);
    // This case is already covered by single-character to NFKC mapping.
    if (nfkc == nfkd) {
      continue;
    }
    // Expand all possible sequences which are normalized into the same
    // `nfkd`.
    for (const auto &nfkd_orig : ExpandUnnormalized(nfkd, norm2orig)) {
      if (nfkd_orig != nfkc) {
        nfkc_map[nfkd_orig] = nfkc;
      }
    }
  }

  RETURN_IF_ERROR(Builder::RemoveRedundantMap(&nfkc_map));
  *chars_map = std::move(nfkc_map);
#endif  // ENABLE_NFKC_COMPILE
  return util::OkStatus();
}
}  // namespace
#endif  // ENABLE_NFKC_COMPILE

// static
util::Status Builder::BuildNFKCMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  ABSL_LOG(INFO) << "Running BuildNFKCMap";
  BuildMapInternal(chars_map, ToNFKC, ToNFKD);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif

  return util::OkStatus();
}

// static
util::Status Builder::BuildNFCMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  ABSL_LOG(INFO) << "Running BuildNFCMap";
  BuildMapInternal(chars_map, ToNFC, ToNFD);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif
  return util::OkStatus();
}

util::Status Builder::BuildNmtNFKCMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  ABSL_LOG(INFO) << "Running BuildNmtNFKCMap";

  CharsMap nfkc_map;
  RETURN_IF_ERROR(BuildNFKCMap(&nfkc_map));
  RETURN_IF_ERROR(MergeNmtMap(&nfkc_map));
  RETURN_IF_ERROR(RemoveRedundantMap(&nfkc_map));

  *chars_map = std::move(nfkc_map);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif

  return util::OkStatus();
}

// static
util::Status Builder::MergeUnicodeCaseFoldMap(Builder::CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  for (auto &c : *chars_map) {
    std::vector<char32> trg;
    for (char32 c : c.second) trg.push_back(u_foldCase(c, U_FOLD_CASE_DEFAULT));
    c.second = trg;
  }

  constexpr int kMaxUnicode = 0x10FFFF;
  for (char32 cp = 1; cp <= kMaxUnicode; ++cp) {
    if (!U_IS_UNICODE_CHAR(cp)) {
      continue;
    }
    if (chars_map->find({cp}) != chars_map->end()) continue;
    const char32 trg = u_foldCase(cp, U_FOLD_CASE_DEFAULT);
    if (trg != cp) (*chars_map)[{cp}] = {trg};
  }

  RETURN_IF_ERROR(RemoveRedundantMap(chars_map));
#endif

  return util::OkStatus();
}

// static
util::Status Builder::MergeNmtMap(Builder::CharsMap *chars_map) {
  // Other code points considered as whitespace.
  (*chars_map)[{0x0009}] = {0x20};  // TAB
  (*chars_map)[{0x000A}] = {0x20};  // LINE FEED
  (*chars_map)[{0x000C}] = {0x20};  // FORM FEED
  (*chars_map)[{0x000D}] = {0x20};  // CARRIAGE RETURN
  (*chars_map)[{0x1680}] = {0x20};  // OGHAM SPACE MARK
  (*chars_map)[{0x200B}] = {0x20};  // ZERO WIDTH SPACE
  (*chars_map)[{0x200E}] = {0x20};  // LEFT-TO-RIGHT MARK
  (*chars_map)[{0x200F}] = {0x20};  // RIGHT-TO-LEFT MARK
  (*chars_map)[{0x2028}] = {0x20};  // LINE SEPARATOR
  (*chars_map)[{0x2029}] = {0x20};  // PARAGRAPH SEPARATOR
  (*chars_map)[{0x2581}] = {0x20};  // LOWER ONE EIGHT BLOCK
  (*chars_map)[{0xFEFF}] = {0x20};  // ZERO WIDTH NO-BREAK
  (*chars_map)[{0xFFFD}] = {0x20};  // REPLACEMENT CHARACTER
  (*chars_map)[{0x200C}] = {0x20};  // ZERO WIDTH NON-JOINER
  //  (*chars_map)[{0x200D}] = {0x20};  // ZERO WIDTH JOINER

  // Ascii Control characters
  (*chars_map)[{0x0001}] = {};
  (*chars_map)[{0x0002}] = {};
  (*chars_map)[{0x0003}] = {};
  (*chars_map)[{0x0004}] = {};
  (*chars_map)[{0x0005}] = {};
  (*chars_map)[{0x0006}] = {};
  (*chars_map)[{0x0007}] = {};
  (*chars_map)[{0x0008}] = {};
  (*chars_map)[{0x000B}] = {};
  (*chars_map)[{0x000E}] = {};
  (*chars_map)[{0x000F}] = {};
  (*chars_map)[{0x0010}] = {};
  (*chars_map)[{0x0011}] = {};
  (*chars_map)[{0x0012}] = {};
  (*chars_map)[{0x0013}] = {};
  (*chars_map)[{0x0014}] = {};
  (*chars_map)[{0x0015}] = {};
  (*chars_map)[{0x0016}] = {};
  (*chars_map)[{0x0017}] = {};
  (*chars_map)[{0x0018}] = {};
  (*chars_map)[{0x0019}] = {};
  (*chars_map)[{0x001A}] = {};
  (*chars_map)[{0x001B}] = {};
  (*chars_map)[{0x001C}] = {};
  (*chars_map)[{0x001D}] = {};
  (*chars_map)[{0x001E}] = {};
  (*chars_map)[{0x001F}] = {};

  //  <control-007F>..<control-009F>
  (*chars_map)[{0x007F}] = {};
  (*chars_map)[{0x008F}] = {};
  (*chars_map)[{0x009F}] = {};

  // Do not normalize FULL_WIDTH TILDE, since FULL_WIDTH TILDE
  // and HALF_WIDTH TILDE are used differently in Japanese.
  (*chars_map).erase({0xFF5E});

  return util::OkStatus();
}

// static
util::Status Builder::BuildNFKC_CFMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  CharsMap nfkc_map;
  RETURN_IF_ERROR(Builder::BuildNFKCMap(&nfkc_map));
  RETURN_IF_ERROR(Builder::MergeUnicodeCaseFoldMap(&nfkc_map));
  *chars_map = std::move(nfkc_map);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif

  return util::OkStatus();
}

//  static
util::Status Builder::BuildNmtNFKC_CFMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  CharsMap nfkc_map;
  RETURN_IF_ERROR(Builder::BuildNmtNFKCMap(&nfkc_map));
  RETURN_IF_ERROR(Builder::MergeUnicodeCaseFoldMap(&nfkc_map));
  *chars_map = std::move(nfkc_map);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif

  return util::OkStatus();
}

// static
util::Status Builder::BuildNFKDMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  constexpr int kMaxUnicode = 0x10FFFF;
  for (char32 cp = 1; cp <= kMaxUnicode; ++cp) {
    if (!U_IS_UNICODE_CHAR(cp)) {
      continue;
    }
    const auto nfkd = ToNFKD({cp});
    if (nfkd.size() >= 2 || (nfkd.size() == 1 && nfkd[0] != cp)) {
      (*chars_map)[{cp}] = nfkd;
    }
  }
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif
  return util::OkStatus();
}

// static
util::Status Builder::BuildNFDMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  constexpr int kMaxUnicode = 0x10FFFF;
  for (char32 cp = 1; cp <= kMaxUnicode; ++cp) {
    if (!U_IS_UNICODE_CHAR(cp)) {
      continue;
    }
    const auto nfd = ToNFD({cp});
    if (nfd.size() >= 2 || (nfd.size() == 1 && nfd[0] != cp)) {
      (*chars_map)[{cp}] = nfd;
    }
  }

#else
  ABSL_LOG(ERROR) << kCompileError;
#endif
  return util::OkStatus();
}

// static
util::Status Builder::BuildNFKD_CFMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  CharsMap nfkd_map;
  RETURN_IF_ERROR(Builder::BuildNFKDMap(&nfkd_map));
  RETURN_IF_ERROR(Builder::MergeUnicodeCaseFoldMap(&nfkd_map));
  *chars_map = std::move(nfkd_map);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif
  return util::OkStatus();
}

// static
util::Status Builder::BuildNFC_CFMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  CharsMap nfc_map;
  RETURN_IF_ERROR(Builder::BuildNFKDMap(&nfc_map));
  RETURN_IF_ERROR(Builder::MergeUnicodeCaseFoldMap(&nfc_map));
  *chars_map = std::move(nfc_map);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif
  return util::OkStatus();
}

// static
util::Status Builder::BuildNFD_CFMap(CharsMap *chars_map) {
#ifdef ENABLE_NFKC_COMPILE
  CharsMap nfd_map;
  RETURN_IF_ERROR(Builder::BuildNFDMap(&nfd_map));
  RETURN_IF_ERROR(Builder::MergeUnicodeCaseFoldMap(&nfd_map));
  *chars_map = std::move(nfd_map);
#else
  ABSL_LOG(ERROR) << kCompileError;
#endif
  return util::OkStatus();
}

// static
util::Status Builder::LoadCharsMap(absl::string_view filename,
                                   CharsMap *chars_map) {
  ABSL_LOG(INFO) << "Loading mapping file: " << filename.data();
  CHECK_OR_RETURN(chars_map);

  auto input = filesystem::NewReadableFile(filename);

  RETURN_IF_ERROR(input->status());

  std::string line;
  chars_map->clear();
  while (input->ReadLine(&line)) {
    std::vector<std::string> fields =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    ABSL_CHECK_GE(fields.size(), 1);
    if (fields.size() == 1) fields.push_back("");  // Deletion rule.
    std::vector<char32> src, trg;
    for (auto s : absl::StrSplit(fields[0], ' ')) {
      if (s.empty()) continue;
      absl::ConsumePrefix(&s, "U+");
      src.push_back(string_util::HexToInt<char32>(s));
    }
    for (auto s : absl::StrSplit(fields[1], ' ')) {
      if (s.empty()) continue;
      absl::ConsumePrefix(&s, "U+");
      trg.push_back(string_util::HexToInt<char32>(s));
    }
    CHECK_OR_RETURN(!src.empty());
    (*chars_map)[src] = trg;
  }

  return util::OkStatus();
}

// static
util::Status Builder::SaveCharsMap(absl::string_view filename,
                                   const Builder::CharsMap &chars_map) {
  auto output = filesystem::NewWritableFile(filename);
  RETURN_IF_ERROR(output->status());

  for (const auto &c : chars_map) {
    std::vector<std::string> src, trg;
    string_util::UnicodeText srcu, trgu;
    for (char32 v : c.first) {
      src.push_back(string_util::IntToHex(v));
      srcu.push_back(v);
    }
    for (char32 v : c.second) {
      trg.push_back(string_util::IntToHex(v));
      trgu.push_back(v);
    }
    std::string line = absl::StrJoin(src, " ") + "\t" +
                       absl::StrJoin(trg, " ") + "\t# " +
                       string_util::UnicodeTextToUTF8(c.first) + " => " +
                       string_util::UnicodeTextToUTF8(c.second);
    line = absl::StrReplaceAll(
        line,
        {{"\b", " "}, {"\v", " "}, {"\f", " "}, {"\n", " "}, {"\r", " "}});
    output->WriteLine(line);
  }

  return util::OkStatus();
}

// static
util::Status Builder::RemoveRedundantMap(CharsMap *chars_map) {
  CHECK_OR_RETURN(chars_map);

  CharsMap new_chars_map;
  size_t max_len = 0;
  for (const auto &p : *chars_map) {
    max_len = std::max(p.first.size(), max_len);
    if (p.first.size() == 1) {
      new_chars_map.insert(p);
    }
  }
  CHECK_GT_OR_RETURN(max_len, 0);

  // Checks whether the rules with size of `len` can be normalized by
  // the rules with size of [1 .. len - 1].
  for (size_t len = 2; len <= max_len; ++len) {
    for (const auto &p : *chars_map) {
      if (p.first.size() == len &&
          p.second != Normalize(new_chars_map, p.first, len - 1)) {
        new_chars_map.insert(p);
      }
    }
  }

  // Verify all characters in `chars_map` are normalized by `new_chars_map`.
  for (const auto &p : *chars_map) {
    CHECK_EQ_OR_RETURN(p.second, Normalize(new_chars_map, p.first, max_len));
  }

  *chars_map = std::move(new_chars_map);

  return util::OkStatus();
}
}  // namespace normalizer
}  // namespace sentencepiece
