// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
}

namespace weblayer {

class WebContentsViewDelegateImpl : public content::WebContentsViewDelegate {
 public:
  explicit WebContentsViewDelegateImpl(content::WebContents* web_contents);

  WebContentsViewDelegateImpl(const WebContentsViewDelegateImpl&) = delete;
  WebContentsViewDelegateImpl& operator=(const WebContentsViewDelegateImpl&) =
      delete;

  ~WebContentsViewDelegateImpl() override;

  // WebContentsViewDelegate overrides.
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_CONTENTS_VIEW_DELEGATE_IMPL_H_
