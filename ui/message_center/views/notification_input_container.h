// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_INPUT_CONTAINER_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_INPUT_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

namespace ui {
class KeyEvent;
class Layer;
}  // namespace ui

namespace views {
class ImageButton;
class InkDropContainerView;
class Textfield;
}  // namespace views

namespace message_center {

class MESSAGE_CENTER_EXPORT NotificationInputDelegate {
 public:
  virtual void OnNotificationInputSubmit(size_t index,
                                         const std::u16string& text) = 0;
  virtual ~NotificationInputDelegate() = default;
};

// A view which shows a textfield with a send button for notifications.
class MESSAGE_CENTER_EXPORT NotificationInputContainer
    : public views::View,
      public views::TextfieldController,
      public views::LayoutDelegate {
  METADATA_HEADER(NotificationInputContainer, views::View)

 public:
  explicit NotificationInputContainer(
      NotificationInputDelegate* delegate = nullptr);
  NotificationInputContainer(const NotificationInputContainer&) = delete;
  NotificationInputContainer& operator=(const NotificationInputContainer&) =
      delete;
  ~NotificationInputContainer() override;

  // Initializes the view after construction. Necessary to ensure the derived
  // class is called for certain functions.
  void Init();

  // Sets `textfield_`'s  `index`. Used to keep track of this views index in the
  // row of actions in the parent view.
  void SetTextfieldIndex(int index);
  size_t GetTextfieldIndex() const;

  // Sets `textfield_`'s placeholder string to `placeholder` or the default if
  // not supplied.
  void SetPlaceholderText(const std::optional<std::u16string>& placeholder);

  // Animates the background, if one exists.
  void AnimateBackground(const ui::Event& event);

  // views::View:
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  void OnThemeChanged() override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnAfterUserAction(views::Textfield* sender) override;

  // Overridden from views::LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  views::Textfield* textfield() const { return textfield_; }
  views::ImageButton* button() const { return button_; }

 private:
  // Installs the layout manager with appropriate spacing.
  virtual views::BoxLayout* InstallLayoutManager();

  // Installs the ink drop, and layers required to animate it without occluding
  // the textfield and button.
  virtual views::InkDropContainerView* InstallInkDrop();

  // Gets padding for `textfield_`.
  virtual gfx::Insets GetTextfieldPadding() const;

  // Gets padding for `button_`.
  virtual gfx::Insets GetSendButtonPadding() const;

  // Sets the custom highlight path for `button_`.
  virtual void SetSendButtonHighlightPath();

  // Gets the id for the default placeholder string for `textfield_`.
  virtual int GetDefaultPlaceholderStringId() const;

  // Gets the id for the default accessible name string for `button_`.
  virtual int GetDefaultAccessibleNameStringId() const;

  // Sets the visible background of `textfield_`.
  virtual void StyleTextfield();

  // Updates image and coloring for `button_`.
  virtual void UpdateButtonImage();

  // Unowned.
  const raw_ptr<NotificationInputDelegate> delegate_;

  // Owned by view hierarchy.
  const raw_ptr<views::Textfield> textfield_;
  // Owned by view hierarchy.
  const raw_ptr<views::ImageButton> button_;
  // Owned by the view hierarchy. Used to show an inkdrop effect during
  // animation.
  raw_ptr<views::InkDropContainerView> ink_drop_container_ = nullptr;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_INPUT_CONTAINER_H_
