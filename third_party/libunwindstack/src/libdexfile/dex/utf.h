/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_UTF_H_
#define ART_LIBDEXFILE_DEX_UTF_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <type_traits>

#include "base/macros.h"

/*
 * All UTF-8 in art is actually modified UTF-8. Mostly, this distinction
 * doesn't matter.
 *
 * See http://en.wikipedia.org/wiki/UTF-8#Modified_UTF-8 for the details.
 */
namespace art {

/*
 * Returns the number of UTF-16 characters in the given modified UTF-8 string.
 */
size_t CountModifiedUtf8Chars(const char* utf8);
size_t CountModifiedUtf8Chars(const char* utf8, size_t byte_count);

/*
 * Convert from Modified UTF-8 to UTF-16.
 */
void ConvertModifiedUtf8ToUtf16(uint16_t* utf16_out, const char* utf8_in);
void ConvertModifiedUtf8ToUtf16(uint16_t* utf16_out, size_t out_chars,
                                const char* utf8_in, size_t in_bytes);

/*
 * Compare two modified UTF-8 strings as UTF-16 code point values in a non-locale sensitive manner
 */
ALWAYS_INLINE int CompareModifiedUtf8ToModifiedUtf8AsUtf16CodePointValues(const char* utf8_1,
                                                                          const char* utf8_2);

/*
 * Compare a null-terminated modified UTF-8 string with a UTF-16 string (not null-terminated)
 * as code point values in a non-locale sensitive manner.
 */
int CompareModifiedUtf8ToUtf16AsCodePointValues(const char* utf8, const uint16_t* utf16,
                                                size_t utf16_length);

/*
 * Helper template for converting UTF-16 to UTF-8 and similar encodings.
 *
 * Template arguments:
 *    kUseShortZero: Encode U+0000 as a single byte with value 0 (otherwise emit 0xc0 0x80).
 *    kUse4ByteSequence: Encode valid surrogate pairs as a 4-byte sequence.
 *    kReplaceBadSurrogates: Replace unmatched surrogates with '?' (otherwise use 3-byte sequence).
 *                           Must be false if kUse4ByteSequence is false.
 *    Append: The type of the `append` functor. Should be deduced automatically.
 *
 * Encoding               kUseShortZero kUse4ByteSequence kReplaceBadSurrogates
 * UTF-8                  true          true              true
 * Modified UTF8          false         false             n/a
 * JNI GetStringUTFChars  false         true              false
 */
template <bool kUseShortZero, bool kUse4ByteSequence, bool kReplaceBadSurrogates, typename Append>
void ConvertUtf16ToUtf8(const uint16_t* utf16, size_t char_count, Append&& append);

/*
 * Returns the number of modified UTF-8 bytes needed to represent the given
 * UTF-16 string.
 */
size_t CountModifiedUtf8BytesInUtf16(const uint16_t* chars, size_t char_count);

/*
 * Convert from UTF-16 to Modified UTF-8. Note that the output is _not_
 * NUL-terminated. You probably need to call CountModifiedUtf8BytesInUtf16 before calling
 * this anyway, so if you want a NUL-terminated string, you know where to
 * put the NUL byte.
 */
void ConvertUtf16ToModifiedUtf8(char* utf8_out, size_t byte_count,
                                const uint16_t* utf16_in, size_t char_count);

/*
 * The java.lang.String hashCode() algorithm.
 */
template<typename MemoryType>
int32_t ComputeUtf16Hash(const MemoryType* chars, size_t char_count) {
  static_assert(std::is_same_v<MemoryType, char> ||
                std::is_same_v<MemoryType, uint8_t> ||
                std::is_same_v<MemoryType, uint16_t>);
  using UnsignedMemoryType = std::make_unsigned_t<MemoryType>;
  uint32_t hash = 0;
  while (char_count--) {
    hash = hash * 31 + static_cast<UnsignedMemoryType>(*chars++);
  }
  return static_cast<int32_t>(hash);
}

int32_t ComputeUtf16HashFromModifiedUtf8(const char* utf8, size_t utf16_length);

// Compute a hash code of a modified UTF-8 string. Not the standard java hash since it returns a
// uint32_t and hashes individual chars instead of codepoint words.
uint32_t ComputeModifiedUtf8Hash(const char* chars);
uint32_t ComputeModifiedUtf8Hash(std::string_view chars);

// The starting value of a modified UTF-8 hash.
constexpr uint32_t StartModifiedUtf8Hash() {
  return 0u;
}

// Update a modified UTF-8 hash with one character.
ALWAYS_INLINE
inline uint32_t UpdateModifiedUtf8Hash(uint32_t hash, char c) {
  return hash * 31u + static_cast<uint8_t>(c);
}

// Update a modified UTF-8 hash with characters of a `std::string_view`.
ALWAYS_INLINE
inline uint32_t UpdateModifiedUtf8Hash(uint32_t hash, std::string_view chars) {
  for (char c : chars) {
    hash = UpdateModifiedUtf8Hash(hash, c);
  }
  return hash;
}

/*
 * Retrieve the next UTF-16 character or surrogate pair from a UTF-8 string.
 * single byte, 2-byte and 3-byte UTF-8 sequences result in a single UTF-16
 * character (possibly one half of a surrogate) whereas 4-byte UTF-8 sequences
 * result in a surrogate pair. Use GetLeadingUtf16Char and GetTrailingUtf16Char
 * to process the return value of this function.
 *
 * Advances "*utf8_data_in" to the start of the next character.
 *
 * WARNING: If a string is corrupted by dropping a '\0' in the middle
 * of a multi byte sequence, you can end up overrunning the buffer with
 * reads (and possibly with the writes if the length was computed and
 * cached before the damage). For performance reasons, this function
 * assumes that the string being parsed is known to be valid (e.g., by
 * already being verified). Most strings we process here are coming
 * out of dex files or other internal translations, so the only real
 * risk comes from the JNI NewStringUTF call.
 */
uint32_t GetUtf16FromUtf8(const char** utf8_data_in);

/**
 * Gets the leading UTF-16 character from a surrogate pair, or the sole
 * UTF-16 character from the return value of GetUtf16FromUtf8.
 */
ALWAYS_INLINE uint16_t GetLeadingUtf16Char(uint32_t maybe_pair);

/**
 * Gets the trailing UTF-16 character from a surrogate pair, or 0 otherwise
 * from the return value of GetUtf16FromUtf8.
 */
ALWAYS_INLINE uint16_t GetTrailingUtf16Char(uint32_t maybe_pair);

// Returns a printable (escaped) version of a character.
std::string PrintableChar(uint16_t ch);

// Returns an ASCII string corresponding to the given UTF-8 string.
// Java escapes are used for non-ASCII characters.
std::string PrintableString(const char* utf8);

}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_UTF_H_
