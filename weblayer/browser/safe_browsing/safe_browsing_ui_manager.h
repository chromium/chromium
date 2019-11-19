// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UI_MANAGER_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UI_MANAGER_H_

#include "components/safe_browsing/base_ui_manager.h"

namespace weblayer {

class SafeBrowsingUIManager : public safe_browsing::BaseUIManager {
 public:
  // Construction needs to happen on the UI thread.
  SafeBrowsingUIManager();

  // BaseUIManager overrides.
  void SendSerializedThreatDetails(const std::string& serialized) override;

 protected:
  ~SafeBrowsingUIManager() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIManager);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UI_MANAGER_H_