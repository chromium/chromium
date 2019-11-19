// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_CACHEINVALIDATION_DEPS_GMOCK_H_
#define GOOGLE_CACHEINVALIDATION_DEPS_GMOCK_H_

#include "testing/gmock/include/gmock/gmock.h"

namespace testing {
namespace internal {

// WhenDeserializedAs and EqualsProto are utilities that aren't part of gmock.

// Implements WhenDeserializedAs<Proto>(proto_matcher).
template <class Proto>
class WhenDeserializedAsMatcher {
 public:
  typedef Matcher<const Proto&> InnerMatcher;

  explicit WhenDeserializedAsMatcher(const InnerMatcher& proto_matcher)
      : proto_matcher_(proto_matcher) {}

  virtual ~WhenDeserializedAsMatcher() {}

  // Deserializes the string as a protobuf of the same type as the expected
  // protobuf.
  Proto* Deserialize(const std::string& str) const {
    Proto* proto = new Proto;
    if (proto->ParsePartialFromString(str)) {
      return proto;
    } else {
      delete proto;
      return NULL;
    }
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "can be deserialized as a protobuf that ";
    proto_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "cannot be deserialized as a protobuf that ";
    proto_matcher_.DescribeTo(os);
  }

  bool MatchAndExplain(const std::string& arg,
                       MatchResultListener* listener) const {
    // Deserializes the string arg as a protobuf of the same type as the
    // expected protobuf.
    std::unique_ptr<const Proto> deserialized_arg(Deserialize(arg));
    // No need to explain the match result.
    return (deserialized_arg.get() != NULL) &&
        proto_matcher_.Matches(*deserialized_arg);
  }

 private:
  const InnerMatcher proto_matcher_;
};

}  // namespace internal

namespace proto {

// WhenDeserializedAs<Proto>(m) is a matcher that matches a string
// that can be deserialized as a protobuf of type Proto that matches
// m, which can be any valid protobuf matcher.
template <class Proto, class InnerMatcher>
inline PolymorphicMatcher<internal::WhenDeserializedAsMatcher<Proto> >
WhenDeserializedAs(const InnerMatcher& inner_matcher) {
  return MakePolymorphicMatcher(
      internal::WhenDeserializedAsMatcher<Proto>(
          SafeMatcherCast<const Proto&>(inner_matcher)));
}

}  // namespace proto

MATCHER_P(EqualsProto, message, "") {
  // TODO(ghc): This implementation assume protobuf serialization is
  // deterministic, which is true in practice but technically not something that
  // code is supposed to rely on.  However, it vastly simplifies the
  // implementation...
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace testing

#endif  // GOOGLE_CACHEINVALIDATION_DEPS_GMOCK_H_
