// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Note: do not add any copy or move constructors to this class: doing so will
// break test coverage that we don't clobber the class name by trying to emit
// replacements for synthesized functions.
class C {
 public:
  // Make sure initializers are updated to use the new names.
  C()
      : flag_field_(~0),
        field_mentioning_http_and_https_(1),
        should_rename_(0) {}

  int Method() {
    // Test that references to fields are updated correctly.
    return instance_count_ + flag_field_ + field_mentioning_http_and_https_;
  }

  // Test that a field without a m_ prefix is correctly renamed.
  static int instance_count_;

 protected:
  // Test that a field with a m_ prefix is correctly renamed.
  const int flag_field_;
  // Statics should be named with s_, but make sure s_ and m_ are both correctly
  // stripped.
  static int static_count_;
  static int static_count_with_bad_name_;
  // Make sure that acronyms don't confuse the underscore inserter.
  int field_mentioning_http_and_https_;
  // Already Google style, should not change.
  int already_google_style_;

  union {
    // Anonymous union members should be renamed, as should contructor
    // initializers of them.
    char* should_rename_;
    int* does_rename_;
  };
};

struct Derived : public C {
  using C::flag_field_;
  using C::field_mentioning_http_and_https_;
};

int C::instance_count_ = 0;

// Structs are like classes.
struct S {
  int integer_field_;
  int wants_rename;
  int google_style_already;
};

// Unions also use struct-style naming.
union U {
  char four_chars[4];
  short two_shorts[2];
  int one_hopefully_four_byte_int;
  int has_prefix_;
};

// https://crbug.com/640749#c1: Some type traits are inside blink namespace.
struct IsGarbageCollectedMixin {
  static const bool value = true;
  static const bool safe_to_compare_to_empty_or_deleted = false;
};

}  // namespace blink

namespace not_blink {

// These are traits for WTF types that may be defined outside of blink such
// as in mojo. But their names are unique so we can globally treat them as
// type traits for renaming.
struct GloballyKnownTraits {
  static const bool safe_to_compare_to_empty_or_deleted = false;
};

}  // namespace not_blink

namespace WTF {

void TestForTraits() {
  bool a = blink::IsGarbageCollectedMixin::safe_to_compare_to_empty_or_deleted;
  bool b = not_blink::GloballyKnownTraits::safe_to_compare_to_empty_or_deleted;
}

// We don't want to capitalize fields in type traits
// (i.e. the |value| -> |kValue| rename is undesirable below).
struct TypeTrait1 {
  static const bool value = true;
};

// Some type traits are implemented as classes, not structs
// (e.g. WTF::IsGarbageCollectedType or WTF::IsAssignable).
// We should not perform a |value| -> |kValue| rename in the type trait below.
template <typename T>
class TypeTrait2 {
 public:
  static const bool value = false;
};
template <>
class TypeTrait2<void> {
 public:
  static const bool value = false;
};

// Some type traits have static methods.  We should not perform
// a |value| -> |kValue| rename in the type trait below.
template <typename T, typename U>
struct IsSubclass {
 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  static YesType SubclassCheck(U*);
  static NoType SubclassCheck(...);
  static T* t_;

 public:
  static const bool value = sizeof(SubclassCheck(t_)) == sizeof(YesType);
};

// Some type traits have deleted instance methods.  We should not perform
// a |value| -> |kValue| rename in the type trait below.
template <typename U = void>
struct IsTraceableInCollection {
  // Expanded from STATIC_ONLY(IsTraceableInCollection) macro:
 private:
  IsTraceableInCollection() = delete;
  IsTraceableInCollection(const IsTraceableInCollection&) = delete;
  IsTraceableInCollection& operator=(const IsTraceableInCollection&) = delete;
  void* operator new(unsigned long) = delete;
  void* operator new(unsigned long, void*) = delete;

 public:
  static const bool value = true;
};

// Some type traits have a non-boolean value.
enum LifetimeManagementType {
  kRefCountedLifetime,
  kGarbageCollectedLifetime,
};
template <typename T>
struct LifetimeOf {
 private:
  // Okay to rename |isGarbageCollected| to |kIsGarbageCollected|.
  static const bool kIsGarbageCollected = true;

 public:
  // Expecting no rename of |value|.
  static const LifetimeManagementType value =
      !kIsGarbageCollected ? kRefCountedLifetime : kGarbageCollectedLifetime;
};

template <typename T>
struct GenericHashTraitsBase {
  // We don't want to capitalize fields in type traits
  // (i.e. the |value| -> |kValue| rename is undesirable below).
  // This problem is prevented by IsCallee heuristic.
  static const int kWeakHandlingFlag = TypeTrait2<T>::value ? 123 : 456;
};

template <int Format>
struct IntermediateFormat {
  // Some type traits have int type.  Example below is loosely based on
  // third_party/WebKit/Source/platform/graphics/gpu/WebGLImageConversion.cpp
  static const int value = (Format == 123) ? 456 : 789;
};

};  // namespace WTF

void F() {
  // Test that references to a static field are correctly rewritten.
  blink::C::instance_count_++;
  // Force instantiation of a copy constructor for blink::C to make sure field
  // initializers for synthesized functions don't cause weird rewrites.
  blink::C c;
  blink::C c2 = c;

  bool b1 = WTF::TypeTrait1::value;
  bool b2 = WTF::TypeTrait2<void>::value;
}
