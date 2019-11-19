// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/crypto/test/gtest_main.h"

namespace {

class BoringSSLTestSuite : public base::TestSuite {
 public:
  BoringSSLTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

  void Initialize() override {
    TestSuite::Initialize();
    bssl::SetupGoogleTest();
  }
};

}  // namespace

int main(int argc, char** argv) {
  BoringSSLTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&BoringSSLTestSuite::Run, base::Unretained(&test_suite)));
}
