// Copyright 2021 TF.Text Authors.
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
// limitations under the License.

#include "tensorflow_text/core/kernels/whitespace_tokenizer_config_builder.h"

#include <iterator>
#include <string>

#include "icu4c/source/common/unicode/appendable.h"
#include "icu4c/source/common/unicode/bytestream.h"
#include "icu4c/source/common/unicode/edits.h"
#include "icu4c/source/common/unicode/normalizer2.h"
#include "icu4c/source/common/unicode/schriter.h"
#include "icu4c/source/common/unicode/stringoptions.h"
#include "icu4c/source/common/unicode/stringpiece.h"
#include "icu4c/source/common/unicode/uchar.h"
#include "icu4c/source/common/unicode/ucnv.h"
#include "icu4c/source/common/unicode/ucnv_err.h"
#include "icu4c/source/common/unicode/umachine.h"
#include "icu4c/source/common/unicode/uniset.h"
#include "icu4c/source/common/unicode/unistr.h"
#include "icu4c/source/common/unicode/uset.h"
#include "icu4c/source/common/unicode/utf.h"
#include "icu4c/source/common/unicode/utf8.h"
#include "icu4c/source/common/unicode/utypes.h"

namespace tensorflow {
namespace text {

std::string BuildWhitespaceString() {
  icu::UnicodeString unicode_string;
  icu::UnicodeStringAppendable appendable_unicode_string(unicode_string);
  // The maximum codepoint in Unicode is 0x0010FFFF.
  for (UChar32 cp = 0; cp <= 0x0010FFFF; ++cp) {
    if (U_IS_UNICODE_CHAR(cp) && u_isUWhiteSpace(cp)) {
      appendable_unicode_string.appendCodePoint(cp);
    }
  }
  std::string str;
  unicode_string.toUTF8String(str);
  return str;
}

std::string BuildWhitespaceTokenizerConfig() {
  // The maximum codepoint in Unicode is 0x0010FFFF.
  UChar32 max_unicode_char = 0x0010FFFF;
  // The string will hold our bit array
  std::string bitset((max_unicode_char >> 3) + 1, 0);
  auto bitdata = bitset.begin();
  UChar32 largest_whitespace = 0;
  int shift = 0;
  for (UChar32 cp = 0; cp <= max_unicode_char; ++cp, ++shift) {
    if (shift == 8) {
      ++bitdata;
      shift = 0;
    }
    bool is_whitespace = U_IS_UNICODE_CHAR(cp) && u_isUWhiteSpace(cp);
    largest_whitespace = is_whitespace ? cp : largest_whitespace;
    *bitdata |= is_whitespace << shift;
  }
  return bitset.substr(0, (largest_whitespace >> 3) + 1);
}

}  // namespace text
}  // namespace tensorflow
