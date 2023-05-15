// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_AURA_CONTEXT_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_AURA_CONTEXT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"

namespace aura {
class WindowTreeHost;
}

namespace display {
class Screen;
}

namespace wm {
class CursorManager;
class FocusController;
}  // namespace wm

namespace webui_examples {

// Holds the necessary services so that a Aura WindowTreeHost behaves like a
// normal application window, responding to focus and cursor events for example.
class AuraContext {
 public:
  // Represents a WindowTreeHost with customizations and cleanup necessary for
  // normal application window interaction.]
  class ContextualizedWindowTreeHost {
   public:
    ContextualizedWindowTreeHost(
        base::PassKey<AuraContext>,
        AuraContext* context,
        std::unique_ptr<aura::WindowTreeHost> window_tree_host);
    ~ContextualizedWindowTreeHost();

    aura::WindowTreeHost* window_tree_host() { return window_tree_host_.get(); }

   private:
    raw_ptr<AuraContext> const context_;
    std::unique_ptr<aura::WindowTreeHost> const window_tree_host_;
  };

  AuraContext();
  AuraContext(const AuraContext&) = delete;
  AuraContext& operator=(const AuraContext&) = delete;
  ~AuraContext();

  std::unique_ptr<ContextualizedWindowTreeHost> CreateWindowTreeHost();

 private:
  class NativeCursorManager;

  void InitializeWindowTreeHost(aura::WindowTreeHost* host);
  void UninitializeWindowTreeHost(aura::WindowTreeHost* host);

  std::unique_ptr<display::Screen> screen_;
  std::unique_ptr<wm::FocusController> focus_controller_;
  std::unique_ptr<wm::CursorManager> cursor_manager_;
  raw_ptr<NativeCursorManager> native_cursor_manager_;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_AURA_AURA_CONTEXT_H_
