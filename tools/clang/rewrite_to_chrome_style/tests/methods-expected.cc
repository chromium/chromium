// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gen/thing.h"

namespace v8 {

class InterfaceOutsideOfBlink {
 public:
  virtual void nonBlinkVirtual() = 0;
};

}  // namespace v8

namespace blink {

class InsideOfBlink : public v8::InterfaceOutsideOfBlink {
 public:
  // This function overrides something outside of blink so don't rename it.
  void nonBlinkVirtual() override {}
  // This function is in blink so rename it.
  virtual void BlinkVirtual() {}
};

class MyIterator {};
using my_iterator = char*;

class Task {
 public:
  // Already style-compliant methods shouldn't change.
  void OutputDebugString() {}

  // Tests that the declarations for methods are updated.
  void DoTheWork();
  // Overload to test using declarations that introduce multiple shadow
  // declarations.
  void DoTheWork(int);
  virtual void ReallyDoTheWork() = 0;

  // Note: this is purposely copyable and assignable, to make sure the Clang
  // tool doesn't try to emit replacements for things that aren't explicitly
  // written.

  // Overloaded operators should not be rewritten.
  Task& operator++() { return *this; }

  // Conversion functions should not be rewritten.
  explicit operator int() const { return 42; }

  // These are special functions that we don't rename so that range-based
  // for loops and STL things work.
  MyIterator begin() { return {}; }
  my_iterator end() { return {}; }
  my_iterator rbegin() { return {}; }
  MyIterator rend() { return {}; }
  // The trace() method is used by Oilpan, but we plan to tweak the Oilpan's
  // clang plugin, so that it recognizes the new method name.
  void Trace() {}
  // These are used by std::unique_lock and std::lock_guard.
  void lock() {}
  void unlock() {}
  void try_lock() {}
};

class Other {
  // Static begin/end/trace don't count, and should be renamed.
  static MyIterator Begin() { return {}; }
  static my_iterator End() { return {}; }
  static void Trace() {}
  static void Lock() {}
};

// Test that the actual method definition is also updated.
void Task::DoTheWork() {
  ReallyDoTheWork();
}

template <typename T>
class Testable {
 public:
  typedef T Testable::*UnspecifiedBoolType;
  // This method has a reference to a member in a "member context" and a
  // "non-member context" to verify both are rewritten.
  operator UnspecifiedBoolType() { return ptr_ ? &Testable::ptr_ : 0; }

 private:
  int ptr_;
};

namespace subname {

class SubnameParent {
  virtual void SubnameMethod() {}
};

}  // namespace subname

class SubnameChild : public subname::SubnameParent {
  // This subclasses from blink::subname::SubnameParent and should be renamed.
  void SubnameMethod() override {}
};

class GenChild : public blink::GenClass {
  // This subclasses from the blink namespace but in the gen directory so it
  // should not be renamed.
  void genMethod() override {}
};

}  // namespace blink

// Test that overrides from outside the Blink namespace are also updated.
class BovineTask : public blink::Task {
 public:
  using Task::DoTheWork;
  void ReallyDoTheWork() override;
};

class SuperBovineTask : public BovineTask {
 public:
  using BovineTask::ReallyDoTheWork;
};

void BovineTask::ReallyDoTheWork() {
  DoTheWork();
  // Calls via an overridden method should also be updated.
  ReallyDoTheWork();
}

// Finally, test that method pointers are also updated.
void F() {
  void (blink::Task::*p1)() = &blink::Task::DoTheWork;
  void (blink::Task::*p2)() = &BovineTask::DoTheWork;
  void (blink::Task::*p3)() = &blink::Task::ReallyDoTheWork;
  void (BovineTask::*p4)() = &BovineTask::ReallyDoTheWork;
}

bool G() {
  // Use the Testable class to rewrite the method.
  blink::Testable<int> tt;
  return tt;
}

class SubclassOfInsideOfBlink : public blink::InsideOfBlink {
 public:
  // This function overrides something outside of blink so don't rename it.
  void nonBlinkVirtual() override {}
  // This function overrides something in blink so rename it.
  void BlinkVirtual() override {}
};

class TestSubclassInsideOfBlink : public SubclassOfInsideOfBlink {
 public:
 public:
  // This function overrides something outside of blink so don't rename it.
  void nonBlinkVirtual() override {}
  // This function overrides something in blink so rename it.
  void BlinkVirtual() override {}
};

namespace blink {

struct StructInBlink {
  // Structs in blink should rename their methods to capitals.
  bool Function() { return true; }
};

class BitVector {
 public:
  class OutOfLineBits {};
  enum Foo { kBlah };
  struct Bar {};
  class Baz {};
  class FooBar {};

  // Should be renamed to GetReadyState, because of
  // ShouldPrefixFunctionName heuristic.
  int GetReadyState() { return 123; }

  template <typename T>
  class MyRefPtr {};

  // Naive renaming will break the build, by leaving return type the same
  // as the method name - to avoid this "Get" prefix needs to be prepended
  // as suggested in https://crbug.com/582312#c17.
  const OutOfLineBits* GetOutOfLineBits() const { return nullptr; }
  Foo GetFoo() { return kBlah; }
  const Bar& GetBar() const { return bar_; }
  MyRefPtr<Baz> GetBaz() { return MyRefPtr<Baz>(); }
  const MyRefPtr<FooBar>& GetFooBar() { return foobar_; }

 private:
  Bar bar_;
  MyRefPtr<FooBar> foobar_;
};

namespace get_prefix_vs_inheritance {

// Regression test for https://crbug.com/673031:
// 1. |frame| accessor/method should be renamed in the same way for
//    WebFrameImplBase and WebLocalFrameImpl.
// 2. Need to rename |frame| to |GetFrame| (not to |Frame|) to avoid
//    a conflict with the Frame type.

class FrameFoo {};
class LocalFrame : public FrameFoo {};

class WebFrameImplBase {
 public:
  // Using |frameFoo| to test inheritance, and NOT just the presence on the
  // ShouldPrefixFunctionName list.
  virtual FrameFoo* GetFrameFoo() const = 0;
};

class WebLocalFrameImpl : public WebFrameImplBase {
 public:
  LocalFrame* GetFrameFoo() const override { return nullptr; }
};

// This is also a regression test for https://crbug.com/673031.  We should NOT
// rewrite in a non-virtual case, because walking the inheritance chain of the
// return type depends too much on unrelated context (i.e. walking the
// inheritance chain might not be possible if the return type is
// forward-declared).
class LayoutObjectFoo {};
class LayoutBoxModelObject : public LayoutObjectFoo {};
class PaintLayerStackingNode {
 public:
  // |layoutObjectFoo| should NOT be renamed to |GetLayoutObjectFoo| (just to
  // |LayoutObjectFoo|) - see the big comment above.  We use layoutObject*Foo*
  // to test inheritance-related behavior and avoid testing whether method name
  // is covered via ShouldPrefixFunctionName.
  LayoutBoxModelObject* LayoutObjectFoo() { return nullptr; }
};

}  // namespace get_prefix_vs_inheritance

namespace blocklisting_of_method_and_function_names {

class Foo {
  // Expecting |swap| method to be renamed to |Swap| - we blocklist renaming of
  // |swap| *function*, because it needs to have the same casing as std::swap,
  // so that ADL can kick-in and pull it from another namespace depending on the
  // bargument.  We have a choice to rename or not rename |swap| *methods* - we
  // chose to rename to be consistent (i.e. we rename |clear| -> |Clear|) and
  // because Google C++ Styke Guide uses "Swap" in examples.
  void Swap() {}
  static void Swap(Foo& x, Foo& y) {}

  // We don't rename |begin|, so that <algorithms> and other templates that
  // expect |begin|, |end|, etc. continue to work.  This is only necessary
  // for instance methods - renaming static methods and funcitons is okay.
  void begin() {}
  static void Begin(int x) {}

  // https://crbug.com672902: std-like names should not be rewritten.
  void emplace_back(int x) {}
  void insert(int x) {}
  void push_back(int x) {}
  int* back() { return nullptr; }
  int* front() { return nullptr; }
  void erase() {}
  bool empty() { return true; }
};

void Begin(int x) {}
void swap(Foo& x, Foo& y) {}

}  // blocklisting_of_method_and_function_names

}  // namespace blink

namespace WTF {

struct StructInWTF {
  // Structs in WTF should rename their methods to capitals.
  bool Function() { return true; }
};

}  // namespace WTF

void F2() {
  blink::StructInBlink b;
  b.Function();
  WTF::StructInWTF w;
  w.Function();
}

namespace blink {

class ClassDeclaredInsideBlink {
 public:
  static void MethodDefinedOutsideBlink();
};

namespace internal {

class InternalClass {
 public:
  static void Method();
};

}  // namespace internal

// Tests for --method-blocklist cmdline parameter.
class IdlTestClass {
 public:
  static int NotBlocklistedMethod() { return 123; }
  int NotBlocklistedMethod(int x) { return 123; }

  static int idlStaticMethod() { return 123; }
  int idlInstanceMethod() { return 123; }

  template <typename T>
  int idlTemplateMethod(T x) {
    return 123;
  }
};

template <typename T>
class IdlTemplateClass {
 public:
  int idlInstanceMethod(T x) { return 123; }
};

}  // namespace blink

// https://crbug.com/640688 - need to rewrite method name below.
void blink::ClassDeclaredInsideBlink::MethodDefinedOutsideBlink() {}
void blink::internal::InternalClass::Method() {}
