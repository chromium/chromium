// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/scoped_views_test_helper.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_helper.h"

namespace views {

ScopedViewsTestHelper::ScopedViewsTestHelper()
    : ScopedViewsTestHelper(base::WrapUnique(new TestViewsDelegate)) {}

ScopedViewsTestHelper::ScopedViewsTestHelper(
    std::unique_ptr<TestViewsDelegate> views_delegate)
    : test_views_delegate_(std::move(views_delegate)),
      platform_test_helper_(PlatformTestHelper::Create()) {
  // The ContextFactory must exist before any Compositors are created.
  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  platform_test_helper_->InitializeContextFactory(&context_factory,
                                                  &context_factory_private);

  test_views_delegate_->set_context_factory(context_factory);
  test_views_delegate_->set_context_factory_private(context_factory_private);

  test_helper_.reset(
      ViewsTestHelper::Create(context_factory, context_factory_private));
  test_helper_->SetUp();

  ui::InitializeInputMethodForTesting();
  ui::TestClipboard::CreateForCurrentThread();
}

ScopedViewsTestHelper::~ScopedViewsTestHelper() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
  ui::ShutdownInputMethodForTesting();
  test_helper_->TearDown();
  test_helper_.reset();

  test_views_delegate_.reset();

  // The Mus PlatformTestHelper has state that is deleted by destruction of
  // ui::TestContextFactories.
  platform_test_helper_.reset();
}

gfx::NativeWindow ScopedViewsTestHelper::GetContext() {
  return test_helper_->GetContext();
}

}  // namespace views
