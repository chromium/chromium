/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "maldoca/ole/endian_reader.h"

#include "maldoca/base/utf8/unicodetext.h"

namespace maldoca {

void DecodeUTF16(absl::string_view input, std::string *output) {
  std::u16string unicode;
  uint16_t character;
  while (input.size() >= sizeof(character)) {
    LittleEndianReader::ConsumeUInt16(&input, &character);
    // TODO: investigate potential parser breakage without those replacements.
    // Replace C0 controls (CR LF HT FF are allowed).
    if (character <= 0x8 || character == 0xd ||
        (character >= 0x10 && character <= 0x1f) ||
        // Replace C1 controls.
        (character >= 0x7f && character <= 0x9f) ||
        // Replace Unicode's private use area:
        // https://en.wikipedia.org/wiki/Private_Use_Areas
        (character >= 0xe000 && character <= 0xf8ff)) {
      character = ' ';
    }
    unicode.push_back(character);
  }
  *output = base::UTF16ToUTF8(unicode);
}

}  // namespace maldoca

