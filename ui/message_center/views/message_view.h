// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
}  // namespace views

namespace message_center {

namespace test {
class MessagePopupCollectionTest;
}

class Notification;
class NotificationControlButtonsView;

// An base class for a notification entry. Contains background and other
// elements shared by derived notification views.
// TODO(pkasting): This class only subclasses InkDropHostView because the
// NotificationViewMD subclass needs ink drop functionality.  Rework ink drops
// to not need to be the base class of views which use them, and move the
// functionality to the subclass that uses these.
class MESSAGE_CENTER_EXPORT MessageView
    : public views::InkDropHostView,
      public views::SlideOutControllerDelegate,
      public views::FocusChangeListener {
 public:
  static const char kViewClassName[];

  class SlideObserver {
   public:
    virtual ~SlideObserver() = default;

    virtual void OnSlideStarted(const std::string& notification_id) {}
    virtual void OnSlideChanged(const std::string& notification_id) {}
    virtual void OnSlideOut(const std::string& notification_id) {}
  };

  enum class Mode {
    // Normal mode.
    NORMAL = 0,
    // "Pinned" mode flag. This mode is for pinned notification.
    // When this mode is enabled:
    //  - Swipe: partially possible, but limited to the half position
    //  - Close button: hidden
    //  - Settings and snooze button: visible
    PINNED = 1,
    // "Setting" mode flag. This mode is for showing inline setting panel in
    // the notification view.
    // When this mode is enabled:
    //  - Swipe: prohibited
    //  - Close button: hidden
    //  - Settings and snooze button: hidden
    SETTING = 2,
  };

  explicit MessageView(const Notification& notification);
  ~MessageView() override;

  // Updates this view with the new data contained in the notification.
  virtual void UpdateWithNotification(const Notification& notification);

  // Creates a shadow around the notification and changes slide-out behavior.
  void SetIsNested();

  virtual NotificationControlButtonsView* GetControlButtonsView() const = 0;

  virtual void SetExpanded(bool expanded);
  virtual bool IsExpanded() const;
  virtual bool IsAutoExpandingAllowed() const;
  virtual bool IsManuallyExpandedOrCollapsed() const;
  virtual void SetManuallyExpandedOrCollapsed(bool value);
  virtual void CloseSwipeControl();
  virtual void SlideOutAndClose(int direction);

  // Update corner radii of the notification. Subclasses will override this to
  // implement rounded corners if they don't use MessageView's default
  // background.
  virtual void UpdateCornerRadius(int top_radius, int bottom_radius);

  // Invoked when the container view of MessageView (e.g. MessageCenterView in
  // ash) is starting the animation that possibly hides some part of
  // the MessageView.
  // During the animation, MessageView should comply with the Z order in views.
  virtual void OnContainerAnimationStarted();
  virtual void OnContainerAnimationEnded();

  void OnCloseButtonPressed();
  virtual void OnSettingsButtonPressed(const ui::Event& event);
  virtual void OnSnoozeButtonPressed(const ui::Event& event);

  // views::InkDropHostView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBlur() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void RemovedFromWidget() override;
  void AddedToWidget() override;
  const char* GetClassName() const final;

  // views::SlideOutControllerDelegate:
  ui::Layer* GetSlideOutLayer() override;
  void OnSlideStarted() override;
  void OnSlideChanged(bool in_progress) override;
  void OnSlideOut() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  void AddSlideObserver(SlideObserver* observer);
  void RemoveSlideObserver(SlideObserver* observer);

  Mode GetMode() const;

  // Gets the current horizontal scroll offset of the view by slide gesture.
  float GetSlideAmount() const;

  // Set "setting" mode. This overrides "pinned" mode. See the comment of
  // MessageView::Mode enum for detail.
  void SetSettingMode(bool setting_mode);

  // Disables slide by vertical swipe regardless of the current notification
  // mode.
  void DisableSlideForcibly(bool disable);

  // Updates the width of the buttons which are hidden and avail by swipe.
  void SetSlideButtonWidth(int coutrol_button_width);

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }
  std::string notification_id() const { return notification_id_; }

 protected:
  virtual void UpdateControlButtonsVisibility();

  // Changes the background color and schedules a paint.
  virtual void SetDrawBackgroundAsActive(bool active);

  void SetCornerRadius(int top_radius, int bottom_radius);

  views::ScrollView* scroller() { return scroller_; }

  bool is_nested() const { return is_nested_; }

 private:
  friend class test::MessagePopupCollectionTest;

  class HighlightPathGenerator;

  // Gets the highlight path for the notification based on bounds and corner
  // radii.
  SkPath GetHighlightPath() const;

  // Returns the ideal slide mode by calculating the current status.
  views::SlideOutController::SlideMode CalculateSlideMode() const;

  // Returns if the control buttons should be shown.
  bool ShouldShowControlButtons() const;

  std::string notification_id_;
  views::ScrollView* scroller_ = nullptr;

  base::string16 accessible_name_;

  // Flag if the notification is set to pinned or not. See the comment in
  // MessageView::Mode for detail.
  bool pinned_ = false;

  // "fixed" mode flag. See the comment in MessageView::Mode for detail.
  bool setting_mode_ = false;

  views::SlideOutController slide_out_controller_;
  base::ObserverList<SlideObserver>::Unchecked slide_observers_;

  // True if |this| is embedded in another view. Equivalent to |!top_level| in
  // MessageViewFactory parlance.
  bool is_nested_ = false;

  // True if the slide is disabled forcibly.
  bool disable_slide_ = false;

  views::FocusManager* focus_manager_ = nullptr;
  std::unique_ptr<views::FocusRing> focus_ring_;

  // Radius values used to determine the rounding for the rounded rectangular
  // shape of the notification.
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
