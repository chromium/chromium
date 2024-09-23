// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Canonicalizer functions for working with and resolving relative URLs.

#include <algorithm>
#include <ostream>
#include <string_view>

#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "url/url_canon.h"
#include "url/url_canon_internal.h"
#include "url/url_constants.h"
#include "url/url_features.h"
#include "url/url_file.h"
#include "url/url_parse_internal.h"
#include "url/url_util.h"
#include "url/url_util_internal.h"

namespace url {

namespace {

// Firefox does a case-sensitive compare (which is probably wrong--Mozilla bug
// 379034), whereas IE is case-insensitive.
//
// We choose to be more permissive like IE. We don't need to worry about
// unescaping or anything here: neither IE or Firefox allow this. We also
// don't have to worry about invalid scheme characters since we are comparing
// against the canonical scheme of the base.
//
// The base URL should always be canonical, therefore it should be ASCII.
template<typename CHAR>
bool AreSchemesEqual(const char* base,
                     const Component& base_scheme,
                     const CHAR* cmp,
                     const Component& cmp_scheme) {
  if (base_scheme.len != cmp_scheme.len)
    return false;
  for (int i = 0; i < base_scheme.len; i++) {
    // We assume the base is already canonical, so we don't have to
    // canonicalize it.
    if (CanonicalSchemeChar(cmp[cmp_scheme.begin + i]) !=
        base[base_scheme.begin + i])
      return false;
  }
  return true;
}

#ifdef WIN32

// Here, we also allow Windows paths to be represented as "/C:/" so we can be
// consistent about URL paths beginning with slashes. This function is like
// DoesBeginWindowsDrivePath except that it also requires a slash at the
// beginning.
template<typename CHAR>
bool DoesBeginSlashWindowsDriveSpec(const CHAR* spec, int start_offset,
                                    int spec_len) {
  if (start_offset >= spec_len)
    return false;
  return IsSlashOrBackslash(spec[start_offset]) &&
         DoesBeginWindowsDriveSpec(spec, start_offset + 1, spec_len);
}

#endif  // WIN32

template <typename CHAR>
bool IsValidScheme(const CHAR* url, const Component& scheme) {
  // Caller should ensure that the |scheme| is not empty.
  DCHECK_NE(0, scheme.len);

  // From https://url.spec.whatwg.org/#scheme-start-state:
  //   scheme start state:
  //     1. If c is an ASCII alpha, append c, lowercased, to buffer, and set
  //        state to scheme state.
  //     2. Otherwise, if state override is not given, set state to no scheme
  //        state, and decrease pointer by one.
  //     3. Otherwise, validation error, return failure.
  // Note that both step 2 and step 3 mean that the scheme was not valid.
  if (!base::IsAsciiAlpha(url[scheme.begin]))
    return false;

  // From https://url.spec.whatwg.org/#scheme-state:
  //   scheme state:
  //     1. If c is an ASCII alphanumeric, U+002B (+), U+002D (-), or U+002E
  //        (.), append c, lowercased, to buffer.
  //     2. Otherwise, if c is U+003A (:), then [...]
  //
  // We begin at |scheme.begin + 1|, because the character at |scheme.begin| has
  // already been checked by base::IsAsciiAlpha above.
  int scheme_end = scheme.end();
  for (int i = scheme.begin + 1; i < scheme_end; i++) {
    if (!CanonicalSchemeChar(url[i]))
      return false;
  }

  return true;
}

// See IsRelativeURL in the header file for usage.
template<typename CHAR>
bool DoIsRelativeURL(const char* base,
                     const Parsed& base_parsed,
                     const CHAR* url,
                     int url_len,
                     bool is_base_hierarchical,
                     bool* is_relative,
                     Component* relative_component) {
  *is_relative = false;  // So we can default later to not relative.

  // Trim whitespace and construct a new range for the substring.
  int begin = 0;
  TrimURL(url, &begin, &url_len);
  if (begin >= url_len) {
    // Empty URLs are relative, but do nothing.
    if (!is_base_hierarchical) {
      // Don't allow relative URLs if the base scheme doesn't support it.
      return false;
    }
    *relative_component = Component(begin, 0);
    *is_relative = true;
    return true;
  }

#ifdef WIN32
  // We special case paths like "C:\foo" so they can link directly to the
  // file on Windows (IE compatibility). The security domain stuff should
  // prevent a link like this from actually being followed if its on a
  // web page.
  //
  // We treat "C:/foo" as an absolute URL. We can go ahead and treat "/c:/"
  // as relative, as this will just replace the path when the base scheme
  // is a file and the answer will still be correct.
  //
  // We require strict backslashes when detecting UNC since two forward
  // slashes should be treated a a relative URL with a hostname.
  if (DoesBeginWindowsDriveSpec(url, begin, url_len) ||
      DoesBeginUNCPath(url, begin, url_len, true))
    return true;
#endif  // WIN32

  // See if we've got a scheme, if not, we know this is a relative URL.
  // BUT, just because we have a scheme, doesn't make it absolute.
  // "http:foo.html" is a relative URL with path "foo.html". If the scheme is
  // empty, we treat it as relative (":foo"), like IE does.
  Component scheme;
  const bool scheme_is_empty =
      !ExtractScheme(url, url_len, &scheme) || scheme.len == 0;
  if (scheme_is_empty) {
    if (url[begin] == '#') {
      // |url| is a bare fragment (e.g. "#foo"). This can be resolved against
      // any base. Fall-through.
    } else if (!is_base_hierarchical) {
      // Don't allow relative URLs if the base scheme doesn't support it.
      return false;
    }

    *relative_component = MakeRange(begin, url_len);
    *is_relative = true;
    return true;
  }

  // If the scheme isn't valid, then it's relative.
  if (!IsValidScheme(url, scheme)) {
    if (url[begin] == '#') {
      // |url| is a bare fragment (e.g. "#foo:bar"). This can be resolved
      // against any base. Fall-through.
    } else if (!is_base_hierarchical) {
      // Don't allow relative URLs if the base scheme doesn't support it.
      return false;
    }
    *relative_component = MakeRange(begin, url_len);
    *is_relative = true;
    return true;
  }

  // If base scheme is not standard, or the schemes are different, we can't
  // count it as relative.
  //
  // URL Standard: https://url.spec.whatwg.org/#scheme-state
  //
  // scheme state:
  // > 2.6. Otherwise, if url is special, base is non-null, and base’s scheme is
  // >      url’s scheme:
  if ((IsUsingStandardCompliantNonSpecialSchemeURLParsing() &&
       !IsStandard(base, base_parsed.scheme)) ||
      !AreSchemesEqual(base, base_parsed.scheme, url, scheme)) {
    return true;
  }

  // When the scheme that they both share is not hierarchical, treat the
  // incoming scheme as absolute (this way with the base of "data:foo",
  // "data:bar" will be reported as absolute.
  if (!is_base_hierarchical)
    return true;

  int colon_offset = scheme.end();

  // If it's a filesystem URL, the only valid way to make it relative is not to
  // supply a scheme. There's no equivalent to e.g. http:index.html.
  if (CompareSchemeComponent(url, scheme, kFileSystemScheme))
    return true;

  // ExtractScheme guarantees that the colon immediately follows what it
  // considers to be the scheme. CountConsecutiveSlashes will handle the
  // case where the begin offset is the end of the input.
  int num_slashes = CountConsecutiveSlashes(url, colon_offset + 1, url_len);

  if (num_slashes == 0 || num_slashes == 1) {
    // No slashes means it's a relative path like "http:foo.html". One slash
    // is an absolute path. "http:/home/foo.html"
    *is_relative = true;
    *relative_component = MakeRange(colon_offset + 1, url_len);
    return true;
  }

  // Two or more slashes after the scheme we treat as absolute.
  return true;
}

// Copies all characters in the range [begin, end) of |spec| to the output,
// up until and including the last slash. There should be a slash in the
// range, if not, nothing will be copied.
//
// For stardard URLs the input should be canonical, but when resolving relative
// URLs on a non-standard base (like "data:") the input can be anything.
void CopyToLastSlash(const char* spec,
                     int begin,
                     int end,
                     CanonOutput* output) {
  // Find the last slash.
  int last_slash = -1;
  for (int i = end - 1; i >= begin; i--) {
    if (spec[i] == '/' || spec[i] == '\\') {
      last_slash = i;
      break;
    }
  }
  if (last_slash < 0)
    return;  // No slash.

  // Copy.
  for (int i = begin; i <= last_slash; i++)
    output->push_back(spec[i]);
}

// Copies a single component from the source to the output. This is used
// when resolving relative URLs and a given component is unchanged. Since the
// source should already be canonical, we don't have to do anything special,
// and the input is ASCII.
void CopyOneComponent(const char* source,
                      const Component& source_component,
                      CanonOutput* output,
                      Component* output_component) {
  if (!source_component.is_valid()) {
    // This component is not present.
    *output_component = Component();
    return;
  }

  output_component->begin = output->length();
  int source_end = source_component.end();
  for (int i = source_component.begin; i < source_end; i++)
    output->push_back(source[i]);
  output_component->len = output->length() - output_component->begin;
}

#ifdef WIN32

// Called on Windows when the base URL is a file URL, this will copy the "C:"
// to the output, if there is a drive letter and if that drive letter is not
// being overridden by the relative URL. Otherwise, do nothing.
//
// It will return the index of the beginning of the next character in the
// base to be processed: if there is a "C:", the slash after it, or if
// there is no drive letter, the slash at the beginning of the path, or
// the end of the base. This can be used as the starting offset for further
// path processing.
template<typename CHAR>
int CopyBaseDriveSpecIfNecessary(const char* base_url,
                                 int base_path_begin,
                                 int base_path_end,
                                 const CHAR* relative_url,
                                 int path_start,
                                 int relative_url_len,
                                 CanonOutput* output) {
  if (base_path_begin >= base_path_end)
    return base_path_begin;  // No path.

  // If the relative begins with a drive spec, don't do anything. The existing
  // drive spec in the base will be replaced.
  if (DoesBeginWindowsDriveSpec(relative_url, path_start, relative_url_len)) {
    return base_path_begin;  // Relative URL path is "C:/foo"
  }

  // The path should begin with a slash (as all canonical paths do). We check
  // if it is followed by a drive letter and copy it.
  if (DoesBeginSlashWindowsDriveSpec(base_url,
                                     base_path_begin,
                                     base_path_end)) {
    // Copy the two-character drive spec to the output. It will now look like
    // "file:///C:" so the rest of it can be treated like a standard path.
    output->push_back('/');
    output->push_back(base_url[base_path_begin + 1]);
    output->push_back(base_url[base_path_begin + 2]);
    return base_path_begin + 3;
  }

  return base_path_begin;
}

#endif  // WIN32

// A subroutine of DoResolveRelativeURL, this resolves the URL knowning that
// the input is a relative path or less (query or ref).
template <typename CHAR>
bool DoResolveRelativePath(const char* base_url,
                           const Parsed& base_parsed,
                           bool base_is_file,
                           const CHAR* relative_url,
                           const Component& relative_component,
                           CharsetConverter* query_converter,
                           CanonMode canon_mode,
                           CanonOutput* output,
                           Parsed* out_parsed) {
  bool success = true;

  // We know the authority section didn't change, copy it to the output. We
  // also know we have a path so can copy up to there.
  Component path, query, ref;
  ParsePathInternal(relative_url, relative_component, &path, &query, &ref);

  // Canonical URLs always have a path, so we can use that offset. Reserve
  // enough room for the base URL, the new path, and some extra bytes for
  // possible escaped characters.
  output->ReserveSizeIfNeeded(base_parsed.path.begin +
                              std::max({path.end(), query.end(), ref.end()}));

  // Append a base URL up to the beginning of base URL's path.
  if (base_parsed.path.is_empty()) {
    // A non-special URL may have an empty path (e.g. "git://host"). In these
    // cases, attempting to use `base_parsed.path` is invalid.
    output->Append(base_url, base_parsed.Length());
  } else if (url::IsUsingStandardCompliantNonSpecialSchemeURLParsing() &&
             !base_parsed.host.is_valid() &&
             // Exclude a file URL and an URL with an inner-path because we are
             // interested in only non-special URLs here.
             //
             // If we don't exclude a file URL here, for example, `new
             // URL("test", "file:///tmp").href` will result in
             // "file:/tmp/mock/test" instead of "file:///tmp/mock/test".
             !base_is_file && !base_parsed.inner_parsed()) {
    // The URL is a path-only non-special URL. e.g. "git:/path".
    //
    // In this case, we can't use `base_parsed.path.begin` because it may append
    // "/." wrongly if the URL is, for example, "git:/.//a", where
    // `base_parsed.path` represents "//a", instead of "/.//a". We want to
    // append "git:", instead of "git:/.".
    //
    // Fortunately, we can use `base_parsed.scheme.end()` here because we don't
    // need to append a user, a password, a host, nor a port when a host is
    // invalid.
    output->Append(base_url, base_parsed.scheme.end());
    output->Append(":");
  } else {
    output->Append(base_url, base_parsed.path.begin);
  }

  if (path.is_nonempty()) {
    // The path is replaced or modified.
    int true_path_begin = output->length();

    // For file: URLs on Windows, we don't want to treat the drive letter and
    // colon as part of the path for relative file resolution when the
    // incoming URL does not provide a drive spec. We save the true path
    // beginning so we can fix it up after we are done.
    int base_path_begin = base_parsed.path.begin;
#ifdef WIN32
    if (base_is_file) {
      base_path_begin = CopyBaseDriveSpecIfNecessary(
          base_url, base_parsed.path.begin, base_parsed.path.end(),
          relative_url, relative_component.begin, relative_component.end(),
          output);
      // Now the output looks like either "file://" or "file:///C:"
      // and we can start appending the rest of the path. |base_path_begin|
      // points to the character in the base that comes next.
    }
#endif  // WIN32

    if (IsSlashOrBackslash(relative_url[path.begin])) {
      // Easy case: the path is an absolute path on the server, so we can
      // just replace everything from the path on with the new versions.
      // Since the input should be canonical hierarchical URL, we should
      // always have a path.
      success &= CanonicalizePath(relative_url, path,
                                  output, &out_parsed->path);
    } else {
      // Relative path, replace the query, and reference. We take the
      // original path with the file part stripped, and append the new path.
      // The canonicalizer will take care of resolving ".." and "."
      size_t path_begin = output->length();

      if (base_parsed.path.is_empty() && !path.is_empty()) {
        // Ensure a leading "/" is present before appending a non-empty relative
        // path when the base URL's path is empty, as can occur with non-special
        // URLs. This prevents incorrect path concatenation, such as resolving
        // "path" based on "git://host" resulting in "git://hostpath" instead of
        // the intended "git://host/path".
        output->push_back('/');
      }

      CopyToLastSlash(base_url, base_path_begin, base_parsed.path.end(),
                      output);
      success &= CanonicalizePartialPathInternal(relative_url, path, path_begin,
                                                 canon_mode, output);
      out_parsed->path = MakeRange(path_begin, output->length());

      // Copy the rest of the stuff after the path from the relative path.
    }

    // To avoid path being treated as the host, prepend "/." to the path".
    //
    // Example:
    //
    // > const url = new URL("/.//path", "git:/");
    // > url.href
    // => The result should be "git:/.//path", instead of "git://path".
    if (IsUsingStandardCompliantNonSpecialSchemeURLParsing() &&
        !base_parsed.host.is_valid() && out_parsed->path.is_valid() &&
        out_parsed->path.as_string_view_on(output->view().data())
            .starts_with("//")) {
      size_t prior_output_length = output->length();
      output->Insert(out_parsed->path.begin, "/.");
      // Adjust path.
      out_parsed->path.begin += output->length() - prior_output_length;
      true_path_begin = out_parsed->path.begin;
    }
    // Finish with the query and reference part (these can't fail).
    CanonicalizeQuery(relative_url, query, query_converter,
                      output, &out_parsed->query);
    CanonicalizeRef(relative_url, ref, output, &out_parsed->ref);

    // Fix the path beginning to add back the "C:" we may have written above.
    out_parsed->path = MakeRange(true_path_begin, out_parsed->path.end());
    return success;
  }

  // If we get here, the path is unchanged: copy to output.
  CopyOneComponent(base_url, base_parsed.path, output, &out_parsed->path);

  if (query.is_valid()) {
    // Just the query specified, replace the query and reference (ignore
    // failures for refs)
    CanonicalizeQuery(relative_url, query, query_converter,
                      output, &out_parsed->query);
    CanonicalizeRef(relative_url, ref, output, &out_parsed->ref);
    return success;
  }

  // If we get here, the query is unchanged: copy to output. Note that the
  // range of the query parameter doesn't include the question mark, so we
  // have to add it manually if there is a component.
  if (base_parsed.query.is_valid())
    output->push_back('?');
  CopyOneComponent(base_url, base_parsed.query, output, &out_parsed->query);

  if (ref.is_valid()) {
    // Just the reference specified: replace it (ignoring failures).
    CanonicalizeRef(relative_url, ref, output, &out_parsed->ref);
    return success;
  }

  // We should always have something to do in this function, the caller checks
  // that some component is being replaced.
  DCHECK(false) << "Not reached";
  return success;
}

// Resolves a relative URL that contains a host. Typically, these will
// be of the form "//www.google.com/foo/bar?baz#ref" and the only thing which
// should be kept from the original URL is the scheme.
template<typename CHAR>
bool DoResolveRelativeHost(const char* base_url,
                           const Parsed& base_parsed,
                           const CHAR* relative_url,
                           const Component& relative_component,
                           CharsetConverter* query_converter,
                           CanonOutput* output,
                           Parsed* out_parsed) {
  SchemeType scheme_type = SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  const bool is_standard_scheme =
      GetStandardSchemeType(base_url, base_parsed.scheme, &scheme_type);

  // Parse the relative URL, just like we would for anything following a
  // scheme.
  Parsed relative_parsed;  // Everything but the scheme is valid.

  if (IsUsingStandardCompliantNonSpecialSchemeURLParsing() &&
      !is_standard_scheme) {
    ParseAfterNonSpecialScheme(relative_url, relative_component.end(),
                               relative_component.begin, &relative_parsed);
  } else {
    ParseAfterSpecialScheme(relative_url, relative_component.end(),
                            relative_component.begin, &relative_parsed);
  }

  // Now we can just use the replacement function to replace all the necessary
  // parts of the old URL with the new one.
  Replacements<CHAR> replacements;
  replacements.SetUsername(relative_url, relative_parsed.username);
  replacements.SetPassword(relative_url, relative_parsed.password);
  replacements.SetHost(relative_url, relative_parsed.host);
  replacements.SetPort(relative_url, relative_parsed.port);
  replacements.SetPath(relative_url, relative_parsed.path);
  replacements.SetQuery(relative_url, relative_parsed.query);
  replacements.SetRef(relative_url, relative_parsed.ref);

  // Length() does not include the old scheme, so make sure to add it from the
  // base URL.
  output->ReserveSizeIfNeeded(
      replacements.components().Length() +
      base_parsed.CountCharactersBefore(Parsed::USERNAME, false));
  if (!is_standard_scheme) {
    if (IsUsingStandardCompliantNonSpecialSchemeURLParsing()) {
      return ReplaceNonSpecialURL(base_url, base_parsed, replacements,
                                  query_converter, *output, *out_parsed);
    }
    // A path with an authority section gets canonicalized under standard URL
    // rules, even though the base was not known to be standard.
    scheme_type = SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  }
  return ReplaceStandardURL(base_url, base_parsed, replacements, scheme_type,
                            query_converter, output, out_parsed);
}

// Resolves a relative URL that happens to be an absolute file path. Examples
// include: "//hostname/path", "/c:/foo", and "//hostname/c:/foo".
template <typename CharT>
bool DoResolveAbsoluteFile(const CharT* relative_url,
                           const Component& relative_component,
                           CharsetConverter* query_converter,
                           CanonOutput* output,
                           Parsed* out_parsed) {
  // Parse the file URL. The file URL parsing function uses the same logic
  // as we do for determining if the file is absolute, in which case it will
  // not bother to look for a scheme.
  return CanonicalizeFileURL(
      &relative_url[relative_component.begin], relative_component.len,
      ParseFileURL(std::basic_string_view(
          &relative_url[relative_component.begin], relative_component.len)),
      query_converter, output, out_parsed);
}

// TODO(brettw) treat two slashes as root like Mozilla for FTP?
template<typename CHAR>
bool DoResolveRelativeURL(const char* base_url,
                          const Parsed& base_parsed,
                          bool base_is_file,
                          const CHAR* relative_url,
                          const Component& relative_component,
                          CharsetConverter* query_converter,
                          CanonOutput* output,
                          Parsed* out_parsed) {
  // |base_parsed| is the starting point for our output. Since we may have
  // removed whitespace from |relative_url| before entering this method, we'll
  // carry over the |potentially_dangling_markup| flag.
  bool potentially_dangling_markup = out_parsed->potentially_dangling_markup;
  *out_parsed = base_parsed;
  if (potentially_dangling_markup)
    out_parsed->potentially_dangling_markup = true;

  // A flag-dependent condition check is necessary here because non-special URLs
  // may have an empty path if StandardCompliantNonSpecialSchemeURLParsing flag
  // is enabled.
  //
  // TODO(crbug.com/40063064): Remove the following comment when we enable the
  // flag. The comment makes sense only when the flag is disabled.
  //
  // > Sanity check: the input should have a host or we'll break badly below.
  // > We can only resolve relative URLs with base URLs that have hosts and
  // > paths (even the default path of "/" is OK).
  // >
  // > We allow hosts with no length so we can handle file URLs, for example.
  if (IsUsingStandardCompliantNonSpecialSchemeURLParsing()
          ? base_parsed.scheme.is_empty()
          : base_parsed.path.is_empty()) {
    // On error, return the input (resolving a relative URL on a
    // non-relative base = the base).
    int base_len = base_parsed.Length();
    for (int i = 0; i < base_len; i++) {
      output->push_back(base_url[i]);
    }
    return false;
  }

  if (relative_component.is_empty()) {
    // Empty relative URL, leave unchanged, only removing the ref component.
    int base_len = base_parsed.Length();
    base_len -= base_parsed.ref.len + 1;
    out_parsed->ref.reset();
    output->Append(base_url, base_len);
    return true;
  }

  int num_slashes = CountConsecutiveSlashes(
      relative_url, relative_component.begin, relative_component.end());

#ifdef WIN32
  // On Windows, two slashes for a file path (regardless of which direction
  // they are) means that it's UNC. Two backslashes on any base scheme mean
  // that it's an absolute UNC path (we use the base_is_file flag to control
  // how strict the UNC finder is).
  //
  // We also allow Windows absolute drive specs on any scheme (for example
  // "c:\foo") like IE does. There must be no preceding slashes in this
  // case (we reject anything like "/c:/foo") because that should be treated
  // as a path. For file URLs, we allow any number of slashes since that would
  // be setting the path.
  //
  // This assumes the absolute path resolver handles absolute URLs like this
  // properly. DoCanonicalize does this.
  int after_slashes = relative_component.begin + num_slashes;
  if (DoesBeginUNCPath(relative_url, relative_component.begin,
                       relative_component.end(), !base_is_file) ||
      ((num_slashes == 0 || base_is_file) &&
       DoesBeginWindowsDriveSpec(
           relative_url, after_slashes, relative_component.end()))) {
    return DoResolveAbsoluteFile(relative_url, relative_component,
                                 query_converter, output, out_parsed);
  }
#else
  // Other platforms need explicit handling for file: URLs with multiple
  // slashes because the generic scheme parsing always extracts a host, but a
  // file: URL only has a host if it has exactly 2 slashes. Even if it does
  // have a host, we want to use the special host detection logic for file
  // URLs provided by DoResolveAbsoluteFile(), as opposed to the generic host
  // detection logic, for consistency with parsing file URLs from scratch.
  if (base_is_file && num_slashes >= 2) {
    return DoResolveAbsoluteFile(relative_url, relative_component,
                                 query_converter, output, out_parsed);
  }
#endif

  // Any other double-slashes mean that this is relative to the scheme.
  if (num_slashes >= 2) {
    return DoResolveRelativeHost(base_url, base_parsed,
                                 relative_url, relative_component,
                                 query_converter, output, out_parsed);
  }

  // When we get here, we know that the relative URL is on the same host.
  return DoResolveRelativePath(
      base_url, base_parsed, base_is_file, relative_url, relative_component,
      query_converter,
      // TODO(crbug.com/40063064): Support Non-special URLs
      CanonMode::kSpecialURL, output, out_parsed);
}

}  // namespace

bool IsRelativeURL(const char* base,
                   const Parsed& base_parsed,
                   const char* fragment,
                   int fragment_len,
                   bool is_base_hierarchical,
                   bool* is_relative,
                   Component* relative_component) {
  return DoIsRelativeURL<char>(
      base, base_parsed, fragment, fragment_len, is_base_hierarchical,
      is_relative, relative_component);
}

bool IsRelativeURL(const char* base,
                   const Parsed& base_parsed,
                   const char16_t* fragment,
                   int fragment_len,
                   bool is_base_hierarchical,
                   bool* is_relative,
                   Component* relative_component) {
  return DoIsRelativeURL<char16_t>(base, base_parsed, fragment, fragment_len,
                                   is_base_hierarchical, is_relative,
                                   relative_component);
}

bool ResolveRelativeURL(const char* base_url,
                        const Parsed& base_parsed,
                        bool base_is_file,
                        const char* relative_url,
                        const Component& relative_component,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* out_parsed) {
  return DoResolveRelativeURL<char>(
      base_url, base_parsed, base_is_file, relative_url,
      relative_component, query_converter, output, out_parsed);
}

bool ResolveRelativeURL(const char* base_url,
                        const Parsed& base_parsed,
                        bool base_is_file,
                        const char16_t* relative_url,
                        const Component& relative_component,
                        CharsetConverter* query_converter,
                        CanonOutput* output,
                        Parsed* out_parsed) {
  return DoResolveRelativeURL<char16_t>(base_url, base_parsed, base_is_file,
                                        relative_url, relative_component,
                                        query_converter, output, out_parsed);
}

}  // namespace url
