// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_MOJO_BUBBLE_WEB_UI_CONTROLLER_H_
#define UI_WEBUI_MOJO_BUBBLE_WEB_UI_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace gfx {
class Point;
}

namespace content {
class WebUI;
}  // namespace content

namespace ui {

class MenuModel;

class MojoBubbleWebUIController : public MojoWebUIController {
 public:
  class Embedder {
   public:
    virtual void ShowUI() = 0;
    virtual void CloseUI() = 0;
    virtual void ShowContextMenu(gfx::Point point,
                                 std::unique_ptr<ui::MenuModel> menu_model) = 0;
    virtual void HideContextMenu() = 0;
  };

  // By default MojoBubbleWebUIController do not have normal WebUI bindings.
  // Pass |enable_chrome_send| as true if these are needed.
  explicit MojoBubbleWebUIController(content::WebUI* contents,
                                     bool enable_chrome_send = false);
  MojoBubbleWebUIController(const MojoBubbleWebUIController&) = delete;
  MojoBubbleWebUIController& operator=(const MojoBubbleWebUIController&) =
      delete;
  ~MojoBubbleWebUIController() override;

  void set_embedder(base::WeakPtr<Embedder> embedder) { embedder_ = embedder; }
  base::WeakPtr<Embedder> embedder() { return embedder_; }

 private:
  base::WeakPtr<Embedder> embedder_;
};

}  // namespace ui

#endif  // UI_WEBUI_MOJO_BUBBLE_WEB_UI_CONTROLLER_H_
