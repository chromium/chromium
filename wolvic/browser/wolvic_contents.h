// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_WOLVIC_CONTENTS_H_
#define WOLVIC_BROWSER_WOLVIC_CONTENTS_H_

#include "content/public/browser/web_contents_observer.h"

namespace wolvic {

class WolvicContents : public content::WebContentsObserver {
 public:
  WolvicContents(std::unique_ptr<content::WebContents> web_contents);
  void Init();

  WolvicContents(const WolvicContents&) = delete;
  WolvicContents& operator=(const WolvicContents&) = delete;

  ~WolvicContents() override;

  void DidFinishNavigation(content::NavigationHandle*) override;

 private:
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace content

#endif  // WOLVIC_BROWSER_WOLVIC_CONTENTS_H_
