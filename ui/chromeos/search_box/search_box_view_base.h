// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_
#define UI_CHROMEOS_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/chromeos/search_box/search_box_export.h"
#include "ui/events/event_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/widget/widget_delegate.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class BoxLayout;
class ImageView;
class Textfield;
class View;
}  // namespace views

namespace search_box {

class SearchBoxViewDelegate;
class SearchBoxImageButton;

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// SearchBoxActivationSource enum listing in tools/metrics/histograms/enums.xml.
enum class ActivationSource {
  kMousePress = 0,
  kKeyPress = 1,
  kGestureTap = 2,
  kMaxValue = kGestureTap,
};

// TODO(wutao): WidgetDelegateView owns itself and cannot be deleted from the
// views hierarchy automatically. Make SearchBoxViewBase a subclass of View
// instead of WidgetDelegateView.
// SearchBoxViewBase consists of icons and a Textfield. The Textfiled is for
// inputting queries and triggering callbacks. The icons include a search icon,
// a close icon and a back icon for different functionalities. This class
// provides common functions for the search box view across Chrome OS.
class SEARCH_BOX_EXPORT SearchBoxViewBase : public views::WidgetDelegateView,
                                            public views::TextfieldController,
                                            public views::ButtonListener {
 public:
  explicit SearchBoxViewBase(SearchBoxViewDelegate* delegate);
  ~SearchBoxViewBase() override;

  void Init();

  bool HasSearch() const;

  // Returns the bounds to use for the view (including the shadow) given the
  // desired bounds of the search box contents.
  gfx::Rect GetViewBoundsForSearchBoxContentsBounds(
      const gfx::Rect& rect) const;

  views::ImageButton* assistant_button();
  views::ImageButton* back_button();
  views::ImageButton* close_button();
  views::Textfield* search_box() { return search_box_; }

  // Swaps the google icon with the back button.
  void ShowBackOrGoogleIcon(bool show_back_button);

  // Setting the search box active left aligns the placeholder text, changes
  // the color of the placeholder text, and enables cursor blink. Setting the
  // search box inactive center aligns the placeholder text, sets the color, and
  // disables cursor blink.
  void SetSearchBoxActive(bool active, ui::EventType event_type);

  // Handles Gesture and Mouse Events sent from |search_box_|.
  bool OnTextfieldEvent(ui::EventType type);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Allows for search box to be notified of gestures occurring outside, without
  // deactivating the searchbox.
  void NotifyGestureEvent();

  // Overridden from views::WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;
  bool ShouldAdvanceFocusToTopLevelWidget() const override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Used only in the tests to get the current search icon.
  views::ImageView* get_search_icon_for_test() { return search_icon_; }

  // Whether the search box is active.
  bool is_search_box_active() const { return is_search_box_active_; }

  void set_show_close_button_when_active(bool show_button) {
    show_close_button_when_active_ = show_button;
  }

  bool show_assistant_button() { return show_assistant_button_; }

  void OnSearchBoxFocusedChanged();

  // Whether the trimmed query in the search box is empty.
  bool IsSearchBoxTrimmedQueryEmpty() const;

  virtual void ClearSearch();

  // Returns selected view in contents view.
  virtual views::View* GetSelectedViewInContentsView();

 protected:
  // Fires query change notification.
  void NotifyQueryChanged();

  // Nofifies the active status change.
  void NotifyActiveChanged();

  // Sets the search box color.
  void SetSearchBoxColor(SkColor color);
  SkColor search_box_color() const { return search_box_color_; }

  // Updates the visibility of close button.
  void UpdateButtonsVisisbility();

  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;
  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

  views::BoxLayout* box_layout() { return box_layout_; }

  void set_is_tablet_mode(bool is_tablet_mode) {
    is_tablet_mode_ = is_tablet_mode;
  }
  bool is_tablet_mode() const { return is_tablet_mode_; }

  void SetSearchBoxBackgroundCornerRadius(int corner_radius);

  void SetSearchIconImage(gfx::ImageSkia image);

  void SetShowAssistantButton(bool show);

  // Detects |ET_MOUSE_PRESSED| and |ET_GESTURE_TAP| events on the white
  // background of the search box.
  virtual void HandleSearchBoxEvent(ui::LocatedEvent* located_event);

  // Updates the search box's background color.
  virtual void UpdateBackgroundColor(SkColor color);

  // Gets the search box background.
  views::Background* GetSearchBoxBackground();

 private:
  virtual void ModelChanged() {}

  // Shows/hides the virtual keyboard if the search box is active.
  virtual void UpdateKeyboardVisibility() {}

  // Updates model text and selection model with current Textfield info.
  virtual void UpdateModel(bool initiated_by_user) {}

  // Updates the search icon.
  virtual void UpdateSearchIcon() {}

  // Update search box border based on whether the search box is activated.
  virtual void UpdateSearchBoxBorder() {}

  // Setup button's image, accessible name, and tooltip text etc.
  virtual void SetupAssistantButton() {}
  virtual void SetupCloseButton() {}
  virtual void SetupBackButton() {}

  // Records in histograms the activation of the searchbox.
  virtual void RecordSearchBoxActivationHistogram(ui::EventType event_type) {}

  void OnEnabledChanged();

  SearchBoxViewDelegate* delegate_;  // Not owned.

  // Owned by views hierarchy.
  views::View* content_container_;
  views::ImageView* search_icon_ = nullptr;
  SearchBoxImageButton* assistant_button_ = nullptr;
  SearchBoxImageButton* back_button_ = nullptr;
  SearchBoxImageButton* close_button_ = nullptr;
  views::Textfield* search_box_;
  views::View* search_box_right_space_ = nullptr;

  // Owned by |content_container_|. It is deleted when the view is deleted.
  views::BoxLayout* box_layout_ = nullptr;

  // Whether the search box is active.
  bool is_search_box_active_ = false;
  // Whether to show close button if the search box is active.
  bool show_close_button_when_active_ = false;
  // Whether to show assistant button.
  bool show_assistant_button_ = false;
  // Whether tablet mode is active.
  bool is_tablet_mode_ = false;
  // The current search box color.
  SkColor search_box_color_ = kDefaultSearchboxColor;

  views::PropertyChangedSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&SearchBoxViewBase::OnEnabledChanged,
                              base::Unretained(this)));

  DISALLOW_COPY_AND_ASSIGN(SearchBoxViewBase);
};

}  // namespace search_box

#endif  // UI_CHROMEOS_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_
