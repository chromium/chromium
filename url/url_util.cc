// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/url_util.h"

#include <stddef.h>
#include <string.h>

#include <atomic>
#include <ostream>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "url/url_canon_internal.h"
#include "url/url_constants.h"
#include "url/url_features.h"
#include "url/url_file.h"
#include "url/url_parse_internal.h"
#include "url/url_util_internal.h"

namespace url {

namespace {

// A pair for representing a standard scheme name and the SchemeType for it.
struct SchemeWithType {
  std::string scheme;
  SchemeType type;
};

// A pair for representing a scheme and a custom protocol handler for it.
//
// This pair of strings must be normalized protocol handler parameters as
// described in the Custom Handler specification.
// https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
struct SchemeWithHandler {
  std::string scheme;
  std::string handler;
};

// List of currently registered schemes and associated properties.
struct SchemeRegistry {
  // Standard format schemes (see header for details).
  std::vector<SchemeWithType> standard_schemes = {
      {kHttpsScheme, SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION},
      {kHttpScheme, SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION},
      // Yes, file URLs can have a hostname, so file URLs should be handled as
      // "standard". File URLs never have a port as specified by the SchemeType
      // field.  Unlike other SCHEME_WITH_HOST schemes, the 'host' in a file
      // URL may be empty, a behavior which is special-cased during
      // canonicalization.
      {kFileScheme, SCHEME_WITH_HOST},
      {kFtpScheme, SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION},
      {kWssScheme,
       SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION},  // WebSocket secure.
      {kWsScheme, SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION},  // WebSocket.
      {kFileSystemScheme, SCHEME_WITHOUT_AUTHORITY},
  };

  // Schemes that are allowed for referrers.
  //
  // WARNING: Adding (1) a non-"standard" scheme or (2) a scheme whose URLs have
  // opaque origins could lead to surprising behavior in some of the referrer
  // generation logic. In order to avoid surprises, be sure to have adequate
  // test coverage in each of the multiple code locations that compute
  // referrers.
  std::vector<SchemeWithType> referrer_schemes = {
      {kHttpsScheme, SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION},
      {kHttpScheme, SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION},
  };

  // Schemes that do not trigger mixed content warning.
  std::vector<std::string> secure_schemes = {
      kHttpsScheme,
      kWssScheme,
      kDataScheme,
      kAboutScheme,
  };

  // Schemes that normal pages cannot link to or access (i.e., with the same
  // security rules as those applied to "file" URLs).
  std::vector<std::string> local_schemes = {
      kFileScheme,
  };

  // Schemes that cause pages loaded with them to not have access to pages
  // loaded with any other URL scheme.
  std::vector<std::string> no_access_schemes = {
      kAboutScheme,
      kJavaScriptScheme,
      kDataScheme,
  };

  // Schemes that can be sent CORS requests.
  std::vector<std::string> cors_enabled_schemes = {
      kHttpsScheme,
      kHttpScheme,
      kDataScheme,
  };

  // Schemes that can be used by web to store data (local storage, etc).
  std::vector<std::string> web_storage_schemes = {
      kHttpsScheme, kHttpScheme, kFileScheme, kFtpScheme, kWssScheme, kWsScheme,
  };

  // Schemes that can bypass the Content-Security-Policy (CSP) checks.
  std::vector<std::string> csp_bypassing_schemes = {};

  // Schemes that are strictly empty documents, allowing them to commit
  // synchronously.
  std::vector<std::string> empty_document_schemes = {
      kAboutScheme,
  };

  // Non-special schemes that should be treated as opaque path URLs for
  // compatibility reasons.
  std::vector<std::string> opaque_non_special_schemes = {
      // See https://crrev.com/c/5465607 for the reason.
      kAndroidScheme,
      // Temporarily opted-out. See https://crrev.com/c/5569365.
      kDrivefsScheme,
      // Temporarily opted-out. See https://crrev.com/c/5568919.
      kChromeosSteamScheme,
      kSteamScheme,
      // Temporarily opted-out. See https://crrev.com/c/5578066.
      kMaterializedViewScheme,
  };

  // Schemes with a predefined default custom handler.
  std::vector<SchemeWithHandler> predefined_handler_schemes;

  bool allow_non_standard_schemes = false;
};

// See the LockSchemeRegistries declaration in the header.
bool scheme_registries_locked = false;

// Ensure that the schemes aren't modified after first use.
static std::atomic<bool> g_scheme_registries_used{false};

// Gets the scheme registry without locking the schemes. This should *only* be
// used for adding schemes to the registry.
SchemeRegistry* GetSchemeRegistryWithoutLocking() {
  static base::NoDestructor<SchemeRegistry> registry;
  return registry.get();
}

const SchemeRegistry& GetSchemeRegistry() {
#if DCHECK_IS_ON()
  g_scheme_registries_used.store(true);
#endif
  return *GetSchemeRegistryWithoutLocking();
}

// Pass this enum through for methods which would like to know if whitespace
// removal is necessary.
enum WhitespaceRemovalPolicy {
  REMOVE_WHITESPACE,
  DO_NOT_REMOVE_WHITESPACE,
};

// Given a string and a range inside the string, compares it to the given
// lower-case |compare_to| buffer.
template<typename CHAR>
inline bool DoCompareSchemeComponent(const CHAR* spec,
                                     const Component& component,
                                     const char* compare_to) {
  if (component.is_empty())
    return compare_to[0] == 0;  // When component is empty, match empty scheme.
  return base::EqualsCaseInsensitiveASCII(
      std::basic_string_view(&spec[component.begin], component.len),
      compare_to);
}

// Returns true and sets |type| to the SchemeType of the given scheme
// identified by |scheme| within |spec| if in |schemes|.
template<typename CHAR>
bool DoIsInSchemes(const CHAR* spec,
                   const Component& scheme,
                   SchemeType* type,
                   const std::vector<SchemeWithType>& schemes) {
  if (scheme.is_empty())
    return false;  // Empty or invalid schemes are non-standard.

  for (const SchemeWithType& scheme_with_type : schemes) {
    if (base::EqualsCaseInsensitiveASCII(
            std::basic_string_view(&spec[scheme.begin], scheme.len),
            scheme_with_type.scheme)) {
      *type = scheme_with_type.type;
      return true;
    }
  }
  return false;
}

template<typename CHAR>
bool DoIsStandard(const CHAR* spec, const Component& scheme, SchemeType* type) {
  return DoIsInSchemes(spec, scheme, type,
                       GetSchemeRegistry().standard_schemes);
}

template <typename CHAR>
bool DoIsOpaqueNonSpecial(const CHAR* spec, const Component& scheme) {
  if (scheme.is_empty()) {
    return false;
  }
  for (const std::string& s : GetSchemeRegistry().opaque_non_special_schemes) {
    if (base::EqualsCaseInsensitiveASCII(
            std::basic_string_view(&spec[scheme.begin], scheme.len), s)) {
      return true;
    }
  }
  return false;
}

template<typename CHAR>
bool DoFindAndCompareScheme(const CHAR* str,
                            int str_len,
                            const char* compare,
                            Component* found_scheme) {
  // Before extracting scheme, canonicalize the URL to remove any whitespace.
  // This matches the canonicalization done in DoCanonicalize function.
  STACK_UNINITIALIZED RawCanonOutputT<CHAR> whitespace_buffer;
  int spec_len;
  const CHAR* spec =
      RemoveURLWhitespace(str, str_len, &whitespace_buffer, &spec_len, nullptr);

  Component our_scheme;
  if (!ExtractScheme(spec, spec_len, &our_scheme)) {
    // No scheme.
    if (found_scheme)
      *found_scheme = Component();
    return false;
  }
  if (found_scheme)
    *found_scheme = our_scheme;
  return DoCompareSchemeComponent(spec, our_scheme, compare);
}

template <typename CHAR>
bool DoCanonicalize(const CHAR* spec,
                    int spec_len,
                    bool trim_path_end,
                    WhitespaceRemovalPolicy whitespace_policy,
                    CharsetConverter* charset_converter,
                    CanonOutput* output,
                    Parsed* output_parsed) {
  // Trim leading C0 control characters and spaces.
  int begin = 0;
  TrimURL(spec, &begin, &spec_len, trim_path_end);
  DCHECK(0 <= begin && begin <= spec_len);
  spec += begin;
  spec_len -= begin;

  output->ReserveSizeIfNeeded(spec_len);

  // Remove any whitespace from the middle of the relative URL if necessary.
  // Possibly this will result in copying to the new buffer.
  STACK_UNINITIALIZED RawCanonOutputT<CHAR> whitespace_buffer;
  if (whitespace_policy == REMOVE_WHITESPACE) {
    spec = RemoveURLWhitespace(spec, spec_len, &whitespace_buffer, &spec_len,
                               &output_parsed->potentially_dangling_markup);
  }

#ifdef WIN32
  // For Windows, we allow things that look like absolute Windows paths to be
  // fixed up magically to file URLs. This is done for IE compatibility. For
  // example, this will change "c:/foo" into a file URL rather than treating
  // it as a URL with the protocol "c". It also works for UNC ("\\foo\bar.txt").
  // There is similar logic in url_canon_relative.cc for
  //
  // For Max & Unix, we don't do this (the equivalent would be "/foo/bar" which
  // has no meaning as an absolute path name. This is because browsers on Mac
  // & Unix don't generally do this, so there is no compatibility reason for
  // doing so.
  if (DoesBeginUNCPath(spec, 0, spec_len, false) ||
      DoesBeginWindowsDriveSpec(spec, 0, spec_len)) {
    return CanonicalizeFileURL(
        spec, spec_len, ParseFileURL(std::basic_string_view(spec, spec_len)),
        charset_converter, output, output_parsed);
  }
#endif

  Component scheme;
  if (!ExtractScheme(spec, spec_len, &scheme))
    return false;

  // This is the parsed version of the input URL, we have to canonicalize it
  // before storing it in our object.
  bool success;
  SchemeType scheme_type = SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  if (DoCompareSchemeComponent(spec, scheme, url::kFileScheme)) {
    // File URLs are special.
    success = CanonicalizeFileURL(
        spec, spec_len, ParseFileURL(std::basic_string_view(spec, spec_len)),
        charset_converter, output, output_parsed);
  } else if (DoCompareSchemeComponent(spec, scheme, url::kFileSystemScheme)) {
    // Filesystem URLs are special.
    success = CanonicalizeFileSystemURL(
        spec, ParseFileSystemURL(std::basic_string_view(spec, spec_len)),
        charset_converter, output, output_parsed);

  } else if (DoIsStandard(spec, scheme, &scheme_type)) {
    // All "normal" URLs.
    success = CanonicalizeStandardURL(
        spec, ParseStandardURL(std::basic_string_view(spec, spec_len)),
        scheme_type, charset_converter, output, output_parsed);

  } else if (!url::IsUsingStandardCompliantNonSpecialSchemeURLParsing() &&
             DoCompareSchemeComponent(spec, scheme, url::kMailToScheme)) {
    // Mailto URLs are treated like standard URLs, with only a scheme, path,
    // and query.
    //
    // TODO(crbug.com/40063064): Remove the special handling of 'mailto:" scheme
    // URLs. "mailto:" is simply one of non-special URLs.
    success = CanonicalizeMailtoURL(
        spec, spec_len, ParseMailtoURL(std::basic_string_view(spec, spec_len)),
        output, output_parsed);

  } else {
    // Non-special scheme URLs like data: and javascript:.
    if (url::IsUsingStandardCompliantNonSpecialSchemeURLParsing() &&
        !DoIsOpaqueNonSpecial(spec, scheme)) {
      success = CanonicalizeNonSpecialURL(
          spec, spec_len,
          ParseNonSpecialURLInternal(std::basic_string_view(spec, spec_len),
                                     trim_path_end),
          charset_converter, *output, *output_parsed);
    } else {
      success = CanonicalizePathURL(
          spec, spec_len,
          ParsePathURL(std::basic_string_view(spec, spec_len), trim_path_end),
          output, output_parsed);
    }
  }
  return success;
}

template<typename CHAR>
bool DoResolveRelative(const char* base_spec,
                       int base_spec_len,
                       const Parsed& base_parsed,
                       const CHAR* in_relative,
                       int in_relative_length,
                       CharsetConverter* charset_converter,
                       CanonOutput* output,
                       Parsed* output_parsed) {
  // Remove any whitespace from the middle of the relative URL, possibly
  // copying to the new buffer.
  STACK_UNINITIALIZED RawCanonOutputT<CHAR> whitespace_buffer;
  int relative_length;
  const CHAR* relative = RemoveURLWhitespace(
      in_relative, in_relative_length, &whitespace_buffer, &relative_length,
      &output_parsed->potentially_dangling_markup);

  bool base_is_authority_based = false;
  bool base_is_hierarchical = false;
  if (base_spec &&
      base_parsed.scheme.is_nonempty()) {
    int after_scheme = base_parsed.scheme.end() + 1;  // Skip past the colon.
    int num_slashes = CountConsecutiveSlashes(base_spec, after_scheme,
                                              base_spec_len);
    base_is_authority_based = num_slashes > 1;
    base_is_hierarchical = num_slashes > 0;
  }

  bool is_hierarchical_base;

  if (url::IsUsingStandardCompliantNonSpecialSchemeURLParsing()) {
    is_hierarchical_base =
        base_parsed.scheme.is_nonempty() && !base_parsed.has_opaque_path;
  } else {
    SchemeType unused_scheme_type = SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
    is_hierarchical_base =
        base_parsed.scheme.is_nonempty() &&
        DoIsStandard(base_spec, base_parsed.scheme, &unused_scheme_type);
  }

  bool is_relative;
  Component relative_component;
  if (!IsRelativeURL(base_spec, base_parsed, relative, relative_length,
                     (base_is_hierarchical || is_hierarchical_base),
                     &is_relative, &relative_component)) {
    // Error resolving.
    return false;
  }

  // Don't reserve buffer space here. Instead, reserve in DoCanonicalize and
  // ReserveRelativeURL, to enable more accurate buffer sizes.

  // Pretend for a moment that |base_spec| is a standard URL. Normally
  // non-standard URLs are treated as PathURLs, but if the base has an
  // authority we would like to preserve it.
  if (is_relative && base_is_authority_based && !is_hierarchical_base) {
    Parsed base_parsed_authority =
        ParseStandardURL(std::string_view(base_spec, base_spec_len));
    if (base_parsed_authority.host.is_nonempty()) {
      STACK_UNINITIALIZED RawCanonOutputT<char> temporary_output;
      bool did_resolve_succeed =
          ResolveRelativeURL(base_spec, base_parsed_authority, false, relative,
                             relative_component, charset_converter,
                             &temporary_output, output_parsed);
      // The output_parsed is incorrect at this point (because it was built
      // based on base_parsed_authority instead of base_parsed) and needs to be
      // re-created.
      DoCanonicalize(temporary_output.data(), temporary_output.length(), true,
                     REMOVE_WHITESPACE, charset_converter, output,
                     output_parsed);
      return did_resolve_succeed;
    }
  } else if (is_relative) {
    // Relative, resolve and canonicalize.
    bool file_base_scheme = base_parsed.scheme.is_nonempty() &&
        DoCompareSchemeComponent(base_spec, base_parsed.scheme, kFileScheme);
    return ResolveRelativeURL(base_spec, base_parsed, file_base_scheme,
                              relative, relative_component, charset_converter,
                              output, output_parsed);
  }

  // Not relative, canonicalize the input.
  return DoCanonicalize(relative, relative_length, true,
                        DO_NOT_REMOVE_WHITESPACE, charset_converter, output,
                        output_parsed);
}

template<typename CHAR>
bool DoReplaceComponents(const char* spec,
                         int spec_len,
                         const Parsed& parsed,
                         const Replacements<CHAR>& replacements,
                         CharsetConverter* charset_converter,
                         CanonOutput* output,
                         Parsed* out_parsed) {
  // If the scheme is overridden, just do a simple string substitution and
  // re-parse the whole thing. There are lots of edge cases that we really don't
  // want to deal with. Like what happens if I replace "http://e:8080/foo"
  // with a file. Does it become "file:///E:/8080/foo" where the port number
  // becomes part of the path? Parsing that string as a file URL says "yes"
  // but almost no sane rule for dealing with the components individually would
  // come up with that.
  //
  // Why allow these crazy cases at all? Programatically, there is almost no
  // case for replacing the scheme. The most common case for hitting this is
  // in JS when building up a URL using the location object. In this case, the
  // JS code expects the string substitution behavior:
  //   http://www.w3.org/TR/2008/WD-html5-20080610/structured.html#common3
  if (replacements.IsSchemeOverridden()) {
    // Canonicalize the new scheme so it is 8-bit and can be concatenated with
    // the existing spec.
    STACK_UNINITIALIZED RawCanonOutput<128> scheme_replaced;
    Component scheme_replaced_parsed;
    CanonicalizeScheme(replacements.sources().scheme,
                       replacements.components().scheme,
                       &scheme_replaced, &scheme_replaced_parsed);

    // We can assume that the input is canonicalized, which means it always has
    // a colon after the scheme (or where the scheme would be).
    int spec_after_colon = parsed.scheme.is_valid() ? parsed.scheme.end() + 1
                                                    : 1;
    if (spec_len - spec_after_colon > 0) {
      scheme_replaced.Append(&spec[spec_after_colon],
                             spec_len - spec_after_colon);
    }

    // We now need to completely re-parse the resulting string since its meaning
    // may have changed with the different scheme.
    STACK_UNINITIALIZED RawCanonOutput<128> recanonicalized;
    Parsed recanonicalized_parsed;
    DoCanonicalize(scheme_replaced.data(), scheme_replaced.length(), true,
                   REMOVE_WHITESPACE, charset_converter, &recanonicalized,
                   &recanonicalized_parsed);

    // Recurse using the version with the scheme already replaced. This will now
    // use the replacement rules for the new scheme.
    //
    // Warning: this code assumes that ReplaceComponents will re-check all
    // components for validity. This is because we can't fail if DoCanonicalize
    // failed above since theoretically the thing making it fail could be
    // getting replaced here. If ReplaceComponents didn't re-check everything,
    // we wouldn't know if something *not* getting replaced is a problem.
    // If the scheme-specific replacers are made more intelligent so they don't
    // re-check everything, we should instead re-canonicalize the whole thing
    // after this call to check validity (this assumes replacing the scheme is
    // much much less common than other types of replacements, like clearing the
    // ref).
    Replacements<CHAR> replacements_no_scheme = replacements;
    replacements_no_scheme.SetScheme(NULL, Component());
    // If the input URL has potentially dangling markup, set the flag on the
    // output too. Note that in some cases the replacement gets rid of the
    // potentially dangling markup, but this ok since the check will fail
    // closed.
    if (parsed.potentially_dangling_markup) {
      out_parsed->potentially_dangling_markup = true;
    }
    return DoReplaceComponents(recanonicalized.data(), recanonicalized.length(),
                               recanonicalized_parsed, replacements_no_scheme,
                               charset_converter, output, out_parsed);
  }

  // TODO(csharrison): We could be smarter about size to reserve if this is done
  // in callers below, and the code checks to see which components are being
  // replaced, and with what length. If this ends up being a hot spot it should
  // be changed.
  output->ReserveSizeIfNeeded(spec_len);

  // If we get here, then we know the scheme doesn't need to be replaced, so can
  // just key off the scheme in the spec to know how to do the replacements.
  if (DoCompareSchemeComponent(spec, parsed.scheme, url::kFileScheme)) {
    return ReplaceFileURL(spec, parsed, replacements, charset_converter, output,
                          out_parsed);
  }
  if (DoCompareSchemeComponent(spec, parsed.scheme, url::kFileSystemScheme)) {
    return ReplaceFileSystemURL(spec, parsed, replacements, charset_converter,
                                output, out_parsed);
  }
  SchemeType scheme_type = SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  if (DoIsStandard(spec, parsed.scheme, &scheme_type)) {
    return ReplaceStandardURL(spec, parsed, replacements, scheme_type,
                              charset_converter, output, out_parsed);
  }
  if (!IsUsingStandardCompliantNonSpecialSchemeURLParsing() &&
      DoCompareSchemeComponent(spec, parsed.scheme, url::kMailToScheme)) {
    return ReplaceMailtoURL(spec, parsed, replacements, output, out_parsed);
  }

  if (IsUsingStandardCompliantNonSpecialSchemeURLParsing()) {
    return ReplaceNonSpecialURL(spec, parsed, replacements, charset_converter,
                                *output, *out_parsed);
  }
  return ReplacePathURL(spec, parsed, replacements, output, out_parsed);
}

void DoSchemeModificationPreamble() {
  // If this assert triggers, it means you've called Add*Scheme after
  // the SchemeRegistry has been used.
  //
  // This normally means you're trying to set up a new scheme too late or using
  // the SchemeRegistry too early in your application's init process.
  DCHECK(!g_scheme_registries_used.load())
      << "Trying to add a scheme after the lists have been used. "
         "Make sure that you haven't added any static GURL initializers in tests.";

  // If this assert triggers, it means you've called Add*Scheme after
  // LockSchemeRegistries has been called (see the header file for
  // LockSchemeRegistries for more).
  //
  // This normally means you're trying to set up a new scheme too late in your
  // application's init process. Locate where your app does this initialization
  // and calls LockSchemeRegistries, and add your new scheme there.
  DCHECK(!scheme_registries_locked)
      << "Trying to add a scheme after the lists have been locked.";
}

void DoAddSchemeWithHandler(const char* new_scheme,
                            const char* handler,
                            std::vector<SchemeWithHandler>* schemes) {
  DoSchemeModificationPreamble();
  DCHECK(schemes);
  DCHECK(strlen(new_scheme) > 0);
  DCHECK(strlen(handler) > 0);
  DCHECK_EQ(base::ToLowerASCII(new_scheme), new_scheme);
  DCHECK(!base::Contains(*schemes, new_scheme, &SchemeWithHandler::scheme));
  schemes->push_back({new_scheme, handler});
}

void DoAddScheme(const char* new_scheme, std::vector<std::string>* schemes) {
  DoSchemeModificationPreamble();
  DCHECK(schemes);
  DCHECK(strlen(new_scheme) > 0);
  DCHECK_EQ(base::ToLowerASCII(new_scheme), new_scheme);
  DCHECK(!base::Contains(*schemes, new_scheme));
  schemes->push_back(new_scheme);
}

void DoAddSchemeWithType(const char* new_scheme,
                         SchemeType type,
                         std::vector<SchemeWithType>* schemes) {
  DoSchemeModificationPreamble();
  DCHECK(schemes);
  DCHECK(strlen(new_scheme) > 0);
  DCHECK_EQ(base::ToLowerASCII(new_scheme), new_scheme);
  DCHECK(!base::Contains(*schemes, new_scheme, &SchemeWithType::scheme));
  schemes->push_back({new_scheme, type});
}

}  // namespace

void ClearSchemesForTests() {
  DCHECK(!g_scheme_registries_used.load())
      << "Schemes already used "
      << "(use ScopedSchemeRegistryForTests to relax for tests).";
  DCHECK(!scheme_registries_locked)
      << "Schemes already locked "
      << "(use ScopedSchemeRegistryForTests to relax for tests).";
  *GetSchemeRegistryWithoutLocking() = SchemeRegistry();
}

class ScopedSchemeRegistryInternal {
 public:
  ScopedSchemeRegistryInternal()
      : registry_(std::make_unique<SchemeRegistry>(
            *GetSchemeRegistryWithoutLocking())) {
    g_scheme_registries_used.store(false);
    scheme_registries_locked = false;
  }
  ~ScopedSchemeRegistryInternal() {
    *GetSchemeRegistryWithoutLocking() = *registry_;
    g_scheme_registries_used.store(true);
    scheme_registries_locked = true;
  }

 private:
  std::unique_ptr<SchemeRegistry> registry_;
};

ScopedSchemeRegistryForTests::ScopedSchemeRegistryForTests()
    : internal_(std::make_unique<ScopedSchemeRegistryInternal>()) {}

ScopedSchemeRegistryForTests::~ScopedSchemeRegistryForTests() = default;

void EnableNonStandardSchemesForAndroidWebView() {
  DoSchemeModificationPreamble();
  GetSchemeRegistryWithoutLocking()->allow_non_standard_schemes = true;
}

bool AllowNonStandardSchemesForAndroidWebView() {
  return GetSchemeRegistry().allow_non_standard_schemes;
}

void AddStandardScheme(const char* new_scheme, SchemeType type) {
  DoAddSchemeWithType(new_scheme, type,
                      &GetSchemeRegistryWithoutLocking()->standard_schemes);
}

std::vector<std::string> GetStandardSchemes() {
  std::vector<std::string> result;
  result.reserve(GetSchemeRegistry().standard_schemes.size());
  for (const auto& entry : GetSchemeRegistry().standard_schemes) {
    result.push_back(entry.scheme);
  }
  return result;
}

void AddReferrerScheme(const char* new_scheme, SchemeType type) {
  DoAddSchemeWithType(new_scheme, type,
                      &GetSchemeRegistryWithoutLocking()->referrer_schemes);
}

void AddSecureScheme(const char* new_scheme) {
  DoAddScheme(new_scheme, &GetSchemeRegistryWithoutLocking()->secure_schemes);
}

const std::vector<std::string>& GetSecureSchemes() {
  return GetSchemeRegistry().secure_schemes;
}

void AddLocalScheme(const char* new_scheme) {
  DoAddScheme(new_scheme, &GetSchemeRegistryWithoutLocking()->local_schemes);
}

const std::vector<std::string>& GetLocalSchemes() {
  return GetSchemeRegistry().local_schemes;
}

void AddNoAccessScheme(const char* new_scheme) {
  DoAddScheme(new_scheme,
              &GetSchemeRegistryWithoutLocking()->no_access_schemes);
}

const std::vector<std::string>& GetNoAccessSchemes() {
  return GetSchemeRegistry().no_access_schemes;
}

void AddCorsEnabledScheme(const char* new_scheme) {
  DoAddScheme(new_scheme,
              &GetSchemeRegistryWithoutLocking()->cors_enabled_schemes);
}

const std::vector<std::string>& GetCorsEnabledSchemes() {
  return GetSchemeRegistry().cors_enabled_schemes;
}

void AddWebStorageScheme(const char* new_scheme) {
  DoAddScheme(new_scheme,
              &GetSchemeRegistryWithoutLocking()->web_storage_schemes);
}

const std::vector<std::string>& GetWebStorageSchemes() {
  return GetSchemeRegistry().web_storage_schemes;
}

void AddCSPBypassingScheme(const char* new_scheme) {
  DoAddScheme(new_scheme,
              &GetSchemeRegistryWithoutLocking()->csp_bypassing_schemes);
}

const std::vector<std::string>& GetCSPBypassingSchemes() {
  return GetSchemeRegistry().csp_bypassing_schemes;
}

void AddEmptyDocumentScheme(const char* new_scheme) {
  DoAddScheme(new_scheme,
              &GetSchemeRegistryWithoutLocking()->empty_document_schemes);
}

const std::vector<std::string>& GetEmptyDocumentSchemes() {
  return GetSchemeRegistry().empty_document_schemes;
}

void AddPredefinedHandlerScheme(const char* new_scheme, const char* handler) {
  DoAddSchemeWithHandler(
      new_scheme, handler,
      &GetSchemeRegistryWithoutLocking()->predefined_handler_schemes);
}

std::vector<std::pair<std::string, std::string>> GetPredefinedHandlerSchemes() {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(GetSchemeRegistry().predefined_handler_schemes.size());
  for (const SchemeWithHandler& entry :
       GetSchemeRegistry().predefined_handler_schemes) {
    result.emplace_back(entry.scheme, entry.handler);
  }
  return result;
}

void LockSchemeRegistries() {
  scheme_registries_locked = true;
}

bool IsStandard(const char* spec, const Component& scheme) {
  SchemeType unused_scheme_type;
  return DoIsStandard(spec, scheme, &unused_scheme_type);
}

bool IsStandardScheme(std::string_view scheme) {
  return IsStandard(scheme.data(),
                    Component(0, base::checked_cast<int>(scheme.size())));
}

bool GetStandardSchemeType(const char* spec,
                           const Component& scheme,
                           SchemeType* type) {
  return DoIsStandard(spec, scheme, type);
}

bool GetStandardSchemeType(const char16_t* spec,
                           const Component& scheme,
                           SchemeType* type) {
  return DoIsStandard(spec, scheme, type);
}

bool IsStandard(const char16_t* spec, const Component& scheme) {
  SchemeType unused_scheme_type;
  return DoIsStandard(spec, scheme, &unused_scheme_type);
}

bool IsReferrerScheme(const char* spec, const Component& scheme) {
  SchemeType unused_scheme_type;
  return DoIsInSchemes(spec, scheme, &unused_scheme_type,
                       GetSchemeRegistry().referrer_schemes);
}

bool FindAndCompareScheme(const char* str,
                          int str_len,
                          const char* compare,
                          Component* found_scheme) {
  return DoFindAndCompareScheme(str, str_len, compare, found_scheme);
}

bool FindAndCompareScheme(const char16_t* str,
                          int str_len,
                          const char* compare,
                          Component* found_scheme) {
  return DoFindAndCompareScheme(str, str_len, compare, found_scheme);
}

bool DomainIs(std::string_view canonical_host,
              std::string_view canonical_domain) {
  if (canonical_host.empty() || canonical_domain.empty())
    return false;

  // If the host name ends with a dot but the input domain doesn't, then we
  // ignore the dot in the host name.
  size_t host_len = canonical_host.length();
  if (canonical_host.back() == '.' && canonical_domain.back() != '.')
    --host_len;

  if (host_len < canonical_domain.length())
    return false;

  // |host_first_pos| is the start of the compared part of the host name, not
  // start of the whole host name.
  const char* host_first_pos =
      canonical_host.data() + host_len - canonical_domain.length();

  if (std::string_view(host_first_pos, canonical_domain.length()) !=
      canonical_domain) {
    return false;
  }

  // Make sure there aren't extra characters in host before the compared part;
  // if the host name is longer than the input domain name, then the character
  // immediately before the compared part should be a dot. For example,
  // www.google.com has domain "google.com", but www.iamnotgoogle.com does not.
  if (canonical_domain[0] != '.' && host_len > canonical_domain.length() &&
      *(host_first_pos - 1) != '.') {
    return false;
  }

  return true;
}

bool HostIsIPAddress(std::string_view host) {
  STACK_UNINITIALIZED url::RawCanonOutputT<char, 128> ignored_output;
  url::CanonHostInfo host_info;
  url::CanonicalizeIPAddress(host.data(), Component(0, host.length()),
                             &ignored_output, &host_info);
  return host_info.IsIPAddress();
}

bool Canonicalize(const char* spec,
                  int spec_len,
                  bool trim_path_end,
                  CharsetConverter* charset_converter,
                  CanonOutput* output,
                  Parsed* output_parsed) {
  return DoCanonicalize(spec, spec_len, trim_path_end, REMOVE_WHITESPACE,
                        charset_converter, output, output_parsed);
}

bool Canonicalize(const char16_t* spec,
                  int spec_len,
                  bool trim_path_end,
                  CharsetConverter* charset_converter,
                  CanonOutput* output,
                  Parsed* output_parsed) {
  return DoCanonicalize(spec, spec_len, trim_path_end, REMOVE_WHITESPACE,
                        charset_converter, output, output_parsed);
}

bool ResolveRelative(const char* base_spec,
                     int base_spec_len,
                     const Parsed& base_parsed,
                     const char* relative,
                     int relative_length,
                     CharsetConverter* charset_converter,
                     CanonOutput* output,
                     Parsed* output_parsed) {
  return DoResolveRelative(base_spec, base_spec_len, base_parsed,
                           relative, relative_length,
                           charset_converter, output, output_parsed);
}

bool ResolveRelative(const char* base_spec,
                     int base_spec_len,
                     const Parsed& base_parsed,
                     const char16_t* relative,
                     int relative_length,
                     CharsetConverter* charset_converter,
                     CanonOutput* output,
                     Parsed* output_parsed) {
  return DoResolveRelative(base_spec, base_spec_len, base_parsed,
                           relative, relative_length,
                           charset_converter, output, output_parsed);
}

bool ReplaceComponents(const char* spec,
                       int spec_len,
                       const Parsed& parsed,
                       const Replacements<char>& replacements,
                       CharsetConverter* charset_converter,
                       CanonOutput* output,
                       Parsed* out_parsed) {
  return DoReplaceComponents(spec, spec_len, parsed, replacements,
                             charset_converter, output, out_parsed);
}

bool ReplaceComponents(const char* spec,
                       int spec_len,
                       const Parsed& parsed,
                       const Replacements<char16_t>& replacements,
                       CharsetConverter* charset_converter,
                       CanonOutput* output,
                       Parsed* out_parsed) {
  return DoReplaceComponents(spec, spec_len, parsed, replacements,
                             charset_converter, output, out_parsed);
}

void DecodeURLEscapeSequences(std::string_view input,
                              DecodeURLMode mode,
                              CanonOutputW* output) {
  if (input.empty()) {
    return;
  }

  STACK_UNINITIALIZED RawCanonOutputT<char> unescaped_chars;
  for (size_t i = 0; i < input.length(); i++) {
    if (input[i] == '%') {
      unsigned char ch;
      if (DecodeEscaped(input.data(), &i, input.length(), &ch)) {
        unescaped_chars.push_back(ch);
      } else {
        // Invalid escape sequence, copy the percent literal.
        unescaped_chars.push_back('%');
      }
    } else {
      // Regular non-escaped 8-bit character.
      unescaped_chars.push_back(input[i]);
    }
  }

  int output_initial_length = output->length();
  // Convert that 8-bit to UTF-16. It's not clear IE does this at all to
  // JavaScript URLs, but Firefox and Safari do.
  size_t unescaped_length = unescaped_chars.length();
  for (size_t i = 0; i < unescaped_length; i++) {
    unsigned char uch = static_cast<unsigned char>(unescaped_chars.at(i));
    if (uch < 0x80) {
      // Non-UTF-8, just append directly
      output->push_back(uch);
    } else {
      // next_ch will point to the last character of the decoded
      // character.
      size_t next_character = i;
      base_icu::UChar32 code_point;
      if (ReadUTFCharLossy(unescaped_chars.data(), &next_character,
                           unescaped_length, &code_point)) {
        // Valid UTF-8 character, convert to UTF-16.
        AppendUTF16Value(code_point, output);
        i = next_character;
      } else if (mode == DecodeURLMode::kUTF8) {
        DCHECK_EQ(code_point, 0xFFFD);
        AppendUTF16Value(code_point, output);
        i = next_character;
      } else {
        // If there are any sequences that are not valid UTF-8, we
        // revert |output| changes, and promote any bytes to UTF-16. We
        // copy all characters from the beginning to the end of the
        // identified sequence.
        output->set_length(output_initial_length);
        for (size_t j = 0; j < unescaped_chars.length(); ++j)
          output->push_back(static_cast<unsigned char>(unescaped_chars.at(j)));
        break;
      }
    }
  }
}

void EncodeURIComponent(std::string_view input, CanonOutput* output) {
  for (unsigned char c : input) {
    if (IsComponentChar(c)) {
      output->push_back(c);
    } else {
      AppendEscapedChar(c, output);
    }
  }
}

bool IsURIComponentChar(char c) {
  return IsComponentChar(c);
}

bool CompareSchemeComponent(const char* spec,
                            const Component& component,
                            const char* compare_to) {
  return DoCompareSchemeComponent(spec, component, compare_to);
}

bool CompareSchemeComponent(const char16_t* spec,
                            const Component& component,
                            const char* compare_to) {
  return DoCompareSchemeComponent(spec, component, compare_to);
}

bool HasInvalidURLEscapeSequences(std::string_view input) {
  for (size_t i = 0; i < input.size(); i++) {
    if (input[i] == '%') {
      unsigned char ch;
      if (!DecodeEscaped(input.data(), &i, input.size(), &ch)) {
        return true;
      }
    }
  }
  return false;
}

bool IsAndroidWebViewHackEnabledScheme(std::string_view scheme) {
  return AllowNonStandardSchemesForAndroidWebView() &&
         !IsStandardScheme(scheme);
}

}  // namespace url
