// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/check.h"
#include "base/cpu_reduction_experiment.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_features.h"

namespace url {

namespace {

// This table lists the canonical version of all characters we allow in the
// input, with 0 indicating it is disallowed. We use the magic kEsc value to
// indicate that this character should be escaped. At present, ' ' (SPACE) and
// '*' (asterisk) are still non-compliant to the URL Standard. See
// https://crbug.com/1416013 for details.
const unsigned char kEsc = 0xff;
// clang-format off
const unsigned char kHostCharLookup[0x80] = {
// 00-1f: all are invalid
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
//  ' '   !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /
    kEsc,'!', '"',  0,  '$',  0,  '&', '\'','(', ')', kEsc, '+', ',', '-', '.',  0,
//   0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';' , 0,  '=',  0,   0,
//   @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O
     0,  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
//   P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '[',  0,  ']',  0,  '_',
//   `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
//   p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{',  0, '}',  '~',  0 };
// clang-format on

// https://url.spec.whatwg.org/#forbidden-host-code-point
const uint8_t kForbiddenHost = 0x1;

// TODO(crbug.com/40063064): Merge other lookup tables into this table. That can
// be probably done after https://crbug.com/1416013 is resolved.
//
// This table is currently only used for an opaque-host in non-special URLs.
const uint8_t kHostCharacterTable[128] = {
    kForbiddenHost,  // 0x00 (NUL)
    0,               // 0x01
    0,               // 0x02
    0,               // 0x03
    0,               // 0x04
    0,               // 0x05
    0,               // 0x06
    0,               // 0x07
    0,               // 0x08
    kForbiddenHost,  // 0x09 (TAB)
    kForbiddenHost,  // 0x0A (LF)
    0,               // 0x0B
    0,               // 0x0C
    kForbiddenHost,  // 0x0D (CR)
    0,               // 0x0E
    0,               // 0x0F
    0,               // 0x10
    0,               // 0x11
    0,               // 0x12
    0,               // 0x13
    0,               // 0x14
    0,               // 0x15
    0,               // 0x16
    0,               // 0x17
    0,               // 0x18
    0,               // 0x19
    0,               // 0x1A
    0,               // 0x1B
    0,               // 0x1C
    0,               // 0x1D
    0,               // 0x1E
    0,               // 0x1F
    kForbiddenHost,  // ' '
    0,               // '!'
    0,               // '"'
    kForbiddenHost,  // '#'
    0,               // '$'
    0,               // '%'
    0,               // '&'
    0,               // '\''
    0,               // '('
    0,               // ')'
    0,               // '*'
    0,               // '+'
    0,               // ','
    0,               // '-'
    0,               // '.'
    kForbiddenHost,  // '/'
    0,               // '0'
    0,               // '1'
    0,               // '2'
    0,               // '3'
    0,               // '4'
    0,               // '5'
    0,               // '6'
    0,               // '7'
    0,               // '8'
    0,               // '9'
    kForbiddenHost,  // ':'
    0,               // ';'
    kForbiddenHost,  // '<'
    0,               // '='
    kForbiddenHost,  // '>'
    kForbiddenHost,  // '?'
    kForbiddenHost,  // '@'
    0,               // 'A'
    0,               // 'B'
    0,               // 'C'
    0,               // 'D'
    0,               // 'E'
    0,               // 'F'
    0,               // 'G'
    0,               // 'H'
    0,               // 'I'
    0,               // 'J'
    0,               // 'K'
    0,               // 'L'
    0,               // 'M'
    0,               // 'N'
    0,               // 'O'
    0,               // 'P'
    0,               // 'Q'
    0,               // 'R'
    0,               // 'S'
    0,               // 'T'
    0,               // 'U'
    0,               // 'V'
    0,               // 'W'
    0,               // 'X'
    0,               // 'Y'
    0,               // 'Z'
    kForbiddenHost,  // '['
    kForbiddenHost,  // '\\'
    kForbiddenHost,  // ']'
    kForbiddenHost,  // '^'
    0,               // '_'
    0,               // '`'
    0,               // 'a'
    0,               // 'b'
    0,               // 'c'
    0,               // 'd'
    0,               // 'e'
    0,               // 'f'
    0,               // 'g'
    0,               // 'h'
    0,               // 'i'
    0,               // 'j'
    0,               // 'k'
    0,               // 'l'
    0,               // 'm'
    0,               // 'n'
    0,               // 'o'
    0,               // 'p'
    0,               // 'q'
    0,               // 'r'
    0,               // 's'
    0,               // 't'
    0,               // 'u'
    0,               // 'v'
    0,               // 'w'
    0,               // 'x'
    0,               // 'y'
    0,               // 'z'
    0,               // '{'
    kForbiddenHost,  // '|'
    0,               // '}'
    0,               // '~'
    0,               // 0x7F (DEL)
};
// clang-format on

bool IsForbiddenHostCodePoint(uint8_t ch) {
  return ch <= 0x7F && (kHostCharacterTable[ch] & kForbiddenHost);
}

// RFC1034 maximum FQDN length.
constexpr size_t kMaxHostLength = 253;

// Generous padding to account for the fact that UTS#46 normalization can cause
// a long string to actually shrink and fit within the 253 character RFC1034
// FQDN length limit. Note that this can still be too short for pathological
// cases: An arbitrary number of characters (e.g. U+00AD SOFT HYPHEN) can be
// removed from the input by UTS#46 processing. However, this should be
// sufficient for all normally-encountered, non-abusive hostname strings.
constexpr size_t kMaxHostBufferLength = kMaxHostLength * 5;

constexpr size_t kTempHostBufferLen = 1024;
using StackBuffer = RawCanonOutputT<char, kTempHostBufferLen>;
using StackBufferW = RawCanonOutputT<char16_t, kTempHostBufferLen>;

// Scans a host name and fills in the output flags according to what we find.
// |has_non_ascii| will be true if there are any non-7-bit characters, and
// |has_escaped| will be true if there is a percent sign.
template<typename CHAR, typename UCHAR>
void ScanHostname(const CHAR* spec,
                  const Component& host,
                  bool* has_non_ascii,
                  bool* has_escaped) {
  int end = host.end();
  *has_non_ascii = false;
  *has_escaped = false;
  for (int i = host.begin; i < end; i++) {
    if (static_cast<UCHAR>(spec[i]) >= 0x80)
      *has_non_ascii = true;
    else if (spec[i] == '%')
      *has_escaped = true;
  }
}

// Canonicalizes a host name that is entirely 8-bit characters (even though
// the type holding them may be 16 bits. Escaped characters will be unescaped.
// Non-7-bit characters (for example, UTF-8) will be passed unchanged.
//
// The |*has_non_ascii| flag will be true if there are non-7-bit characters in
// the output.
//
// This function is used in two situations:
//
//  * When the caller knows there is no non-ASCII or percent escaped
//    characters. This is what DoHost does. The result will be a completely
//    canonicalized host since we know nothing weird can happen (escaped
//    characters could be unescaped to non-7-bit, so they have to be treated
//    with suspicion at this point). It does not use the |has_non_ascii| flag.
//
//  * When the caller has an 8-bit string that may need unescaping.
//    DoComplexHost calls us this situation to do unescaping and validation.
//    After this, it may do other IDN operations depending on the value of the
//    |*has_non_ascii| flag.
//
// The return value indicates if the output is a potentially valid host name.
template <CanonMode canon_mode, typename INCHAR, typename OUTCHAR>
bool DoSimpleHost(const INCHAR* host,
                  size_t host_len,
                  CanonOutputT<OUTCHAR>* output,
                  bool* has_non_ascii) {
  *has_non_ascii = false;

  bool success = true;
  for (size_t i = 0; i < host_len; ++i) {
    unsigned int source = host[i];
    if (source == '%') {
      // Unescape first, if possible.
      // Source will be used only if decode operation was successful.
      if (!DecodeEscaped(host, &i, host_len,
                         reinterpret_cast<unsigned char*>(&source))) {
        // Invalid escaped character. There is nothing that can make this
        // host valid. We append an escaped percent so the URL looks reasonable
        // and mark as failed.
        AppendEscapedChar('%', output);
        success = false;
        continue;
      }
    }

    if (source < 0x80) {
      // We have ASCII input, we can use our lookup table.
      unsigned char replacement = kHostCharLookup[source];
      if (!replacement) {
        // Invalid character, add it as percent-escaped and mark as failed.
        AppendEscapedChar(source, output);
        success = false;
      } else if (replacement == kEsc) {
        // This character is valid but should be escaped.
        AppendEscapedChar(source, output);
        if (source == ' ' &&
            url::IsDisallowingSpaceCharacterInURLHostParsing() &&
            canon_mode != CanonMode::kFileURL) {
          success = false;
        }
      } else {
        // Common case, the given character is valid in a hostname, the lookup
        // table tells us the canonical representation of that character (lower
        // cased).
        output->push_back(replacement);
      }
    } else {
      // It's a non-ascii char. Just push it to the output.
      // In case where we have char16 input, and char output it's safe to
      // cast char16->char only if input string was converted to ASCII.
      output->push_back(static_cast<OUTCHAR>(source));
      *has_non_ascii = true;
    }
  }
  return success;
}

// Canonicalizes a host that requires IDN conversion. Returns true on success
template <CanonMode canon_mode>
bool DoIDNHost(const char16_t* src, size_t src_len, CanonOutput* output) {
  int original_output_len = output->length();  // So we can rewind below.

  // We need to escape URL before doing IDN conversion, since punicode strings
  // cannot be escaped after they are created.
  RawCanonOutputW<kTempHostBufferLen> url_escaped_host;
  bool has_non_ascii;
  DoSimpleHost<canon_mode>(src, src_len, &url_escaped_host, &has_non_ascii);
  if (url_escaped_host.length() > kMaxHostBufferLength) {
    AppendInvalidNarrowString(src, 0, src_len, output);
    return false;
  }

  StackBufferW wide_output;
  if (!IDNToASCII(url_escaped_host.view(), &wide_output)) {
    // Some error, give up. This will write some reasonable looking
    // representation of the string to the output.
    AppendInvalidNarrowString(src, 0, src_len, output);
    return false;
  }

  // Now we check the ASCII output like a normal host. It will also handle
  // unescaping. Although we unescaped everything before this function call, if
  // somebody does %00 as fullwidth, ICU will convert this to ASCII.
  bool success = DoSimpleHost<canon_mode>(
      wide_output.data(), wide_output.length(), output, &has_non_ascii);
  if (has_non_ascii) {
    // ICU generated something that DoSimpleHost didn't think looked like
    // ASCII. This is quite rare, but ICU might convert some characters to
    // percent signs which might generate new escape sequences which might in
    // turn be invalid. An example is U+FE6A "small percent" which ICU will
    // name prep into an ASCII percent and then we can interpret the following
    // characters as escaped characters.
    //
    // If DoSimpleHost didn't think the output was ASCII, just escape the
    // thing we gave ICU and give up. DoSimpleHost will have handled a further
    // level of escaping from ICU for simple ASCII cases (i.e. if ICU generates
    // a new escaped ASCII sequence like "%41" we'll unescape it) but it won't
    // do more (like handle escaped non-ASCII sequences). Handling the escaped
    // ASCII isn't strictly necessary, but DoSimpleHost handles this case
    // anyway so we handle it/
    output->set_length(original_output_len);
    AppendInvalidNarrowString(wide_output.data(), 0, wide_output.length(),
                              output);
    return false;
  }
  return success;
}

// 8-bit convert host to its ASCII version: this converts the UTF-8 input to
// UTF-16. The has_escaped flag should be set if the input string requires
// unescaping.
template <CanonMode canon_mode>
bool DoComplexHost(const char* host,
                   size_t host_len,
                   bool has_non_ascii,
                   bool has_escaped,
                   CanonOutput* output) {
  // Save the current position in the output. We may write stuff and rewind it
  // below, so we need to know where to rewind to.
  size_t begin_length = output->length();

  // Points to the UTF-8 data we want to convert. This will either be the
  // input or the unescaped version written to |*output| if necessary.
  const char* utf8_source;
  size_t utf8_source_len;
  bool are_all_escaped_valid = true;
  if (has_escaped) {
    // Unescape before converting to UTF-16 for IDN. We write this into the
    // output because it most likely does not require IDNization, and we can
    // save another huge stack buffer. It will be replaced below if it requires
    // IDN. This will also update our non-ASCII flag so we know whether the
    // unescaped input requires IDN.
    if (!DoSimpleHost<canon_mode>(host, host_len, output, &has_non_ascii)) {
      // Error with some escape sequence. We'll call the current output
      // complete. DoSimpleHost will have written some "reasonable" output
      // for the invalid escapes, but the output could be non-ASCII and
      // needs to go through re-encoding below.
      are_all_escaped_valid = false;
    }

    // Unescaping may have left us with ASCII input, in which case the
    // unescaped version we wrote to output is complete.
    if (!has_non_ascii) {
      return are_all_escaped_valid;
    }

    // Save the pointer into the data was just converted (it may be appended to
    // other data in the output buffer).
    utf8_source = &output->data()[begin_length];
    utf8_source_len = output->length() - begin_length;
  } else {
    // We don't need to unescape, use input for IDNization later. (We know the
    // input has non-ASCII, or the simple version would have been called
    // instead of us.)
    utf8_source = host;
    utf8_source_len = host_len;
  }

  // Non-ASCII input requires IDN, convert to UTF-16 and do the IDN conversion.
  // Above, we may have used the output to write the unescaped values to, so
  // we have to rewind it to where we started after we convert it to UTF-16.
  StackBufferW utf16;
  if (!ConvertUTF8ToUTF16(utf8_source, utf8_source_len, &utf16)) {
    // In this error case, the input may or may not be the output.
    StackBuffer utf8;
    for (size_t i = 0; i < utf8_source_len; i++)
      utf8.push_back(utf8_source[i]);
    output->set_length(begin_length);
    AppendInvalidNarrowString(utf8.data(), 0, utf8.length(), output);
    return false;
  }
  output->set_length(begin_length);

  // This will call DoSimpleHost which will do normal ASCII canonicalization
  // and also check for IP addresses in the outpt.
  return DoIDNHost<canon_mode>(utf16.data(), utf16.length(), output) &&
         are_all_escaped_valid;
}

// UTF-16 convert host to its ASCII version. The set up is already ready for
// the backend, so we just pass through. The has_escaped flag should be set if
// the input string requires unescaping.
template <CanonMode canon_mode>
bool DoComplexHost(const char16_t* host,
                   size_t host_len,
                   bool has_non_ascii,
                   bool has_escaped,
                   CanonOutput* output) {
  if (has_escaped) {
    // Yikes, we have escaped characters with wide input. The escaped
    // characters should be interpreted as UTF-8. To solve this problem,
    // we convert to UTF-8, unescape, then convert back to UTF-16 for IDN.
    //
    // We don't bother to optimize the conversion in the ASCII case (which
    // *could* just be a copy) and use the UTF-8 path, because it should be
    // very rare that host names have escaped characters, and it is relatively
    // fast to do the conversion anyway.
    StackBuffer utf8;
    if (!ConvertUTF16ToUTF8(host, host_len, &utf8)) {
      AppendInvalidNarrowString(host, 0, host_len, output);
      return false;
    }

    // Once we convert to UTF-8, we can use the 8-bit version of the complex
    // host handling code above.
    return DoComplexHost<canon_mode>(utf8.data(), utf8.length(), has_non_ascii,
                                     has_escaped, output);
  }

  // No unescaping necessary, we can safely pass the input to ICU. This
  // function will only get called if we either have escaped or non-ascii
  // input, so it's safe to just use ICU now. Even if the input is ASCII,
  // this function will do the right thing (just slower than we could).
  return DoIDNHost<canon_mode>(host, host_len, output);
}

template <typename CHAR, typename UCHAR, CanonMode canon_mode>
bool DoHostSubstring(const CHAR* spec,
                     const Component& host,
                     CanonOutput* output) {
  DCHECK(host.is_valid());

  bool has_non_ascii, has_escaped;
  ScanHostname<CHAR, UCHAR>(spec, host, &has_non_ascii, &has_escaped);

  if (has_non_ascii || has_escaped) {
    return DoComplexHost<canon_mode>(&spec[host.begin],
                                     static_cast<size_t>(host.len),
                                     has_non_ascii, has_escaped, output);
  }

  const bool success = DoSimpleHost<canon_mode>(
      &spec[host.begin], static_cast<size_t>(host.len), output, &has_non_ascii);
  DCHECK(!has_non_ascii);
  return success;
}

template <typename CharT>
bool DoOpaqueHost(const std::basic_string_view<CharT> host,
                  CanonOutput& output) {
  // URL Standard: https://url.spec.whatwg.org/#concept-opaque-host-parser

  size_t host_len = host.size();

  for (size_t i = 0; i < host_len; ++i) {
    char16_t ch = host[i];
    // The characters '[', ':', and ']', are checked later in
    // `CanonicalizeIPv6Address` function.
    if (ch != '[' && ch != ']' && ch != ':' && IsForbiddenHostCodePoint(ch)) {
      return false;
    }

    // Implementation note:
    //
    // URL Standard: Step 3 in
    // https://url.spec.whatwg.org/#concept-opaque-host-parser
    //
    // > 3. If input contains a U+0025 (%) and the two code points following
    // > it are not ASCII hex digits, invalid-URL-unit validation error.
    //
    // `invalid-URL-unit` is NOT marked as failure. We don't need to consider
    // step 3 here.

    // URL Standard: Step 4 in
    // https://url.spec.whatwg.org/#concept-opaque-host-parser
    //
    // > 4. Return the result of running UTF-8 percent-encode on input using
    // > the C0 control percent-encode set.
    if (IsInC0ControlPercentEncodeSet(ch)) {
      AppendUTF8EscapedChar(host.data(), &i, host_len, &output);
    } else {
      output.push_back(ch);
    }
  }
  return true;
}

template <typename CHAR, typename UCHAR, CanonMode canon_mode>
void DoHost(const CHAR* spec,
            const Component& host,
            CanonOutput& output,
            CanonHostInfo& host_info) {
  // URL Standard: https://url.spec.whatwg.org/#host-parsing

  // Keep track of output's initial length, so we can rewind later.
  const int output_begin = output.length();

  if (host.is_empty()) {
    // Empty hosts don't need anything.
    host_info.family = CanonHostInfo::NEUTRAL;
    // Carry over the valid empty host for non-special URLs.
    //
    // Component(0, 0) should be considered invalid here for historical reasons.
    //
    // TODO(crbug.com/40063064): Update the callers so that they don't pass
    // Component(0, 0) as an invalid `host`.
    if (host.begin != 0 && host.len == 0) {
      host_info.out_host = Component(output_begin, 0);
    } else {
      host_info.out_host = Component();
    }
    return;
  }

  bool success;
  if constexpr (canon_mode == CanonMode::kSpecialURL ||
                canon_mode == CanonMode::kFileURL) {
    success = DoHostSubstring<CHAR, UCHAR, canon_mode>(spec, host, &output);
  } else {
    // URL Standard: https://url.spec.whatwg.org/#concept-opaque-host-parser
    success = DoOpaqueHost(host.as_string_view_on(spec), output);
  }

  if (success) {
    // After all the other canonicalization, check if we ended up with an IP
    // address. IP addresses are small, so writing into this temporary buffer
    // should not cause an allocation.
    RawCanonOutput<64> canon_ip;

    if constexpr (canon_mode == CanonMode::kSpecialURL ||
                  canon_mode == CanonMode::kFileURL) {
      CanonicalizeIPAddress(output.data(),
                            MakeRange(output_begin, output.length()), &canon_ip,
                            &host_info);
    } else {
      // Non-special URLs support only IPv6.
      CanonicalizeIPv6Address(output.data(),
                              MakeRange(output_begin, output.length()),
                              canon_ip, host_info);
    }

    // If we got an IPv4/IPv6 address, copy the canonical form back to the
    // real buffer. Otherwise, it's a hostname or broken IP, in which case
    // we just leave it in place.
    if (host_info.IsIPAddress()) {
      output.set_length(output_begin);
      output.Append(canon_ip.view());
    }
  } else {
    // Canonicalization failed. Set BROKEN to notify the caller.
    host_info.family = CanonHostInfo::BROKEN;
  }
  host_info.out_host = MakeRange(output_begin, output.length());
}

}  // namespace

bool CanonicalizeHost(const char* spec,
                      const Component& host,
                      CanonOutput* output,
                      Component* out_host) {
  DCHECK(output);
  DCHECK(out_host);
  return CanonicalizeSpecialHost(spec, host, *output, *out_host);
}

bool CanonicalizeHost(const char16_t* spec,
                      const Component& host,
                      CanonOutput* output,
                      Component* out_host) {
  DCHECK(output);
  DCHECK(out_host);
  return CanonicalizeSpecialHost(spec, host, *output, *out_host);
}

bool CanonicalizeSpecialHost(const char* spec,
                             const Component& host,
                             CanonOutput& output,
                             Component& out_host) {
  CanonHostInfo host_info;
  DoHost<char, unsigned char, CanonMode::kSpecialURL>(spec, host, output,
                                                      host_info);
  out_host = host_info.out_host;
  return (host_info.family != CanonHostInfo::BROKEN);
}

bool CanonicalizeSpecialHost(const char16_t* spec,
                             const Component& host,
                             CanonOutput& output,
                             Component& out_host) {
  CanonHostInfo host_info;
  DoHost<char16_t, char16_t, CanonMode::kSpecialURL>(spec, host, output,
                                                     host_info);
  out_host = host_info.out_host;
  return (host_info.family != CanonHostInfo::BROKEN);
}

bool CanonicalizeFileHost(const char* spec,
                          const Component& host,
                          CanonOutput& output,
                          Component& out_host) {
  CanonHostInfo host_info;
  DoHost<char, unsigned char, CanonMode::kFileURL>(spec, host, output,
                                                   host_info);
  out_host = host_info.out_host;
  return (host_info.family != CanonHostInfo::BROKEN);
}

bool CanonicalizeFileHost(const char16_t* spec,
                          const Component& host,
                          CanonOutput& output,
                          Component& out_host) {
  CanonHostInfo host_info;
  DoHost<char16_t, char16_t, CanonMode::kFileURL>(spec, host, output,
                                                  host_info);
  out_host = host_info.out_host;
  return (host_info.family != CanonHostInfo::BROKEN);
}

bool CanonicalizeNonSpecialHost(const char* spec,
                                const Component& host,
                                CanonOutput& output,
                                Component& out_host) {
  CanonHostInfo host_info;
  DoHost<char, unsigned char, CanonMode::kNonSpecialURL>(spec, host, output,
                                                         host_info);
  out_host = host_info.out_host;
  return (host_info.family != CanonHostInfo::BROKEN);
}

bool CanonicalizeNonSpecialHost(const char16_t* spec,
                                const Component& host,
                                CanonOutput& output,
                                Component& out_host) {
  CanonHostInfo host_info;
  DoHost<char16_t, char16_t, CanonMode::kNonSpecialURL>(spec, host, output,
                                                        host_info);
  out_host = host_info.out_host;
  return (host_info.family != CanonHostInfo::BROKEN);
}

void CanonicalizeHostVerbose(const char* spec,
                             const Component& host,
                             CanonOutput* output,
                             CanonHostInfo* host_info) {
  DCHECK(output);
  DCHECK(host_info);
  CanonicalizeSpecialHostVerbose(spec, host, *output, *host_info);
}

void CanonicalizeHostVerbose(const char16_t* spec,
                             const Component& host,
                             CanonOutput* output,
                             CanonHostInfo* host_info) {
  DCHECK(output);
  DCHECK(host_info);
  CanonicalizeSpecialHostVerbose(spec, host, *output, *host_info);
}

void CanonicalizeSpecialHostVerbose(const char* spec,
                                    const Component& host,
                                    CanonOutput& output,
                                    CanonHostInfo& host_info) {
  DoHost<char, unsigned char, CanonMode::kSpecialURL>(spec, host, output,
                                                      host_info);
}

void CanonicalizeSpecialHostVerbose(const char16_t* spec,
                                    const Component& host,
                                    CanonOutput& output,
                                    CanonHostInfo& host_info) {
  DoHost<char16_t, char16_t, CanonMode::kSpecialURL>(spec, host, output,
                                                     host_info);
}

void CanonicalizeFileHostVerbose(const char* spec,
                                 const Component& host,
                                 CanonOutput& output,
                                 CanonHostInfo& host_info) {
  DoHost<char, unsigned char, CanonMode::kFileURL>(spec, host, output,
                                                   host_info);
}

void CanonicalizeFileHostVerbose(const char16_t* spec,
                                 const Component& host,
                                 CanonOutput& output,
                                 CanonHostInfo& host_info) {
  DoHost<char16_t, char16_t, CanonMode::kFileURL>(spec, host, output,
                                                  host_info);
}

bool CanonicalizeHostSubstring(const char* spec,
                               const Component& host,
                               CanonOutput* output) {
  return DoHostSubstring<char, unsigned char, CanonMode::kSpecialURL>(
      spec, host, output);
}

bool CanonicalizeHostSubstring(const char16_t* spec,
                               const Component& host,
                               CanonOutput* output) {
  return DoHostSubstring<char16_t, char16_t, CanonMode::kSpecialURL>(spec, host,
                                                                     output);
}

void CanonicalizeNonSpecialHostVerbose(const char* spec,
                                       const Component& host,
                                       CanonOutput& output,
                                       CanonHostInfo& host_info) {
  DoHost<char, unsigned char, CanonMode::kNonSpecialURL>(spec, host, output,
                                                         host_info);
}

void CanonicalizeNonSpecialHostVerbose(const char16_t* spec,
                                       const Component& host,
                                       CanonOutput& output,
                                       CanonHostInfo& host_info) {
  DoHost<char16_t, char16_t, CanonMode::kNonSpecialURL>(spec, host, output,
                                                        host_info);
}

}  // namespace url
