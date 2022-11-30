// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_FILL_LAYOUT_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_FILL_LAYOUT_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/layout_manager.h"

namespace aura {
class Window;
}

namespace webui_examples {

class FillLayout : public aura::LayoutManager {
 public:
  explicit FillLayout(aura::Window* root);
  FillLayout(const FillLayout&) = delete;
  FillLayout& operator=(const FillLayout&) = delete;
  ~FillLayout() override;

 private:
  // aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  raw_ptr<aura::Window> const root_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_FILL_LAYOUT_H_
