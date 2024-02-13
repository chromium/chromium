// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_control_button_factory.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace message_center {

class MessageView;

class MESSAGE_CENTER_EXPORT NotificationControlButtonsView
    : public views::View {
  METADATA_HEADER(NotificationControlButtonsView, views::View)

 public:
  explicit NotificationControlButtonsView(MessageView* message_view = nullptr);
  NotificationControlButtonsView(const NotificationControlButtonsView&) =
      delete;
  NotificationControlButtonsView& operator=(
      const NotificationControlButtonsView&) = delete;
  ~NotificationControlButtonsView() override;

  // Default control button icons.
  inline static const gfx::VectorIcon& kDefaultCloseIcon =
      kNotificationCloseButtonIcon;
  inline static const gfx::VectorIcon& kDefaultSettingsIcon =
      kNotificationSettingsButtonIcon;
  inline static const gfx::VectorIcon& kDefaultSnoozeIcon =
      kNotificationSnoozeButtonIcon;

  // Default horizontal spacing between control buttons.
  constexpr static int kDefaultBetweenButtonSpacing = 0;

  void OnThemeChanged() override;

  // Change the visibility of the close button. True to show, false to hide.
  void ShowCloseButton(bool show);
  // Change the visibility of the settings button. True to show, false to hide.
  void ShowSettingsButton(bool show);
  // Change the visibility of the settings button. True to show, false to hide.
  // Default: hidden.
  void ShowSnoozeButton(bool show);
  // Change the visibility of all buttons. True to show, false to hide.
  void ShowButtons(bool show);

  // Return the focus status of any button. True if the focus is on any button,
  // false otherwise.
  bool IsAnyButtonFocused() const;

  // Sets the icon for the close, settings, and snooze buttons. Note that this
  // will only have an effect the next time the buttons are shown. That is, if
  // the XYZ button is currently visible when this method is called, then
  // `SetXYZButtonIcon(false)` followed (eventually) by `SetXYZButtonIcon(true)`
  // will need to occur before the new icon will be used. Of course, if the XYZ
  // button is currently not visible when this method is called, then the new
  // icon will be used the next time `SetXYZButtonIcon(true)` occurs.
  void SetCloseButtonIcon(const gfx::VectorIcon& icon);
  void SetSettingsButtonIcon(const gfx::VectorIcon& icon);
  void SetSnoozeButtonIcon(const gfx::VectorIcon& icon);

  // Sets the icon size for the close, settings, and snooze buttons. Like
  // `SetXYZButtonIcon()` above, this will only have an effect the next time the
  // buttons are shown. Note that setting this to 0 (which is the default value)
  // means `gfx::GetDefaultSizeOfVectorIcon()` is used to determine the icon
  // size.
  void SetButtonIconSize(int size);

  // Sets the icon color for the close, settings, and snooze buttons.
  void SetButtonIconColors(SkColor color);

  // Sets the background color to ensure proper readability.
  void SetBackgroundColor(SkColor color);

  // Sets the horizontal spacing between buttons. Invalidates the layout.
  void SetBetweenButtonSpacing(int spacing);

  void SetMessageView(MessageView* message_view);

  void SetNotificationControlButtonFactory(
      std::unique_ptr<NotificationControlButtonFactory>
          notification_control_button_factory);

  // Methods for retrieving the control buttons directly.
  views::ImageButton* close_button() { return close_button_; }
  views::ImageButton* settings_button() { return settings_button_; }
  views::ImageButton* snooze_button() { return snooze_button_; }

 private:
  // Updates the `views::LayoutManager` used to lay out this `views::View`'s
  // children.
  void UpdateLayoutManager();

  // Updates the button icon colors to the value of DetermineButtonIconColor().
  void UpdateButtonIconColors();

  // Determines the button icon color to use given |icon_color_| and
  // |background_color_| ensuring readability.
  SkColor DetermineButtonIconColor() const;

  // Returns the icon for the close, settings, and snooze buttons.
  const gfx::VectorIcon& GetCloseButtonIcon() const;
  const gfx::VectorIcon& GetSettingsButtonIcon() const;
  const gfx::VectorIcon& GetSnoozeButtonIcon() const;

  raw_ptr<MessageView> message_view_;
  raw_ptr<const gfx::VectorIcon> close_button_icon_ = nullptr;
  raw_ptr<const gfx::VectorIcon> settings_button_icon_ = nullptr;
  raw_ptr<const gfx::VectorIcon> snooze_button_icon_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> close_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> settings_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> snooze_button_ = nullptr;

  // The color used for the close, settings, and snooze icons.
  std::optional<SkColor> icon_color_;

  // The background color for readability of the icons.
  SkColor background_color_ = SK_ColorTRANSPARENT;

  // The horizontal spacing between buttons.
  int between_button_spacing_ = kDefaultBetweenButtonSpacing;

  // The size of an icon within one of the control buttons. When set to 0, which
  // is the default value, then `gfx::GetDefaultSizeOfVectorIcon()` is used to
  // determine the icon size. Otherwise this value is used directly.
  int icon_size_ = 0;

  // Owned by this `views::View`:
  std::unique_ptr<NotificationControlButtonFactory>
      notification_control_button_factory_;
};

BEGIN_VIEW_BUILDER(MESSAGE_CENTER_EXPORT,
                   NotificationControlButtonsView,
                   views::View)
VIEW_BUILDER_PROPERTY(MessageView*, MessageView)
VIEW_BUILDER_PROPERTY(const gfx::VectorIcon&,
                      CloseButtonIcon,
                      const gfx::VectorIcon&)
VIEW_BUILDER_PROPERTY(const gfx::VectorIcon&,
                      SettingsButtonIcon,
                      const gfx::VectorIcon&)
VIEW_BUILDER_PROPERTY(const gfx::VectorIcon&,
                      SnoozeButtonIcon,
                      const gfx::VectorIcon&)
VIEW_BUILDER_PROPERTY(int, ButtonIconSize)
VIEW_BUILDER_PROPERTY(SkColor, ButtonIconColors)
VIEW_BUILDER_PROPERTY(int, BetweenButtonSpacing)
VIEW_BUILDER_PROPERTY(std::unique_ptr<NotificationControlButtonFactory>,
                      NotificationControlButtonFactory)
END_VIEW_BUILDER

}  // namespace message_center

DEFINE_VIEW_BUILDER(MESSAGE_CENTER_EXPORT,
                    message_center::NotificationControlButtonsView)

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_
