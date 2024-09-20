// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/origin.h"

#include <stdint.h>

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/unguessable_token.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"
#include "url/url_features.h"
#include "url/url_util.h"

namespace url {

Origin::Origin() : nonce_(Nonce()) {}

Origin Origin::Create(const GURL& url) {
  if (!url.is_valid())
    return Origin();

  SchemeHostPort tuple;

  if (url.SchemeIsFileSystem()) {
    tuple = SchemeHostPort(*url.inner_url());
  } else if (url.SchemeIsBlob()) {
    // If we're dealing with a 'blob:' URL, https://url.spec.whatwg.org/#origin
    // defines the origin as the origin of the URL which results from parsing
    // the "path", which boils down to everything after the scheme. GURL's
    // 'GetContent()' gives us exactly that.
    tuple = SchemeHostPort(GURL(url.GetContent()));
  } else {
    tuple = SchemeHostPort(url);

    // It's SchemeHostPort's responsibility to filter out unrecognized schemes;
    // sanity check that this is happening.
    DCHECK(!tuple.IsValid() || url.IsStandard() ||
           base::Contains(GetLocalSchemes(), url.scheme_piece()) ||
           AllowNonStandardSchemesForAndroidWebView());
  }

  if (!tuple.IsValid())
    return Origin();
  return Origin(std::move(tuple));
}

Origin Origin::Resolve(const GURL& url, const Origin& base_origin) {
  if (url.SchemeIs(kAboutScheme) || url.is_empty())
    return base_origin;
  Origin result = Origin::Create(url);
  if (!result.opaque())
    return result;
  return base_origin.DeriveNewOpaqueOrigin();
}

Origin::Origin(const Origin&) = default;
Origin& Origin::operator=(const Origin&) = default;
Origin::Origin(Origin&&) noexcept = default;
Origin& Origin::operator=(Origin&&) noexcept = default;
Origin::~Origin() = default;

// static
std::optional<Origin> Origin::UnsafelyCreateTupleOriginWithoutNormalization(
    std::string_view scheme,
    std::string_view host,
    uint16_t port) {
  SchemeHostPort tuple(std::string(scheme), std::string(host), port,
                       SchemeHostPort::CHECK_CANONICALIZATION);
  if (!tuple.IsValid())
    return std::nullopt;
  return Origin(std::move(tuple));
}

// static
std::optional<Origin> Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
    std::string_view precursor_scheme,
    std::string_view precursor_host,
    uint16_t precursor_port,
    const Origin::Nonce& nonce) {
  SchemeHostPort precursor(std::string(precursor_scheme),
                           std::string(precursor_host), precursor_port,
                           SchemeHostPort::CHECK_CANONICALIZATION);
  // For opaque origins, it is okay for the SchemeHostPort to be invalid;
  // however, this should only arise when the arguments indicate the
  // canonical representation of the invalid SchemeHostPort.
  if (!precursor.IsValid() &&
      !(precursor_scheme.empty() && precursor_host.empty() &&
        precursor_port == 0)) {
    return std::nullopt;
  }
  return Origin(std::move(nonce), std::move(precursor));
}

// static
Origin Origin::CreateFromNormalizedTuple(std::string scheme,
                                         std::string host,
                                         uint16_t port) {
  SchemeHostPort tuple(std::move(scheme), std::move(host), port,
                       SchemeHostPort::ALREADY_CANONICALIZED);
  if (!tuple.IsValid())
    return Origin();
  return Origin(std::move(tuple));
}

// static
Origin Origin::CreateOpaqueFromNormalizedPrecursorTuple(
    std::string precursor_scheme,
    std::string precursor_host,
    uint16_t precursor_port,
    const Origin::Nonce& nonce) {
  SchemeHostPort precursor(std::move(precursor_scheme),
                           std::move(precursor_host), precursor_port,
                           SchemeHostPort::ALREADY_CANONICALIZED);
  // For opaque origins, it is okay for the SchemeHostPort to be invalid.
  return Origin(std::move(nonce), std::move(precursor));
}

std::string Origin::Serialize() const {
  if (opaque())
    return "null";

  if (scheme() == kFileScheme)
    return "file://";

  return tuple_.Serialize();
}

GURL Origin::GetURL() const {
  if (opaque())
    return GURL();

  if (scheme() == kFileScheme)
    return GURL("file:///");

  return tuple_.GetURL();
}

const base::UnguessableToken* Origin::GetNonceForSerialization() const {
  return nonce_ ? &nonce_->token() : nullptr;
}

bool Origin::IsSameOriginWith(const Origin& other) const {
  // scheme/host/port must match, even for opaque origins where |tuple_| holds
  // the precursor origin.
  return std::tie(tuple_, nonce_) == std::tie(other.tuple_, other.nonce_);
}

bool Origin::IsSameOriginWith(const GURL& url) const {
  if (opaque())
    return false;

  // The `url::Origin::Create` call here preserves how IsSameOriginWith was used
  // historically, even though in some scenarios it is not clearly correct:
  // - Origin of about:blank and about:srcdoc cannot be correctly
  //   computed/recovered.
  // - Ideally passing an invalid `url` would be a caller error (e.g. a DCHECK).
  // - The caller intent is not always clear wrt handling the outer-vs-inner
  //   origins/URLs in blob: and filesystem: schemes.
  return IsSameOriginWith(url::Origin::Create(url));
}

bool Origin::CanBeDerivedFrom(const GURL& url) const {
  DCHECK(url.is_valid());

  // For "no access" schemes, blink's SecurityOrigin will always create an
  // opaque unique one. However, about: scheme is also registered as such but
  // does not behave this way, therefore exclude it from this check.
  if (base::Contains(url::GetNoAccessSchemes(), url.scheme()) &&
      !url.SchemeIs(kAboutScheme)) {
    // If |this| is not opaque, definitely return false as the expectation
    // is for opaque origin.
    if (!opaque())
      return false;

    // And if it is unique opaque origin, it definitely is fine. But if there
    // is a precursor stored, we should fall through to compare the tuples.
    if (!tuple_.IsValid())
      return true;
  }

  SchemeHostPort url_tuple;

  // Optimization for the common, success case: Scheme/Host/Port match on the
  // precursor, and the URL is standard. Opaqueness does not matter as a tuple
  // origin can always create an opaque tuple origin.
  if (url.IsStandard()) {
    // Note: if extra copies of the scheme and host are undesirable, this check
    // can be implemented using std::string_view comparisons, but it has to
    // account explicitly checks on port numbers.
    if (url.SchemeIsFileSystem()) {
      url_tuple = SchemeHostPort(*url.inner_url());
    } else {
      url_tuple = SchemeHostPort(url);
    }
    return url_tuple == tuple_;

    // Blob URLs still contain an inner origin, however it is not accessible
    // through inner_url(), therefore it requires specific case to handle it.
  } else if (url.SchemeIsBlob()) {
    // If |this| doesn't contain any precursor information, it is an unique
    // opaque origin. It is valid case, as any browser-initiated navigation
    // to about:blank or data: URL will result in a document with such
    // origin and it is valid for it to create blob: URLs.
    if (!tuple_.IsValid())
      return true;

    url_tuple = SchemeHostPort(GURL(url.GetContent()));
    return url_tuple == tuple_;
  }

  // At this point, the URL has non-standard scheme.
  DCHECK(!url.IsStandard());

  // All about: URLs (about:blank, about:srcdoc) inherit their origin from
  // the context which navigated them, which means that they can be in any
  // type of origin.
  if (url.SchemeIs(kAboutScheme))
    return true;

  // All data: URLs commit in opaque origins, therefore |this| must be opaque
  // if |url| has data: scheme.
  if (url.SchemeIs(kDataScheme))
    return opaque();

  // If |this| does not have valid precursor tuple, it is unique opaque origin,
  // which is what we expect non-standard schemes to get.
  if (!tuple_.IsValid())
    return true;

  // However, when there is precursor present, that must match.
  if (IsUsingStandardCompliantNonSpecialSchemeURLParsing()) {
    return SchemeHostPort(url) == tuple_;
  } else {
    // Match only the scheme because host and port are unavailable for
    // non-special URLs when the flag is disabled.
    return url.scheme() == tuple_.scheme();
  }
}

bool Origin::DomainIs(std::string_view canonical_domain) const {
  return !opaque() && url::DomainIs(tuple_.host(), canonical_domain);
}

bool Origin::operator<(const Origin& other) const {
  return std::tie(tuple_, nonce_) < std::tie(other.tuple_, other.nonce_);
}

Origin Origin::DeriveNewOpaqueOrigin() const {
  return Origin(Nonce(), tuple_);
}

const base::UnguessableToken* Origin::GetNonceForTesting() const {
  return GetNonceForSerialization();
}

std::string Origin::GetDebugString(bool include_nonce) const {
  // Handle non-opaque origins first, as they are simpler.
  if (!opaque()) {
    std::string out = Serialize();
    if (scheme() == kFileScheme)
      base::StrAppend(&out, {" [internally: ", tuple_.Serialize(), "]"});
    return out;
  }

  // For opaque origins, log the nonce and precursor as well. Without this,
  // EXPECT_EQ failures between opaque origins are nearly impossible to
  // understand.
  std::string out = base::StrCat({Serialize(), " [internally:"});
  if (include_nonce) {
    out += " (";
    if (nonce_->raw_token().is_empty())
      out += "nonce TBD";
    else
      out += nonce_->raw_token().ToString();
    out += ")";
  }
  if (!tuple_.IsValid())
    base::StrAppend(&out, {" anonymous]"});
  else
    base::StrAppend(&out, {" derived from ", tuple_.Serialize(), "]"});
  return out;
}

Origin::Origin(SchemeHostPort tuple) : tuple_(std::move(tuple)) {
  DCHECK(!opaque());
  DCHECK(tuple_.IsValid());
}

// Constructs an opaque origin derived from |precursor|.
Origin::Origin(const Nonce& nonce, SchemeHostPort precursor)
    : tuple_(std::move(precursor)), nonce_(std::move(nonce)) {
  DCHECK(opaque());
  // |precursor| is retained, but not accessible via scheme()/host()/port().
  DCHECK_EQ("", scheme());
  DCHECK_EQ("", host());
  DCHECK_EQ(0U, port());
}

std::optional<std::string> Origin::SerializeWithNonce() const {
  return SerializeWithNonceImpl();
}

std::optional<std::string> Origin::SerializeWithNonceAndInitIfNeeded() {
  GetNonceForSerialization();
  return SerializeWithNonceImpl();
}

// The pickle is saved in the following format, in order:
// string - tuple_.GetURL().spec().
// uint64_t (if opaque) - high bits of nonce if opaque. 0 if not initialized.
// uint64_t (if opaque) - low bits of nonce if opaque. 0 if not initialized.
std::optional<std::string> Origin::SerializeWithNonceImpl() const {
  if (!opaque() && !tuple_.IsValid())
    return std::nullopt;

  base::Pickle pickle;
  pickle.WriteString(tuple_.Serialize());
  if (opaque() && !nonce_->raw_token().is_empty()) {
    pickle.WriteUInt64(nonce_->token().GetHighForSerialization());
    pickle.WriteUInt64(nonce_->token().GetLowForSerialization());
  } else if (opaque()) {
    // Nonce hasn't been initialized.
    pickle.WriteUInt64(0);
    pickle.WriteUInt64(0);
  }

  base::span<const uint8_t> data(static_cast<const uint8_t*>(pickle.data()),
                                 pickle.size());
  // Base64 encode the data to make it nicer to play with.
  return base::Base64Encode(data);
}

// static
std::optional<Origin> Origin::Deserialize(std::string_view value) {
  std::string data;
  if (!base::Base64Decode(value, &data))
    return std::nullopt;

  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(data));
  base::PickleIterator reader(pickle);

  std::string pickled_url;
  if (!reader.ReadString(&pickled_url))
    return std::nullopt;
  GURL url(pickled_url);

  // If only a tuple was serialized, then this origin is not opaque. For opaque
  // origins, we expect two uint64's to be left in the pickle.
  bool is_opaque = !reader.ReachedEnd();

  // Opaque origins without a tuple are ok.
  if (!is_opaque && !url.is_valid())
    return std::nullopt;
  SchemeHostPort tuple(url);

  // Possible successful early return if the pickled Origin was not opaque.
  if (!is_opaque) {
    Origin origin(tuple);
    if (origin.opaque())
      return std::nullopt;  // Something went horribly wrong.
    return origin;
  }

  uint64_t nonce_high = 0;
  if (!reader.ReadUInt64(&nonce_high))
    return std::nullopt;

  uint64_t nonce_low = 0;
  if (!reader.ReadUInt64(&nonce_low))
    return std::nullopt;

  std::optional<base::UnguessableToken> nonce_token =
      base::UnguessableToken::Deserialize(nonce_high, nonce_low);

  Origin::Nonce nonce;
  if (nonce_token.has_value()) {
    // The serialized nonce wasn't empty, so copy it here.
    nonce = Origin::Nonce(nonce_token.value());
  }
  Origin origin;
  origin.nonce_ = std::move(nonce);
  origin.tuple_ = tuple;
  return origin;
}

void Origin::WriteIntoTrace(perfetto::TracedValue context) const {
  std::move(context).WriteString(GetDebugString());
}

size_t Origin::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(tuple_);
}

std::ostream& operator<<(std::ostream& out, const url::Origin& origin) {
  out << origin.GetDebugString();
  return out;
}

std::ostream& operator<<(std::ostream& out, const url::Origin::Nonce& nonce) {
  // Subtle: don't let logging trigger lazy-generation of the token value.
  if (nonce.raw_token().is_empty())
    return (out << "(nonce TBD)");
  else
    return (out << nonce.raw_token());
}

bool IsSameOriginWith(const GURL& a, const GURL& b) {
  return Origin::Create(a).IsSameOriginWith(Origin::Create(b));
}

Origin::Nonce::Nonce() = default;
Origin::Nonce::Nonce(const base::UnguessableToken& token) : token_(token) {
  CHECK(!token_.is_empty());
}

const base::UnguessableToken& Origin::Nonce::token() const {
  // Inspecting the value of a nonce triggers lazy-generation.
  // TODO(dcheng): UnguessableToken::is_empty should go away -- what sentinel
  // value to use instead?
  if (token_.is_empty())
    token_ = base::UnguessableToken::Create();
  return token_;
}

const base::UnguessableToken& Origin::Nonce::raw_token() const {
  return token_;
}

// Copying a Nonce triggers lazy-generation of the token.
Origin::Nonce::Nonce(const Origin::Nonce& other) : token_(other.token()) {}

Origin::Nonce& Origin::Nonce::operator=(const Origin::Nonce& other) {
  // Copying a Nonce triggers lazy-generation of the token.
  token_ = other.token();
  return *this;
}

// Moving a nonce does NOT trigger lazy-generation of the token.
Origin::Nonce::Nonce(Origin::Nonce&& other) noexcept : token_(other.token_) {
  other.token_ = base::UnguessableToken();  // Reset |other|.
}

Origin::Nonce& Origin::Nonce::operator=(Origin::Nonce&& other) noexcept {
  token_ = other.token_;
  other.token_ = base::UnguessableToken();  // Reset |other|.
  return *this;
}

bool Origin::Nonce::operator<(const Origin::Nonce& other) const {
  // When comparing, lazy-generation is required of both tokens, so that an
  // ordering is established.
  return token() < other.token();
}

bool Origin::Nonce::operator==(const Origin::Nonce& other) const {
  // Equality testing doesn't actually require that the tokens be generated.
  // If the tokens are both zero, equality only holds if they're the same
  // object.
  return (other.token_ == token_) && !(token_.is_empty() && (&other != this));
}

bool Origin::Nonce::operator!=(const Origin::Nonce& other) const {
  return !(*this == other);
}

namespace debug {

ScopedOriginCrashKey::ScopedOriginCrashKey(
    base::debug::CrashKeyString* crash_key,
    const url::Origin* value)
    : scoped_string_value_(
          crash_key,
          value ? value->GetDebugString(false /* include_nonce */)
                : "nullptr") {}

ScopedOriginCrashKey::~ScopedOriginCrashKey() = default;

}  // namespace debug

}  // namespace url
