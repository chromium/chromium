// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_
#define UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
class TestContextFactories;
}

namespace views {

class Widget;

class PlatformTestHelper {
 public:
  using Factory =
      base::RepeatingCallback<std::unique_ptr<PlatformTestHelper>(void)>;

  PlatformTestHelper();
  virtual ~PlatformTestHelper();

  static void set_factory(Factory factory);
  static std::unique_ptr<PlatformTestHelper> Create();

  // Simulate an OS-level destruction of the native window held by |widget|.
  virtual void SimulateNativeDestroy(Widget* widget);

  virtual void InitializeContextFactory(
      ui::ContextFactory** factory,
      ui::ContextFactoryPrivate** factory_private);

 private:
  std::unique_ptr<ui::TestContextFactories> context_factories_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelper);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_
