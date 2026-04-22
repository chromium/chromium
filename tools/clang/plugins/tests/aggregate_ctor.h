// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_AGGREGATE_CTOR_H_
#define TOOLS_CLANG_PLUGINS_TESTS_AGGREGATE_CTOR_H_

#include <string>

// This class has a weight of 12 (4x std::string fields, which count as 3 each),
// but it should not trigger complex constructor warnings since the type is an
// aggregate.
struct HeavyAggregate {
  std::string s1;
  std::string s2;
  std::string s3;
  std::string s4;
};

// `virtual` means this should not be considered an aggregate and should trigger
// complex constructor warnings.
struct VirtualNotAggregate {
  virtual void Method() {}

  std::string s1;
  std::string s2;
  std::string s3;
  std::string s4;
};

// `private` means this should not be considered an aggregate and should trigger
// complex constructor warnings.
struct PrivateNotAggregate {
 private:
  std::string s1;
  std::string s2;
  std::string s3;
  std::string s4;
};

// A user-declared constructor means this should not be considered an aggregate
// and should trigger complex constructor warnings.
struct UserDeclaredNotAggregate {
  UserDeclaredNotAggregate() {}

  std::string s1;
  std::string s2;
  std::string s3;
  std::string s4;
};

// No warnings should be triggered since `DerivedAggregate` is still an
// aggregate.
struct DerivedAggregate : HeavyAggregate {
  std::string d1;
  std::string d2;
  std::string d3;
  std::string d4;
};

// A base class has a `virtual` method, so this is not an aggregate and should
// trigger constructor warnings.
struct DerivedNotAggregate1 : VirtualNotAggregate {
  std::string d1;
  std::string d2;
  std::string d3;
  std::string d4;
};

// A base class has `private` fields, so this is not an aggregate and should
// trigger constructor warnings.
struct DerivedNotAggregate2 : PrivateNotAggregate {
  std::string d1;
  std::string d2;
  std::string d3;
  std::string d4;
};

// A base class has a user-declared constructor, so this is not an aggregate and
// should trigger constructor warnings.
struct DerivedNotAggregate3 : UserDeclaredNotAggregate {
  std::string d1;
  std::string d2;
  std::string d3;
  std::string d4;
};

#endif  // TOOLS_CLANG_PLUGINS_TESTS_AGGREGATE_CTOR_H_
