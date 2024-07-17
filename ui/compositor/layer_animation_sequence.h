// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATION_SEQUENCE_H_
#define UI_COMPOSITOR_LAYER_ANIMATION_SEQUENCE_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer_animation_element.h"

namespace ui {

class LayerAnimationDelegate;
class LayerAnimationObserver;

// Contains a collection of layer animation elements to be played one after
// another. Although it has a similar interface to LayerAnimationElement, it is
// not a LayerAnimationElement (i.e., it is not permitted to have a sequence in
// a sequence). Sequences own their elements, and sequences are themselves owned
// by a LayerAnimator.
//
// TODO(vollick) Create a 'blended' sequence for transitioning between
// sequences.
class COMPOSITOR_EXPORT LayerAnimationSequence {
 public:
  LayerAnimationSequence();
  // Takes ownership of the given element and adds it to the sequence.
  explicit LayerAnimationSequence(
      std::unique_ptr<LayerAnimationElement> element);

  LayerAnimationSequence(const LayerAnimationSequence&) = delete;
  LayerAnimationSequence& operator=(const LayerAnimationSequence&) = delete;

  virtual ~LayerAnimationSequence();

  // Sets the start time for the animation. This must be called before the
  // first call to {Start, IsFinished}. Once the animation is finished, this
  // must be called again in order to restart the animation.
  void set_start_time(base::TimeTicks start_time) { start_time_ = start_time; }
  base::TimeTicks start_time() const { return start_time_; }

  // Sets a flag indicating that this sequence will start together with other
  // sequences, and at least one of the sequences in this group has a threaded
  // first element.
  void set_waiting_for_group_start(bool waiting) {
    waiting_for_group_start_ = waiting;
  }
  bool waiting_for_group_start() { return waiting_for_group_start_; }

  // This must be called before the first call to Progress. If starting the
  // animation involves dispatching to another thread, then this will proceed
  // with that dispatch, ultimately resulting in the animation getting an
  // effective start time (the time the animation starts on the other thread).
  void Start(LayerAnimationDelegate* delegate);

  // Updates the delegate to the appropriate value for |now|. Requests a
  // redraw if it is required.
  void Progress(base::TimeTicks now, LayerAnimationDelegate* delegate);

  // Returns true if calling Progress now, with the given time, will finish
  // the animation.
  bool IsFinished(base::TimeTicks time);

  // Updates the delegate to the end of the animation; if this sequence is
  // repeating, updates the delegate to the end of one repetition of the
  // sequence.
  void ProgressToEnd(LayerAnimationDelegate* delegate);

  // Sets the target value to the value that would have been set had
  // the sequence completed. Does nothing if the sequence is repeating.
  void GetTargetValue(LayerAnimationElement::TargetValue* target) const;

  // Aborts the given animation.
  void Abort(LayerAnimationDelegate* delegate);

  // All properties modified by the sequence.
  LayerAnimationElement::AnimatableProperties properties() const {
    return properties_;
  }

  // Adds an element to the sequence. The sequences takes ownership of this
  // element.
  void AddElement(std::unique_ptr<LayerAnimationElement> element);

  // Sequences can repeat indefinitely.
  void set_is_repeating(bool is_repeating) { is_repeating_ = is_repeating; }
  bool is_repeating() const { return is_repeating_; }

  // Returns true if this sequence has at least one element conflicting with a
  // property in |other|.
  bool HasConflictingProperty(
      LayerAnimationElement::AnimatableProperties other) const;

  // Returns true if the first element animates on the compositor thread.
  bool IsFirstElementThreaded(LayerAnimationDelegate* delegate) const;

  // Used to identify groups of sequences that are supposed to start together.
  // Once started, used to identify the sequence that owns a particular
  // threaded animation.
  int animation_group_id() const { return animation_group_id_; }
  void set_animation_group_id(int id) { animation_group_id_ = id; }

  // These functions are used for adding or removing observers from the observer
  // list. The observers are notified when animations end.
  void AddObserver(LayerAnimationObserver* observer);
  void RemoveObserver(LayerAnimationObserver* observer);

  // Called when a threaded animation is actually started.
  void OnThreadedAnimationStarted(base::TimeTicks monotonic_time,
                                  cc::TargetProperty::Type target_property,
                                  int group_id);

  // Called when the animator schedules this sequence.
  void OnScheduled();

  // Called when the animator is destroyed.
  void OnAnimatorDestroyed();

  // Called when the animator is attached to/detached from a Compositor.
  void OnAnimatorAttached(LayerAnimationDelegate* delegate);
  void OnAnimatorDetached();

  // The last_progressed_fraction of the element most recently progressed by
  // by this sequence. Returns 0.0 if no elements have been progressed.
  double last_progressed_fraction() const { return last_progressed_fraction_; }

  size_t size() const;

  LayerAnimationElement* FirstElement() const;

  std::string ToString() const;

  base::WeakPtr<LayerAnimationSequence> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class LayerAnimatorTestController;

  using Elements = std::vector<std::unique_ptr<LayerAnimationElement>>;

  FRIEND_TEST_ALL_PREFIXES(LayerAnimatorTest,
                           ObserverReleasedBeforeAnimationSequenceEnds);

  std::string ElementsToString() const;

  // Notifies the observers that this sequence has been scheduled.
  void NotifyScheduled();

  // Notifies the observers that this sequence has been started.
  void NotifyStarted();

  // Notifies the observers that this sequence has ended.
  void NotifyEnded();

  // Notifies the observers that this sequence has ended and will repeat.
  void NotifyWillRepeat();

  // Notifies the observers that this sequence has been aborted.
  void NotifyAborted();

  // The currently animating element.
  LayerAnimationElement* CurrentElement() const;

  // Returns the total duration of all `LayerAnimationElement`s in the sequence.
  base::TimeDelta GetTotalDurationOfAllElements() const;

  // The union of all the properties modified by all elements in the sequence.
  LayerAnimationElement::AnimatableProperties properties_;

  // The elements in the sequence.
  Elements elements_;

  // True if the sequence should be looped forever.
  bool is_repeating_;

  // These are used when animating to efficiently find the next element.
  size_t last_element_;
  base::TimeTicks last_start_;

  // The start time of the current run of the sequence.
  base::TimeTicks start_time_;

  // True if this sequence will start together with other sequences, and at
  // least one of the sequences in this group has a threaded first element.
  bool waiting_for_group_start_;

  // Identifies groups of sequences that are supposed to start together.
  // Also used to identify the owner of a particular threaded animation; any
  // in-progress threaded animation owned by this sequence will have this
  // group id.
  int animation_group_id_;

  // These parties are notified when layer animations end.
  base::ObserverList<LayerAnimationObserver>::UncheckedAndDanglingUntriaged
      observers_;

  // Tracks the last_progressed_fraction() of the most recently progressed
  // element.
  double last_progressed_fraction_;

  // TODO(vollick) Eventually, the LayerAnimator will switch to a model where
  // new work is scheduled rather than calling methods directly. This should
  // make it impossible for temporary pointers to running animations to go
  // stale. When this happens, there will be no need for LayerAnimationSequences
  // to support weak pointers.
  base::WeakPtrFactory<LayerAnimationSequence> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATION_SEQUENCE_H_
