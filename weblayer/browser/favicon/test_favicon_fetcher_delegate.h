// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_TEST_FAVICON_FETCHER_DELEGATE_H_
#define WEBLAYER_BROWSER_FAVICON_TEST_FAVICON_FETCHER_DELEGATE_H_

#include <memory>

#include "ui/gfx/image/image.h"
#include "weblayer/public/favicon_fetcher_delegate.h"

namespace base {
class RunLoop;
}

namespace weblayer {

// Records calls to OnFaviconChanged().
class TestFaviconFetcherDelegate : public FaviconFetcherDelegate {
 public:
  TestFaviconFetcherDelegate();
  TestFaviconFetcherDelegate(const TestFaviconFetcherDelegate&) = delete;
  TestFaviconFetcherDelegate& operator=(const TestFaviconFetcherDelegate&) =
      delete;
  ~TestFaviconFetcherDelegate() override;

  // Waits for OnFaviconChanged() to be called.
  void WaitForFavicon();

  // Waits for a non-empty favicon. This returns immediately if a non-empty
  // image was supplied to OnFaviconChanged() and ClearLastImage() hasn't been
  // called.
  void WaitForNonemptyFavicon();

  void ClearLastImage();

  const gfx::Image& last_image() const { return last_image_; }
  int on_favicon_changed_call_count() const {
    return on_favicon_changed_call_count_;
  }

  // FaviconFetcherDelegate:
  void OnFaviconChanged(const gfx::Image& image) override;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  gfx::Image last_image_;
  bool waiting_for_nonempty_image_ = false;
  int on_favicon_changed_call_count_ = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_TEST_FAVICON_FETCHER_DELEGATE_H_
