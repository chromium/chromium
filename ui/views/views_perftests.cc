// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/views/views_test_suite.h"

int main(int argc, char** argv) {
  views::ViewsTestSuite test_suite(argc, argv);
  mojo::core::Init();
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&views::ViewsTestSuite::Run,
                     base::Unretained(&test_suite)));
}
