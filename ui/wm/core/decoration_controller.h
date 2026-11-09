// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_DECORATION_CONTROLLER_H_
#define UI_WM_CORE_DECORATION_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "ui/decoration/decoration.h"
#include "ui/decoration/shadow.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Env;
class Window;
}  // namespace aura

namespace ui {
class ColorProvider;
}

namespace wm {

class ActivationClient;
class DecorationControllerDelegate;

// DecorationController observes changes to windows and creates and updates drop
// shadows as needed. DecorationController itself is light weight and per
// ActivationClient. DecorationController delegates to its implementation class,
// which observes all window creation.
class COMPONENT_EXPORT(UI_WM) DecorationController
    : public ActivationChangeObserver {
 public:
  // Returns the shadow decoration for the |window|, or NULL if no shadow
  // exists.
  static ui::Decoration* GetDecorationForWindow(aura::Window* window);
  // Generate an elevation to shadow colors map with given color provider.
  static ui::decoration::Shadow::ElevationToColorsMap GenerateShadowColorsMap(
      const ui::ColorProvider* color_provider);

  DecorationController(ActivationClient* activation_client,
                       std::unique_ptr<DecorationControllerDelegate> delegate,
                       aura::Env* env = nullptr);

  DecorationController(const DecorationController&) = delete;
  DecorationController& operator=(const DecorationController&) = delete;

  ~DecorationController() override;

  bool IsShadowVisibleForWindow(aura::Window* window);

  // Updates the shadow for |window|. Does nothing if |window| is not observed
  // by the shadow controller impl. This function should be called if the shadow
  // needs to be modified outside of normal window changes (eg. window
  // activation, window property change).
  void UpdateDecorationForWindow(aura::Window* window);

  // ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  bool IsObservingWindowForTest(aura::Window* window) const;

 private:
  class Impl;

  raw_ptr<ActivationClient> activation_client_;

  scoped_refptr<Impl> impl_;
};

}  // namespace wm

#endif  // UI_WM_CORE_DECORATION_CONTROLLER_H_
