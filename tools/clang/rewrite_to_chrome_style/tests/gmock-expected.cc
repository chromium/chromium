// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"

namespace blink {

namespace simple_test {

class Interface {
 public:
  virtual void MyMethod(int my_param) {}
};

class MockedInterface : public Interface {
 public:
  MOCK_METHOD1(MyMethod, void(int));
};

void Test() {
  MockedInterface mocked_interface;
  EXPECT_CALL(mocked_interface, MyMethod(1));
  EXPECT_CALL(
      mocked_interface,  // A comment to prevent reformatting into single line.
      MyMethod(1));
  mocked_interface.MyMethod(123);

  int arg;
  ON_CALL(mocked_interface, MyMethod(1))
      .WillByDefault(testing::SaveArg<0>(&arg));
}

}  // namespace simple_test

namespace no_base_method_to_override {

class MockDestructible {
 public:
  MOCK_METHOD0(Destruct, void());
};

void Test() {
  MockDestructible destructible;
  EXPECT_CALL(destructible, Destruct());
}

}  // namespace no_base_method_to_override

}  // namespace blink
