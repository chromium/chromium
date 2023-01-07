// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
#define UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_

#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

namespace ui {

// MojoWebUIController is intended for WebUI pages that use Mojo. It is
// expected that subclasses will:
// . Add all Mojo Bindings Resources via AddResourcePath(), eg:
//     source->AddResourcePath("chrome/browser/ui/webui/omnibox/omnibox.mojom",
//                              IDR_OMNIBOX_MOJO_JS);
// . Overload void BindInterface(mojo::PendingReceiver<InterfaceName>) for all
//   Mojo Interfaces it wishes to handle.
// . Use WEB_UI_CONTROLLER_TYPE_DECL macro in .h file and
//   WEB_UI_CONTROLLER_TYPE_IMPL macro in .cc file.
// . Register all Mojo Interfaces it wishes to handle in the appropriate
//   BinderMap:
//     - chrome/browser/chrome_browser_interface_binders.cc for chrome/ WebUIs;
//     - content/browser/browser_interface_binders.cc for content/ WebUIs.
class MojoWebUIController : public content::WebUIController {
 public:
  // By default MojoWebUIControllers do not have normal WebUI bindings. Pass
  // |enable_chrome_send| as true if these are needed.
  explicit MojoWebUIController(content::WebUI* contents,
                               bool enable_chrome_send = false);

  MojoWebUIController(const MojoWebUIController&) = delete;
  MojoWebUIController& operator=(const MojoWebUIController&) = delete;

  ~MojoWebUIController() override;
};

}  // namespace ui

#endif  // UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
