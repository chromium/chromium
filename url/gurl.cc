// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/gurl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <ostream>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "url/url_canon_stdstring.h"
#include "url/url_util.h"

GURL::GURL() : is_valid_(false) {
}

GURL::GURL(const GURL& other)
    : spec_(other.spec_),
      is_valid_(other.is_valid_),
      parsed_(other.parsed_) {
  if (other.inner_url_)
    inner_url_ = std::make_unique<GURL>(*other.inner_url_);
  // Valid filesystem urls should always have an inner_url_.
  DCHECK(!is_valid_ || !SchemeIsFileSystem() || inner_url_);
}

GURL::GURL(GURL&& other) noexcept
    : spec_(std::move(other.spec_)),
      is_valid_(other.is_valid_),
      parsed_(other.parsed_),
      inner_url_(std::move(other.inner_url_)) {
  other.is_valid_ = false;
  other.parsed_ = url::Parsed();
}

GURL::GURL(std::string_view url_string) {
  InitCanonical(url_string, true);
}

GURL::GURL(std::u16string_view url_string) {
  InitCanonical(url_string, true);
}

GURL::GURL(const std::string& url_string, RetainWhiteSpaceSelector) {
  InitCanonical(url_string, false);
}

GURL::GURL(const char* canonical_spec,
           size_t canonical_spec_len,
           const url::Parsed& parsed,
           bool is_valid)
    : spec_(canonical_spec, canonical_spec_len),
      is_valid_(is_valid),
      parsed_(parsed) {
  InitializeFromCanonicalSpec();
}

GURL::GURL(std::string canonical_spec, const url::Parsed& parsed, bool is_valid)
    : spec_(std::move(canonical_spec)), is_valid_(is_valid), parsed_(parsed) {
  InitializeFromCanonicalSpec();
}

template <typename T, typename CharT>
void GURL::InitCanonical(T input_spec, bool trim_path_end) {
  url::StdStringCanonOutput output(&spec_);
  is_valid_ = url::Canonicalize(
      input_spec.data(), static_cast<int>(input_spec.length()), trim_path_end,
      NULL, &output, &parsed_);

  output.Complete();  // Must be done before using string.
  if (is_valid_ && SchemeIsFileSystem()) {
    inner_url_ = std::make_unique<GURL>(spec_.data(), parsed_.Length(),
                                        *parsed_.inner_parsed(), true);
  }
  // Valid URLs always have non-empty specs.
  DCHECK(!is_valid_ || !spec_.empty());
}

void GURL::InitializeFromCanonicalSpec() {
  if (is_valid_ && SchemeIsFileSystem()) {
    inner_url_ = std::make_unique<GURL>(spec_.data(), parsed_.Length(),
                                        *parsed_.inner_parsed(), true);
  }

#ifndef NDEBUG
  // For testing purposes, check that the parsed canonical URL is identical to
  // what we would have produced. Skip checking for invalid URLs have no meaning
  // and we can't always canonicalize then reproducibly.
  if (is_valid_) {
    DCHECK(!spec_.empty());
    url::Component scheme;
    // We can't do this check on the inner_url of a filesystem URL, as
    // canonical_spec actually points to the start of the outer URL, so we'd
    // end up with infinite recursion in this constructor.
    if (!url::FindAndCompareScheme(spec_.data(), spec_.length(),
                                   url::kFileSystemScheme, &scheme) ||
        scheme.begin == parsed_.scheme.begin) {
      // We need to retain trailing whitespace on path URLs, as the |parsed_|
      // spec we originally received may legitimately contain trailing white-
      // space on the path or  components e.g. if the #ref has been
      // removed from a "foo:hello #ref" URL (see http://crbug.com/291747).
      GURL test_url(spec_, RETAIN_TRAILING_PATH_WHITEPACE);

      DCHECK_EQ(test_url.is_valid_, is_valid_);
      DCHECK_EQ(test_url.spec_, spec_);

      DCHECK_EQ(test_url.parsed_.scheme, parsed_.scheme);
      DCHECK_EQ(test_url.parsed_.username, parsed_.username);
      DCHECK_EQ(test_url.parsed_.password, parsed_.password);
      DCHECK_EQ(test_url.parsed_.host, parsed_.host);
      DCHECK_EQ(test_url.parsed_.port, parsed_.port);
      DCHECK_EQ(test_url.parsed_.path, parsed_.path);
      DCHECK_EQ(test_url.parsed_.query, parsed_.query);
      DCHECK_EQ(test_url.parsed_.ref, parsed_.ref);
    }
  }
#endif
}

GURL::~GURL() = default;

GURL& GURL::operator=(const GURL& other) {
  spec_ = other.spec_;
  is_valid_ = other.is_valid_;
  parsed_ = other.parsed_;

  if (!other.inner_url_)
    inner_url_.reset();
  else if (inner_url_)
    *inner_url_ = *other.inner_url_;
  else
    inner_url_ = std::make_unique<GURL>(*other.inner_url_);

  return *this;
}

GURL& GURL::operator=(GURL&& other) noexcept {
  spec_ = std::move(other.spec_);
  is_valid_ = other.is_valid_;
  parsed_ = other.parsed_;
  inner_url_ = std::move(other.inner_url_);

  other.is_valid_ = false;
  other.parsed_ = url::Parsed();
  return *this;
}

const std::string& GURL::spec() const {
  if (is_valid_ || spec_.empty())
    return spec_;

  // TODO(crbug.com/40580068): Make sure this no longer hits before making
  // NOTREACHED();
  DUMP_WILL_BE_NOTREACHED() << "Trying to get the spec of an invalid URL!";
  return base::EmptyString();
}

// Note: code duplicated below (it's inconvenient to use a template here).
GURL GURL::Resolve(std::string_view relative) const {
  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  GURL result;
  url::StdStringCanonOutput output(&result.spec_);
  if (!url::ResolveRelative(spec_.data(), static_cast<int>(spec_.length()),
                            parsed_, relative.data(),
                            static_cast<int>(relative.length()),
                            nullptr, &output, &result.parsed_)) {
    // Error resolving, return an empty URL.
    return GURL();
  }

  output.Complete();
  result.is_valid_ = true;
  if (result.SchemeIsFileSystem()) {
    result.inner_url_ =
        std::make_unique<GURL>(result.spec_.data(), result.parsed_.Length(),
                               *result.parsed_.inner_parsed(), true);
  }
  return result;
}

// Note: code duplicated above (it's inconvenient to use a template here).
GURL GURL::Resolve(std::u16string_view relative) const {
  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  GURL result;
  url::StdStringCanonOutput output(&result.spec_);
  if (!url::ResolveRelative(spec_.data(), static_cast<int>(spec_.length()),
                            parsed_, relative.data(),
                            static_cast<int>(relative.length()),
                            nullptr, &output, &result.parsed_)) {
    // Error resolving, return an empty URL.
    return GURL();
  }

  output.Complete();
  result.is_valid_ = true;
  if (result.SchemeIsFileSystem()) {
    result.inner_url_ =
        std::make_unique<GURL>(result.spec_.data(), result.parsed_.Length(),
                               *result.parsed_.inner_parsed(), true);
  }
  return result;
}

// Note: code duplicated below (it's inconvenient to use a template here).
GURL GURL::ReplaceComponents(const Replacements& replacements) const {
  GURL result;

  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  url::StdStringCanonOutput output(&result.spec_);
  result.is_valid_ = url::ReplaceComponents(
      spec_.data(), static_cast<int>(spec_.length()), parsed_, replacements,
      NULL, &output, &result.parsed_);

  output.Complete();

  result.ProcessFileSystemURLAfterReplaceComponents();
  return result;
}

// Note: code duplicated above (it's inconvenient to use a template here).
GURL GURL::ReplaceComponents(const ReplacementsW& replacements) const {
  GURL result;

  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  url::StdStringCanonOutput output(&result.spec_);
  result.is_valid_ = url::ReplaceComponents(
      spec_.data(), static_cast<int>(spec_.length()), parsed_, replacements,
      NULL, &output, &result.parsed_);

  output.Complete();

  result.ProcessFileSystemURLAfterReplaceComponents();

  return result;
}

void GURL::ProcessFileSystemURLAfterReplaceComponents() {
  if (!is_valid_)
    return;
  if (SchemeIsFileSystem()) {
    inner_url_ = std::make_unique<GURL>(spec_.data(), parsed_.Length(),
                                        *parsed_.inner_parsed(), true);
  }
}

GURL GURL::DeprecatedGetOriginAsURL() const {
  // This doesn't make sense for invalid or nonstandard URLs, so return
  // the empty URL.
  if (!is_valid_ || !IsStandard())
    return GURL();

  if (SchemeIsFileSystem())
    return inner_url_->DeprecatedGetOriginAsURL();

  Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearPath();
  replacements.ClearQuery();
  replacements.ClearRef();

  return ReplaceComponents(replacements);
}

GURL GURL::GetAsReferrer() const {
  if (!is_valid() || !IsReferrerScheme(spec_.data(), parsed_.scheme))
    return GURL();

  if (!has_ref() && !has_username() && !has_password())
    return GURL(*this);

  Replacements replacements;
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  return ReplaceComponents(replacements);
}

GURL GURL::GetWithEmptyPath() const {
  // This doesn't make sense for invalid or nonstandard URLs, so return
  // the empty URL.
  if (!is_valid_ || !IsStandard())
    return GURL();

  // We could optimize this since we know that the URL is canonical, and we are
  // appending a canonical path, so avoiding re-parsing.
  GURL other(*this);
  if (parsed_.path.len == 0)
    return other;

  // Clear everything after the path.
  other.parsed_.query.reset();
  other.parsed_.ref.reset();

  // Set the path, since the path is longer than one, we can just set the
  // first character and resize.
  other.spec_[other.parsed_.path.begin] = '/';
  other.parsed_.path.len = 1;
  other.spec_.resize(other.parsed_.path.begin + 1);
  return other;
}

GURL GURL::GetWithoutFilename() const {
  return Resolve(".");
}

GURL GURL::GetWithoutRef() const {
  if (!has_ref())
    return GURL(*this);

  Replacements replacements;
  replacements.ClearRef();
  return ReplaceComponents(replacements);
}

bool GURL::IsStandard() const {
  return url::IsStandard(spec_.data(), parsed_.scheme);
}

bool GURL::IsAboutBlank() const {
  return IsAboutUrl(url::kAboutBlankPath);
}

bool GURL::IsAboutSrcdoc() const {
  return IsAboutUrl(url::kAboutSrcdocPath);
}

bool GURL::SchemeIs(std::string_view lower_ascii_scheme) const {
  DCHECK(base::IsStringASCII(lower_ascii_scheme));
  DCHECK(base::ToLowerASCII(lower_ascii_scheme) == lower_ascii_scheme);

  if (!has_scheme())
    return lower_ascii_scheme.empty();
  return scheme_piece() == lower_ascii_scheme;
}

bool GURL::SchemeIsHTTPOrHTTPS() const {
  return SchemeIs(url::kHttpsScheme) || SchemeIs(url::kHttpScheme);
}

bool GURL::SchemeIsWSOrWSS() const {
  return SchemeIs(url::kWsScheme) || SchemeIs(url::kWssScheme);
}

bool GURL::SchemeIsCryptographic() const {
  if (!has_scheme())
    return false;
  return SchemeIsCryptographic(scheme_piece());
}

bool GURL::SchemeIsCryptographic(std::string_view lower_ascii_scheme) {
  DCHECK(base::IsStringASCII(lower_ascii_scheme));
  DCHECK(base::ToLowerASCII(lower_ascii_scheme) == lower_ascii_scheme);

  return lower_ascii_scheme == url::kHttpsScheme ||
         lower_ascii_scheme == url::kWssScheme;
}

bool GURL::SchemeIsLocal() const {
  // The `filesystem:` scheme is not in the Fetch spec, but Chromium still
  // supports it in large part. It should be treated as a local scheme too.
  return SchemeIs(url::kAboutScheme) || SchemeIs(url::kBlobScheme) ||
         SchemeIs(url::kDataScheme) || SchemeIs(url::kFileSystemScheme);
}

int GURL::IntPort() const {
  if (parsed_.port.is_nonempty())
    return url::ParsePort(spec_.data(), parsed_.port);
  return url::PORT_UNSPECIFIED;
}

int GURL::EffectiveIntPort() const {
  int int_port = IntPort();
  if (int_port == url::PORT_UNSPECIFIED && IsStandard())
    return url::DefaultPortForScheme(std::string_view(
        spec_.data() + parsed_.scheme.begin, parsed_.scheme.len));
  return int_port;
}

std::string GURL::ExtractFileName() const {
  url::Component file_component;
  url::ExtractFileName(spec_.data(), parsed_.path, &file_component);
  return ComponentString(file_component);
}

std::string_view GURL::PathForRequestPiece() const {
  DCHECK(parsed_.path.is_nonempty())
      << "Canonical path for requests should be non-empty";
  if (parsed_.ref.is_valid()) {
    // Clip off the reference when it exists. The reference starts after the
    // #-sign, so we have to subtract one to also remove it.
    return std::string_view(spec_).substr(
        parsed_.path.begin, parsed_.ref.begin - parsed_.path.begin - 1);
  }
  // Compute the actual path length, rather than depending on the spec's
  // terminator. If we're an inner_url, our spec continues on into our outer
  // URL's path/query/ref.
  int path_len = parsed_.path.len;
  if (parsed_.query.is_valid())
    path_len = parsed_.query.end() - parsed_.path.begin;

  return std::string_view(spec_).substr(parsed_.path.begin, path_len);
}

std::string GURL::PathForRequest() const {
  return std::string(PathForRequestPiece());
}

std::string GURL::HostNoBrackets() const {
  return std::string(HostNoBracketsPiece());
}

std::string_view GURL::HostNoBracketsPiece() const {
  // If host looks like an IPv6 literal, strip the square brackets.
  url::Component h(parsed_.host);
  if (h.len >= 2 && spec_[h.begin] == '[' && spec_[h.end() - 1] == ']') {
    h.begin++;
    h.len -= 2;
  }
  return ComponentStringPiece(h);
}

std::string GURL::GetContent() const {
  return std::string(GetContentPiece());
}

std::string_view GURL::GetContentPiece() const {
  if (!is_valid_)
    return std::string_view();
  url::Component content_component = parsed_.GetContent();
  if (!SchemeIs(url::kJavaScriptScheme) && parsed_.ref.is_valid())
    content_component.len -= parsed_.ref.len + 1;
  return ComponentStringPiece(content_component);
}

bool GURL::HostIsIPAddress() const {
  return is_valid_ && url::HostIsIPAddress(host_piece());
}

const GURL& GURL::EmptyGURL() {
  static base::NoDestructor<GURL> empty_gurl;
  return *empty_gurl;
}

bool GURL::DomainIs(std::string_view canonical_domain) const {
  if (!is_valid_)
    return false;

  // FileSystem URLs have empty host_piece, so check this first.
  if (inner_url_ && SchemeIsFileSystem())
    return inner_url_->DomainIs(canonical_domain);
  return url::DomainIs(host_piece(), canonical_domain);
}

bool GURL::EqualsIgnoringRef(const GURL& other) const {
  int ref_position = parsed_.CountCharactersBefore(url::Parsed::REF, true);
  int ref_position_other =
      other.parsed_.CountCharactersBefore(url::Parsed::REF, true);
  return std::string_view(spec_).substr(0, ref_position) ==
         std::string_view(other.spec_).substr(0, ref_position_other);
}

void GURL::Swap(GURL* other) {
  spec_.swap(other->spec_);
  std::swap(is_valid_, other->is_valid_);
  std::swap(parsed_, other->parsed_);
  inner_url_.swap(other->inner_url_);
}

size_t GURL::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(spec_) +
         base::trace_event::EstimateMemoryUsage(inner_url_) +
         (parsed_.inner_parsed() ? sizeof(url::Parsed) : 0);
}

bool GURL::IsAboutUrl(std::string_view allowed_path) const {
  if (!SchemeIs(url::kAboutScheme))
    return false;

  if (has_host() || has_username() || has_password() || has_port())
    return false;

  return IsAboutPath(path_piece(), allowed_path);
}

// static
bool GURL::IsAboutPath(std::string_view actual_path,
                       std::string_view allowed_path) {
  if (!base::StartsWith(actual_path, allowed_path))
    return false;

  if (actual_path.size() == allowed_path.size()) {
    DCHECK_EQ(actual_path, allowed_path);
    return true;
  }

  if ((actual_path.size() == allowed_path.size() + 1) &&
      actual_path.back() == '/') {
    DCHECK_EQ(actual_path, std::string(allowed_path) + '/');
    return true;
  }

  return false;
}

void GURL::WriteIntoTrace(perfetto::TracedValue context) const {
  std::move(context).WriteString(possibly_invalid_spec());
}

std::ostream& operator<<(std::ostream& out, const GURL& url) {
  return out << url.possibly_invalid_spec();
}

bool operator==(const GURL& x, const GURL& y) {
  return x.possibly_invalid_spec() == y.possibly_invalid_spec();
}

bool operator==(const GURL& x, std::string_view spec) {
  DCHECK_EQ(GURL(spec).possibly_invalid_spec(), spec)
      << "Comparisons of GURLs and strings must ensure as a precondition that "
         "the string is fully canonicalized.";
  return x.possibly_invalid_spec() == spec;
}

namespace url::debug {

ScopedUrlCrashKey::ScopedUrlCrashKey(base::debug::CrashKeyString* crash_key,
                                     const GURL& url)
    : scoped_string_value_(
          crash_key,
          url.is_empty() ? "<empty url>" : url.possibly_invalid_spec()) {}

ScopedUrlCrashKey::~ScopedUrlCrashKey() = default;

}  // namespace url::debug
