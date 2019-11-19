// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/scheme_host_port.h"

#include <stdint.h>
#include <string.h>

#include <tuple>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace url {

namespace {

bool IsCanonicalHost(const base::StringPiece& host) {
  std::string canon_host;

  // Try to canonicalize the host (copy/pasted from net/base. :( ).
  const Component raw_host_component(0,
                                     base::checked_cast<int>(host.length()));
  StdStringCanonOutput canon_host_output(&canon_host);
  CanonHostInfo host_info;
  CanonicalizeHostVerbose(host.data(), raw_host_component,
                          &canon_host_output, &host_info);

  if (host_info.out_host.is_nonempty() &&
      host_info.family != CanonHostInfo::BROKEN) {
    // Success!  Assert that there's no extra garbage.
    canon_host_output.Complete();
    DCHECK_EQ(host_info.out_host.len, static_cast<int>(canon_host.length()));
  } else {
    // Empty host, or canonicalization failed.
    canon_host.clear();
  }

  return host == canon_host;
}

bool IsValidInput(const base::StringPiece& scheme,
                  const base::StringPiece& host,
                  uint16_t port,
                  SchemeHostPort::ConstructPolicy policy) {
  // Empty schemes are never valid.
  if (scheme.empty())
    return false;

  SchemeType scheme_type = SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION;
  bool is_standard = GetStandardSchemeType(
      scheme.data(),
      Component(0, base::checked_cast<int>(scheme.length())),
      &scheme_type);
  if (!is_standard) {
    // To be consistent with blink, local non-standard schemes are currently
    // allowed to be tuple origins. Nonstandard schemes don't have hostnames,
    // so their tuple is just ("protocol", "", 0).
    //
    // TODO: Migrate "content:" and "externalfile:" to be standard schemes, and
    // remove this local scheme exception.
    if (base::Contains(GetLocalSchemes(), scheme) && host.empty() && port == 0)
      return true;

    // Otherwise, allow non-standard schemes only if the Android WebView
    // workaround is enabled.
    return AllowNonStandardSchemesForAndroidWebView();
  }

  switch (scheme_type) {
    case SCHEME_WITH_HOST_AND_PORT:
    case SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION:
      // A URL with |scheme| is required to have the host and port (may be
      // omitted in a serialization if it's the same as the default value).
      // Return an invalid instance if either of them is not given.
      if (host.empty() || port == 0)
        return false;

      // Don't do an expensive canonicalization if the host is already
      // canonicalized.
      DCHECK(policy == SchemeHostPort::CHECK_CANONICALIZATION ||
             IsCanonicalHost(host));
      if (policy == SchemeHostPort::CHECK_CANONICALIZATION &&
          !IsCanonicalHost(host)) {
        return false;
      }

      return true;

    case SCHEME_WITH_HOST:
      if (port != 0) {
        // Return an invalid object if a URL with the scheme never represents
        // the port data but the given |port| is non-zero.
        return false;
      }

      // Don't do an expensive canonicalization if the host is already
      // canonicalized.
      DCHECK(policy == SchemeHostPort::CHECK_CANONICALIZATION ||
             IsCanonicalHost(host));
      if (policy == SchemeHostPort::CHECK_CANONICALIZATION &&
          !IsCanonicalHost(host)) {
        return false;
      }

      return true;

    case SCHEME_WITHOUT_AUTHORITY:
      return false;

    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

SchemeHostPort::SchemeHostPort() : port_(0) {
}

SchemeHostPort::SchemeHostPort(std::string scheme,
                               std::string host,
                               uint16_t port,
                               ConstructPolicy policy)
    : port_(0) {
  if (!IsValidInput(scheme, host, port, policy)) {
    DCHECK(IsInvalid());
    return;
  }

  scheme_ = std::move(scheme);
  host_ = std::move(host);
  port_ = port;
  DCHECK(!IsInvalid()) << "Scheme: " << scheme_ << " Host: " << host_
                       << " Port: " << port;
}

SchemeHostPort::SchemeHostPort(base::StringPiece scheme,
                               base::StringPiece host,
                               uint16_t port)
    : SchemeHostPort(scheme.as_string(),
                     host.as_string(),
                     port,
                     ConstructPolicy::CHECK_CANONICALIZATION) {}

SchemeHostPort::SchemeHostPort(const GURL& url) : port_(0) {
  if (!url.is_valid())
    return;

  base::StringPiece scheme = url.scheme_piece();
  base::StringPiece host = url.host_piece();

  // A valid GURL never returns PORT_INVALID.
  int port = url.EffectiveIntPort();
  if (port == PORT_UNSPECIFIED) {
    port = 0;
  } else {
    DCHECK_GE(port, 0);
    DCHECK_LE(port, 65535);
  }

  if (!IsValidInput(scheme, host, port, ALREADY_CANONICALIZED))
    return;

  scheme.CopyToString(&scheme_);
  host.CopyToString(&host_);
  port_ = port;
}

SchemeHostPort::~SchemeHostPort() = default;

bool SchemeHostPort::IsInvalid() const {
  // It suffices to just check |scheme_| for emptiness; the other fields are
  // never present without it.
  DCHECK(!scheme_.empty() || host_.empty());
  DCHECK(!scheme_.empty() || port_ == 0);
  return scheme_.empty();
}

std::string SchemeHostPort::Serialize() const {
  // Null checking for |parsed| in SerializeInternal is probably slower than
  // just filling it in and discarding it here.
  url::Parsed parsed;
  return SerializeInternal(&parsed);
}

GURL SchemeHostPort::GetURL() const {
  url::Parsed parsed;
  std::string serialized = SerializeInternal(&parsed);

  if (IsInvalid())
    return GURL(std::move(serialized), parsed, false);

  // SchemeHostPort does not have enough information to determine if an empty
  // host is valid or not for the given scheme. Force re-parsing.
  DCHECK(!scheme_.empty());
  if (host_.empty())
    return GURL(serialized);

  // If the serialized string is passed to GURL for parsing, it will append an
  // empty path "/". Add that here. Note: per RFC 6454 we cannot do this for
  // normal Origin serialization.
  DCHECK(!parsed.path.is_valid());
  parsed.path = Component(serialized.length(), 1);
  serialized.append("/");
  return GURL(std::move(serialized), parsed, true);
}

bool SchemeHostPort::operator<(const SchemeHostPort& other) const {
  return std::tie(port_, scheme_, host_) <
         std::tie(other.port_, other.scheme_, other.host_);
}

std::string SchemeHostPort::SerializeInternal(url::Parsed* parsed) const {
  std::string result;
  if (IsInvalid())
    return result;

  // Reserve enough space for the "normal" case of scheme://host/.
  result.reserve(scheme_.size() + host_.size() + 4);

  if (!scheme_.empty()) {
    parsed->scheme = Component(0, scheme_.length());
    result.append(scheme_);
  }

  result.append(kStandardSchemeSeparator);

  if (!host_.empty()) {
    parsed->host = Component(result.length(), host_.length());
    result.append(host_);
  }

  if (port_ == 0)
    return result;

  // Omit the port component if the port matches with the default port
  // defined for the scheme, if any.
  int default_port = DefaultPortForScheme(scheme_.data(),
                                          static_cast<int>(scheme_.length()));
  if (default_port == PORT_UNSPECIFIED)
    return result;
  if (port_ != default_port) {
    result.push_back(':');
    std::string port(base::NumberToString(port_));
    parsed->port = Component(result.length(), port.length());
    result.append(std::move(port));
  }

  return result;
}

std::ostream& operator<<(std::ostream& out,
                         const SchemeHostPort& scheme_host_port) {
  return out << scheme_host_port.Serialize();
}

}  // namespace url
