// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_constants.h"
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
    DCHECK(tuple.IsInvalid() || url.IsStandard() ||
           base::Contains(GetLocalSchemes(), url.scheme_piece()) ||
           AllowNonStandardSchemesForAndroidWebView());
  }

  if (tuple.IsInvalid())
    return Origin();
  return Origin(std::move(tuple));
}

Origin Origin::Resolve(const GURL& url, const Origin& base_origin) {
  if (url.SchemeIs(kAboutScheme))
    return base_origin;
  Origin result = Origin::Create(url);
  if (!result.opaque())
    return result;
  return base_origin.DeriveNewOpaqueOrigin();
}

Origin::Origin(const Origin& other) = default;
Origin& Origin::operator=(const Origin& other) = default;
Origin::Origin(Origin&& other) = default;
Origin& Origin::operator=(Origin&& other) = default;
Origin::~Origin() = default;

// static
base::Optional<Origin> Origin::UnsafelyCreateTupleOriginWithoutNormalization(
    base::StringPiece scheme,
    base::StringPiece host,
    uint16_t port) {
  SchemeHostPort tuple(scheme.as_string(), host.as_string(), port,
                       SchemeHostPort::CHECK_CANONICALIZATION);
  if (tuple.IsInvalid())
    return base::nullopt;
  return Origin(std::move(tuple));
}

// static
base::Optional<Origin> Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
    base::StringPiece precursor_scheme,
    base::StringPiece precursor_host,
    uint16_t precursor_port,
    const Origin::Nonce& nonce) {
  SchemeHostPort precursor(precursor_scheme.as_string(),
                           precursor_host.as_string(), precursor_port,
                           SchemeHostPort::CHECK_CANONICALIZATION);
  // For opaque origins, it is okay for the SchemeHostPort to be invalid;
  // however, this should only arise when the arguments indicate the
  // canonical representation of the invalid SchemeHostPort.
  if (precursor.IsInvalid() &&
      !(precursor_scheme.empty() && precursor_host.empty() &&
        precursor_port == 0)) {
    return base::nullopt;
  }
  return Origin(std::move(nonce), std::move(precursor));
}

// static
Origin Origin::CreateFromNormalizedTuple(std::string scheme,
                                         std::string host,
                                         uint16_t port) {
  SchemeHostPort tuple(std::move(scheme), std::move(host), port,
                       SchemeHostPort::ALREADY_CANONICALIZED);
  if (tuple.IsInvalid())
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

base::Optional<base::UnguessableToken> Origin::GetNonceForSerialization()
    const {
  // TODO(nasko): Consider not making a copy here, but return a reference to
  // the nonce.
  return nonce_ ? base::make_optional(nonce_->token()) : base::nullopt;
}

bool Origin::IsSameOriginWith(const Origin& other) const {
  // scheme/host/port must match, even for opaque origins where |tuple_| holds
  // the precursor origin.
  return std::tie(tuple_, nonce_) == std::tie(other.tuple_, other.nonce_);
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
    if (tuple_.IsInvalid())
      return true;
  }

  SchemeHostPort url_tuple;

  // Optimization for the common, success case: Scheme/Host/Port match on the
  // precursor, and the URL is standard. Opaqueness does not matter as a tuple
  // origin can always create an opaque tuple origin.
  if (url.IsStandard()) {
    // Note: if extra copies of the scheme and host are undesirable, this check
    // can be implemented using StringPiece comparisons, but it has to account
    // explicitly checks on port numbers.
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
    if (tuple_.IsInvalid())
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
  if (tuple_.IsInvalid())
    return true;

  // However, when there is precursor present, the schemes must match.
  return url.scheme() == tuple_.scheme();
}

bool Origin::DomainIs(base::StringPiece canonical_domain) const {
  return !opaque() && url::DomainIs(tuple_.host(), canonical_domain);
}

bool Origin::operator<(const Origin& other) const {
  return std::tie(tuple_, nonce_) < std::tie(other.tuple_, other.nonce_);
}

Origin Origin::DeriveNewOpaqueOrigin() const {
  return Origin(Nonce(), tuple_);
}

std::string Origin::GetDebugString() const {
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
  std::string nonce = nonce_->raw_token().is_empty()
                          ? std::string("nonce TBD")
                          : nonce_->raw_token().ToString();

  std::string out = base::StrCat({Serialize(), " [internally: (", nonce, ")"});
  if (tuple_.IsInvalid())
    base::StrAppend(&out, {" anonymous]"});
  else
    base::StrAppend(&out, {" derived from ", tuple_.Serialize(), "]"});
  return out;
}

Origin::Origin(SchemeHostPort tuple) : tuple_(std::move(tuple)) {
  DCHECK(!opaque());
  DCHECK(!tuple_.IsInvalid());
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

Origin::Nonce::Nonce() {}
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
Origin::Nonce::Nonce(Origin::Nonce&& other) : token_(other.token_) {
  other.token_ = base::UnguessableToken();  // Reset |other|.
}

Origin::Nonce& Origin::Nonce::operator=(Origin::Nonce&& other) {
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

}  // namespace url
