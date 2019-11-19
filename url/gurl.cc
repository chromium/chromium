// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/gurl.h"

#include <stddef.h>

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
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
    inner_url_.reset(new GURL(*other.inner_url_));
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

GURL::GURL(base::StringPiece url_string) {
  InitCanonical(url_string, true);
}

GURL::GURL(base::StringPiece16 url_string) {
  InitCanonical(url_string, true);
}

GURL::GURL(const std::string& url_string, RetainWhiteSpaceSelector) {
  InitCanonical(base::StringPiece(url_string), false);
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

template<typename STR>
void GURL::InitCanonical(base::BasicStringPiece<STR> input_spec,
                         bool trim_path_end) {
  url::StdStringCanonOutput output(&spec_);
  is_valid_ = url::Canonicalize(
      input_spec.data(), static_cast<int>(input_spec.length()), trim_path_end,
      NULL, &output, &parsed_);

  output.Complete();  // Must be done before using string.
  if (is_valid_ && SchemeIsFileSystem()) {
    inner_url_.reset(new GURL(spec_.data(), parsed_.Length(),
                              *parsed_.inner_parsed(), true));
  }
  // Valid URLs always have non-empty specs.
  DCHECK(!is_valid_ || !spec_.empty());
}

void GURL::InitializeFromCanonicalSpec() {
  if (is_valid_ && SchemeIsFileSystem()) {
    inner_url_.reset(
        new GURL(spec_.data(), parsed_.Length(),
                 *parsed_.inner_parsed(), true));
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

      DCHECK(test_url.is_valid_ == is_valid_);
      DCHECK(test_url.spec_ == spec_);

      DCHECK(test_url.parsed_.scheme == parsed_.scheme);
      DCHECK(test_url.parsed_.username == parsed_.username);
      DCHECK(test_url.parsed_.password == parsed_.password);
      DCHECK(test_url.parsed_.host == parsed_.host);
      DCHECK(test_url.parsed_.port == parsed_.port);
      DCHECK(test_url.parsed_.path == parsed_.path);
      DCHECK(test_url.parsed_.query == parsed_.query);
      DCHECK(test_url.parsed_.ref == parsed_.ref);
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
    inner_url_.reset(new GURL(*other.inner_url_));

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

  DCHECK(false) << "Trying to get the spec of an invalid URL!";
  return base::EmptyString();
}

bool GURL::operator<(const GURL& other) const {
  return spec_ < other.spec_;
}

bool GURL::operator>(const GURL& other) const {
  return spec_ > other.spec_;
}

// Note: code duplicated below (it's inconvenient to use a template here).
GURL GURL::Resolve(base::StringPiece relative) const {
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
    result.inner_url_.reset(
        new GURL(result.spec_.data(), result.parsed_.Length(),
                 *result.parsed_.inner_parsed(), true));
  }
  return result;
}

// Note: code duplicated above (it's inconvenient to use a template here).
GURL GURL::Resolve(base::StringPiece16 relative) const {
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
    result.inner_url_.reset(
        new GURL(result.spec_.data(), result.parsed_.Length(),
                 *result.parsed_.inner_parsed(), true));
  }
  return result;
}

// Note: code duplicated below (it's inconvenient to use a template here).
GURL GURL::ReplaceComponents(
    const url::Replacements<char>& replacements) const {
  GURL result;

  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  url::StdStringCanonOutput output(&result.spec_);
  result.is_valid_ = url::ReplaceComponents(
      spec_.data(), static_cast<int>(spec_.length()), parsed_, replacements,
      NULL, &output, &result.parsed_);

  output.Complete();
  if (result.is_valid_ && result.SchemeIsFileSystem()) {
    result.inner_url_.reset(new GURL(result.spec_.data(),
                                     result.parsed_.Length(),
                                     *result.parsed_.inner_parsed(), true));
  }
  return result;
}

// Note: code duplicated above (it's inconvenient to use a template here).
GURL GURL::ReplaceComponents(
    const url::Replacements<base::char16>& replacements) const {
  GURL result;

  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  url::StdStringCanonOutput output(&result.spec_);
  result.is_valid_ = url::ReplaceComponents(
      spec_.data(), static_cast<int>(spec_.length()), parsed_, replacements,
      NULL, &output, &result.parsed_);

  output.Complete();
  if (result.is_valid_ && result.SchemeIsFileSystem()) {
    result.inner_url_.reset(new GURL(result.spec_.data(),
                                     result.parsed_.Length(),
                                     *result.parsed_.inner_parsed(), true));
  }
  return result;
}

GURL GURL::GetOrigin() const {
  // This doesn't make sense for invalid or nonstandard URLs, so return
  // the empty URL.
  if (!is_valid_ || !IsStandard())
    return GURL();

  if (SchemeIsFileSystem())
    return inner_url_->GetOrigin();

  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearPath();
  replacements.ClearQuery();
  replacements.ClearRef();

  return ReplaceComponents(replacements);
}

GURL GURL::GetAsReferrer() const {
  if (!SchemeIsValidForReferrer())
    return GURL();

  if (!has_ref() && !has_username() && !has_password())
    return GURL(*this);

  url::Replacements<char> replacements;
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

bool GURL::IsStandard() const {
  return url::IsStandard(spec_.data(), parsed_.scheme);
}

bool GURL::IsAboutBlank() const {
  return IsAboutUrl(url::kAboutBlankPath);
}

bool GURL::IsAboutSrcdoc() const {
  return IsAboutUrl(url::kAboutSrcdocPath);
}

bool GURL::SchemeIs(base::StringPiece lower_ascii_scheme) const {
  DCHECK(base::IsStringASCII(lower_ascii_scheme));
  DCHECK(base::ToLowerASCII(lower_ascii_scheme) == lower_ascii_scheme);

  if (parsed_.scheme.len <= 0)
    return lower_ascii_scheme.empty();
  return scheme_piece() == lower_ascii_scheme;
}

bool GURL::SchemeIsHTTPOrHTTPS() const {
  return SchemeIs(url::kHttpScheme) || SchemeIs(url::kHttpsScheme);
}

bool GURL::SchemeIsValidForReferrer() const {
  return is_valid_ && IsReferrerScheme(spec_.data(), parsed_.scheme);
}

bool GURL::SchemeIsWSOrWSS() const {
  return SchemeIs(url::kWsScheme) || SchemeIs(url::kWssScheme);
}

bool GURL::SchemeIsCryptographic() const {
  if (parsed_.scheme.len <= 0)
    return false;
  return SchemeIsCryptographic(scheme_piece());
}

bool GURL::SchemeIsCryptographic(base::StringPiece lower_ascii_scheme) {
  DCHECK(base::IsStringASCII(lower_ascii_scheme));
  DCHECK(base::ToLowerASCII(lower_ascii_scheme) == lower_ascii_scheme);

  return lower_ascii_scheme == url::kHttpsScheme ||
         lower_ascii_scheme == url::kWssScheme;
}

int GURL::IntPort() const {
  if (parsed_.port.is_nonempty())
    return url::ParsePort(spec_.data(), parsed_.port);
  return url::PORT_UNSPECIFIED;
}

int GURL::EffectiveIntPort() const {
  int int_port = IntPort();
  if (int_port == url::PORT_UNSPECIFIED && IsStandard())
    return url::DefaultPortForScheme(spec_.data() + parsed_.scheme.begin,
                                     parsed_.scheme.len);
  return int_port;
}

std::string GURL::ExtractFileName() const {
  url::Component file_component;
  url::ExtractFileName(spec_.data(), parsed_.path, &file_component);
  return ComponentString(file_component);
}

base::StringPiece GURL::PathForRequestPiece() const {
  DCHECK(parsed_.path.len > 0)
      << "Canonical path for requests should be non-empty";
  if (parsed_.ref.len >= 0) {
    // Clip off the reference when it exists. The reference starts after the
    // #-sign, so we have to subtract one to also remove it.
    return base::StringPiece(&spec_[parsed_.path.begin],
                             parsed_.ref.begin - parsed_.path.begin - 1);
  }
  // Compute the actual path length, rather than depending on the spec's
  // terminator. If we're an inner_url, our spec continues on into our outer
  // URL's path/query/ref.
  int path_len = parsed_.path.len;
  if (parsed_.query.is_valid())
    path_len = parsed_.query.end() - parsed_.path.begin;

  return base::StringPiece(&spec_[parsed_.path.begin], path_len);
}

std::string GURL::PathForRequest() const {
  return PathForRequestPiece().as_string();
}

std::string GURL::HostNoBrackets() const {
  return HostNoBracketsPiece().as_string();
}

base::StringPiece GURL::HostNoBracketsPiece() const {
  // If host looks like an IPv6 literal, strip the square brackets.
  url::Component h(parsed_.host);
  if (h.len >= 2 && spec_[h.begin] == '[' && spec_[h.end() - 1] == ']') {
    h.begin++;
    h.len -= 2;
  }
  return ComponentStringPiece(h);
}

std::string GURL::GetContent() const {
  if (!is_valid_)
    return std::string();
  std::string content = ComponentString(parsed_.GetContent());
  if (!SchemeIs(url::kJavaScriptScheme) && parsed_.ref.len >= 0)
    content.erase(content.size() - parsed_.ref.len - 1);
  return content;
}

bool GURL::HostIsIPAddress() const {
  return is_valid_ && url::HostIsIPAddress(host_piece());
}

const GURL& GURL::EmptyGURL() {
  static base::NoDestructor<GURL> empty_gurl;
  return *empty_gurl;
}

bool GURL::DomainIs(base::StringPiece canonical_domain) const {
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
  return base::StringPiece(spec_).substr(0, ref_position) ==
         base::StringPiece(other.spec_).substr(0, ref_position_other);
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

bool GURL::IsAboutUrl(base::StringPiece allowed_path) const {
  if (!SchemeIs(url::kAboutScheme))
    return false;

  if (has_host() || has_username() || has_password() || has_port())
    return false;

  if (!path_piece().starts_with(allowed_path))
    return false;

  if (path_piece().size() == allowed_path.size()) {
    DCHECK_EQ(path_piece(), allowed_path);
    return true;
  }

  if ((path_piece().size() == allowed_path.size() + 1) &&
      path_piece().back() == '/') {
    DCHECK_EQ(path_piece(), allowed_path.as_string() + '/');
    return true;
  }

  return false;
}

std::ostream& operator<<(std::ostream& out, const GURL& url) {
  return out << url.possibly_invalid_spec();
}

bool operator==(const GURL& x, const GURL& y) {
  return x.possibly_invalid_spec() == y.possibly_invalid_spec();
}

bool operator!=(const GURL& x, const GURL& y) {
  return !(x == y);
}

bool operator==(const GURL& x, const base::StringPiece& spec) {
  DCHECK_EQ(GURL(spec).possibly_invalid_spec(), spec);
  return x.possibly_invalid_spec() == spec;
}

bool operator==(const base::StringPiece& spec, const GURL& x) {
  return x == spec;
}

bool operator!=(const GURL& x, const base::StringPiece& spec) {
  return !(x == spec);
}

bool operator!=(const base::StringPiece& spec, const GURL& x) {
  return !(x == spec);
}
