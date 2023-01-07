// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_ERROR_PAGE_HELPER_H_
#define WEBLAYER_RENDERER_ERROR_PAGE_HELPER_H_

#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "weblayer/common/error_page_helper.mojom.h"

namespace weblayer {

// A class that allows error pages to handle user interaction by handling their
// javascript commands. Currently only SSL and safebrowsing related
// interstitials are supported.
// This is a stripped down version of Chrome's NetErrorHelper.
// TODO(crbug.com/1073624): Share this logic with NetErrorHelper.
class ErrorPageHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ErrorPageHelper>,
      public mojom::ErrorPageHelper {
 public:
  ErrorPageHelper(const ErrorPageHelper&) = delete;
  ErrorPageHelper& operator=(const ErrorPageHelper&) = delete;

  // Creates an ErrorPageHelper which will observe and tie its lifetime to
  // |render_frame|, if it's a main frame. ErrorPageHelpers will not be created
  // for sub frames.
  static void Create(content::RenderFrame* render_frame);

  // Returns the ErrorPageHelper for the frame, if it exists.
  static ErrorPageHelper* GetForFrame(content::RenderFrame* render_frame);

  // Called when the current navigation results in an error.
  void PrepareErrorPage();

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishLoad() override;
  void OnDestruct() override;

  // mojom::ErrorPageHelper:
  void DisableErrorPageHelperForNextError() override;

 private:
  explicit ErrorPageHelper(content::RenderFrame* render_frame);
  ~ErrorPageHelper() override;

  void BindErrorPageHelper(
      mojo::PendingAssociatedReceiver<mojom::ErrorPageHelper> receiver);

  // Set to true in PrepareErrorPage().
  bool is_preparing_for_error_page_ = false;

  // Set to the value of |is_preparing_for_error_page_| in
  // DidCommitProvisionalLoad(). Used to determine if the security interstitial
  // should be shown when the load finishes.
  bool show_error_page_in_finish_load_ = false;

  // Set to true when the embedder injects its own error page. When the
  // embedder injects its own error page the support here is not needed and
  // disabled.
  bool is_disabled_for_next_error_ = false;

  mojo::AssociatedReceiver<mojom::ErrorPageHelper> error_page_helper_receiver_{
      this};

  base::WeakPtrFactory<ErrorPageHelper> weak_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_ERROR_PAGE_HELPER_H_
