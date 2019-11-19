// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class LinearAnimation;
}  // namespace gfx

namespace display {
class Display;
}  // namespace display

namespace message_center {

class MessagePopupView;
class Notification;
class PopupAlignmentDelegate;

// Container of notification popups usually shown at the right bottom of the
// screen. Manages animation state and updates these popup widgets.
class MESSAGE_CENTER_EXPORT MessagePopupCollection
    : public MessageCenterObserver,
      public gfx::AnimationDelegate {
 public:
  MessagePopupCollection();
  ~MessagePopupCollection() override;

  // Update popups based on current |state_|.
  void Update();

  // Reset all popup positions. Called from PopupAlignmentDelegate when
  // alignment and work area might be changed.
  void ResetBounds();

  // Notify the popup size is changed. Called from MessagePopupView.
  void NotifyPopupResized();

  // Notify the popup is closed. Called from MessagePopupView.
  void NotifyPopupClosed(MessagePopupView* popup);

  // MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnCenterVisibilityChanged(Visibility visibility) override;
  void OnBlockingStateChanged(NotificationBlocker* blocker) override;

  // AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Find the message popup view for the given notification id. Return nullptr
  // if it does not exist.
  MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id);

  // Called when a new toast appears or toasts are rearranged in the |display|.
  // The subclass may override this method to check the current desktop status
  // so that the toasts are arranged at the correct place. Return true if
  // alignment is actually changed.
  virtual bool RecomputeAlignment(const display::Display& display) = 0;

  // Sets the parent container for popups. If it does not set a parent a
  // default parent will be used (e.g. the native desktop on Windows).
  virtual void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      views::Widget::InitParams* init_params) = 0;

  void set_inverse() { inverse_ = true; }

 protected:
  // Returns the x-origin for the given toast bounds in the current work area.
  virtual int GetToastOriginX(const gfx::Rect& toast_bounds) const = 0;

  // Returns the baseline height of the current work area. That is the starting
  // point if there are no other toasts.
  virtual int GetBaseline() const = 0;

  // Returns the rect of the current work area.
  virtual gfx::Rect GetWorkArea() const = 0;

  // Returns true if the toast should be aligned top down.
  virtual bool IsTopDown() const = 0;

  // Returns true if the toasts are positioned at the left side of the desktop
  // so that their reveal animation should happen from left side.
  virtual bool IsFromLeft() const = 0;

  // Returns true if the display which notifications show on is the primary
  // display.
  virtual bool IsPrimaryDisplayForNotification() const = 0;

  // virtual for testing.
  virtual MessagePopupView* CreatePopup(const Notification& notification);
  virtual void RestartPopupTimers();
  virtual void PausePopupTimers();

  gfx::LinearAnimation* animation() { return animation_.get(); }

 private:
  // MessagePopupCollection always runs single animation at one time.
  // State is an enum of which animation is running right now.
  // If |state_| is IDLE, animation_->is_animating() is always false and vice
  // versa.
  enum class State {
    // No animation is running.
    IDLE,

    // Fading in an added notification.
    FADE_IN,

    // Fading out a removed notification. After the animation, if there are
    // still remaining notifications, it will transition to MOVE_DOWN.
    FADE_OUT,

    // Moving down notifications. Notification collapsing and resizing are also
    // done in MOVE_DOWN.
    MOVE_DOWN,

    // Moving up notifications in order to show new one by FADE_IN. This is only
    // used when |inverse_| is true.
    MOVE_UP_FOR_INVERSE
  };

  // Stores animation related state of a popup.
  struct PopupItem {
    // Notification ID.
    std::string id;

    // The bounds that the popup starts animating from.
    // If |is_animating| is false, it is ignored. Also the value is only used
    // when the animation type is FADE_IN or MOVE_DOWN.
    gfx::Rect start_bounds;

    // The final bounds of the popup.
    gfx::Rect bounds;

    // The popup is waiting for MOVE_UP_FOR_INVERSE animation so that it can
    // FADE_IN after that. The value is only used when the animation type is
    // MOVE_UP_FOR_INVERSE.
    bool will_fade_in = false;

    // If the popup is animating.
    bool is_animating = false;

    // Unowned.
    MessagePopupView* popup = nullptr;
  };

  // Transition from animation state (FADE_IN, FADE_OUT, and MOVE_DOWN) to
  // IDLE state or next animation state (MOVE_DOWN).
  void TransitionFromAnimation();

  // Transition from IDLE state to animation state (FADE_IN, FADE_OUT or
  // MOVE_DOWN).
  void TransitionToAnimation();

  // Pause or restart popup timers depending on |state_|.
  void UpdatePopupTimers();

  // Calculate |bounds| of all popups and moves old |bounds| to |start_bounds|.
  void CalculateBounds();

  // Update bounds and opacity of popups during animation.
  void UpdateByAnimation();

  // Add a new popup to |popup_items_| for FADE_IN animation.
  // Return true if a popup is actually added. It may still return false when
  // HasAddedPopup() return true by the lack of work area to show popup.
  bool AddPopup();

  // Mark |is_animating| flag of removed popup to true for FADE_OUT animation.
  void MarkRemovedPopup();

  // Mark |is_animating| flag of all popups for MOVE_DOWN animation.
  void MoveDownPopups();

  // Get the y-axis edge of the new popup. In usual bottom-to-top layout, it
  // means the topmost y-axis when |item| is added.
  int GetNextEdge(const PopupItem& item) const;

  // Returns true if the edge is outside work area.
  bool IsNextEdgeOutsideWorkArea(const PopupItem& item) const;

  // Implements hot mode. The purpose of hot mode is to allow a user to
  // continually close many notifications by mouse without moving it. Similar
  // functionality is also implemented in browser tab strips.
  void StartHotMode();
  void ResetHotMode();

  void CloseAnimatingPopups();
  bool CloseTransparentPopups();
  void ClosePopupsOutsideWorkArea();
  void RemoveClosedPopupItems();

  // Stops all the animation and closes all the popups immediately.
  void CloseAllPopupsNow();

  // Collapse all existing popups. Return true if size of any popup is actually
  // changed.
  bool CollapseAllPopups();

  // Return true if there is a new popup to add.
  bool HasAddedPopup() const;
  // Return true is there is a old popup to remove.
  bool HasRemovedPopup() const;

  // Return true if any popup is hovered by mouse.
  bool IsAnyPopupHovered() const;
  // Return true if any popup is activated.
  bool IsAnyPopupActive() const;

  // Returns the popup which is visually |index_from_top|-th from the top.
  // When |inverse_| is false, it's same as popup_items_[i].
  PopupItem* GetPopupItem(size_t index_from_top);

  // Animation state. See the comment of State.
  State state_ = State::IDLE;

  // Covers all animation performed by MessagePopupCollection. When the
  // animation is running, it is always one of FADE_IN (sliding in and opacity
  // change), FADE_OUT (opacity change), and MOVE_DOWN (sliding down).
  // MessagePopupCollection does not use implicit animation. The position and
  // opacity changes are explicitly set from UpdateByAnimation().
  const std::unique_ptr<gfx::LinearAnimation> animation_;

  // Notification popups. The first element is the oldest one.
  std::vector<PopupItem> popup_items_;

  // True during Update() to avoid reentrancy. For example, popup size change
  // might be notified during Update() because Update() changes popup sizes, but
  // popup might change the size by itself e.g. expanding notification by mouse.
  bool is_updating_ = false;

  // If true, popup sizes are resized on the next time Update() is called with
  // IDLE state.
  bool resize_requested_ = false;

  // Hot mode related variables. See StartHotMode() and ResetHotMode().

  // True if the close button of the popup at |hot_index_| is hot.
  bool is_hot_ = false;

  // An index in |popup_items_|. Only valid if |is_hot_| is true.
  size_t hot_index_ = 0;

  // Fixed Y coordinate of the popup at |hot_index_|. While |is_hot_| is true,
  // CalculateBounds() always lays out popups in a way the top of the popup at
  // |hot_index_| is aligned to |hot_top_|. Only valid if |is_hot_| is true.
  int hot_top_ = 0;

  // Invert ordering of notification popups i.e. showing the latest notification
  // at the top. It changes the state transition like this:
  // Normal:
  //   * a new notification comes in: FADE_IN
  //   * a notification comes out: FADE_OUT -> MOVE_DOWN
  // Inverted:
  //   * a new notification comes in: MOVE_UP_FOR_INVERSE -> FADE_IN
  //   * a notification comes out: FADE_OUT
  bool inverse_ = false;

  base::WeakPtrFactory<MessagePopupCollection> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MessagePopupCollection);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
