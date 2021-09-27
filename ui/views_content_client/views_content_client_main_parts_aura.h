// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_AURA_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_AURA_H_

#include <memory>

#include "base/macros.h"
#include "ui/views_content_client/views_content_client_main_parts.h"

namespace wm {
class WMState;
}

namespace ui {

class ViewsContentClientMainPartsAura : public ViewsContentClientMainParts {
 public:
  ViewsContentClientMainPartsAura(const ViewsContentClientMainPartsAura&) =
      delete;
  ViewsContentClientMainPartsAura& operator=(
      const ViewsContentClientMainPartsAura&) = delete;

 protected:
  ViewsContentClientMainPartsAura(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);
  ~ViewsContentClientMainPartsAura() override;

  // content::BrowserMainParts:
  void ToolkitInitialized() override;
  void PostMainMessageLoopRun() override;

 private:
  std::unique_ptr<::wm::WMState> wm_state_;
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_AURA_H_
