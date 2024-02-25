// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATION_OBSERVER_H_
#define UI_COMPOSITOR_LAYER_ANIMATION_OBSERVER_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer_animation_element.h"

namespace ui {

namespace test {
class CountCheckingLayerAnimationObserver;
class LayerAnimationObserverTestApi;
}  // namespace test

class LayerAnimationSequence;
class ScopedLayerAnimationSettings;
class ImplicitAnimationObserver;

// LayerAnimationObservers are notified when animations complete.
class COMPOSITOR_EXPORT LayerAnimationObserver  {
 public:
  // Called when the |sequence| starts.
  virtual void OnLayerAnimationStarted(LayerAnimationSequence* sequence);

  // Called when the |sequence| ends. Not called if |sequence| is aborted.
  virtual void OnLayerAnimationEnded(
      LayerAnimationSequence* sequence) = 0;

  // Called when a |sequence| repetition ends and will repeat. Not called if
  // |sequence| is aborted.
  virtual void OnLayerAnimationWillRepeat(LayerAnimationSequence* sequence) {}

  // Called if |sequence| is aborted for any reason. Should never do anything
  // that may cause another animation to be started.
  virtual void OnLayerAnimationAborted(
      LayerAnimationSequence* sequence) = 0;

  // Called when the animation is scheduled.
  virtual void OnLayerAnimationScheduled(
      LayerAnimationSequence* sequence) = 0;

 protected:
  typedef std::set<raw_ptr<LayerAnimationSequence, SetExperimental>>
      AttachedSequences;

  LayerAnimationObserver();
  virtual ~LayerAnimationObserver();

  // If the LayerAnimator is destroyed during an animation, the animations are
  // aborted. The resulting NotifyAborted notifications will NOT be sent to
  // this observer if this function returns false. An observer who wants to
  // receive the NotifyAborted notifications during destruction can override
  // this function to return true.
  //
  // *** IMPORTANT ***: If a class overrides this function to return true and
  // that class is a direct or indirect owner of the LayerAnimationSequence
  // being observed, then the class must explicitly remove itself as an
  // observer during destruction of the LayerAnimationObserver! This is to
  // ensure that a partially destroyed observer isn't notified with an
  // OnLayerAnimationAborted() call when the LayerAnimator is destroyed.
  //
  // This opt-in pattern is used because it is common for a class to be the
  // observer of a LayerAnimationSequence that it owns indirectly because it
  // owns the Layer which owns the LayerAnimator which owns the
  // LayerAnimationSequence.
  virtual bool RequiresNotificationWhenAnimatorDestroyed() const;

  // Called when |this| is added to |sequence|'s observer list.
  virtual void OnAttachedToSequence(LayerAnimationSequence* sequence);

  // Called when |this| is removed to |sequence|'s observer list.
  virtual void OnDetachedFromSequence(LayerAnimationSequence* sequence);

  // Called when the relevant animator attaches to an animation timeline.
  virtual void OnAnimatorAttachedToTimeline() {}

  // Called when the relevant animator detaches from an animation timeline.
  virtual void OnAnimatorDetachedFromTimeline() {}

  // Detaches this observer from all sequences it is currently observing.
  void StopObserving();

  const AttachedSequences& attached_sequences() const {
    return attached_sequences_;
  }

 private:
  friend class LayerAnimationSequence;
  friend class test::CountCheckingLayerAnimationObserver;
  friend class test::LayerAnimationObserverTestApi;

  // Called when |this| is added to |sequence|'s observer list.
  void AttachedToSequence(LayerAnimationSequence* sequence);

  // Called when |this| is removed to |sequence|'s observer list.
  // This will only result in notifications if |send_notification| is true.
  void DetachedFromSequence(LayerAnimationSequence* sequence,
                            bool send_notification);

  AttachedSequences attached_sequences_;
};

// An implicit animation observer is intended to be used in conjunction with a
// ScopedLayerAnimationSettings object in order to receive a notification when
// all implicit animations complete.
// TODO(bruthig): Unify the ImplicitAnimationObserver with the
// CallbackLayerAnimationObserver. (See www.crbug.com/542825).
class COMPOSITOR_EXPORT ImplicitAnimationObserver
    : public LayerAnimationObserver {
 public:
  ImplicitAnimationObserver();
  ~ImplicitAnimationObserver() override;

  // Called when the first animation sequence has started.
  virtual void OnImplicitAnimationsScheduled() {}

  virtual void OnImplicitAnimationsCompleted() = 0;

 protected:
  // Deactivates the observer and clears the collection of animations it is
  // waiting for.
  void StopObservingImplicitAnimations();

  // Returns whether animation for |property| was aborted.
  // Note that if the property wasn't animated, then it couldn't have been
  // aborted, so this will return false for that property.
  bool WasAnimationAbortedForProperty(
      LayerAnimationElement::AnimatableProperty property) const;

  // Returns whether animation for |property| was completed successfully.
  // Note that if the property wasn't animated, then it couldn't have been
  // completed, so this will return false for that property.
  bool WasAnimationCompletedForProperty(
      LayerAnimationElement::AnimatableProperty property) const;

 private:
  enum AnimationStatus {
    ANIMATION_STATUS_UNKNOWN,
    ANIMATION_STATUS_COMPLETED,
    ANIMATION_STATUS_ABORTED,
  };

  friend class ScopedLayerAnimationSettings;

  // LayerAnimationObserver implementation
  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override;
  void OnAttachedToSequence(LayerAnimationSequence* sequence) override;
  void OnDetachedFromSequence(LayerAnimationSequence* sequence) override;

  // OnImplicitAnimationsCompleted is not fired unless the observer is active.
  bool active() const { return active_; }
  void SetActive(bool active);

  void CheckCompleted();

  void UpdatePropertyAnimationStatus(LayerAnimationSequence* sequence,
                                     AnimationStatus status);
  AnimationStatus AnimationStatusForProperty(
      LayerAnimationElement::AnimatableProperty property) const;

  bool active_ = false;

  typedef std::map<LayerAnimationElement::AnimatableProperty,
                   AnimationStatus> PropertyAnimationStatusMap;
  PropertyAnimationStatusMap property_animation_status_;

  // True if OnLayerAnimationScheduled() has been called at least once.
  bool first_sequence_scheduled_ = false;

  // For tracking whether this object has been destroyed. Must be last.
  base::WeakPtrFactory<ImplicitAnimationObserver> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATION_OBSERVER_H_
