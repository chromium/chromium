// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CLOSURE_ANIMATION_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CLOSURE_ANIMATION_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view_tracker.h"
#include "ui/views/views_export.h"

namespace views {

class MenuItemView;
class SubmenuView;

// This class implements the Mac menu closure animation:
//    1) For 100ms, the selected item is drawn as unselected
//    2) Then, for another 100ms, the selected item is drawn as selected
//    3) Then, and the window fades over 250ms to transparency
// Note that this class is owned by the involved MenuController, so if the menu
// is destructed early for any reason, this class will be destructed also, which
// will stop the timer or animation (if they are running), so the callback will
// *not* be run - which is good, since the MenuController that would have
// received it is being deleted.
//
// This class also supports animating a menu away without animating the
// selection effect, which is achieved by passing nullptr for the item to
// animate. In this case, the animation skips straight to step 3 above.
class VIEWS_EXPORT MenuClosureAnimationMac final
    : public gfx::AnimationDelegate {
 public:
  // Disables (or, with `disable` = false, enables) animations for the lifetime
  // of this object, restoring the previous state on destruction.
  class VIEWS_EXPORT ScopedDisableAnimationsForTesting {
   public:
    explicit ScopedDisableAnimationsForTesting(bool disable = true);
    ScopedDisableAnimationsForTesting(
        const ScopedDisableAnimationsForTesting&) = delete;
    ScopedDisableAnimationsForTesting& operator=(
        const ScopedDisableAnimationsForTesting&) = delete;
    ~ScopedDisableAnimationsForTesting();

   private:
    const bool previous_value_;
  };

  // After this closure animation is done, |callback| is run to finish
  // dismissing the menu. If |item| is given, this will animate the item being
  // accepted before animating the menu closing; if |item| is nullptr, only the
  // menu closure will be animated.
  MenuClosureAnimationMac(MenuItemView* item,
                          SubmenuView* menu,
                          base::OnceClosure callback);

  MenuClosureAnimationMac(const MenuClosureAnimationMac&) = delete;
  MenuClosureAnimationMac& operator=(const MenuClosureAnimationMac&) = delete;

  ~MenuClosureAnimationMac() override;

  // Start the animation.
  void Start();

  // Returns the MenuItemView this animation targets.
  MenuItemView* item() { return item_; }

  // Returns the SubmenuView this animation targets, or null if it has been
  // destroyed.
  SubmenuView* menu();

 private:
  enum class AnimationStep {
    kStart,
    kUnselected,
    kSelected,
    kFading,
    kFinish,
  };

  AnimationStep NextStepFor(AnimationStep step) const;

  void AdvanceAnimation();

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  base::OnceClosure callback_;
  base::OneShotTimer timer_;
  std::unique_ptr<gfx::Animation> fade_animation_;
  raw_ptr<MenuItemView> item_;
  // Tracked rather than a raw pointer since the submenu may be destroyed while
  // the fade is running (e.g. when the menu model is rebuilt).
  ViewTracker menu_;
  AnimationStep step_ = AnimationStep::kStart;
  base::WeakPtrFactory<MenuClosureAnimationMac> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CLOSURE_ANIMATION_MAC_H_
