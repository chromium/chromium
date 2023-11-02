// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_ANIMATION_DELEGATE_NOTIFIER_H_
#define UI_GFX_ANIMATION_ANIMATION_DELEGATE_NOTIFIER_H_

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {

// AnimationDelegateNotifier adapts AnimationDelegate (which is used by
// inheritance) into an object that is used by composition. This can be useful
// to compose the functionality of an AnimationDelegate subclass into an object
// that inherits directly from AnimationDelegate.
template <class AnimationDelegateType = gfx::AnimationDelegate>
class AnimationDelegateNotifier : public AnimationDelegateType {
 public:
  template <typename... Args>
  AnimationDelegateNotifier(gfx::AnimationDelegate* owner, Args&&... args)
      : AnimationDelegateType(std::forward<Args>(args)...), owner_(owner) {
    DCHECK(owner_);
  }

  ~AnimationDelegateNotifier() override = default;

  // AnimationDelegateType:
  void AnimationEnded(const Animation* animation) override {
    AnimationDelegateType::AnimationEnded(animation);
    owner_->AnimationEnded(animation);
  }

  void AnimationProgressed(const Animation* animation) override {
    AnimationDelegateType::AnimationProgressed(animation);
    owner_->AnimationProgressed(animation);
  }

  void AnimationCanceled(const Animation* animation) override {
    AnimationDelegateType::AnimationCanceled(animation);
    owner_->AnimationCanceled(animation);
  }

  void AnimationContainerWasSet(AnimationContainer* container) override {
    AnimationDelegateType::AnimationContainerWasSet(container);
    owner_->AnimationContainerWasSet(container);
  }

 private:
  const raw_ptr<gfx::AnimationDelegate> owner_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_ANIMATION_DELEGATE_NOTIFIER_H_
