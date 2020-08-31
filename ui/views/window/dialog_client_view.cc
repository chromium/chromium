// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_client_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {

// The group used by the buttons.  This name is chosen voluntarily big not to
// conflict with other groups that could be in the dialog content.
constexpr int kButtonGroup = 6666;

// Returns true if the given view should be shown (i.e. exists and is
// visible).
bool ShouldShow(View* view) {
  return view && view->GetVisible();
}

// Returns the bounding box required to contain |size1| and |size2|, placed one
// atop the other.
gfx::Size GetBoundingSizeForVerticalStack(const gfx::Size& size1,
                                          const gfx::Size& size2) {
  return gfx::Size(std::max(size1.width(), size2.width()),
                   size1.height() + size2.height());
}

}  // namespace

// Simple container to bubble child view changes up the view hierarchy.
class DialogClientView::ButtonRowContainer : public View {
 public:
  explicit ButtonRowContainer(DialogClientView* owner) : owner_(owner) {}

  // View:
  void ChildPreferredSizeChanged(View* child) override {
    owner_->ChildPreferredSizeChanged(child);
  }
  void ChildVisibilityChanged(View* child) override {
    owner_->ChildVisibilityChanged(child);
  }

 private:
  DialogClientView* const owner_;

  DISALLOW_COPY_AND_ASSIGN(ButtonRowContainer);
};

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, public:

DialogClientView::DialogClientView(Widget* owner, View* contents_view)
    : ClientView(owner, contents_view),
      button_row_insets_(
          LayoutProvider::Get()->GetInsetsMetric(INSETS_DIALOG_BUTTON_ROW)) {
  // Doing this now ensures this accelerator will have lower priority than
  // one set by the contents view.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  button_row_container_ =
      AddChildView(std::make_unique<ButtonRowContainer>(this));
}

DialogClientView::~DialogClientView() {
  DialogDelegate* dialog = GetWidget() ? GetDialogDelegate() : nullptr;
  if (dialog)
    dialog->RemoveObserver(this);
}

void DialogClientView::SetButtonRowInsets(const gfx::Insets& insets) {
  button_row_insets_ = insets;
  if (GetWidget())
    UpdateDialogButtons();
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, View overrides:

gfx::Size DialogClientView::CalculatePreferredSize() const {
  gfx::Size contents_size = ClientView::CalculatePreferredSize();
  const gfx::Insets& content_margins = GetDialogDelegate()->margins();
  contents_size.Enlarge(content_margins.width(), content_margins.height());
  return GetBoundingSizeForVerticalStack(
      contents_size, button_row_container_->GetPreferredSize());
}

gfx::Size DialogClientView::GetMinimumSize() const {
  return GetBoundingSizeForVerticalStack(
      ClientView::GetMinimumSize(), button_row_container_->GetMinimumSize());
}

gfx::Size DialogClientView::GetMaximumSize() const {
  constexpr int kUnconstrained = 0;
  DCHECK(gfx::Size(kUnconstrained, kUnconstrained) ==
         button_row_container_->GetMaximumSize());
  gfx::Size max_size = ClientView::GetMaximumSize();

  // If the height is constrained, add the button row height. Leave the width as
  // it is (be it constrained or unconstrained).
  if (max_size.height() != kUnconstrained)
    max_size.Enlarge(0, button_row_container_->GetPreferredSize().height());

  // Note not all constraints can be met. E.g. it's possible here for
  // GetMinimumSize().width() to be larger than max_size.width() since the
  // former includes the button row width, but the latter does not. It is up to
  // the DialogDelegate to ensure its maximum size is reasonable for the buttons
  // it wants, or leave it unconstrained.
  return max_size;
}

void DialogClientView::VisibilityChanged(View* starting_from, bool is_visible) {
  ClientView::VisibilityChanged(starting_from, is_visible);
  input_protector_.VisibilityChanged(is_visible);
}

void DialogClientView::Layout() {
  button_row_container_->SetSize(
      gfx::Size(width(), button_row_container_->GetHeightForWidth(width())));
  button_row_container_->SetY(height() - button_row_container_->height());
  if (contents_view()) {
    gfx::Rect contents_bounds(width(), button_row_container_->y());
    contents_bounds.Inset(GetDialogDelegate()->margins());
    contents_view()->SetBoundsRect(contents_bounds);
  }
}

bool DialogClientView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);

  GetWidget()->CloseWithReason(Widget::ClosedReason::kEscKeyPressed);
  return true;
}

void DialogClientView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View* const child = details.child;

  ClientView::ViewHierarchyChanged(details);

  if (details.is_add) {
    if (child == this) {
      UpdateDialogButtons();
      GetDialogDelegate()->AddObserver(this);
    }
    return;
  }

  if (details.parent != button_row_container_)
    return;

  // SetupViews() removes all children, managing data members itself.
  if (adding_or_removing_views_)
    return;

  // Otherwise, this should only happen during teardown. Ensure there are no
  // references to deleted Views.
  button_row_container_->SetLayoutManager(nullptr);

  if (child == ok_button_)
    ok_button_ = nullptr;
  else if (child == cancel_button_)
    cancel_button_ = nullptr;
  else if (child == extra_view_)
    extra_view_ = nullptr;
}

void DialogClientView::OnThemeChanged() {
  ClientView::OnThemeChanged();
  // The old dialog style needs an explicit background color, while the new
  // dialog style simply inherits the bubble's frame view color.
  const DialogDelegate* dialog = GetDialogDelegate();

  if (dialog && !dialog->use_custom_frame()) {
    SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_DialogBackground)));
  }
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, ButtonListener implementation:

void DialogClientView::ButtonPressed(Button* sender, const ui::Event& event) {
  // Check for a valid delegate to avoid handling events after destruction.
  if (!GetDialogDelegate())
    return;

  if (input_protector_.IsPossiblyUnintendedInteraction(event))
    return;

  if (sender == ok_button_)
    GetDialogDelegate()->AcceptDialog();
  else if (sender == cancel_button_)
    GetDialogDelegate()->CancelDialog();
  else
    NOTREACHED();
}

void DialogClientView::ResetViewShownTimeStampForTesting() {
  input_protector_.ResetForTesting();
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, private:

DialogDelegate* DialogClientView::GetDialogDelegate() const {
  return GetWidget()->widget_delegate()->AsDialogDelegate();
}

void DialogClientView::ChildVisibilityChanged(View* child) {
  // Showing or hiding |extra_view_| can alter which columns have linked sizes.
  if (child == extra_view_)
    UpdateDialogButtons();
  InvalidateLayout();
}

void DialogClientView::OnDialogChanged() {
  UpdateDialogButtons();
}

void DialogClientView::UpdateDialogButtons() {
  SetupLayout();
  InvalidateLayout();
}

void DialogClientView::UpdateDialogButton(LabelButton** member,
                                          ui::DialogButton type) {
  DialogDelegate* const delegate = GetDialogDelegate();
  if (!(delegate->GetDialogButtons() & type)) {
    delete *member;
    *member = nullptr;
    return;
  }

  const bool is_default = delegate->GetDefaultDialogButton() == type &&
                          (type != ui::DIALOG_BUTTON_CANCEL ||
                           PlatformStyle::kDialogDefaultButtonCanBeCancel);
  const base::string16 title = delegate->GetDialogButtonLabel(type);

  if (*member) {
    LabelButton* button = *member;
    button->SetEnabled(delegate->IsDialogButtonEnabled(type));
    button->SetIsDefault(is_default);
    button->SetText(title);
    return;
  }

  auto button = std::make_unique<MdTextButton>(this, title);
  button->SetProminent(is_default);
  button->SetIsDefault(is_default);
  button->SetEnabled(delegate->IsDialogButtonEnabled(type));

  const int minimum_width = LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH);
  button->SetMinSize(gfx::Size(minimum_width, 0));

  button->SetGroup(kButtonGroup);

  *member = button_row_container_->AddChildView(std::move(button));
}

int DialogClientView::GetExtraViewSpacing() const {
  if (!ShouldShow(extra_view_) || !(ok_button_ || cancel_button_))
    return 0;

  return LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
}

std::array<View*, DialogClientView::kNumButtons>
DialogClientView::GetButtonRowViews() {
  View* first = ShouldShow(extra_view_) ? extra_view_ : nullptr;
  View* second = cancel_button_;
  View* third = ok_button_;
  if (PlatformStyle::kIsOkButtonLeading)
    std::swap(second, third);
  return {{first, second, third}};
}

void DialogClientView::SetupLayout() {
  base::AutoReset<bool> auto_reset(&adding_or_removing_views_, true);
  FocusManager* focus_manager = GetFocusManager();
  ViewTracker view_tracker(focus_manager->GetFocusedView());

  // Clobber any existing LayoutManager since it has weak references to child
  // Views which may be removed by SetupViews().
  button_row_container_->SetLayoutManager(nullptr);

  SetupViews();

  const std::array<View*, kNumButtons> views = GetButtonRowViews();

  // Visibility changes on |extra_view_| must be observed to re-Layout. However,
  // when hidden it's not included in the button row (it can't influence layout)
  // and it can't be added to |button_row_container_| (GridLayout complains).
  // So add it, hidden, to |this| so it can be observed.
  if (extra_view_) {
    if (!views[0])
      AddChildView(extra_view_);
    else
      button_row_container_->AddChildViewAt(extra_view_, 0);
  }

  GridLayout* layout = button_row_container_->SetLayoutManager(
      std::make_unique<views::GridLayout>());
  layout->set_minimum_size(minimum_size_);

  if (std::count(views.begin(), views.end(), nullptr) == kNumButtons)
    return;

  // The |resize_percent| constants. There's only one stretchy column (padding
  // to the left of ok/cancel buttons).
  constexpr float kFixed = 0.f;
  constexpr float kStretchy = 1.f;

  // Button row is [ extra <pad+stretchy> second <pad> third ]. Ensure the <pad>
  // column is zero width if there isn't a button on either side.
  // GetExtraViewSpacing() handles <pad+stretchy>.
  LayoutProvider* const layout_provider = LayoutProvider::Get();
  const int button_spacing = (ok_button_ && cancel_button_)
                                 ? layout_provider->GetDistanceMetric(
                                       DISTANCE_RELATED_BUTTON_HORIZONTAL)
                                 : 0;

  constexpr int kButtonRowId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kButtonRowId);

  // Rather than giving |button_row_container_| a Border, incorporate the insets
  // into the layout. This simplifies min/max size calculations.
  column_set->AddPaddingColumn(kFixed, button_row_insets_.left());
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kFixed,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(kStretchy, GetExtraViewSpacing());
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kFixed,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(kFixed, button_spacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kFixed,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(kFixed, button_row_insets_.right());

  // Track which columns to link sizes under MD.
  constexpr int kViewToColumnIndex[] = {1, 3, 5};
  std::vector<int> columns_to_link;

  // Skip views that are not a button, or are a specific subclass of Button
  // that should never be linked. Otherwise, link everything.
  auto should_link = [](views::View* view) {
    return Button::AsButton(view) &&
           view->GetClassName() != Checkbox::kViewClassName &&
           view->GetClassName() != ImageButton::kViewClassName;
  };

  layout->StartRowWithPadding(kFixed, kButtonRowId, kFixed,
                              button_row_insets_.top());
  for (size_t view_index = 0; view_index < kNumButtons; ++view_index) {
    if (views[view_index]) {
      layout->AddExistingView(views[view_index]);
      if (should_link(views[view_index]))
        columns_to_link.push_back(kViewToColumnIndex[view_index]);
    } else {
      layout->SkipColumns(1);
    }
  }

  column_set->set_linked_column_size_limit(
      layout_provider->GetDistanceMetric(DISTANCE_BUTTON_MAX_LINKABLE_WIDTH));
  column_set->LinkColumnSizes(columns_to_link);

  layout->AddPaddingRow(kFixed, button_row_insets_.bottom());

  // The default focus is lost when child views are added back into the dialog.
  // This restores focus if the button is still available.
  View* previously_focused_view = view_tracker.view();
  if (previously_focused_view && !focus_manager->GetFocusedView() &&
      Contains(previously_focused_view)) {
    previously_focused_view->RequestFocus();
  }
}

void DialogClientView::SetupViews() {
  if (PlatformStyle::kIsOkButtonLeading) {
    UpdateDialogButton(&ok_button_, ui::DIALOG_BUTTON_OK);
    UpdateDialogButton(&cancel_button_, ui::DIALOG_BUTTON_CANCEL);
  } else {
    UpdateDialogButton(&cancel_button_, ui::DIALOG_BUTTON_CANCEL);
    UpdateDialogButton(&ok_button_, ui::DIALOG_BUTTON_OK);
  }

  auto disowned_extra_view = GetDialogDelegate()->DisownExtraView();
  if (!disowned_extra_view)
    return;

  delete extra_view_;
  extra_view_ = disowned_extra_view.release();
  if (extra_view_ && Button::AsButton(extra_view_))
    extra_view_->SetGroup(kButtonGroup);
}

BEGIN_METADATA(DialogClientView, ClientView)
END_METADATA

}  // namespace views
