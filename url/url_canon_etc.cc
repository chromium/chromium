// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <array>
#include <string_view>

// Canonicalizers for random bits that aren't big enough for their own files.

#include <string.h>

#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"

namespace url {

namespace {

// Returns true if the given character should be removed from the middle of a
// URL.
inline bool IsRemovableURLWhitespace(int ch) {
  return ch == '\r' || ch == '\n' || ch == '\t';
}

// Backend for RemoveURLWhitespace (see declaration in url_canon.h).
// It sucks that we have to do this, since this takes about 13% of the total URL
// canonicalization time.
template <typename CHAR>
const CHAR* DoRemoveURLWhitespace(const CHAR* input,
                                  int input_len,
                                  CanonOutputT<CHAR>* buffer,
                                  int* output_len,
                                  bool* potentially_dangling_markup) {
  // Fast verification that there's nothing that needs removal. This is the 99%
  // case, so we want it to be fast and don't care about impacting the speed
  // when we do find whitespace.
  bool found_whitespace = false;
  if (sizeof(*input) == 1 && input_len >= kMinimumLengthForSIMD) {
    // For large strings, memchr is much faster than any scalar code we can
    // write, even if we need to run it three times. (If this turns out to still
    // be a bottleneck, we could write our own vector code, but given that
    // memchr is so fast, it's unlikely to be relevant.)
    found_whitespace = memchr(input, '\n', input_len) != nullptr ||
                       memchr(input, '\r', input_len) != nullptr ||
                       memchr(input, '\t', input_len) != nullptr;
  } else {
    for (int i = 0; i < input_len; i++) {
      if (!IsRemovableURLWhitespace(input[i]))
        continue;
      found_whitespace = true;
      break;
    }
  }

  if (!found_whitespace) {
    // Didn't find any whitespace, we don't need to do anything. We can just
    // return the input as the output.
    *output_len = input_len;
    return input;
  }

  // Skip whitespace removal for `data:` URLs.
  //
  // TODO(mkwst): Ideally, this would use something like `base::StartsWith`, but
  // that turns out to be difficult to do correctly given this function's
  // character type templating.
  if (input_len > 5 && input[0] == 'd' && input[1] == 'a' && input[2] == 't' &&
      input[3] == 'a' && input[4] == ':') {
    *output_len = input_len;
    return input;
  }

  // Remove the whitespace into the new buffer and return it.
  for (int i = 0; i < input_len; i++) {
    if (!IsRemovableURLWhitespace(input[i])) {
      if (potentially_dangling_markup && input[i] == 0x3C)
        *potentially_dangling_markup = true;
      buffer->push_back(input[i]);
    }
  }
  *output_len = buffer->length();
  return buffer->data();
}

// Contains the canonical version of each possible input letter in the scheme
// (basically, lower-cased). The corresponding entry will be 0 if the letter
// is not allowed in a scheme.
// clang-format off
const std::array<char, 0x80> kSchemeCanonical = {
// 00-1f: all are invalid
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
//  ' '   !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  '+',  0,  '-', '.',  0,
//   0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
//   @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O
     0 , 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
//   P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',  0,   0 ,  0,   0 ,  0,
//   `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o
     0 , 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
//   p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',  0 ,  0 ,  0 ,  0 ,  0 };
// clang-format on

// This could be a table lookup as well by setting the high bit for each
// valid character, but it's only called once per URL, and it makes the lookup
// table easier to read not having extra stuff in it.
inline bool IsSchemeFirstChar(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

template <typename CHAR, typename UCHAR>
bool DoScheme(std::optional<std::basic_string_view<CHAR>> input,
              CanonOutput* output,
              Component* out_scheme) {
  if (!input.has_value() || input->empty()) {
    // Scheme is unspecified or empty, convert to empty by appending a colon.
    *out_scheme = Component(output->length(), 0);
    output->push_back(':');
    return false;
  }

  auto input_value = input.value();

  // The output scheme starts from the current position.
  out_scheme->begin = output->length();

  // Danger: it's important that this code does not strip any characters;
  // it only emits the canonical version (be it valid or escaped) for each
  // of the input characters. Stripping would put it out of sync with
  // FindAndCompareScheme, which could cause some security checks on
  // schemes to be incorrect.
  bool success = true;
  for (size_t i = 0; i < input_value.length(); i++) {
    UCHAR ch = static_cast<UCHAR>(input_value[i]);
    char replacement = 0;
    if (ch < 0x80) {
      if (i == 0) {
        // Need to do a special check for the first letter of the scheme.
        if (IsSchemeFirstChar(static_cast<unsigned char>(ch)))
          replacement = kSchemeCanonical[ch];
      } else {
        replacement = kSchemeCanonical[ch];
      }
    }

    if (replacement) {
      output->push_back(replacement);
    } else if (ch == '%') {
      // Canonicalizing the scheme multiple times should lead to the same
      // result. Since invalid characters will be escaped, we need to preserve
      // the percent to avoid multiple escaping. The scheme will be invalid.
      success = false;
      output->push_back('%');
    } else {
      // Invalid character, store it but mark this scheme as invalid.
      success = false;

      // This will escape the output and also handle encoding issues.
      // Ignore the return value since we already failed.
      AppendUTF8EscapedChar(input_value.data(), &i, input_value.length(),
                            output);
    }
  }

  // The output scheme ends with the the current position, before appending
  // the colon.
  out_scheme->len = output->length() - out_scheme->begin;
  output->push_back(':');
  return success;
}

// The username and password components reference ranges in the corresponding
// *_spec strings. Typically, these specs will be the same (we're
// canonicalizing a single source string), but may be different when
// replacing components.
template <typename CHAR, typename UCHAR>
bool DoUserInfo(std::optional<std::basic_string_view<CHAR>> username,
                std::optional<std::basic_string_view<CHAR>> password,
                CanonOutput* output,
                Component* out_username,
                Component* out_password) {
  if ((!username.has_value() || username->empty()) &&
      (!password.has_value() || password->empty())) {
    // Common case: no user info. We strip empty username/passwords.
    *out_username = Component();
    *out_password = Component();
    return true;
  }

  // Write the username.
  out_username->begin = output->length();
  if (username.has_value() && !username->empty()) {
    // This will escape characters not valid for the username.
    AppendStringOfType(username.value(), CHAR_USERINFO, output);
  }
  out_username->len = output->length() - out_username->begin;

  // When there is a password, we need the separator. Note that we strip
  // empty but specified passwords.
  if (password.has_value() && !password->empty()) {
    output->push_back(':');
    out_password->begin = output->length();
    AppendStringOfType(password.value(), CHAR_USERINFO, output);
    out_password->len = output->length() - out_password->begin;
  } else {
    *out_password = Component();
  }

  output->push_back('@');
  return true;
}

// Helper functions for converting port integers to strings.
inline void WritePortInt(char* output, int output_len, int port) {
  _itoa_s(port, output, output_len, 10);
}

// This function will prepend the colon if there will be a port.
template <typename CHAR, typename UCHAR>
bool DoPort(const CHAR* spec,
            const Component& port,
            int default_port_for_scheme,
            CanonOutput* output,
            Component* out_port) {
  int port_num = ParsePort(spec, port);
  if (port_num == PORT_UNSPECIFIED || port_num == default_port_for_scheme) {
    *out_port = Component();
    return true;  // Leave port empty.
  }

  if (port_num == PORT_INVALID) {
    // Invalid port: We'll copy the text from the input so the user can see
    // what the error was, and mark the URL as invalid by returning false.
    output->push_back(':');
    out_port->begin = output->length();
    AppendInvalidNarrowString(spec, static_cast<size_t>(port.begin),
                              static_cast<size_t>(port.end()), output);
    out_port->len = output->length() - out_port->begin;
    return false;
  }

  // Convert port number back to an integer. Max port value is 5 digits, and
  // the Parsed::ExtractPort will have made sure the integer is in range.
  const int buf_size = 6;
  std::array<char, buf_size> buf;
  WritePortInt(buf.data(), buf_size, port_num);

  // Append the port number to the output, preceded by a colon.
  output->push_back(':');
  out_port->begin = output->length();
  for (int i = 0; i < buf_size && buf[i]; i++)
    output->push_back(buf[i]);

  out_port->len = output->length() - out_port->begin;
  return true;
}

// clang-format off
//   Percent-escape all characters from the fragment percent-encode set
//   https://url.spec.whatwg.org/#fragment-percent-encode-set
const std::array<bool, 0x80> kShouldEscapeCharInFragment = {
//  Control characters (0x00-0x1F)
    true,  true,  true,  true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,  true,  true,  true,
//  ' '    !      "      #      $      %      &      '
    true,  false, true,  false, false, false, false, false,
//  (      )      *      +      ,      -      .      /
    false, false, false, false, false, false, false, false,
//  0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, false,
//  8      9      :      ;      <      =      >      ?
    false, false, false, false, true,  false, true,  false,
//  @      A      B      C      D      E      F      G
    false, false, false, false, false, false, false, false,
//  H      I      J      K      L      M      N      O
    false, false, false, false, false, false, false, false,
//  P      Q      R      S      T      U      V      W
    false, false, false, false, false, false, false, false,
//  X      Y      Z      [      \      ]      ^      _
    false, false, false, false, false, false, false, false,
//  `      a      b      c      d      e      f      g
    true,  false, false, false, false, false, false, false,
//  h      i      j      k      l      m      n      o
    false, false, false, false, false, false, false, false,
//  p      q      r      s      t      u      v      w
    false, false, false, false, false, false, false, false,
//  x      y      z      {      |      }      ~      DELETE
    false, false, false, false, false, false, false, true
};
// clang-format on

template <typename CHAR, typename UCHAR>
void DoCanonicalizeRef(std::optional<std::basic_string_view<CHAR>> input,
                       CanonOutput* output,
                       Component* out_ref) {
  if (!input.has_value()) {
    // Common case of no ref.
    *out_ref = Component();
    return;
  }
  auto input_value = input.value();

  // Append the ref separator. Note that we need to do this even when the ref
  // is empty but present.
  output->push_back('#');
  out_ref->begin = output->length();

  // Now iterate through all the characters, converting to UTF-8 and validating.
  for (size_t i = 0; i < input_value.length(); ++i) {
    UCHAR current_char = static_cast<UCHAR>(input.value()[i]);
    if (current_char < 0x80) {
      if (kShouldEscapeCharInFragment[current_char])
        AppendEscapedChar(static_cast<unsigned char>(input_value[i]), output);
      else
        output->push_back(static_cast<char>(input_value[i]));
    } else {
      AppendUTF8EscapedChar(input_value.data(), &i, input_value.length(),
                            output);
    }
  }
  out_ref->len = output->length() - out_ref->begin;
}

}  // namespace

const char* RemoveURLWhitespace(const char* input,
                                int input_len,
                                CanonOutputT<char>* buffer,
                                int* output_len,
                                bool* potentially_dangling_markup) {
  return DoRemoveURLWhitespace(input, input_len, buffer, output_len,
                               potentially_dangling_markup);
}

const char16_t* RemoveURLWhitespace(const char16_t* input,
                                    int input_len,
                                    CanonOutputT<char16_t>* buffer,
                                    int* output_len,
                                    bool* potentially_dangling_markup) {
  return DoRemoveURLWhitespace(input, input_len, buffer, output_len,
                               potentially_dangling_markup);
}

char CanonicalSchemeChar(char16_t ch) {
  if (ch >= 0x80)
    return 0;  // Non-ASCII is not supported by schemes.
  return kSchemeCanonical[ch];
}

bool CanonicalizeScheme(std::optional<std::string_view> input,
                        CanonOutput* output,
                        Component* out_scheme) {
  return DoScheme<char, unsigned char>(input, output, out_scheme);
}

bool CanonicalizeScheme(std::optional<std::u16string_view> input,
                        CanonOutput* output,
                        Component* out_scheme) {
  return DoScheme<char16_t, char16_t>(input, output, out_scheme);
}

bool CanonicalizeUserInfo(std::optional<std::string_view> username,
                          std::optional<std::string_view> password,
                          CanonOutput* output,
                          Component* out_username,
                          Component* out_password) {
  return DoUserInfo<char, unsigned char>(username, password, output,
                                         out_username, out_password);
}

bool CanonicalizeUserInfo(std::optional<std::u16string_view> username,
                          std::optional<std::u16string_view> password,
                          CanonOutput* output,
                          Component* out_username,
                          Component* out_password) {
  return DoUserInfo<char16_t, char16_t>(username, password, output,
                                        out_username, out_password);
}

bool CanonicalizePort(const char* spec,
                      const Component& port,
                      int default_port_for_scheme,
                      CanonOutput* output,
                      Component* out_port) {
  return DoPort<char, unsigned char>(spec, port, default_port_for_scheme,
                                     output, out_port);
}

bool CanonicalizePort(const char16_t* spec,
                      const Component& port,
                      int default_port_for_scheme,
                      CanonOutput* output,
                      Component* out_port) {
  return DoPort<char16_t, char16_t>(spec, port, default_port_for_scheme, output,
                                    out_port);
}

void CanonicalizeRef(std::optional<std::string_view> input,
                     CanonOutput* output,
                     Component* out_ref) {
  DoCanonicalizeRef<char, unsigned char>(input, output, out_ref);
}

void CanonicalizeRef(std::optional<std::u16string_view> input,
                     CanonOutput* output,
                     Component* out_ref) {
  DoCanonicalizeRef<char16_t, char16_t>(input, output, out_ref);
}

}  // namespace url
