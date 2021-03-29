// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
}

namespace weblayer {

class WebContentsViewDelegateImpl : public content::WebContentsViewDelegate {
 public:
  explicit WebContentsViewDelegateImpl(content::WebContents* web_contents);
  ~WebContentsViewDelegateImpl() override;

  // WebContentsViewDelegate overrides.
  void ShowContextMenu(content::RenderFrameHost* render_frame_host,
                       const content::ContextMenuParams& params) override;

 private:
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewDelegateImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_
