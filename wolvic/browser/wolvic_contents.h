// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_WOLVIC_CONTENTS_H_
#define WOLVIC_BROWSER_WOLVIC_CONTENTS_H_

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "content/public/browser/web_contents_observer.h"

namespace wolvic {

class WolvicContents : public content::WebContentsObserver {
 public:
  static WolvicContents* FromWebContents(content::WebContents* web_contents);

  WolvicContents(std::unique_ptr<content::WebContents> web_contents);
  void Init();

  WolvicContents(const WolvicContents&) = delete;
  WolvicContents& operator=(const WolvicContents&) = delete;

  ~WolvicContents() override;

  void DidFinishNavigation(content::NavigationHandle*) override;

  void SetDelegate(std::unique_ptr<web_contents_delegate_android::WebContentsDelegateAndroid> delegate);

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<web_contents_delegate_android::WebContentsDelegateAndroid> web_contents_delegate_;
};

}  // namespace content

#endif  // WOLVIC_BROWSER_WOLVIC_CONTENTS_H_
