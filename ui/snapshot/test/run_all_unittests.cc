// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/compositor/test/test_suite.h"

int main(int argc, char** argv) {
  mojo::core::Init();

#if defined(USE_AURA)
  ui::test::CompositorTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&ui::test::CompositorTestSuite::Run,
                     base::Unretained(&test_suite)));
#else
  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
#endif
}
