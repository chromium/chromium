// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/url_canon_internal.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#ifdef __SSE2__
#include <immintrin.h>
#elif defined(__aarch64__)
#include <arm_neon.h>
#endif

#include <cstdio>
#include <string>

#include "base/bits.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "url/url_features.h"

namespace url {

namespace {

// Find the initial segment of the given string that consists solely
// of characters valid for CHAR_QUERY. (We can have false negatives in
// one specific case, namely the exclamation mark 0x21, but false negatives
// are fine, and it's not worth adding a separate test for.) This is
// a fast path to speed up checking of very long query strings that are
// already valid, which happen on some web pages.
//
// This has some startup cost to load the constants and such, so it's
// usually not worth it for short strings.
size_t FindInitialQuerySafeString(std::string_view source) {
#if defined(__SSE2__) || defined(__aarch64__)
  constexpr size_t kChunkSize = 16;
  size_t i;
  for (i = 0; i < base::bits::AlignDown(source.length(), kChunkSize);
       i += kChunkSize) {
    char b __attribute__((vector_size(16)));
    memcpy(&b, source.data() + i, sizeof(b));

    // Compare each element with the ranges for CHAR_QUERY
    // (see kSharedCharTypeTable), vectorized so that it creates
    // a mask of which elements match. For completeness, we could
    // have had (...) | b == 0x21 here, but exclamation marks are
    // rare and the extra test costs us some time.
    auto mask = b >= 0x24 && b <= 0x7e && b != 0x27 && b != 0x3c && b != 0x3e;

#ifdef __SSE2__
    if (_mm_movemask_epi8(reinterpret_cast<__m128i>(mask)) != 0xffff) {
      return i;
    }
#else
    if (vminvq_u8(reinterpret_cast<uint8x16_t>(mask)) == 0) {
      return i;
    }
#endif
  }
  return i;
#else
  // Need SIMD support (with fast reductions) for this to be efficient.
  return 0;
#endif
}

template <typename CHAR, typename UCHAR>
void DoAppendStringOfType(std::basic_string_view<CHAR> source,
                          SharedCharTypes type,
                          CanonOutput* output) {
  size_t i = 0;
  size_t length = source.length();
  // We only instantiate this for char, to avoid a Clang crash
  // (and because Append() does not support converting).
  if constexpr (sizeof(CHAR) == 1) {
    if (type == CHAR_QUERY && length >= kMinimumLengthForSIMD) {
      i = FindInitialQuerySafeString(source);
      output->Append(source.data(), i);
    }
  }
  for (; i < length; i++) {
    if (static_cast<UCHAR>(source[i]) >= 0x80) {
      // ReadUTFCharLossy will fill the code point with
      // kUnicodeReplacementCharacter when the input is invalid, which is what
      // we want.
      base_icu::UChar32 code_point;
      ReadUTFCharLossy(source.data(), &i, length, &code_point);
      AppendUTF8EscapedValue(code_point, output);
    } else {
      // Just append the 7-bit character, possibly escaping it.
      unsigned char uch = static_cast<unsigned char>(source[i]);
      if (!IsCharOfType(uch, type))
        AppendEscapedChar(uch, output);
      else
        output->push_back(uch);
    }
  }
}

// This function assumes the input values are all contained in 8-bit,
// although it allows any type. Returns true if input is valid, false if not.
template <typename CHAR, typename UCHAR>
void DoAppendInvalidNarrowString(const CHAR* spec,
                                 size_t begin,
                                 size_t end,
                                 CanonOutput* output) {
  for (size_t i = begin; i < end; i++) {
    UCHAR uch = static_cast<UCHAR>(spec[i]);
    if (uch >= 0x80) {
      // Handle UTF-8/16 encodings. This call will correctly handle the error
      // case by appending the invalid character.
      AppendUTF8EscapedChar(spec, &i, end, output);
    } else if (uch <= ' ' || uch == 0x7f) {
      // This function is for error handling, so we escape all control
      // characters and spaces, but not anything else since we lack
      // context to do something more specific.
      AppendEscapedChar(static_cast<unsigned char>(uch), output);
    } else {
      output->push_back(static_cast<char>(uch));
    }
  }
}

// Overrides one component, see the Replacements structure for
// what the various combionations of source pointer and component mean.
void DoOverrideComponent(const char* override_source,
                         const Component& override_component,
                         const char** dest,
                         Component* dest_component) {
  if (override_source) {
    *dest = override_source;
    *dest_component = override_component;
  }
}

// Similar to DoOverrideComponent except that it takes a UTF-16 input and does
// not actually set the output character pointer.
//
// The input is converted to UTF-8 at the end of the given buffer as a temporary
// holding place. The component identifying the portion of the buffer used in
// the |utf8_buffer| will be specified in |*dest_component|.
//
// This will not actually set any |dest| pointer like DoOverrideComponent
// does because all of the pointers will point into the |utf8_buffer|, which
// may get resized while we're overriding a subsequent component. Instead, the
// caller should use the beginning of the |utf8_buffer| as the string pointer
// for all components once all overrides have been prepared.
bool PrepareUTF16OverrideComponent(
    bool should_override,
    std::optional<std::u16string_view> override_source,
    CanonOutput* utf8_buffer,
    Component* dest_component) {
  bool success = true;

  if (should_override) {
    if (!override_source.has_value()) {
      // Non-"valid" component (means delete), so we need to preserve that.
      *dest_component = Component();
    } else {
      auto override_source_value = override_source.value();

      // Convert to UTF-8.
      dest_component->begin = utf8_buffer->length();
      success = ConvertUTF16ToUTF8(override_source_value, utf8_buffer);
      dest_component->len = utf8_buffer->length() - dest_component->begin;
    }
  }

  return success;
}

}  // namespace

const char kCharToHexLookup[8] = {
    0,         // 0x00 - 0x1f
    '0',       // 0x20 - 0x3f: digits 0 - 9 are 0x30 - 0x39
    'A' - 10,  // 0x40 - 0x5f: letters A - F are 0x41 - 0x46
    'a' - 10,  // 0x60 - 0x7f: letters a - f are 0x61 - 0x66
    0,         // 0x80 - 0x9F
    0,         // 0xA0 - 0xBF
    0,         // 0xC0 - 0xDF
    0,         // 0xE0 - 0xFF
};

const base_icu::UChar32 kUnicodeReplacementCharacter = 0xfffd;

void AppendStringOfType(std::string_view source,
                        SharedCharTypes type,
                        CanonOutput* output) {
  DoAppendStringOfType<char, unsigned char>(source, type, output);
}

void AppendStringOfType(std::u16string_view source,
                        SharedCharTypes type,
                        CanonOutput* output) {
  DoAppendStringOfType<char16_t, char16_t>(source, type, output);
}

bool ReadUTFCharLossy(const char* str,
                      size_t* begin,
                      size_t length,
                      base_icu::UChar32* code_point_out) {
  if (!base::ReadUnicodeCharacter(str, length, begin, code_point_out)) {
    *code_point_out = kUnicodeReplacementCharacter;
    return false;
  }
  return true;
}

bool ReadUTFCharLossy(const char16_t* str,
                      size_t* begin,
                      size_t length,
                      base_icu::UChar32* code_point_out) {
  if (!base::ReadUnicodeCharacter(str, length, begin, code_point_out)) {
    *code_point_out = kUnicodeReplacementCharacter;
    return false;
  }
  return true;
}

void AppendInvalidNarrowString(const char* spec,
                               size_t begin,
                               size_t end,
                               CanonOutput* output) {
  DoAppendInvalidNarrowString<char, unsigned char>(spec, begin, end, output);
}

void AppendInvalidNarrowString(const char16_t* spec,
                               size_t begin,
                               size_t end,
                               CanonOutput* output) {
  DoAppendInvalidNarrowString<char16_t, char16_t>(spec, begin, end, output);
}

bool ConvertUTF16ToUTF8(std::u16string_view input, CanonOutput* output) {
  bool success = true;
  for (size_t i = 0; i < input.length(); i++) {
    base_icu::UChar32 code_point;
    success &= ReadUTFCharLossy(input.data(), &i, input.length(), &code_point);
    AppendUTF8Value(code_point, output);
  }
  return success;
}

bool ConvertUTF8ToUTF16(std::string_view input,
                        CanonOutputT<char16_t>* output) {
  bool success = true;
  for (size_t i = 0; i < input.length(); i++) {
    base_icu::UChar32 code_point;
    success &= ReadUTFCharLossy(input.data(), &i, input.length(), &code_point);
    AppendUTF16Value(code_point, output);
  }
  return success;
}

void SetupOverrideComponents(const char* base,
                             const Replacements<char>& repl,
                             URLComponentSource<char>* source,
                             Parsed* parsed) {
  // Get the source and parsed structures of the things we are replacing.
  const URLComponentSource<char>& repl_source = repl.sources();
  const Parsed& repl_parsed = repl.components();

  DoOverrideComponent(repl_source.scheme, repl_parsed.scheme, &source->scheme,
                      &parsed->scheme);
  DoOverrideComponent(repl_source.username, repl_parsed.username,
                      &source->username, &parsed->username);
  DoOverrideComponent(repl_source.password, repl_parsed.password,
                      &source->password, &parsed->password);

  DoOverrideComponent(repl_source.host, repl_parsed.host, &source->host,
                      &parsed->host);
  if (!url::IsUsingStandardCompliantNonSpecialSchemeURLParsing()) {
    // For backward compatibility, the following is probably required while the
    // flag is disabled by default.
    if (parsed->host.len == -1) {
      parsed->host.len = 0;
    }
  }

  DoOverrideComponent(repl_source.port, repl_parsed.port, &source->port,
                      &parsed->port);
  DoOverrideComponent(repl_source.path, repl_parsed.path, &source->path,
                      &parsed->path);
  DoOverrideComponent(repl_source.query, repl_parsed.query, &source->query,
                      &parsed->query);
  DoOverrideComponent(repl_source.ref, repl_parsed.ref, &source->ref,
                      &parsed->ref);
}

bool SetupUTF16OverrideComponents(const char* base,
                                  const Replacements<char16_t>& repl,
                                  CanonOutput* utf8_buffer,
                                  URLComponentSource<char>* source,
                                  Parsed* parsed) {
  bool success = true;

  // Get the source and parsed structures of the things we are replacing.
  const URLComponentSource<char16_t>& repl_source = repl.sources();
  const Parsed& repl_parsed = repl.components();

  success &= PrepareUTF16OverrideComponent(
      repl_source.scheme != nullptr,
      repl_parsed.scheme.maybe_as_string_view_on(repl_source.scheme),
      utf8_buffer, &parsed->scheme);
  success &= PrepareUTF16OverrideComponent(
      repl_source.username != nullptr,
      repl_parsed.username.maybe_as_string_view_on(repl_source.username),
      utf8_buffer, &parsed->username);
  success &= PrepareUTF16OverrideComponent(
      repl_source.password != nullptr,
      repl_parsed.password.maybe_as_string_view_on(repl_source.password),
      utf8_buffer, &parsed->password);
  success &= PrepareUTF16OverrideComponent(
      repl_source.host != nullptr,
      repl_parsed.host.maybe_as_string_view_on(repl_source.host), utf8_buffer,
      &parsed->host);
  success &= PrepareUTF16OverrideComponent(
      repl_source.port != nullptr,
      repl_parsed.port.maybe_as_string_view_on(repl_source.port), utf8_buffer,
      &parsed->port);
  success &= PrepareUTF16OverrideComponent(
      repl_source.path != nullptr,
      repl_parsed.path.maybe_as_string_view_on(repl_source.path), utf8_buffer,
      &parsed->path);
  success &= PrepareUTF16OverrideComponent(
      repl_source.query != nullptr,
      repl_parsed.query.maybe_as_string_view_on(repl_source.query), utf8_buffer,
      &parsed->query);
  success &= PrepareUTF16OverrideComponent(
      repl_source.ref != nullptr,
      repl_parsed.ref.maybe_as_string_view_on(repl_source.ref), utf8_buffer,
      &parsed->ref);

  // PrepareUTF16OverrideComponent will not have set the data pointer since the
  // buffer could be resized, invalidating the pointers. We set the data
  // pointers for affected components now that the buffer is finalized.
  if (repl_source.scheme)
    source->scheme = utf8_buffer->data();
  if (repl_source.username)
    source->username = utf8_buffer->data();
  if (repl_source.password)
    source->password = utf8_buffer->data();
  if (repl_source.host)
    source->host = utf8_buffer->data();
  if (repl_source.port)
    source->port = utf8_buffer->data();
  if (repl_source.path)
    source->path = utf8_buffer->data();
  if (repl_source.query)
    source->query = utf8_buffer->data();
  if (repl_source.ref)
    source->ref = utf8_buffer->data();

  return success;
}

#ifndef WIN32

int _itoa_s(int value, char* buffer, size_t size_in_chars, int radix) {
  const char* format_str;
  if (radix == 10)
    format_str = "%d";
  else if (radix == 16)
    format_str = "%x";
  else
    return EINVAL;

  int written = snprintf(buffer, size_in_chars, format_str, value);
  if (static_cast<size_t>(written) >= size_in_chars) {
    // Output was truncated, or written was negative.
    return EINVAL;
  }
  return 0;
}

#endif  // !WIN32

}  // namespace url
