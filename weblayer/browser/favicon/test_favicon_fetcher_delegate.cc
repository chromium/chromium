// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/favicon/test_favicon_fetcher_delegate.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace weblayer {

TestFaviconFetcherDelegate::TestFaviconFetcherDelegate() = default;

TestFaviconFetcherDelegate::~TestFaviconFetcherDelegate() = default;

void TestFaviconFetcherDelegate::WaitForFavicon() {
  ASSERT_EQ(nullptr, run_loop_.get());
  waiting_for_nonempty_image_ = false;
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void TestFaviconFetcherDelegate::WaitForNonemptyFavicon() {
  if (!last_image_.IsEmpty())
    return;

  run_loop_ = std::make_unique<base::RunLoop>();
  waiting_for_nonempty_image_ = true;
  run_loop_->Run();
  run_loop_.reset();
}

void TestFaviconFetcherDelegate::ClearLastImage() {
  last_image_ = gfx::Image();
  on_favicon_changed_call_count_ = 0;
}

void TestFaviconFetcherDelegate::OnFaviconChanged(const gfx::Image& image) {
  last_image_ = image;
  ++on_favicon_changed_call_count_;
  if (run_loop_ && (!waiting_for_nonempty_image_ || !image.IsEmpty()))
    run_loop_->Quit();
}

}  // namespace weblayer
