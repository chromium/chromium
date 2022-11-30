// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define DEFINE_TYPE_CASTS(thisType, argumentType, argumentName, predicate) \
  inline thisType* To##thisType(argumentType* argumentName) {              \
    if (!predicate)                                                        \
      asm("int 3");                                                        \
    return static_cast<thisType*>(argumentName);                           \
  }                                                                        \
  inline long long ToInt(argumentType* argumentName) {                     \
    return reinterpret_cast<long long>(argumentName);                      \
  }

#define LIKELY(x) x

namespace blink {

struct Base {};
struct Derived : public Base {};

DEFINE_TYPE_CASTS(Derived, Base, the_object, true);

void F() {
  Base* base_ptr = new Derived;
  Derived* derived_ptr = ToDerived(base_ptr);
  long long as_int = ToInt(base_ptr);
  // 'derivedPtr' should be renamed: it's a reference to a declaration defined
  // outside a macro invocation.
  if (LIKELY(derived_ptr)) {
    delete derived_ptr;
  }
}

#define CALL_METHOD_FROM_MACRO()           \
  void CallMethodFromMacro() { Method(); } \
  void Pmethod() override {}

struct WithMacroP {
  virtual void Pmethod() {}
};

struct WithMacro : public WithMacroP {
  void Method() {}
  CALL_METHOD_FROM_MACRO();
};

#define DEFINE_WITH_TOKEN_CONCATENATION2(arg1, arg2) \
  void arg1##arg2() {}
// We definitely don't want to rewrite |arg1| on the previous line into
// either |Arg1| or |Frg1| or |Brg1| or |Foo| or |Baz|.

// We might or might not want to rewrite |foo|->|Foo| and |baz|->|Baz| below.
// The test below just spells out the current behavior of the tool (which one
// can argue is accidental).
DEFINE_WITH_TOKEN_CONCATENATION2(foo, Bar1)
DEFINE_WITH_TOKEN_CONCATENATION2(baz, Bar2)

void TokenConcatenationTest2() {
  // We might or might not want to rewrite |foo|->|Foo| and |baz|->|Baz| below.
  // The test below just spells out the current behavior of the tool (which one
  // can argue is accidental).
  fooBar1();
  bazBar2();
}

class FieldsMacro {
 public:
  // We shouldn't rewrite |m_fooBar| -> |foo_bar_|, because we cannot rewrite
  // |m_##name| -> |???|.
  FieldsMacro() : m_fooBar(123), m_barBaz(456) {}

#define DECLARE_FIELD(name, Name) \
 private:                         \
  int m_##name;                   \
                                  \
 public:                          \
  int name() { return m_##name; } \
  void Set##Name(int name) { m_##name = name; }

  DECLARE_FIELD(FooBar, FooBar)
  DECLARE_FIELD(BarBaz, BarBaz)
};

int FieldsMacroTest() {
  FieldsMacro fm;
  fm.SetFooBar(789);
  return fm.FooBar() + fm.BarBaz();
}

}  // namespace blink
