// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
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
           base::ContainsValue(GetLocalSchemes(), url.scheme_piece()));
  }

  if (tuple.IsInvalid())
    return Origin();
  return Origin(std::move(tuple));
}

Origin Origin::Resolve(const GURL& url, const Origin& base_origin) {
  if (url.IsAboutBlank())
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

bool Origin::DomainIs(base::StringPiece canonical_domain) const {
  return !opaque() && url::DomainIs(tuple_.host(), canonical_domain);
}

bool Origin::operator<(const Origin& other) const {
  return std::tie(tuple_, nonce_) < std::tie(other.tuple_, other.nonce_);
}

Origin Origin::DeriveNewOpaqueOrigin() const {
  return Origin(Nonce(), tuple_);
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
  out << origin.Serialize();

  if (origin.opaque()) {
    // For opaque origins, log the nonce and precursor as well. Without this,
    // EXPECT_EQ failures between opaque origins are nearly impossible to
    // understand.
    out << " [internally: " << *origin.nonce_;
    if (origin.tuple_.IsInvalid())
      out << " anonymous";
    else
      out << " derived from " << origin.tuple_;
    out << "]";
  } else if (origin.scheme() == kFileScheme) {
    out << " [internally: " << origin.tuple_ << "]";
  }
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
