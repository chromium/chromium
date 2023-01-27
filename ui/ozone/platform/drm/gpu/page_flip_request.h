// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_REQUEST_H_
#define UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_REQUEST_H_

#include "base/atomic_ref_count.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace ui {

class PageFlipRequest : public base::RefCounted<PageFlipRequest> {
 public:
  using PageFlipCallback =
      base::OnceCallback<void(unsigned int /* frame */,
                              base::TimeTicks /* timestamp */)>;

  explicit PageFlipRequest(const base::TimeDelta& refresh_interval);

  PageFlipRequest(const PageFlipRequest&) = delete;
  PageFlipRequest& operator=(const PageFlipRequest&) = delete;

  // Takes ownership of the swap completion callback to allow
  // asynchronous notification of completion.
  //
  // This is only called once all of the individual flips have have been
  // successfully submitted.
  void TakeCallback(PresentationOnceCallback callback);

  // Add a page flip to this request. Once all page flips return, the
  // overall callback is made with the timestamp from the page flip event
  // that returned last.
  PageFlipCallback AddPageFlip();

  int page_flip_count() const { return page_flip_count_; }

 private:
  void Signal(unsigned int frame, base::TimeTicks timestamp);

  friend class base::RefCounted<PageFlipRequest>;
  ~PageFlipRequest();

  PresentationOnceCallback callback_;
  int page_flip_count_ = 0;
  const base::TimeDelta refresh_interval_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_PAGE_FLIP_REQUEST_H_
