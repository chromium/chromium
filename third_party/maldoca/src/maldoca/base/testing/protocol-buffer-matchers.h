// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// gMock matchers used to validate protocol buffer arguments.

// WHAT THIS IS
// ============
//
// This library defines the following matchers in the ::maldoca::testing
// namespace:
//
//   EqualsProto(pb)              The argument equals pb.
//   EqualsInitializedProto(pb)   The argument is initialized and equals pb.
//   EquivToProto(pb)             The argument is equivalent to pb.
//   EquivToInitializedProto(pb)  The argument is initialized and equivalent
//                                to pb.
//   IsInitializedProto()         The argument is an initialized protobuf.
//
// where:
//
//   - pb can be either a protobuf value or a human-readable string
//     representation of it.
//   - When pb is a string, the matcher can optionally accept a
//     template argument for the type of the protobuf,
//     e.g. EqualsProto<Foo>("foo: 1").
//   - "equals" is defined as the argument's Equals(pb) method returns true.
//   - "equivalent to" is defined as the argument's Equivalent(pb) method
//     returns true.
//   - "initialized" means that the argument's IsInitialized() method returns
//     true.
//
// These matchers can match either a protobuf value or a pointer to
// it.  They make a copy of pb, and thus can out-live pb.  When the
// match fails, the matchers print a detailed message (the value of
// the actual protobuf, the value of the expected protobuf, and which
// fields are different).
//
// This library also defines the following matcher transformer
// functions in the ::maldoca::testing::proto namespace:
//
//   Approximately(m, margin, fraction)
//                     The same as m, except that it compares
//                     floating-point fields approximately (using
//                     google::protobuf::util::MessageDifferencer's APPROXIMATE
//                     comparison option).  m can be any of the
//                     Equals* and EquivTo* protobuf matchers above. If margin
//                     is specified, floats and doubles will be considered
//                     approximately equal if they are within that margin, i.e.
//                     abs(expected - actual) <= margin. If fraction is
//                     specified, floats and doubles will be considered
//                     approximately equal if they are within a fraction of
//                     their magnitude, i.e. abs(expected - actual) <=
//                     fraction * max(abs(expected), abs(actual)). Two fields
//                     will be considered equal if they're within the fraction
//                     _or_ within the margin, so omitting or setting the
//                     fraction to 0.0 will only check against the margin.
//                     Similarly, setting the margin to 0.0 will only check
//                     using the fraction. If margin and fraction are omitted,
//                     MathLimits<T>::kStdError for that type (T=float or
//                     T=double) is used for both the margin and fraction.
//   TreatingNaNsAsEqual(m)
//                     The same as m, except that treats floating-point fields
//                     that are NaN as equal. m can be any of the Equals* and
//                     EquivTo* protobuf matchers above.
//   IgnoringFields(fields, m)
//                     The same as m, except the specified fields will be
//                     ignored when matching (using
//                     google::protobuf::util::MessageDifferencer::IgnoreField).
//                     fields is represented as a container or an initializer
//                     list of strings and each element is specified by their
//                     fully qualified names, i.e., the names corresponding to
//                     FieldDescriptor.full_name().  m can be
//                     any of the Equals* and EquivTo* protobuf matchers above.
//                     It can also be any of the transformer matchers listed
//                     here (e.g. Approximately, TreatingNaNsAsEqual) as long as
//                     the intent of the each concatenated matcher is mutually
//                     exclusive (e.g. using IgnoringFields in conjunction with
//                     Partially can have different results depending on whether
//                     the fields specified in IgnoringFields is part of the
//                     fields covered by Partially).
//   IgnoringFieldPaths(field_paths, m)
//                     The same as m, except the specified fields will be
//                     ignored when matching. field_paths is represented as a
//                     container or an initializer list of strings and
//                     each element is specified by their path relative to the
//                     proto being matched by m. Paths can contain indices
//                     and/or extensions. Examples:
//                       Ignores field singular_field/repeated_field:
//                         singular_field
//                         repeated_field
//                       Ignores just the third repeated_field instance:
//                         repeated_field[2]
//                       Ignores some_field in singular_nested/repeated_nested:
//                         singular_nested.some_field
//                         repeated_nested.some_field
//                       Ignores some_field in instance 2 of repeated_nested:
//                         repeated_nested[2].some_field
//                       Ignores extension SomeExtension.msg of repeated_nested:
//                         repeated_nested.(package.SomeExtension.msg)
//                       Ignores subfield of extension:
//                         repeated_nested.(package.SomeExtension.msg).subfield
//                     The same restrictions as for IgnoringFields apply.
//   IgnoringRepeatedFieldOrdering(m)
//                     The same as m, except that it ignores the relative
//                     ordering of elements within each repeated field in m.
//                     See
//                     google::protobuf::util::MessageDifferencer::TreatAsSet()
//                     for more details.
//   Partially(m)
//                     The same as m, except that only fields present in
//                     the expected protobuf are considered (using
//                     google::protobuf::util::MessageDifferencer's PARTIAL
//                     comparison option).   m can be any of the
//                     Equals* and EquivTo* protobuf matchers above.
//   WhenDeserialized(typed_pb_matcher)
//                     The string argument is a serialization of a
//                     protobuf that matches typed_pb_matcher.
//                     typed_pb_matcher can be an Equals* or EquivTo*
//                     protobuf matcher (possibly with Approximately()
//                     or Partially() modifiers) where the type of the
//                     protobuf is known at run time (e.g. it cannot
//                     be EqualsProto("...") as it's unclear what type
//                     the string represents).
//   WhenDeserializedAs<PB>(pb_matcher)
//                     Like WhenDeserialized(), except that the type
//                     of the deserialized protobuf must be PB.  Since
//                     the protobuf type is known, pb_matcher can be *any*
//                     valid protobuf matcher, including EqualsProto("...").
//
// Approximately(), TreatingNaNsAsEqual(), Partially(), IgnoringFields(), and
// IgnoringRepeatedFieldOrdering() can be combined (nested)
// and the composition order is irrelevant:
//
//   Approximately(Partially(EquivToProto(pb)))
// and
//   Partially(Approximately(EquivToProto(pb)))
// are the same thing.
//
// EXAMPLES
// ========
//
//   using ::maldoca::testing::EqualsProto;
//   using ::maldoca::testing::EquivToProto;
//   using ::maldoca::testing::proto::Approximately;
//   using ::maldoca::testing::proto::Partially;
//   using ::maldoca::testing::proto::WhenDeserialized;
//
//   // my_pb.Equals(expected_pb).
//   EXPECT_THAT(my_pb, EqualsProto(expected_pb));
//
//   // my_pb is equivalent to a protobuf whose foo field is 1 and
//   // whose bar field is "x".
//   EXPECT_THAT(my_pb, EquivToProto("foo: 1 "
//                                   "bar: 'x'"));
//
//   // my_pb is equal to expected_pb, comparing all floating-point
//   // fields approximately.
//   EXPECT_THAT(my_pb, Approximately(EqualsProto(expected_pb)));
//
//   // my_pb is equivalent to expected_pb.  A field is ignored in the
//   // comparison if it's present in my_pb but not in expected_pb.
//   EXPECT_THAT(my_pb, Partially(EquivToProto(expected_pb)));
//
//   string data;
//   my_pb.SerializeToString(&data);
//   // data can be deserialized to a protobuf that equals expected_pb.
//   EXPECT_THAT(data, WhenDeserialized(EqualsProto(expected_pb)));
//   // The following line doesn't compile, as the matcher doesn't know
//   // the type of the protobuf.
//   // EXPECT_THAT(data, WhenDeserialized(EqualsProto("foo: 1")));

#ifndef MALDOCA_BASE_TESTING_PROTOCOL_BUFFER_MATCHERS_H_
#define MALDOCA_BASE_TESTING_PROTOCOL_BUFFER_MATCHERS_H_

#include <initializer_list>
#include <iostream>  // NOLINT
#include <memory>
#include <sstream>  // NOLINT
#include <string>   // NOLINT
#include <vector>   // NOLINT

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/field_comparator.h"
#include "google/protobuf/util/message_differencer.h"
#include "maldoca/base/logging.h"

namespace maldoca {
namespace testing {
namespace internal {

// Utilities.

// How to compare two fields (equal vs. equivalent).
typedef google::protobuf::util::MessageDifferencer::MessageFieldComparison
    ProtoFieldComparison;

// How to compare two floating-points (exact vs. approximate).
typedef google::protobuf::util::DefaultFieldComparator::FloatComparison
    ProtoFloatComparison;

// How to compare repeated fields (whether the order of elements matters).
typedef google::protobuf::util::MessageDifferencer::RepeatedFieldComparison
    RepeatedFieldComparison;

// Whether to compare all fields (full) or only fields present in the
// expected protobuf (partial).
typedef google::protobuf::util::MessageDifferencer::Scope ProtoComparisonScope;

const ProtoFieldComparison kProtoEqual =
    google::protobuf::util::MessageDifferencer::EQUAL;
const ProtoFieldComparison kProtoEquiv =
    google::protobuf::util::MessageDifferencer::EQUIVALENT;
const ProtoFloatComparison kProtoExact =
    google::protobuf::util::DefaultFieldComparator::EXACT;
const ProtoFloatComparison kProtoApproximate =
    google::protobuf::util::DefaultFieldComparator::APPROXIMATE;
const RepeatedFieldComparison kProtoCompareRepeatedFieldsRespectOrdering =
    google::protobuf::util::MessageDifferencer::AS_LIST;
const RepeatedFieldComparison kProtoCompareRepeatedFieldsIgnoringOrdering =
    google::protobuf::util::MessageDifferencer::AS_SET;
const ProtoComparisonScope kProtoFull =
    google::protobuf::util::MessageDifferencer::FULL;
const ProtoComparisonScope kProtoPartial =
    google::protobuf::util::MessageDifferencer::PARTIAL;

// Options for comparing two protobufs.
struct ProtoComparison {
  ProtoComparison()
      : field_comp(kProtoEqual),
        float_comp(kProtoExact),
        treating_nan_as_equal(false),
        has_custom_margin(false),
        has_custom_fraction(false),
        repeated_field_comp(kProtoCompareRepeatedFieldsRespectOrdering),
        scope(kProtoFull),
        float_margin(0.0),
        float_fraction(0.0) {}

  ProtoFieldComparison field_comp;
  ProtoFloatComparison float_comp;
  bool treating_nan_as_equal;
  bool has_custom_margin;    // only used when float_comp = APPROXIMATE
  bool has_custom_fraction;  // only used when float_comp = APPROXIMATE
  RepeatedFieldComparison repeated_field_comp;
  ProtoComparisonScope scope;
  double float_margin;    // only used when has_custom_margin is set.
  double float_fraction;  // only used when has_custom_fraction is set.
  std::vector<std::string> ignore_fields;
  std::vector<std::string> ignore_field_paths;
};

// Whether the protobuf must be initialized.
const bool kMustBeInitialized = true;
const bool kMayBeUninitialized = false;

// Parses the TextFormat representation of a protobuf, allowing required fields
// to be missing.  Returns true iff successful.
bool ParsePartialFromAscii(const std::string& pb_ascii,
                           google::protobuf::Message* proto,
                           std::string* error_text);

// Returns a protobuf of type Proto by parsing the given TextFormat
// representation of it.  Required fields can be missing, in which case the
// returned protobuf will not be fully initialized.
template <class Proto>
Proto MakePartialProtoFromAscii(const std::string& str) {
  Proto proto;
  std::string error_text;
  CHECK(ParsePartialFromAscii(str, &proto, &error_text))
      << "Failed to parse \"" << str << "\" as a "
      << proto.GetDescriptor()->full_name() << ":\n"
      << error_text;
  return proto;
}

// Returns true iff p and q can be compared (i.e. have the same descriptor).
bool ProtoComparable(const google::protobuf::Message& p,
                     const google::protobuf::Message& q);

// Returns true iff actual and expected are comparable and match.  The
// comp argument specifies how the two are compared.
bool ProtoCompare(const ProtoComparison& comp,
                  const google::protobuf::Message& actual,
                  const google::protobuf::Message& expected);

// Overload for ProtoCompare where the expected message is specified as a text
// proto.  If the text cannot be parsed as a message of the same type as the
// actual message, a CHECK failure will cause the test to fail and no subsequent
// tests will be run.
template <typename Proto>
inline bool ProtoCompare(const ProtoComparison& comp, const Proto& actual,
                         const std::string& expected) {
  return ProtoCompare(comp, actual, MakePartialProtoFromAscii<Proto>(expected));
}

// Describes the types of the expected and the actual protocol buffer.
std::string DescribeTypes(const google::protobuf::Message& expected,
                          const google::protobuf::Message& actual);

// Prints the protocol buffer pointed to by proto.
std::string PrintProtoPointee(const google::protobuf::Message* proto);

// Describes the differences between the two protocol buffers.
std::string DescribeDiff(const ProtoComparison& comp,
                         const google::protobuf::Message& actual,
                         const google::protobuf::Message& expected);

// Common code for implementing EqualsProto, EquivToProto,
// EqualsInitializedProto, and EquivToInitializedProto.
class ProtoMatcherBase {
 public:
  ProtoMatcherBase(
      bool must_be_initialized,     // Must the argument be fully initialized?
      const ProtoComparison& comp)  // How to compare the two protobufs.
      : must_be_initialized_(must_be_initialized), comp_(new auto(comp)) {}

  ProtoMatcherBase(const ProtoMatcherBase& other)
      : must_be_initialized_(other.must_be_initialized_),
        comp_(new auto(*other.comp_)) {}

  ProtoMatcherBase(ProtoMatcherBase&& other) = default;

  virtual ~ProtoMatcherBase() {}

  // Prints the expected protocol buffer.
  virtual void PrintExpectedTo(::std::ostream* os) const = 0;

  // Returns the expected value as a protobuf object; if the object
  // cannot be created (e.g. in ProtoStringMatcher), explains why to
  // 'listener' and returns NULL.  The caller must call
  // DeleteExpectedProto() on the returned value later.
  virtual const google::protobuf::Message* CreateExpectedProto(
      const google::protobuf::Message& arg,  // For determining the type of the
                                             // expected protobuf.
      ::testing::MatchResultListener* listener) const = 0;

  // Deletes the given expected protobuf, which must be obtained from
  // a call to CreateExpectedProto() earlier.
  virtual void DeleteExpectedProto(
      const google::protobuf::Message* expected) const = 0;

  // Makes this matcher compare floating-points approximately.
  void SetCompareApproximately() { comp_->float_comp = kProtoApproximate; }

  // Makes this matcher treating NaNs as equal when comparing floating-points.
  void SetCompareTreatingNaNsAsEqual() { comp_->treating_nan_as_equal = true; }

  // Makes this matcher ignore string elements specified by their fully
  // qualified names, i.e., names corresponding to FieldDescriptor.full_name().
  template <class Iterator>
  void AddCompareIgnoringFields(Iterator first, Iterator last) {
    comp_->ignore_fields.insert(comp_->ignore_fields.end(), first, last);
  }

  // Makes this matcher ignore string elements specified by their relative
  // FieldPath.
  template <class Iterator>
  void AddCompareIgnoringFieldPaths(Iterator first, Iterator last) {
    comp_->ignore_field_paths.insert(comp_->ignore_field_paths.end(), first,
                                     last);
  }

  // Makes this matcher compare repeated fields ignoring ordering of elements.
  void SetCompareRepeatedFieldsIgnoringOrdering() {
    comp_->repeated_field_comp = kProtoCompareRepeatedFieldsIgnoringOrdering;
  }

  // Sets the margin of error for approximate floating point comparison.
  void SetMargin(double margin) {
    CHECK_GE(margin, 0.0) << "Using a negative margin for Approximately";
    comp_->has_custom_margin = true;
    comp_->float_margin = margin;
  }

  // Sets the relative fraction of error for approximate floating point
  // comparison.
  void SetFraction(double fraction) {
    CHECK(0.0 <= fraction && fraction < 1.0)
        << "Fraction for Approximately must be >= 0.0 and < 1.0";
    comp_->has_custom_fraction = true;
    comp_->float_fraction = fraction;
  }

  // Makes this matcher compare protobufs partially.
  void SetComparePartially() { comp_->scope = kProtoPartial; }

  bool MatchAndExplain(const google::protobuf::Message& arg,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(arg, false, listener);
  }

  bool MatchAndExplain(const google::protobuf::Message* arg,
                       ::testing::MatchResultListener* listener) const {
    return (arg != NULL) && MatchAndExplain(*arg, true, listener);
  }

  // Describes the expected relation between the actual protobuf and
  // the expected one.
  void DescribeRelationToExpectedProto(::std::ostream* os) const {
    if (comp_->repeated_field_comp ==
        kProtoCompareRepeatedFieldsIgnoringOrdering) {
      *os << "(ignoring repeated field ordering) ";
    }
    if (!comp_->ignore_fields.empty()) {
      *os << "(ignoring fields: ";
      const char* sep = "";
      for (size_t i = 0; i < comp_->ignore_fields.size(); ++i, sep = ", ")
        *os << sep << comp_->ignore_fields[i];
      *os << ") ";
    }
    if (comp_->float_comp == kProtoApproximate) {
      *os << "approximately ";
      if (comp_->has_custom_margin || comp_->has_custom_fraction) {
        *os << "(";
        if (comp_->has_custom_margin) {
          std::stringstream ss;
          ss << std::setprecision(std::numeric_limits<double>::digits10 + 2)
             << comp_->float_margin;
          *os << "absolute error of float or double fields <= " << ss.str();
        }
        if (comp_->has_custom_margin && comp_->has_custom_fraction) {
          *os << " or ";
        }
        if (comp_->has_custom_fraction) {
          std::stringstream ss;
          ss << std::setprecision(std::numeric_limits<double>::digits10 + 2)
             << comp_->float_fraction;
          *os << "relative error of float or double fields <= " << ss.str();
        }
        *os << ") ";
      }
    }

    *os << (comp_->scope == kProtoPartial ? "partially " : "")
        << (comp_->field_comp == kProtoEqual ? "equal" : "equivalent")
        << (comp_->treating_nan_as_equal ? " (treating NaNs as equal)" : "")
        << " to ";
    PrintExpectedTo(os);
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "is " << (must_be_initialized_ ? "fully initialized and " : "");
    DescribeRelationToExpectedProto(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is " << (must_be_initialized_ ? "not fully initialized or " : "")
        << "not ";
    DescribeRelationToExpectedProto(os);
  }

  bool must_be_initialized() const { return must_be_initialized_; }

  const ProtoComparison& comp() const { return *comp_; }

 private:
  bool MatchAndExplain(const google::protobuf::Message& arg,
                       bool is_matcher_for_pointer,
                       ::testing::MatchResultListener* listener) const;

  const bool must_be_initialized_;
  std::unique_ptr<ProtoComparison> comp_;
};

// Returns a copy of the given proto2 message.
inline google::protobuf::Message* CloneProto2(
    const google::protobuf::Message& src) {
  google::protobuf::Message* clone = src.New();
  clone->CopyFrom(src);
  return clone;
}

// Implements EqualsProto, EquivToProto, EqualsInitializedProto, and
// EquivToInitializedProto, where the matcher parameter is a protobuf.
class ProtoMatcher : public ProtoMatcherBase {
 public:
  ProtoMatcher(
      const google::protobuf::Message& expected,  // The expected protobuf.
      bool must_be_initialized,     // Must the argument be fully initialized?
      const ProtoComparison& comp)  // How to compare the two protobufs.
      : ProtoMatcherBase(must_be_initialized, comp),
        expected_(CloneProto2(expected)) {
    if (must_be_initialized) {
      CHECK(expected.IsInitialized())
          << "The protocol buffer given to *InitializedProto() "
          << "must itself be initialized, but the following required fields "
          << "are missing: " << expected.InitializationErrorString() << ".";
    }
  }

  virtual void PrintExpectedTo(::std::ostream* os) const {
    *os << expected_->GetDescriptor()->full_name() << " ";
    ::testing::internal::UniversalPrint(*expected_, os);
  }

  virtual const google::protobuf::Message* CreateExpectedProto(
      const google::protobuf::Message& /* arg */,
      ::testing::MatchResultListener* /* listener */) const {
    return expected_.get();
  }

  virtual void DeleteExpectedProto(
      const google::protobuf::Message* expected) const {}

  const std::shared_ptr<const google::protobuf::Message>& expected() const {
    return expected_;
  }

 private:
  const std::shared_ptr<const google::protobuf::Message> expected_;
};

// Implements EqualsProto, EquivToProto, EqualsInitializedProto, and
// EquivToInitializedProto, where the matcher parameter is a string.
class ProtoStringMatcher : public ProtoMatcherBase {
 public:
  ProtoStringMatcher(
      const std::string&
          expected,              // The text representing the expected protobuf.
      bool must_be_initialized,  // Must the argument be fully initialized?
      const ProtoComparison comp)  // How to compare the two protobufs.
      : ProtoMatcherBase(must_be_initialized, comp), expected_(expected) {}

  // Parses the expected string as a protobuf of the same type as arg,
  // and returns the parsed protobuf (or NULL when the parse fails).
  // The caller must call DeleteExpectedProto() on the return value
  // later.
  virtual const google::protobuf::Message* CreateExpectedProto(
      const google::protobuf::Message& arg,
      ::testing::MatchResultListener* listener) const {
    google::protobuf::Message* expected_proto = arg.New();
    // We don't insist that the expected string parses as an
    // *initialized* protobuf.  Otherwise EqualsProto("...") may
    // wrongfully fail when the actual protobuf is not fully
    // initialized.  If the user wants to ensure that the actual
    // protobuf is initialized, they should use
    // EqualsInitializedProto("...") instead of EqualsProto("..."),
    // and the MatchAndExplain() function in ProtoMatcherBase will
    // enforce it.
    std::string error_text;
    if (ParsePartialFromAscii(expected_, expected_proto, &error_text)) {
      return expected_proto;
    } else {
      delete expected_proto;
      if (listener->IsInterested()) {
        *listener << "where ";
        PrintExpectedTo(listener->stream());
        *listener << " doesn't parse as a " << arg.GetDescriptor()->full_name()
                  << ":\n"
                  << error_text;
      }
      return NULL;
    }
  }

  virtual void DeleteExpectedProto(
      const google::protobuf::Message* expected) const {
    delete expected;
  }

  virtual void PrintExpectedTo(::std::ostream* os) const {
    *os << "<" << expected_ << ">";
  }

 private:
  const std::string expected_;
};

typedef ::testing::PolymorphicMatcher<ProtoMatcher> PolymorphicProtoMatcher;

// Common code for implementing WhenDeserialized(proto_matcher) and
// WhenDeserializedAs<PB>(proto_matcher).
template <class Proto>
class WhenDeserializedMatcherBase {
 public:
  typedef ::testing::Matcher<const Proto&> InnerMatcher;

  explicit WhenDeserializedMatcherBase(const InnerMatcher& proto_matcher)
      : proto_matcher_(proto_matcher) {}

  virtual ~WhenDeserializedMatcherBase() {}

  // Creates an empty protobuf with the expected type.
  virtual Proto* MakeEmptyProto() const = 0;

  // Type name of the expected protobuf.
  virtual std::string ExpectedTypeName() const = 0;

  // Name of the type argument given to WhenDeserializedAs<>(), or
  // "protobuf" for WhenDeserialized().
  virtual std::string TypeArgName() const = 0;

  // Deserializes the string as a protobuf of the same type as the expected
  // protobuf.
  Proto* Deserialize(google::protobuf::io::ZeroCopyInputStream* input) const {
    Proto* proto = MakeEmptyProto();
    // ParsePartialFromString() parses a serialized representation of a
    // protobuf, allowing required fields to be missing.  This means
    // that we don't insist on the parsed protobuf being fully
    // initialized.  This allows the user to choose whether it should
    // be initialized using EqualsProto vs EqualsInitializedProto, for
    // example.
    if (proto->ParsePartialFromZeroCopyStream(input)) {
      return proto;
    } else {
      delete proto;
      return NULL;
    }
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "can be deserialized as a " << TypeArgName() << " that ";
    proto_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "cannot be deserialized as a " << TypeArgName() << " that ";
    proto_matcher_.DescribeTo(os);
  }

  bool MatchAndExplain(google::protobuf::io::ZeroCopyInputStream* arg,
                       ::testing::MatchResultListener* listener) const {
    // Deserializes the string arg as a protobuf of the same type as the
    // expected protobuf.
    ::std::unique_ptr<const Proto> deserialized_arg(Deserialize(arg));
    if (!listener->IsInterested()) {
      // No need to explain the match result.
      return (deserialized_arg != NULL) &&
             proto_matcher_.Matches(*deserialized_arg);
    }

    ::std::ostream* const os = listener->stream();
    if (deserialized_arg == NULL) {
      *os << "which cannot be deserialized as a " << ExpectedTypeName();
      return false;
    }

    *os << "which deserializes to ";
    UniversalPrint(*deserialized_arg, os);

    ::testing::StringMatchResultListener inner_listener;
    const bool match =
        proto_matcher_.MatchAndExplain(*deserialized_arg, &inner_listener);
    const std::string explain = inner_listener.str();
    if (explain != "") {
      *os << ",\n" << explain;
    }

    return match;
  }

  bool MatchAndExplain(const std::string& str,
                       ::testing::MatchResultListener* listener) const {
    google::protobuf::io::ArrayInputStream input(str.data(), str.size());
    return MatchAndExplain(&input, listener);
  }

  bool MatchAndExplain(absl::string_view sp,
                       ::testing::MatchResultListener* listener) const {
    google::protobuf::io::ArrayInputStream input(sp.data(), sp.size());
    return MatchAndExplain(&input, listener);
  }

  bool MatchAndExplain(const char* str,
                       ::testing::MatchResultListener* listener) const {
    google::protobuf::io::ArrayInputStream input(str, strlen(str));
    return MatchAndExplain(&input, listener);
  }

 private:
  const InnerMatcher proto_matcher_;
};

// Implements WhenDeserialized(proto_matcher).
class WhenDeserializedMatcher
    : public WhenDeserializedMatcherBase<google::protobuf::Message> {
 public:
  explicit WhenDeserializedMatcher(const PolymorphicProtoMatcher& proto_matcher)
      : WhenDeserializedMatcherBase<google::protobuf::Message>(proto_matcher),
        expected_proto_(proto_matcher.impl().expected()) {}

  virtual google::protobuf::Message* MakeEmptyProto() const {
    return expected_proto_->New();
  }

  virtual std::string ExpectedTypeName() const {
    return expected_proto_->GetDescriptor()->full_name();
  }

  virtual std::string TypeArgName() const { return "protobuf"; }

 private:
  // The expected protobuf specified in the inner matcher
  // (proto_matcher_).  We only need a std::shared_ptr to it instead of
  // making a copy, as the expected protobuf will never be changed
  // once created.
  const std::shared_ptr<const google::protobuf::Message> expected_proto_;
};

// Implements WhenDeserializedAs<Proto>(proto_matcher).
template <class Proto>
class WhenDeserializedAsMatcher : public WhenDeserializedMatcherBase<Proto> {
 public:
  typedef ::testing::Matcher<const Proto&> InnerMatcher;

  explicit WhenDeserializedAsMatcher(const InnerMatcher& inner_matcher)
      : WhenDeserializedMatcherBase<Proto>(inner_matcher) {}

  virtual Proto* MakeEmptyProto() const { return new Proto; }

  virtual std::string ExpectedTypeName() const {
    return Proto().GetDescriptor()->full_name();
  }

  virtual std::string TypeArgName() const { return ExpectedTypeName(); }
};

// Implements the IsInitializedProto matcher, which is used to verify that a
// protocol buffer is valid using the IsInitialized method.
class IsInitializedProtoMatcher {
 public:
  void DescribeTo(::std::ostream* os) const {
    *os << "is a fully initialized protocol buffer";
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is not a fully initialized protocol buffer";
  }

  template <typename T>
  bool MatchAndExplain(T& arg,  // NOLINT
                       ::testing::MatchResultListener* listener) const {
    if (!arg.IsInitialized()) {
      *listener << "which is missing the following required fields: "
                << arg.InitializationErrorString();
      return false;
    }
    return true;
  }

  // It's critical for this overload to take a T* instead of a const
  // T*.  Otherwise the other version would be a better match when arg
  // is a pointer to a non-const value.
  template <typename T>
  bool MatchAndExplain(T* arg, ::testing::MatchResultListener* listener) const {
    if (listener->IsInterested() && arg != NULL) {
      *listener << PrintProtoPointee(arg);
    }
    if (arg == NULL) {
      *listener << "which is null";
      return false;
    } else if (!arg->IsInitialized()) {
      *listener << ", which is missing the following required fields: "
                << arg->InitializationErrorString();
      return false;
    } else {
      return true;
    }
  }
};

// Implements EqualsProto and EquivToProto for 2-tuple matchers.
class TupleProtoMatcher {
 public:
  explicit TupleProtoMatcher(const ProtoComparison& comp)
      : comp_(new auto(comp)) {}

  TupleProtoMatcher(const TupleProtoMatcher& other)
      : comp_(new auto(*other.comp_)) {}
  TupleProtoMatcher(TupleProtoMatcher&& other) = default;

  template <typename T1, typename T2>
  operator ::testing::Matcher< ::testing::tuple<T1, T2> >() const {
    return MakeMatcher(new Impl< ::testing::tuple<T1, T2> >(*comp_));
  }
  template <typename T1, typename T2>
  operator ::testing::Matcher<const ::testing::tuple<T1, T2>&>() const {
    return MakeMatcher(new Impl<const ::testing::tuple<T1, T2>&>(*comp_));
  }

  // Allows matcher transformers, e.g., Approximately(), Partially(), etc. to
  // change the behavior of this 2-tuple matcher.
  TupleProtoMatcher& mutable_impl() { return *this; }

  // Makes this matcher compare floating-points approximately.
  void SetCompareApproximately() { comp_->float_comp = kProtoApproximate; }

  // Makes this matcher treating NaNs as equal when comparing floating-points.
  void SetCompareTreatingNaNsAsEqual() { comp_->treating_nan_as_equal = true; }

  // Makes this matcher ignore string elements specified by their fully
  // qualified names, i.e., names corresponding to FieldDescriptor.full_name().
  template <class Iterator>
  void AddCompareIgnoringFields(Iterator first, Iterator last) {
    comp_->ignore_fields.insert(comp_->ignore_fields.end(), first, last);
  }

  // Makes this matcher ignore string elements specified by their relative
  // FieldPath.
  template <class Iterator>
  void AddCompareIgnoringFieldPaths(Iterator first, Iterator last) {
    comp_->ignore_field_paths.insert(comp_->ignore_field_paths.end(), first,
                                     last);
  }

  // Makes this matcher compare repeated fields ignoring ordering of elements.
  void SetCompareRepeatedFieldsIgnoringOrdering() {
    comp_->repeated_field_comp = kProtoCompareRepeatedFieldsIgnoringOrdering;
  }

  // Sets the margin of error for approximate floating point comparison.
  void SetMargin(double margin) {
    CHECK_GE(margin, 0.0) << "Using a negative margin for Approximately";
    comp_->has_custom_margin = true;
    comp_->float_margin = margin;
  }

  // Sets the relative fraction of error for approximate floating point
  // comparison.
  void SetFraction(double fraction) {
    CHECK(0.0 <= fraction && fraction <= 1.0)
        << "Fraction for Relatively must be >= 0.0 and < 1.0";
    comp_->has_custom_fraction = true;
    comp_->float_fraction = fraction;
  }

  // Makes this matcher compares protobufs partially.
  void SetComparePartially() { comp_->scope = kProtoPartial; }

 private:
  template <typename Tuple>
  class Impl : public ::testing::MatcherInterface<Tuple> {
   public:
    explicit Impl(const ProtoComparison& comp) : comp_(comp) {}
    virtual bool MatchAndExplain(
        Tuple args, ::testing::MatchResultListener* /* listener */) const {
      using ::testing::get;
      return ProtoCompare(comp_, get<0>(args), get<1>(args));
    }
    virtual void DescribeTo(::std::ostream* os) const {
      *os << (comp_.field_comp == kProtoEqual ? "are equal" : "are equivalent");
    }
    virtual void DescribeNegationTo(::std::ostream* os) const {
      *os << (comp_.field_comp == kProtoEqual ? "are not equal"
                                              : "are not equivalent");
    }

   private:
    const ProtoComparison comp_;
  };

  std::unique_ptr<ProtoComparison> comp_;
};

}  // namespace internal

// Creates a polymorphic matcher that matches a 2-tuple where
// first.Equals(second) is true.
inline internal::TupleProtoMatcher EqualsProto() {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEqual;
  return internal::TupleProtoMatcher(comp);
}

// Creates a polymorphic matcher that matches a 2-tuple where
// first.Equivalent(second) is true.
inline internal::TupleProtoMatcher EquivToProto() {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return internal::TupleProtoMatcher(comp);
}

// Constructs a matcher that matches the argument if
// argument.Equals(x) or argument->Equals(x) returns true.
inline internal::PolymorphicProtoMatcher EqualsProto(
    const google::protobuf::Message& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEqual;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoMatcher(x, internal::kMayBeUninitialized, comp));
}
inline ::testing::PolymorphicMatcher<internal::ProtoStringMatcher> EqualsProto(
    const std::string& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEqual;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoStringMatcher(x, internal::kMayBeUninitialized, comp));
}
template <class Proto>
inline internal::PolymorphicProtoMatcher EqualsProto(const std::string& str) {
  return EqualsProto(internal::MakePartialProtoFromAscii<Proto>(str));
}

// Constructs a matcher that matches the argument if
// argument.Equivalent(x) or argument->Equivalent(x) returns true.
inline internal::PolymorphicProtoMatcher EquivToProto(
    const google::protobuf::Message& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoMatcher(x, internal::kMayBeUninitialized, comp));
}
inline ::testing::PolymorphicMatcher<internal::ProtoStringMatcher> EquivToProto(
    const std::string& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoStringMatcher(x, internal::kMayBeUninitialized, comp));
}
template <class Proto>
inline internal::PolymorphicProtoMatcher EquivToProto(const std::string& str) {
  return EquivToProto(internal::MakePartialProtoFromAscii<Proto>(str));
}

// Constructs a matcher that matches the argument if
// argument.IsInitialized() or argument->IsInitialized() returns true.
inline ::testing::PolymorphicMatcher<internal::IsInitializedProtoMatcher>
IsInitializedProto() {
  return ::testing::MakePolymorphicMatcher(
      internal::IsInitializedProtoMatcher());
}

// Constructs a matcher that matches an argument whose IsInitialized()
// and Equals(x) methods both return true.  The argument can be either
// a protocol buffer or a pointer to it.
inline internal::PolymorphicProtoMatcher EqualsInitializedProto(
    const google::protobuf::Message& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEqual;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoMatcher(x, internal::kMustBeInitialized, comp));
}
inline ::testing::PolymorphicMatcher<internal::ProtoStringMatcher>
EqualsInitializedProto(const std::string& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEqual;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoStringMatcher(x, internal::kMustBeInitialized, comp));
}
template <class Proto>
inline internal::PolymorphicProtoMatcher EqualsInitializedProto(
    const std::string& str) {
  return EqualsInitializedProto(
      internal::MakePartialProtoFromAscii<Proto>(str));
}

// Constructs a matcher that matches an argument whose IsInitialized()
// and Equivalent(x) methods both return true.  The argument can be
// either a protocol buffer or a pointer to it.
inline internal::PolymorphicProtoMatcher EquivToInitializedProto(
    const google::protobuf::Message& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoMatcher(x, internal::kMustBeInitialized, comp));
}
inline ::testing::PolymorphicMatcher<internal::ProtoStringMatcher>
EquivToInitializedProto(const std::string& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoStringMatcher(x, internal::kMustBeInitialized, comp));
}
template <class Proto>
inline internal::PolymorphicProtoMatcher EquivToInitializedProto(
    const std::string& str) {
  return EquivToInitializedProto(
      internal::MakePartialProtoFromAscii<Proto>(str));
}

namespace proto {

// Approximately(m) returns a matcher that is the same as m, except
// that it compares floating-point fields approximately (using
// google::protobuf::util::MessageDifferencer's APPROXIMATE comparison option).
// The inner matcher m can be any of the Equals* and EquivTo* protobuf
// matchers above.
template <class InnerProtoMatcher>
inline InnerProtoMatcher Approximately(InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().SetCompareApproximately();
  return inner_proto_matcher;
}

// Alternative version of Approximately which takes an explicit margin of error.
template <class InnerProtoMatcher>
inline InnerProtoMatcher Approximately(InnerProtoMatcher inner_proto_matcher,
                                       double margin) {
  inner_proto_matcher.mutable_impl().SetCompareApproximately();
  inner_proto_matcher.mutable_impl().SetMargin(margin);
  return inner_proto_matcher;
}

// Alternative version of Approximately which takes an explicit margin of error
// and a relative fraction of error and will match if either is satisfied.
template <class InnerProtoMatcher>
inline InnerProtoMatcher Approximately(InnerProtoMatcher inner_proto_matcher,
                                       double margin, double fraction) {
  inner_proto_matcher.mutable_impl().SetCompareApproximately();
  inner_proto_matcher.mutable_impl().SetMargin(margin);
  inner_proto_matcher.mutable_impl().SetFraction(fraction);
  return inner_proto_matcher;
}

// TreatingNaNsAsEqual(m) returns a matcher that is the same as m, except that
// it compares floating-point fields such that NaNs are equal.
// The inner matcher m can be any of the Equals* and EquivTo* protobuf matchers
// above.
template <class InnerProtoMatcher>
inline InnerProtoMatcher TreatingNaNsAsEqual(
    InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().SetCompareTreatingNaNsAsEqual();
  return inner_proto_matcher;
}

// IgnoringFields(fields, m) returns a matcher that is the same as m, except the
// specified fields will be ignored when matching
// (using google::protobuf::util::MessageDifferencer::IgnoreField). Each element
// in fields are specified by their fully qualified names, i.e., the names
// corresponding to FieldDescriptor.full_name(). (e.g.
// testing.internal.FooProto2.member). m can be any of the Equals* and EquivTo*
// protobuf matchers above. It can also be any of the transformer matchers
// listed here (e.g. Approximately, TreatingNaNsAsEqual) as long as the intent
// of the each concatenated matcher is mutually exclusive (e.g. using
// IgnoringFields in conjunction with Partially can have different results
// depending on whether the fields specified in IgnoringFields is part of the
// fields covered by Partially).
template <class InnerProtoMatcher, class Container>
inline InnerProtoMatcher IgnoringFields(const Container& ignore_fields,
                                        InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().AddCompareIgnoringFields(
      ignore_fields.begin(), ignore_fields.end());
  return inner_proto_matcher;
}

// See top comment.
template <class InnerProtoMatcher, class Container>
inline InnerProtoMatcher IgnoringFieldPaths(
    const Container& ignore_field_paths,
    InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().AddCompareIgnoringFieldPaths(
      ignore_field_paths.begin(), ignore_field_paths.end());
  return inner_proto_matcher;
}

#ifdef LANG_CXX11
template <class InnerProtoMatcher, class T>
inline InnerProtoMatcher IgnoringFields(std::initializer_list<T> il,
                                        InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().AddCompareIgnoringFields(il.begin(),
                                                              il.end());
  return inner_proto_matcher;
}

template <class InnerProtoMatcher, class T>
inline InnerProtoMatcher IgnoringFieldPaths(
    std::initializer_list<T> il, InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().AddCompareIgnoringFieldPaths(il.begin(),
                                                                  il.end());
  return inner_proto_matcher;
}
#endif  // LANG_CXX11

// IgnoringRepeatedFieldOrdering(m) returns a matcher that is the same as m,
// except that it ignores the relative ordering of elements within each repeated
// field in m. See google::protobuf::MessageDifferencer::TreatAsSet() for more
// details.
template <class InnerProtoMatcher>
inline InnerProtoMatcher IgnoringRepeatedFieldOrdering(
    InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().SetCompareRepeatedFieldsIgnoringOrdering();
  return inner_proto_matcher;
}

// Partially(m) returns a matcher that is the same as m, except that
// only fields present in the expected protobuf are considered (using
// google::protobuf::util::MessageDifferencer's PARTIAL comparison option).  For
// example, Partially(EqualsProto(p)) will ignore any field that's
// not set in p when comparing the protobufs. The inner matcher m can
// be any of the Equals* and EquivTo* protobuf matchers above.
template <class InnerProtoMatcher>
inline InnerProtoMatcher Partially(InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().SetComparePartially();
  return inner_proto_matcher;
}

// WhenDeserialized(m) is a matcher that matches a string that can be
// deserialized as a protobuf that matches m.  m must be a protobuf
// matcher where the expected protobuf type is known at run time.
inline ::testing::PolymorphicMatcher<internal::WhenDeserializedMatcher>
WhenDeserialized(const internal::PolymorphicProtoMatcher& proto_matcher) {
  return ::testing::MakePolymorphicMatcher(
      internal::WhenDeserializedMatcher(proto_matcher));
}

// WhenDeserializedAs<Proto>(m) is a matcher that matches a string
// that can be deserialized as a protobuf of type Proto that matches
// m, which can be any valid protobuf matcher.
template <class Proto, class InnerMatcher>
inline ::testing::PolymorphicMatcher<
    internal::WhenDeserializedAsMatcher<Proto> >
WhenDeserializedAs(const InnerMatcher& inner_matcher) {
  return MakePolymorphicMatcher(internal::WhenDeserializedAsMatcher<Proto>(
      ::testing::SafeMatcherCast<const Proto&>(inner_matcher)));
}

}  // namespace proto
}  // namespace testing
}  // namespace maldoca

#endif  // MALDOCA_BASE_TESTING_PROTOCOL_BUFFER_MATCHERS_H_