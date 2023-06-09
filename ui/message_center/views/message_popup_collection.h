// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/views/message_view.h"
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
      public NotificationViewController,
      public gfx::AnimationDelegate {
 public:
  MessagePopupCollection();
  MessagePopupCollection(const MessagePopupCollection& other) = delete;
  MessagePopupCollection& operator=(const MessagePopupCollection& other) =
      delete;
  ~MessagePopupCollection() override;

  // Update popups based on current |state_|.
  void Update();

  // Reset all popup positions. Called from PopupAlignmentDelegate when
  // alignment and work area might be changed.
  void ResetBounds();

  // Notify the popup size is changed. Called from MessagePopupView.
  void NotifyPopupResized();

  // Notify the popup is closed. Called from MessagePopupView.
  virtual void NotifyPopupClosed(MessagePopupView* popup);

  // NotificationViewController:
  void AnimateResize() override;
  MessageView* GetMessageViewForNotificationId(
      const std::string& notification_id) override;
  void ConvertNotificationViewToGroupedNotificationView(
      const std::string& ungrouped_notification_id,
      const std::string& new_grouped_notification_id) override;
  void ConvertGroupedNotificationViewToNotificationView(
      const std::string& grouped_notification_id,
      const std::string& new_single_notification_id) override;

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

  // Called when a new popup appears or popups are rearranged in the |display|.
  // The subclass may override this method to check the current desktop status
  // so that the popups are arranged at the correct place. Return true if
  // alignment is actually changed.
  virtual bool RecomputeAlignment(const display::Display& display) = 0;

  // Sets the parent container for popups. If it does not set a parent a
  // default parent will be used (e.g. the native desktop on Windows).
  virtual void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      views::Widget::InitParams* init_params) = 0;

  gfx::Rect popup_collection_bounds() { return popup_collection_bounds_; }

 protected:
  // Returns the x-origin for the given popup bounds in the current work area.
  virtual int GetPopupOriginX(const gfx::Rect& popup_bounds) const = 0;

  // Returns the baseline height of the current work area. That is the starting
  // point if there are no other popups.
  virtual int GetBaseline() const = 0;

  // Returns the rect of the current work area.
  virtual gfx::Rect GetWorkArea() const = 0;

  // Returns true if the popup should be aligned top down.
  virtual bool IsTopDown() const = 0;

  // Returns true if the popups are positioned at the left side of the desktop
  // so that their reveal animation should happen from left side.
  virtual bool IsFromLeft() const = 0;

  // Returns true if the display which notifications show on is the primary
  // display.
  virtual bool IsPrimaryDisplayForNotification() const = 0;

  // Returns true if |notification| should be blocked because this display to
  // show the notification is fullscreen. If all (1 of 1, or n of n) displays
  // are fullscreen, the notification will already be blocked by the associated
  // FullscreenNotificationBlocker, but this function is required for the case
  // where there are multiple displays, and the notification should be blocked
  // on those that are fullscreen, but displayed on the others.
  //
  // This function can return false when only a single display is supported
  // since FullscreenNotificationBlocker will have already blocked anything.
  virtual bool BlockForMixedFullscreen(
      const Notification& notification) const = 0;

  // Called when a new popup item is added.
  virtual void NotifyPopupAdded(MessagePopupView* popup) {}
  // Called with |notification_id| when a popup is marked to be removed.
  virtual void NotifyPopupRemoved(const std::string& notification_id) {}

  // Called when popup animation is started/finished.
  virtual void AnimationStarted() {}
  virtual void AnimationFinished() {}

  // TODO(crbug/1241602): std::unique_ptr can be used here and multiple other
  // places.
  virtual MessagePopupView* CreatePopup(const Notification& notification);

  // virtual for testing.
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

    // If the popup is animating.
    bool is_animating = false;

    // Unowned.
    raw_ptr<MessagePopupView, DanglingUntriaged> popup = nullptr;
  };

  // Transition from animation state (FADE_IN, FADE_OUT, and MOVE_DOWN) to
  // IDLE state or next animation state (MOVE_DOWN).
  void TransitionFromAnimation();

  // Transition from IDLE state to animation state (FADE_IN, FADE_OUT or
  // MOVE_DOWN).
  void TransitionToAnimation();

  // Pause or restart popup timers depending on |state_|.
  void UpdatePopupTimers();

  // Calculate and update the bounds for all popups, including moving old
  // `bounds` to `start_bounds` and updating `popup_collection_bounds_`.
  void CalculateAndUpdateBounds();

  // Update bounds and opacity of popups during animation.
  void UpdateByAnimation();

  // Get popup notifications in sort order from MessageCenter, filtered for any
  // that should not show on this display.
  std::vector<Notification*> GetPopupNotifications() const;

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
  // Return true if any popup is focused.
  bool IsAnyPopupFocused() const;

  // Returns the popup which is visually |index_from_top|-th from the top.
  PopupItem* GetPopupItem(size_t index_from_top);

  // Reset |recently_closed_by_user_| to false. Used by
  // |recently_closed_by_user_timer_|
  void ResetRecentlyClosedByUser();

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

  // The bounds of the entire popup collection.
  gfx::Rect popup_collection_bounds_;

  base::ScopedObservation<MessageCenter, MessageCenterObserver>
      message_center_observation_{this};

  base::WeakPtrFactory<MessagePopupCollection> weak_ptr_factory_{this};
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
