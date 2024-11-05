// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_WOLVIC_CONTENTS_H_
#define WOLVIC_BROWSER_WOLVIC_CONTENTS_H_

#include "content/public/browser/web_contents_observer.h"

namespace webapps {
struct InstallableData;
}

namespace wolvic {

class WolvicWebContentsDelegate;

class WolvicContents : public content::WebContentsObserver {
 public:
  static WolvicContents* FromWebContents(content::WebContents* web_contents);

  WolvicContents(std::unique_ptr<content::WebContents> web_contents);
  void Init();

  WolvicContents(const WolvicContents&) = delete;
  WolvicContents& operator=(const WolvicContents&) = delete;

  ~WolvicContents() override;

  void DidFinishNavigation(content::NavigationHandle*) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& url) override;
  void SetDelegate(std::unique_ptr<WolvicWebContentsDelegate>);

 private:
  // Called once the installable data has been fetched.
  void OnDidGetInstallableData(const webapps::InstallableData& data);

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WolvicWebContentsDelegate> web_contents_delegate_;
  base::WeakPtrFactory<WolvicContents> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_WOLVIC_CONTENTS_H_
