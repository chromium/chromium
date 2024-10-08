// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <limits.h>

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_features.h"
#include "url/url_parse_internal.h"

namespace url {

namespace {

enum CharacterFlags {
  // Pass through unchanged, whether escaped or not. This doesn't
  // actually set anything so you can't OR it to check, it's just to make the
  // table below more clear when any other flag is not set.
  PASS = 0,

  // This character requires special handling in DoPartialPathInternal. Doing
  // this test
  // first allows us to filter out the common cases of regular characters that
  // can be directly copied.
  SPECIAL = 1,

  // This character must be escaped in the canonical output. Note that all
  // escaped chars also have the "special" bit set so that the code that looks
  // for this is triggered. Not valid with PASS or ESCAPE
  ESCAPE_BIT = 2,
  ESCAPE = ESCAPE_BIT | SPECIAL,
};

// This table contains one of the above flag values. Note some flags are more
// than one bits because they also turn on the "special" flag. Special is the
// only flag that may be combined with others.
//
// This table was used to be designed to match exactly what IE did with the
// characters, however, which doesn't comply with the URL Standard as of Dec
// 2023. See https://crbug.com/1509295.
//
// Dot is even more special, and the escaped version is handled specially by
// IsDot. Therefore, we don't need the "escape" flag. We just need the "special"
// bit.
//
// clang-format off
const unsigned char kPathCharLookup[0x100] = {
//   NULL     control chars...
     ESCAPE , ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
//   control chars...
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
//   ' '      !        "        #        $        %        &        '        (        )        *        +        ,        -        .        /
     ESCAPE,  PASS,    ESCAPE,  ESCAPE,  PASS,    ESCAPE,  PASS,    PASS,    PASS,    PASS,    PASS,    PASS,    PASS,    PASS    ,SPECIAL, PASS,
//   0        1        2        3        4        5        6        7        8        9        :        ;        <        =        >        ?
     PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS,    PASS,    ESCAPE,  PASS,    ESCAPE,  ESCAPE,
//   @        A        B        C        D        E        F        G        H        I        J        K        L        M        N        O
     PASS,    PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,
//   P        Q        R        S        T        U        V        W        X        Y        Z        [        \        ]        ^        _
     PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS,    ESCAPE,  PASS,    ESCAPE,  PASS    ,
//   `        a        b        c        d        e        f        g        h        i        j        k        l        m        n        o
     ESCAPE,  PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,
//   p        q        r        s        t        u        v        w        x        y        z        {        |        }        ~        <NBSP>
     PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,PASS    ,ESCAPE,  ESCAPE,  ESCAPE,  PASS    ,ESCAPE,
//   ...all the high-bit characters are escaped
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,
     ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE,  ESCAPE};
// clang-format on

enum DotDisposition {
  // The given dot is just part of a filename and is not special.
  NOT_A_DIRECTORY,

  // The given dot is the current directory.
  DIRECTORY_CUR,

  // The given dot is the first of a double dot that should take us up one.
  DIRECTORY_UP
};

// When the path resolver finds a dot, this function is called with the
// character following that dot to see what it is. The return value
// indicates what type this dot is (see above). This code handles the case
// where the dot is at the end of the input.
//
// |*consumed_len| will contain the number of characters in the input that
// express what we found.
//
// If the input is "../foo", |after_dot| = 1, |end| = 6, and
// at the end, |*consumed_len| = 2 for the "./" this function consumed. The
// original dot length should be handled by the caller.
template <typename CHAR>
DotDisposition ClassifyAfterDot(const CHAR* spec,
                                size_t after_dot,
                                size_t end,
                                size_t* consumed_len) {
  if (after_dot == end) {
    // Single dot at the end.
    *consumed_len = 0;
    return DIRECTORY_CUR;
  }
  if (IsSlashOrBackslash(spec[after_dot])) {
    // Single dot followed by a slash.
    *consumed_len = 1;  // Consume the slash
    return DIRECTORY_CUR;
  }

  size_t second_dot_len = IsDot(spec, after_dot, end);
  if (second_dot_len) {
    size_t after_second_dot = after_dot + second_dot_len;
    if (after_second_dot == end) {
      // Double dot at the end.
      *consumed_len = second_dot_len;
      return DIRECTORY_UP;
    }
    if (IsSlashOrBackslash(spec[after_second_dot])) {
      // Double dot followed by a slash.
      *consumed_len = second_dot_len + 1;
      return DIRECTORY_UP;
    }
  }

  // The dots are followed by something else, not a directory.
  *consumed_len = 0;
  return NOT_A_DIRECTORY;
}

// Rewinds the output to the previous slash. It is assumed that the output
// ends with a slash and this doesn't count (we call this when we are
// appending directory paths, so the previous path component has and ending
// slash).
//
// This will stop at the first slash (assumed to be at position
// |path_begin_in_output| and not go any higher than that. Some web pages
// do ".." too many times, so we need to handle that brokenness.
//
// It searches for a literal slash rather than including a backslash as well
// because it is run only on the canonical output.
//
// The output is guaranteed to end in a slash when this function completes.
void BackUpToPreviousSlash(size_t path_begin_in_output, CanonOutput* output) {
  CHECK(output->length() > 0);
  CHECK(path_begin_in_output < output->length());

  size_t i = output->length() - 1;
  DCHECK(output->at(i) == '/');
  if (i == path_begin_in_output)
    return;  // We're at the first slash, nothing to do.

  // Now back up (skipping the trailing slash) until we find another slash.
  do {
    --i;
  } while (output->at(i) != '/' && i > path_begin_in_output);

  // Now shrink the output to just include that last slash we found.
  output->set_length(i + 1);
}

// Canonicalizes and appends the given path to the output. It assumes that if
// the input path starts with a slash, it should be copied to the output.
//
// If there are already path components (this mode is used when appending
// relative paths for resolving), it assumes that the output already has
// a trailing slash and that if the input begins with a slash, it should be
// copied to the output.
//
// We do not collapse multiple slashes in a row to a single slash. It seems
// no web browsers do this, and we don't want incompatibilities, even though
// it would be correct for most systems.
template <typename CHAR, typename UCHAR>
bool DoPartialPathInternal(const CHAR* spec,
                           const Component& path,
                           size_t path_begin_in_output,
                           CanonMode canon_mode,
                           CanonOutput* output) {
  if (path.is_empty())
    return true;

  size_t end = static_cast<size_t>(path.end());

  bool success = true;
  for (size_t i = static_cast<size_t>(path.begin); i < end; i++) {
    UCHAR uch = static_cast<UCHAR>(spec[i]);
    if (sizeof(CHAR) > 1 && uch >= 0x80) {
      // We only need to test wide input for having non-ASCII characters. For
      // narrow input, we'll always just use the lookup table. We don't try to
      // do anything tricky with decoding/validating UTF-8. This function will
      // read one or two UTF-16 characters and append the output as UTF-8. This
      // call will be removed in 8-bit mode.
      success &= AppendUTF8EscapedChar(spec, &i, end, output);
    } else {
      // Normal ASCII character or 8-bit input, use the lookup table.
      unsigned char out_ch = static_cast<unsigned char>(uch);
      unsigned char flags = kPathCharLookup[out_ch];
      if (flags & SPECIAL) {
        // Needs special handling of some sort.
        size_t dotlen;
        if ((dotlen = IsDot(spec, i, end)) > 0) {
          // See if this dot was preceded by a slash in the output.
          //
          // Note that we check this in the case of dots so we don't have to
          // special case slashes. Since slashes are much more common than
          // dots, this actually increases performance measurably (though
          // slightly).
          if (output->length() > path_begin_in_output &&
              output->at(output->length() - 1) == '/') {
            // Slash followed by a dot, check to see if this is means relative
            size_t consumed_len;
            switch (ClassifyAfterDot<CHAR>(spec, i + dotlen, end,
                                           &consumed_len)) {
              case NOT_A_DIRECTORY:
                // Copy the dot to the output, it means nothing special.
                output->push_back('.');
                i += dotlen - 1;
                break;
              case DIRECTORY_CUR:  // Current directory, just skip the input.
                i += dotlen + consumed_len - 1;
                break;
              case DIRECTORY_UP:
                BackUpToPreviousSlash(path_begin_in_output, output);
                i += dotlen + consumed_len - 1;
                break;
            }
          } else {
            // This dot is not preceded by a slash, it is just part of some
            // file name.
            output->push_back('.');
            i += dotlen - 1;
          }

        } else if (out_ch == '\\') {
          if (canon_mode == CanonMode::kSpecialURL ||
              canon_mode == CanonMode::kFileURL) {
            // Backslashes are path separators in special URLs.
            //
            // URL Standard: https://url.spec.whatwg.org/#path-state
            // > 1. url is special and c is U+005C (\)
            //
            // Convert backslashes to forward slashes.
            output->push_back('/');
          } else {
            output->push_back(out_ch);
          }
        } else if (out_ch == '%') {
          // Handle escape sequences.
          unsigned char unused_unescaped_value;
          if (DecodeEscaped(spec, &i, end, &unused_unescaped_value)) {
            // Valid escape sequence. We should just copy it exactly.
            output->push_back('%');
            output->push_back(static_cast<char>(spec[i - 1]));
            output->push_back(static_cast<char>(spec[i]));
          } else {
            // Invalid escape sequence. IE7+ rejects any URLs with such
            // sequences, while other browsers pass them through unchanged. We
            // use the permissive behavior.
            // TODO(brettw): Consider testing IE's strict behavior, which would
            // allow removing the code to handle nested escapes above.
            output->push_back('%');
          }
        } else if (flags & ESCAPE_BIT) {
          // This character should be escaped.
          AppendEscapedChar(out_ch, output);
        }
      } else {
        // Nothing special about this character, just append it.
        output->push_back(out_ch);
      }
    }
  }
  return success;
}

// Perform the same logic as in DoPartialPathInternal(), but updates the
// publicly exposed CanonOutput structure similar to DoPath().  Returns
// true if successful.
template <typename CHAR, typename UCHAR>
bool DoPartialPath(const CHAR* spec,
                   const Component& path,
                   CanonOutput* output,
                   Component* out_path) {
  out_path->begin = output->length();
  bool success = DoPartialPathInternal<CHAR, UCHAR>(
      spec, path, out_path->begin,
      // TODO(crbug.com/40063064): Support Non-special URLs.
      CanonMode::kSpecialURL, output);
  out_path->len = output->length() - out_path->begin;
  return success;
}

template <typename CHAR, typename UCHAR>
bool DoPath(const CHAR* spec,
            const Component& path,
            CanonMode canon_mode,
            CanonOutput* output,
            Component* out_path) {
  // URL Standard:
  // - https://url.spec.whatwg.org/#path-start-state
  // - https://url.spec.whatwg.org/#path-state

  bool success = true;
  out_path->begin = output->length();
  if (path.is_nonempty()) {
    // Write out an initial slash if the input has none. If we just parse a URL
    // and then canonicalize it, it will of course have a slash already. This
    // check is for the replacement and relative URL resolving cases of file
    // URLs.
    if (!IsSlashOrBackslash(spec[path.begin])) {
      output->push_back('/');
    }

    success = DoPartialPathInternal<CHAR, UCHAR>(spec, path, out_path->begin,
                                                 canon_mode, output);
  } else if (canon_mode == CanonMode::kSpecialURL ||
             canon_mode == CanonMode::kFileURL) {
    // No input, canonical path is a slash for special URLs, but it is empty for
    // non-special URLs.
    //
    // Implementation note:
    //
    // According to the URL Standard, for non-special URLs whose parsed path is
    // empty, such as "git://host", the state-machine finishes in the
    // `path-start-state` without entering the `path-state`. As a result, the
    // url's path remains an empty array. Therefore, no slash should be
    // appended.
    output->push_back('/');
  }
  out_path->len = output->length() - out_path->begin;
  return success;
}

}  // namespace

bool CanonicalizePath(const char* spec,
                      const Component& path,
                      CanonMode canon_mode,
                      CanonOutput* output,
                      Component* out_path) {
  return DoPath<char, unsigned char>(spec, path, canon_mode, output, out_path);
}

bool CanonicalizePath(const char16_t* spec,
                      const Component& path,
                      CanonMode canon_mode,
                      CanonOutput* output,
                      Component* out_path) {
  return DoPath<char16_t, char16_t>(spec, path, canon_mode, output, out_path);
}

bool CanonicalizePath(const char* spec,
                      const Component& path,
                      CanonOutput* output,
                      Component* out_path) {
  return DoPath<char, unsigned char>(spec, path, CanonMode::kSpecialURL, output,
                                     out_path);
}

bool CanonicalizePath(const char16_t* spec,
                      const Component& path,
                      CanonOutput* output,
                      Component* out_path) {
  return DoPath<char16_t, char16_t>(spec, path, CanonMode::kSpecialURL, output,
                                    out_path);
}

bool CanonicalizePartialPath(const char* spec,
                             const Component& path,
                             CanonOutput* output,
                             Component* out_path) {
  return DoPartialPath<char, unsigned char>(spec, path, output, out_path);
}

bool CanonicalizePartialPath(const char16_t* spec,
                             const Component& path,
                             CanonOutput* output,
                             Component* out_path) {
  return DoPartialPath<char16_t, char16_t>(spec, path, output, out_path);
}

bool CanonicalizePartialPathInternal(const char* spec,
                                     const Component& path,
                                     size_t path_begin_in_output,
                                     CanonMode canon_mode,
                                     CanonOutput* output) {
  return DoPartialPathInternal<char, unsigned char>(
      spec, path, path_begin_in_output, canon_mode, output);
}

bool CanonicalizePartialPathInternal(const char16_t* spec,
                                     const Component& path,
                                     size_t path_begin_in_output,
                                     CanonMode canon_mode,
                                     CanonOutput* output) {
  return DoPartialPathInternal<char16_t, char16_t>(
      spec, path, path_begin_in_output, canon_mode, output);
}

}  // namespace url
