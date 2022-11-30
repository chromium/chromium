// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

base::test::TaskEnvironment* g_task_environment = nullptr;

}  // namespace

struct PlatformClipboardTraits {
  static Clipboard* Create() {
    DCHECK(!g_task_environment);
    g_task_environment = new base::test::TaskEnvironment(
        base::test::TaskEnvironment::MainThreadType::UI);
    return Clipboard::GetForCurrentThread();
  }

  static void Destroy(Clipboard* clipboard) {
    ASSERT_EQ(Clipboard::GetForCurrentThread(), clipboard);
    Clipboard::DestroyClipboardForCurrentThread();
    delete g_task_environment;
    g_task_environment = nullptr;
  }
};

class PlatformClipboardTestName {
 public:
  template <typename T>
  static std::string GetName(int index) {
    return "PlatformClipboardTest";
  }
};

using TypesToTest = PlatformClipboardTraits;
using NamesOfTypesToTest = PlatformClipboardTestName;

}  // namespace ui

#include "ui/base/clipboard/clipboard_test_template.h"
