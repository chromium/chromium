// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_ORIGIN_H_
#define URL_ORIGIN_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/debug/alias.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "ipc/ipc_param_traits.h"
#include "url/scheme_host_port.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include <jni.h>

namespace base {
namespace android {
template <typename>
class ScopedJavaLocalRef;
template <typename>
class JavaRef;
}  // namespace android
}  // namespace base
#endif  // OS_ANDROID

class GURL;

namespace blink {
class SecurityOrigin;
}  // namespace blink

namespace ipc_fuzzer {
template <class T>
struct FuzzTraits;
}  // namespace ipc_fuzzer

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
struct UrlOriginAdapter;
}  // namespace mojo

namespace url {

namespace mojom {
class OriginDataView;
}  // namespace mojom

// Per https://html.spec.whatwg.org/multipage/origin.html#origin, an origin is
// either:
// - a tuple origin of (scheme, host, port) as described in RFC 6454.
// - an opaque origin with an internal value, and a memory of the tuple origin
//   from which it was derived.
//
// TL;DR: If you need to make a security-relevant decision, use 'url::Origin'.
// If you only need to extract the bits of a URL which are relevant for a
// network connection, use 'url::SchemeHostPort'.
//
// STL;SDR: If you aren't making actual network connections, use 'url::Origin'.
//
// This class ought to be used when code needs to determine if two resources
// are "same-origin", and when a canonical serialization of an origin is
// required. Note that the canonical serialization of an origin *must not* be
// used to determine if two resources are same-origin.
//
// A tuple origin, like 'SchemeHostPort', is composed of a tuple of (scheme,
// host, port), but contains a number of additional concepts which make it
// appropriate for use as a security boundary and access control mechanism
// between contexts. Two tuple origins are same-origin if the tuples are equal.
// A tuple origin may also be re-created from its serialization.
//
// An opaque origin has an internal globally unique identifier. When creating a
// new opaque origin from a URL, a fresh globally unique identifier is
// generated. However, if an opaque origin is copied or moved, the internal
// globally unique identifier is preserved. Two opaque origins are same-origin
// iff the globally unique identifiers match. Unlike tuple origins, an opaque
// origin cannot be re-created from its serialization, which is always the
// string "null".
//
// IMPORTANT: Since opaque origins always serialize as the string "null", it is
// *never* safe to use the serialization for security checks!
//
// A tuple origin and an opaque origin are never same-origin.
//
// There are a few subtleties to note:
//
// * A default constructed Origin is opaque, with no precursor origin.
//
// * Invalid and non-standard GURLs are parsed as opaque origins. This includes
//   non-hierarchical URLs like 'data:text/html,...' and 'javascript:alert(1)'.
//
// * GURLs with schemes of 'filesystem' or 'blob' parse the origin out of the
//   internals of the URL. That is, 'filesystem:https://example.com/temporary/f'
//   is parsed as ('https', 'example.com', 443).
//
// * GURLs with a 'file' scheme are tricky. They are parsed as ('file', '', 0),
//   but their behavior may differ from embedder to embedder.
//   TODO(dcheng): This behavior is not consistent with Blink's notion of file
//   URLs, which always creates an opaque origin.
//
// * The host component of an IPv6 address includes brackets, just like the URL
//   representation.
//
// Usage:
//
// * Origins are generally constructed from an already-canonicalized GURL:
//
//     GURL url("https://example.com/");
//     url::Origin origin = Origin::Create(url);
//     origin.scheme(); // "https"
//     origin.host(); // "example.com"
//     origin.port(); // 443
//     origin.opaque(); // false
//
// * To answer the question "Are |this| and |that| "same-origin" with each
//   other?", use |Origin::IsSameOriginWith|:
//
//     if (this.IsSameOriginWith(that)) {
//       // Amazingness goes here.
//     }
class COMPONENT_EXPORT(URL) Origin {
 public:
  // Creates an opaque Origin with a nonce that is different from all previously
  // existing origins.
  Origin();

  // Creates an Origin from |url|, as described at
  // https://url.spec.whatwg.org/#origin, with the following additions:
  //
  // 1. If |url| is invalid or non-standard, an opaque Origin is constructed.
  // 2. 'filesystem' URLs behave as 'blob' URLs (that is, the origin is parsed
  //    out of everything in the URL which follows the scheme).
  // 3. 'file' URLs all parse as ("file", "", 0).
  static Origin Create(const GURL& url);

  // Creates an Origin for the resource |url| as if it were requested
  // from the context of |base_origin|.  If |url| is standard
  // (in the sense that it embeds a complete origin, like http/https),
  // this returns the same value as would Create().
  //
  // If |url| is "about:blank", this returns a copy of |base_origin|.
  //
  // Otherwise, returns a new opaque origin derived from |base_origin|.
  // In this case, the resulting opaque origin will inherit the tuple
  // (or precursor tuple) of |base_origin|, but will not be same origin
  // with |base_origin|, even if |base_origin| is already opaque.
  static Origin Resolve(const GURL& url, const Origin& base_origin);

  // Copyable and movable.
  Origin(const Origin&);
  Origin& operator=(const Origin&);
  Origin(Origin&&);
  Origin& operator=(Origin&&);

  // Creates an Origin from a |scheme|, |host|, and |port|. All the parameters
  // must be valid and canonicalized. Returns nullopt if any parameter is not
  // canonical, or if all the parameters are empty.
  //
  // This constructor should be used in order to pass 'Origin' objects back and
  // forth over IPC (as transitioning through GURL would risk potentially
  // dangerous recanonicalization); other potential callers should prefer the
  // 'GURL'-based constructor.
  static base::Optional<Origin> UnsafelyCreateTupleOriginWithoutNormalization(
      base::StringPiece scheme,
      base::StringPiece host,
      uint16_t port);

  // Creates an origin without sanity checking that the host is canonicalized.
  // This should only be used when converting between already normalized types,
  // and should NOT be used for IPC. Method takes std::strings for use with move
  // operators to avoid copies.
  static Origin CreateFromNormalizedTuple(std::string scheme,
                                          std::string host,
                                          uint16_t port);

  ~Origin();

  // For opaque origins, these return ("", "", 0).
  const std::string& scheme() const {
    return !opaque() ? tuple_.scheme() : base::EmptyString();
  }
  const std::string& host() const {
    return !opaque() ? tuple_.host() : base::EmptyString();
  }
  uint16_t port() const { return !opaque() ? tuple_.port() : 0; }

  bool opaque() const { return nonce_.has_value(); }

  // An ASCII serialization of the Origin as per Section 6.2 of RFC 6454, with
  // the addition that all Origins with a 'file' scheme serialize to "file://".
  std::string Serialize() const;

  // Two non-opaque Origins are "same-origin" if their schemes, hosts, and ports
  // are exact matches. Two opaque origins are same-origin only if their
  // internal nonce values match. A non-opaque origin is never same-origin with
  // an opaque origin.
  bool IsSameOriginWith(const Origin& other) const;
  bool operator==(const Origin& other) const { return IsSameOriginWith(other); }
  bool operator!=(const Origin& other) const {
    return !IsSameOriginWith(other);
  }

  // This method returns true for any |url| which if navigated to could result
  // in an origin compatible with |this|.
  bool CanBeDerivedFrom(const GURL& url) const;

  // Get the scheme, host, and port from which this origin derives. For
  // a tuple Origin, this gives the same values as calling scheme(), host()
  // and port(). For an opaque Origin that was created by calling
  // Origin::DeriveNewOpaqueOrigin() on a precursor or Origin::Resolve(),
  // this returns the tuple inherited from the precursor.
  //
  // If this Origin is opaque and was created via the default constructor or
  // Origin::Create(), the precursor origin is unknown.
  //
  // Use with great caution: opaque origins should generally not inherit
  // privileges from the origins they derive from. However, in some cases
  // (such as restrictions on process placement, or determining the http lock
  // icon) this information may be relevant to ensure that entering an
  // opaque origin does not grant privileges initially denied to the original
  // non-opaque origin.
  //
  // This method has a deliberately obnoxious name to prompt caution in its use.
  const SchemeHostPort& GetTupleOrPrecursorTupleIfOpaque() const {
    return tuple_;
  }

  // Efficiently returns what GURL(Serialize()) would without re-parsing the
  // URL. This can be used for the (rare) times a GURL representation is needed
  // for an Origin.
  // Note: The returned URL will not necessarily be serialized to the same value
  // as the Origin would. The GURL will have an added "/" path for Origins with
  // valid SchemeHostPorts and file Origins.
  //
  // Try not to use this method under normal circumstances, as it loses type
  // information. Downstream consumers can mistake the returned GURL with a full
  // URL (e.g. with a path component).
  GURL GetURL() const;

  // Same as GURL::DomainIs. If |this| origin is opaque, then returns false.
  bool DomainIs(base::StringPiece canonical_domain) const;

  // Allows Origin to be used as a key in STL (for example, a std::set or
  // std::map).
  bool operator<(const Origin& other) const;

  // Creates a new opaque origin that is guaranteed to be cross-origin to all
  // currently existing origins. An origin created by this method retains its
  // identity across copies. Copies are guaranteed to be same-origin to each
  // other, e.g.
  //
  //   url::Origin page = Origin::Create(GURL("http://example.com"))
  //   url::Origin a = page.DeriveNewOpaqueOrigin();
  //   url::Origin b = page.DeriveNewOpaqueOrigin();
  //   url::Origin c = a;
  //   url::Origin d = b;
  //
  // |a| and |c| are same-origin, since |c| was copied from |a|. |b| and |d| are
  // same-origin as well, since |d| was copied from |b|. All other combinations
  // of origins are considered cross-origin, e.g. |a| is cross-origin to |b| and
  // |d|, |b| is cross-origin to |a| and |c|, |c| is cross-origin to |b| and
  // |d|, and |d| is cross-origin to |a| and |c|.
  Origin DeriveNewOpaqueOrigin() const;

  // Creates a string representation of the object that can be used for logging
  // and debugging. It serializes the internal state, such as the nonce value
  // and precursor information.
  std::string GetDebugString() const;

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject() const;
  static Origin FromJavaObject(
      const base::android::JavaRef<jobject>& java_origin);
#endif  // OS_ANDROID

 private:
  friend class blink::SecurityOrigin;
  friend class OriginTest;
  friend struct mojo::UrlOriginAdapter;
  friend struct ipc_fuzzer::FuzzTraits<Origin>;
  friend struct mojo::StructTraits<url::mojom::OriginDataView, url::Origin>;
  friend IPC::ParamTraits<url::Origin>;
  friend COMPONENT_EXPORT(URL) std::ostream& operator<<(std::ostream& out,
                                                        const Origin& origin);

  // Origin::Nonce is a wrapper around base::UnguessableToken that generates
  // the random value only when the value is first accessed. The lazy generation
  // allows Origin to be default-constructed quickly, without spending time
  // in random number generation.
  //
  // TODO(nick): Should this optimization move into UnguessableToken, once it no
  // longer treats the Null case specially?
  class COMPONENT_EXPORT(URL) Nonce {
   public:
    // Creates a nonce to hold a newly-generated UnguessableToken. The actual
    // token value will be generated lazily.
    Nonce();

    // Creates a nonce to hold an already-generated UnguessableToken value. This
    // constructor should only be used for IPC serialization and testing --
    // regular code should never need to touch the UnguessableTokens directly,
    // and the default constructor is faster.
    explicit Nonce(const base::UnguessableToken& token);

    // Accessor, which lazily initializes the underlying |token_| member.
    const base::UnguessableToken& token() const;

    // Do not use in cases where lazy initialization is expected! This
    // accessor does not initialize the |token_| member.
    const base::UnguessableToken& raw_token() const;

    // Copyable and movable. Copying a Nonce triggers lazy-initialization,
    // moving it does not.
    Nonce(const Nonce&);
    Nonce& operator=(const Nonce&);
    Nonce(Nonce&&);
    Nonce& operator=(Nonce&&);

    // Note that operator<, used by maps type containers, will trigger |token_|
    // lazy-initialization. Equality comparisons do not.
    bool operator<(const Nonce& other) const;
    bool operator==(const Nonce& other) const;
    bool operator!=(const Nonce& other) const;

   private:
    friend class OriginTest;

    // mutable to support lazy generation.
    mutable base::UnguessableToken token_;
  };

  // This needs to be friended within Origin as well, since Nonce is a private
  // nested class of Origin.
  friend COMPONENT_EXPORT(URL) std::ostream& operator<<(std::ostream& out,
                                                        const Nonce& nonce);

  // Creates an origin without sanity checking that the host is canonicalized.
  // This should only be used when converting between already normalized types,
  // and should NOT be used for IPC. Method takes std::strings for use with move
  // operators to avoid copies.
  static Origin CreateOpaqueFromNormalizedPrecursorTuple(
      std::string precursor_scheme,
      std::string precursor_host,
      uint16_t precursor_port,
      const Nonce& nonce);

  // Creates an opaque Origin with the identity given by |nonce|, and an
  // optional precursor origin given by |precursor_scheme|, |precursor_host| and
  // |precursor_port|. Returns nullopt if any parameter is not canonical. When
  // the precursor is unknown, the precursor parameters should be ("", "", 0).
  //
  // This factory method should be used in order to pass opaque Origin objects
  // back and forth over IPC (as transitioning through GURL would risk
  // potentially dangerous recanonicalization).
  static base::Optional<Origin> UnsafelyCreateOpaqueOriginWithoutNormalization(
      base::StringPiece precursor_scheme,
      base::StringPiece precursor_host,
      uint16_t precursor_port,
      const Nonce& nonce);

  // Constructs a non-opaque tuple origin. |tuple| must be valid.
  explicit Origin(SchemeHostPort tuple);

  // Constructs an opaque origin derived from the |precursor| tuple, with the
  // given |nonce|.
  Origin(const Nonce& nonce, SchemeHostPort precursor);

  // Get the nonce associated with this origin, if it is opaque. This should be
  // used only when trying to send an Origin across an IPC pipe.
  base::Optional<base::UnguessableToken> GetNonceForSerialization() const;

  // The tuple is used for both tuple origins (e.g. https://example.com:80), as
  // well as for opaque origins, where it tracks the tuple origin from which
  // the opaque origin was initially derived (we call this the "precursor"
  // origin).
  SchemeHostPort tuple_;

  // The nonce is used for maintaining identity of an opaque origin. This
  // nonce is preserved when an opaque origin is copied or moved. An Origin
  // is considered opaque if and only if |nonce_| holds a value.
  base::Optional<Nonce> nonce_;
};

// Pretty-printers for logging. These expose the internal state of the nonce.
COMPONENT_EXPORT(URL)
std::ostream& operator<<(std::ostream& out, const Origin& origin);
COMPONENT_EXPORT(URL)
std::ostream& operator<<(std::ostream& out, const Origin::Nonce& origin);

COMPONENT_EXPORT(URL) bool IsSameOriginWith(const GURL& a, const GURL& b);

// DEBUG_ALIAS_FOR_ORIGIN(var_name, origin) copies |origin| into a new
// stack-allocated variable named |<var_name>|.  This helps ensure that the
// value of |origin| gets preserved in crash dumps.
#define DEBUG_ALIAS_FOR_ORIGIN(var_name, origin) \
  DEBUG_ALIAS_FOR_CSTR(var_name, (origin).Serialize().c_str(), 128)

}  // namespace url

#endif  // URL_ORIGIN_H_
