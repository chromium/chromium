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
  C() : m_flagField(~0), m_fieldMentioningHTTPAndHTTPS(1), m_shouldRename(0) {}

  int method() {
    // Test that references to fields are updated correctly.
    return instanceCount + m_flagField + m_fieldMentioningHTTPAndHTTPS;
  }

  // Test that a field without a m_ prefix is correctly renamed.
  static int instanceCount;

 protected:
  // Test that a field with a m_ prefix is correctly renamed.
  const int m_flagField;
  // Statics should be named with s_, but make sure s_ and m_ are both correctly
  // stripped.
  static int s_staticCount;
  static int m_staticCountWithBadName;
  // Make sure that acronyms don't confuse the underscore inserter.
  int m_fieldMentioningHTTPAndHTTPS;
  // Already Google style, should not change.
  int already_google_style_;

  union {
    // Anonymous union members should be renamed, as should contructor
    // initializers of them.
    char* m_shouldRename;
    int* m_doesRename;
  };
};

struct Derived : public C {
  using C::m_flagField;
  using C::m_fieldMentioningHTTPAndHTTPS;
};

int C::instanceCount = 0;

// Structs are like classes.
struct S {
  int m_integerField;
  int wantsRename;
  int google_style_already;
};

// Unions also use struct-style naming.
union U {
  char fourChars[4];
  short twoShorts[2];
  int one_hopefully_four_byte_int;
  int m_hasPrefix;
};

// https://crbug.com/640749#c1: Some type traits are inside blink namespace.
struct IsGarbageCollectedMixin {
  static const bool value = true;
  static const bool safeToCompareToEmptyOrDeleted = false;
};

}  // namespace blink

namespace not_blink {

// These are traits for WTF types that may be defined outside of blink such
// as in mojo. But their names are unique so we can globally treat them as
// type traits for renaming.
struct GloballyKnownTraits {
  static const bool safeToCompareToEmptyOrDeleted = false;
};

}  // namespace not_blink

namespace WTF {

void testForTraits() {
  bool a = blink::IsGarbageCollectedMixin::safeToCompareToEmptyOrDeleted;
  bool b = not_blink::GloballyKnownTraits::safeToCompareToEmptyOrDeleted;
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

  static YesType subclassCheck(U*);
  static NoType subclassCheck(...);
  static T* t;

 public:
  static const bool value = sizeof(subclassCheck(t)) == sizeof(YesType);
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
  RefCountedLifetime,
  GarbageCollectedLifetime,
};
template <typename T>
struct LifetimeOf {
 private:
  // Okay to rename |isGarbageCollected| to |kIsGarbageCollected|.
  static const bool isGarbageCollected = true;

 public:
  // Expecting no rename of |value|.
  static const LifetimeManagementType value =
      !isGarbageCollected ? RefCountedLifetime : GarbageCollectedLifetime;
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
  blink::C::instanceCount++;
  // Force instantiation of a copy constructor for blink::C to make sure field
  // initializers for synthesized functions don't cause weird rewrites.
  blink::C c;
  blink::C c2 = c;

  bool b1 = WTF::TypeTrait1::value;
  bool b2 = WTF::TypeTrait2<void>::value;
}
