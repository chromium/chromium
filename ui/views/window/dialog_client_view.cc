// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_client_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
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
  METADATA_HEADER(ButtonRowContainer);
  explicit ButtonRowContainer(DialogClientView* owner) : owner_(owner) {}
  ButtonRowContainer(const ButtonRowContainer&) = delete;
  ButtonRowContainer& operator=(const ButtonRowContainer&) = delete;

  // View:
  void ChildPreferredSizeChanged(View* child) override {
    owner_->ChildPreferredSizeChanged(child);
  }
  void ChildVisibilityChanged(View* child) override {
    owner_->ChildVisibilityChanged(child);
  }

 private:
  const raw_ptr<DialogClientView> owner_;
};

BEGIN_METADATA(DialogClientView, ButtonRowContainer, View)
END_METADATA

DialogClientView::DialogClientView(Widget* owner, View* contents_view)
    : ClientView(owner, contents_view),
      button_row_insets_(
          LayoutProvider::Get()->GetInsetsMetric(INSETS_DIALOG_BUTTON_ROW)),
      input_protector_(
          std::make_unique<views::InputEventActivationProtector>()) {
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

gfx::Size DialogClientView::CalculatePreferredSize() const {
  const gfx::Insets& content_margins = GetDialogDelegate()->margins();

  gfx::Size contents_size;
  const int fixed_width = GetDialogDelegate()->fixed_width();
  if (fixed_width) {
    const int content_width = fixed_width - content_margins.width();
    contents_size = gfx::Size(content_width,
                              ClientView::GetHeightForWidth(content_width));
  } else {
    contents_size = ClientView::CalculatePreferredSize();
  }
  contents_size.Enlarge(content_margins.width(), content_margins.height());
  return GetBoundingSizeForVerticalStack(
      contents_size, button_row_container_->GetPreferredSize());
}

gfx::Size DialogClientView::GetMinimumSize() const {
  // TODO(pbos): Try to find a way for ClientView::GetMinimumSize() to be
  // fixed-width aware. For now this uses min-size = preferred size for
  // fixed-width dialogs (even though min height might not be preferred height).
  // Fixing this might require View::GetMinHeightForWidth().
  if (GetDialogDelegate()->fixed_width())
    return CalculatePreferredSize();

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
  input_protector_->VisibilityChanged(is_visible);
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

  // If there's no close-x (typically the case for modal dialogs) then Cancel
  // the dialog instead of closing the widget as the delegate may likely expect
  // either Accept or Cancel to be called as a result of user action.
  DialogDelegate* const delegate = GetDialogDelegate();
  if (delegate && delegate->EscShouldCancelDialog()) {
    delegate->CancelDialog();
    return true;
  }

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

  // SetupViews() adds/removes children, and manages their position.
  if (adding_or_removing_views_)
    return;

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
    SetBackground(views::CreateSolidBackground(
        GetColorProvider()->GetColor(ui::kColorDialogBackground)));
  }
}

void DialogClientView::UpdateInputProtectorTimeStamp() {
  input_protector_->UpdateViewShownTimeStamp();
}

void DialogClientView::ResetViewShownTimeStampForTesting() {
  input_protector_->ResetForTesting();  // IN-TEST
}

DialogDelegate* DialogClientView::GetDialogDelegate() const {
  return GetWidget()->widget_delegate()->AsDialogDelegate();
}

void DialogClientView::ChildVisibilityChanged(View* child) {
  // Showing or hiding |extra_view_| can alter which columns have linked sizes.
  if (child == extra_view_)
    UpdateDialogButtons();
  InvalidateLayout();
}

void DialogClientView::TriggerInputProtection() {
  input_protector_->UpdateViewShownTimeStamp();
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
    if (*member)
      button_row_container_->RemoveChildViewT(*member);
    *member = nullptr;
    return;
  }

  const bool is_default = delegate->GetDefaultDialogButton() == type &&
                          (type != ui::DIALOG_BUTTON_CANCEL ||
                           PlatformStyle::kDialogDefaultButtonCanBeCancel);
  const std::u16string title = delegate->GetDialogButtonLabel(type);

  if (*member) {
    LabelButton* button = *member;
    button->SetEnabled(delegate->IsDialogButtonEnabled(type));
    button->SetIsDefault(is_default);
    button->SetText(title);
    return;
  }

  const int minimum_width = LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH);

  Builder<View>(button_row_container_)
      .AddChild(
          Builder<MdTextButton>()
              .CopyAddressTo(member)
              .SetCallback(base::BindRepeating(&DialogClientView::ButtonPressed,
                                               base::Unretained(this), type))
              .SetText(title)
              .SetProminent(is_default)
              .SetIsDefault(is_default)
              .SetEnabled(delegate->IsDialogButtonEnabled(type))
              .SetMinSize(gfx::Size(minimum_width, 0))
              .SetGroup(kButtonGroup))
      .BuildChildren();
}

void DialogClientView::ButtonPressed(ui::DialogButton type,
                                     const ui::Event& event) {
  DialogDelegate* const delegate = GetDialogDelegate();
  if (delegate && !input_protector_->IsPossiblyUnintendedInteraction(event)) {
    (type == ui::DIALOG_BUTTON_OK) ? delegate->AcceptDialog()
                                   : delegate->CancelDialog();
  }
}

int DialogClientView::GetExtraViewSpacing() const {
  if (!ShouldShow(extra_view_) || !(ok_button_ || cancel_button_))
    return 0;

  return LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
}

std::array<View*, DialogClientView::kNumButtons>
DialogClientView::GetButtonRowViews() {
  View* first = ShouldShow(extra_view_) ? extra_view_.get() : nullptr;
  View* second = cancel_button_;
  View* third = ok_button_;
  if (cancel_button_ && (PlatformStyle::kIsOkButtonLeading == !!ok_button_))
    std::swap(second, third);
  return {{first, second, third}};
}

void DialogClientView::SetupLayout() {
  base::AutoReset<bool> auto_reset(&adding_or_removing_views_, true);
  FocusManager* focus_manager = GetFocusManager();
  ViewTracker view_tracker(focus_manager->GetFocusedView());

  // Clobber the layout manager in case there are no views in which to layout.
  button_row_container_->SetLayoutManager(nullptr);

  SetupViews();

  const std::array<View*, kNumButtons> views = GetButtonRowViews();

  // Visibility changes on |extra_view_| must be observed to re-Layout. However,
  // when hidden it's not included in the button row (it can't influence layout)
  // and it can't be added to |button_row_container_|. So add it, hidden, to
  // |this| so it can be observed.
  if (extra_view_) {
    if (!views[0])
      AddChildView(extra_view_.get());
    else if (views[0])
      button_row_container_->AddChildViewAt(extra_view_.get(), 0);
  }

  if (base::ranges::count(views, nullptr) == kNumButtons)
    return;

  // This will also clobber any existing layout manager and clear any settings
  // it may already have.
  auto* layout = button_row_container_->SetLayoutManager(
      std::make_unique<views::TableLayout>());
  layout->SetMinimumSize(minimum_size_);

  // The |resize_percent| constants. There's only one stretchy column (padding
  // to the left of ok/cancel buttons).
  constexpr float kFixed = views::TableLayout::kFixedSize;
  constexpr float kStretchy = 1.0f;

  // Button row is [ extra <pad+stretchy> second <pad> third ]. Ensure the
  // <pad> column is zero width if there isn't a button on either side.
  // GetExtraViewSpacing() handles <pad+stretchy>.
  LayoutProvider* const layout_provider = LayoutProvider::Get();
  const int button_spacing = (ok_button_ && cancel_button_)
                                 ? layout_provider->GetDistanceMetric(
                                       DISTANCE_RELATED_BUTTON_HORIZONTAL)
                                 : 0;
  // Rather than giving |button_row_container_| a Border, incorporate the
  // insets into the layout. This simplifies min/max size calculations.
  layout->SetMinimumSize(minimum_size_)
      .AddPaddingColumn(kFixed, button_row_insets_.left())
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, kFixed,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(kStretchy, GetExtraViewSpacing())
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kEnd, kFixed,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(kFixed, button_spacing)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kEnd, kFixed,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(kFixed, button_row_insets_.right())
      .AddPaddingRow(kFixed, button_row_insets_.top())
      .AddRows(1, kFixed)
      .AddPaddingRow(kFixed, button_row_insets_.bottom())
      .SetLinkedColumnSizeLimit(layout_provider->GetDistanceMetric(
          DISTANCE_BUTTON_MAX_LINKABLE_WIDTH));

  // Track which columns to link sizes under MD.
  constexpr size_t kViewToColumnIndex[] = {1, 3, 5};
  std::vector<size_t> columns_to_link;

  // Skip views that are not a button, or are a specific subclass of Button
  // that should never be linked. Otherwise, link everything.
  auto should_link = [](views::View* view) {
    return IsViewClass<Button>(view) && !IsViewClass<Checkbox>(view) &&
           !IsViewClass<ImageButton>(view);
  };

  for (size_t view_index = 0; view_index < kNumButtons; ++view_index) {
    if (views[view_index]) {
      RemoveFillerView(view_index);
      button_row_container_->ReorderChildView(views[view_index], view_index);
      if (should_link(views[view_index]))
        columns_to_link.push_back(kViewToColumnIndex[view_index]);
    } else {
      AddFillerView(view_index);
    }
  }

  layout->LinkColumnSizes(columns_to_link);

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
  if (extra_view_ && IsViewClass<Button>(extra_view_))
    extra_view_->SetGroup(kButtonGroup);
}

void DialogClientView::AddFillerView(size_t view_index) {
  DCHECK_LT(view_index, kNumButtons);
  View*& filler = filler_views_[view_index];
  if (!filler) {
    filler = button_row_container_->AddChildViewAt(
        std::make_unique<View>(),
        std::min(button_row_container_->children().size(), view_index));
  }
}

void DialogClientView::RemoveFillerView(size_t view_index) {
  DCHECK_LT(view_index, kNumButtons);
  View*& filler = filler_views_[view_index];
  if (filler) {
    button_row_container_->RemoveChildViewT(filler);
    filler = nullptr;
  }
}

BEGIN_METADATA(DialogClientView, ClientView)
END_METADATA

}  // namespace views
