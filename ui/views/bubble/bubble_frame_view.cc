// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <algorithm>

#include "build/build_config.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/default_style.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/footnote_container_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/paint_info.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace views {

namespace {

// Get the |vertical| or horizontal amount that |available_bounds| overflows
// |window_bounds|.
int GetOverflowLength(const gfx::Rect& available_bounds,
                      const gfx::Rect& window_bounds,
                      bool vertical) {
  if (available_bounds.IsEmpty() || available_bounds.Contains(window_bounds))
    return 0;

  //  window_bounds
  //  +---------------------------------+
  //  |             top                 |
  //  |      +------------------+       |
  //  | left | available_bounds | right |
  //  |      +------------------+       |
  //  |            bottom               |
  //  +---------------------------------+
  if (vertical)
    return std::max(0, available_bounds.y() - window_bounds.y()) +
           std::max(0, window_bounds.bottom() - available_bounds.bottom());
  return std::max(0, available_bounds.x() - window_bounds.x()) +
         std::max(0, window_bounds.right() - available_bounds.right());
}

// The height of the progress indicator shown at the top of the bubble frame
// view.
constexpr int kProgressIndicatorHeight = 4;

}  // namespace

BubbleFrameView::BubbleFrameView(const gfx::Insets& title_margins,
                                 const gfx::Insets& content_margins)
    : title_margins_(title_margins),
      content_margins_(content_margins),
      footnote_margins_(content_margins_),
      title_icon_(AddChildView(std::make_unique<ImageView>())),
      main_image_(AddChildView(std::make_unique<ImageView>())),
      title_container_(AddChildView(std::make_unique<views::BoxLayoutView>())),
      default_title_(title_container_->AddChildView(
          CreateDefaultTitleLabel(std::u16string()))),
      subtitle_(title_container_->AddChildView(
          CreateLabelWithContextAndStyle(std::u16string(),
                                         style::CONTEXT_LABEL,
                                         style::STYLE_SECONDARY))) {
  title_container_->SetOrientation(BoxLayout::Orientation::kVertical);

  default_title_->SetVisible(false);
  main_image_->SetVisible(false);
  subtitle_->SetVisible(false);

  default_title_->SetTextStyle(style::STYLE_HEADLINE_4);

  auto minimize = CreateMinimizeButton(base::BindRepeating(
      [](BubbleFrameView* view, const ui::Event& event) {
        if (view->input_protector_.IsPossiblyUnintendedInteraction(event))
          return;
        view->GetWidget()->Minimize();
      },
      this));
  minimize->SetProperty(views::kElementIdentifierKey, kMinimizeButtonElementId);
  minimize->SetVisible(false);
  minimize_ = AddChildView(std::move(minimize));

  auto close = CreateCloseButton(base::BindRepeating(
      [](BubbleFrameView* view, const ui::Event& event) {
        if (view->input_protector_.IsPossiblyUnintendedInteraction(event))
          return;
        view->GetWidget()->CloseWithReason(
            Widget::ClosedReason::kCloseButtonClicked);
      },
      this));
  close->SetProperty(views::kElementIdentifierKey, kCloseButtonElementId);
  close->SetVisible(false);
  close_ = AddChildView(std::move(close));

  auto progress_indicator = std::make_unique<ProgressBar>();
  progress_indicator->SetPreferredHeight(kProgressIndicatorHeight);
  progress_indicator->SetPreferredCornerRadii(std::nullopt);
  progress_indicator->SetBackgroundColor(SK_ColorTRANSPARENT);
  progress_indicator->SetVisible(false);
  progress_indicator->GetViewAccessibility().SetIsIgnored(true);
  progress_indicator->SetProperty(views::kElementIdentifierKey,
                                  kProgressIndicatorElementId);
  progress_indicator_ = AddChildView(std::move(progress_indicator));
}

BubbleFrameView::~BubbleFrameView() = default;

// static
std::unique_ptr<Label> BubbleFrameView::CreateDefaultTitleLabel(
    const std::u16string& title_text) {
  std::unique_ptr<Label> label = CreateLabelWithContextAndStyle(
      title_text, style::CONTEXT_DIALOG_TITLE, style::STYLE_PRIMARY);
  if (base::FeatureList::IsEnabled(features::kBubbleFrameViewTitleIsHeading)) {
    label->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    label->GetViewAccessibility().SetHierarchicalLevel(1);
  }
  return label;
}

// static
std::unique_ptr<Button> BubbleFrameView::CreateCloseButton(
    Button::PressedCallback callback) {
  auto close_button = CreateVectorImageButtonWithNativeTheme(
      std::move(callback), vector_icons::kCloseChromeRefreshIcon);
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SizeToPreferredSize();

  InstallCircleHighlightPathGenerator(close_button.get());

  return close_button;
}

// static
std::unique_ptr<Button> BubbleFrameView::CreateMinimizeButton(
    Button::PressedCallback callback) {
  auto minimize_button = CreateVectorImageButtonWithNativeTheme(
      std::move(callback), kWindowControlMinimizeIcon);
  minimize_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  minimize_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  minimize_button->SizeToPreferredSize();

  InstallCircleHighlightPathGenerator(minimize_button.get());

  return minimize_button;
}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  // When NonClientView asks for this, the size of the frame view has been set
  // (i.e. |this|), but not the client view bounds.
  gfx::Rect client_bounds = GetContentsBounds();
  client_bounds.Inset(GetClientInsetsForFrameWidth(client_bounds.width()));
  // Only account for footnote_container_'s height if it's visible, because
  // content_margins_ adds extra padding even if all child views are invisible.
  if (footnote_container_ && footnote_container_->GetVisible()) {
    client_bounds.set_height(client_bounds.height() -
                             footnote_container_->height());
  }
  return client_bounds;
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Size size(GetFrameSizeForClientSize(client_bounds.size()));
  return bubble_border_->GetBounds(gfx::Rect(), size);
}

bool BubbleFrameView::GetClientMask(const gfx::Size& size, SkPath* path) const {
  // NonClientView calls this after setting the client view size from the return
  // of GetBoundsForClientView(); feeding it back in |size|.
  DCHECK_EQ(GetBoundsForClientView().size(), size);
  DCHECK_EQ(GetWidget()->client_view()->size(), size);

  // BubbleFrameView only returns a SkPath for the purpose of clipping the
  // client view's corners so that it fits within the borders of its rounded
  // frame. If a client view is painted to a layer the rounding is handled by
  // the |SetRoundedCornerRadius()| layer API, so we return false here.
  if (GetWidget()->client_view()->layer()) {
    return false;
  }

  const gfx::RoundedCornersF corner_radii = GetClientCornerRadii();

  // If corner radii are all zero we do not need to apply a mask.
  if (corner_radii.IsEmpty()) {
    return false;
  }

  // Format is upper-left x, upper-left y, upper-right x, and so forth,
  // clockwise around the boundary.
  SkScalar radii[]{corner_radii.upper_left(),  corner_radii.upper_left(),
                   corner_radii.upper_right(), corner_radii.upper_right(),
                   corner_radii.lower_right(), corner_radii.lower_right(),
                   corner_radii.lower_left(),  corner_radii.lower_left()};
  path->addRoundRect(SkRect::MakeIWH(size.width(), size.height()), radii);
  return true;
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point)) {
    return HTNOWHERE;
  }
  if (hit_test_transparent_) {
    return HTTRANSPARENT;
  }
#if !BUILDFLAG(IS_WIN)
  // Windows will automatically create a tooltip for the button based on
  // the HTCLOSE or the HTMINBUTTON
  if (close_->GetVisible() && close_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  if (minimize_->GetVisible() && minimize_->GetMirroredBounds().Contains(point))
    return HTMINBUTTON;
#endif

  // Convert to RRectF to accurately represent the rounded corners of the
  // dialog and allow events to pass through the shadows.
  gfx::RRectF round_contents_bounds(gfx::RectF(GetContentsBounds()),
                                    bubble_border_->corner_radius());
  if (bubble_border_->shadow() != BubbleBorder::NO_SHADOW)
    round_contents_bounds.Outset(BubbleBorder::kBorderThicknessDip);
  gfx::RectF rectf_point(point.x(), point.y(), 1, 1);
  if (!round_contents_bounds.Contains(rectf_point)) {
    return HTTRANSPARENT;
  }

  if (point.y() < title_container_->bounds().bottom()) {
    auto* dialog_delegate = GetWidget()->widget_delegate()->AsDialogDelegate();
    if (dialog_delegate && dialog_delegate->draggable()) {
      return HTCAPTION;
    }
  }

  if (!non_client_hit_test_cb_.is_null()) {
    const int result = non_client_hit_test_cb_.Run(point);
    if (result != HTNOWHERE) {
      return result;
    }
  }

  return GetWidget()->client_view()->NonClientHitTest(point);
}

void BubbleFrameView::GetWindowMask(const gfx::Size& size,
                                    SkPath* window_mask) {
  if (bubble_border_->shadow() != BubbleBorder::STANDARD_SHADOW &&
      bubble_border_->shadow() != BubbleBorder::NO_SHADOW)
    return;

  // We don't return a mask for windows with arrows unless they use
  // BubbleBorder::NO_SHADOW.
  if (bubble_border_->shadow() != BubbleBorder::NO_SHADOW &&
      bubble_border_->arrow() != BubbleBorder::NONE &&
      bubble_border_->arrow() != BubbleBorder::FLOAT)
    return;

  // Use a window mask roughly matching the border in the image assets.
  const int kBorderStrokeSize =
      bubble_border_->shadow() == BubbleBorder::NO_SHADOW ? 0 : 1;
  const SkScalar kCornerRadius = SkIntToScalar(bubble_border_->corner_radius());
  const gfx::Insets border_insets = bubble_border_->GetInsets();
  SkRect rect = {
      SkIntToScalar(border_insets.left() - kBorderStrokeSize),
      SkIntToScalar(border_insets.top() - kBorderStrokeSize),
      SkIntToScalar(size.width() - border_insets.right() + kBorderStrokeSize),
      SkIntToScalar(size.height() - border_insets.bottom() +
                    kBorderStrokeSize)};

  if (bubble_border_->shadow() == BubbleBorder::NO_SHADOW) {
    window_mask->addRoundRect(rect, kCornerRadius, kCornerRadius);
  } else {
    static const int kBottomBorderShadowSize = 2;
    rect.fBottom += SkIntToScalar(kBottomBorderShadowSize);
    window_mask->addRect(rect);
  }
}

void BubbleFrameView::ResetWindowControls() {
  // If the close button is not visible, marking it as "ignored" will cause it
  // to be removed from the accessibility tree.
  bool close_is_visible =
      GetWidget()->widget_delegate()->ShouldShowCloseButton();
  bool close_visible_changed = close_->GetVisible() != close_is_visible;

  close_->SetVisible(close_is_visible);
  close_->GetViewAccessibility().SetIsIgnored(!close_is_visible);

  // If the minimize button is not visible, marking it as "ignored" will cause
  // it to be removed from the accessibility tree.
  bool minimize_is_visible = GetWidget()->widget_delegate()->CanMinimize();
  bool minimize_visible_changed =
      minimize_->GetVisible() != minimize_is_visible;

  minimize_->SetVisible(minimize_is_visible);
  minimize_->GetViewAccessibility().SetIsIgnored(!minimize_is_visible);

  if (minimize_visible_changed || close_visible_changed) {
    InvalidateLayout();
  }
}

void BubbleFrameView::UpdateWindowIcon() {
  DCHECK(GetWidget());
  ui::ImageModel image;
  if (GetWidget()->widget_delegate()->ShouldShowWindowIcon()) {
    image = GetWidget()->widget_delegate()->GetWindowIcon();
  }
  title_icon_->SetImage(image);
}

void BubbleFrameView::UpdateWindowTitle() {
  if (default_title_) {
    const WidgetDelegate* delegate = GetWidget()->widget_delegate();
    default_title_->SetVisible(delegate->ShouldShowWindowTitle() &&
                               !delegate->GetWindowTitle().empty());
    default_title_->SetText(delegate->GetWindowTitle());
    UpdateSubtitle();
  }  // custom_title_'s updates are handled by its creator.
  InvalidateLayout();
}

void BubbleFrameView::SizeConstraintsChanged() {}

void BubbleFrameView::InsertClientView(ClientView* client_view) {
  // Place the client view before any footnote view for focus order.
  footnote_container_
      ? AddChildViewAt(client_view, GetIndexOf(footnote_container_).value())
      : AddChildView(client_view);
}

void BubbleFrameView::UpdateWindowRoundedCorners() {
  // BubbleFrameView makes the frame round by drawing a rounded border.
  // Additionally, it rounds `footnote_container_` if present; it makes the
  // client view contents rounded (if needed) by either applying rounded corners
  // to the client view layer or applying a mask.  However, certain
  // implementations of the client view may need to do additional work to have a
  // rounded window.
  GetWidget()->client_view()->UpdateWindowRoundedCorners(GetCornerRadius());
}

bool BubbleFrameView::HasWindowTitle() const {
  return title();
}

bool BubbleFrameView::IsWindowTitleVisible() const {
  return title()->GetVisible();
}

void BubbleFrameView::SetTitleView(std::unique_ptr<View> title_view) {
  DCHECK(title_view);
  if (default_title_) {
    title_container_->RemoveChildViewT(default_title_.ExtractAsDangling());
  }
  if (custom_title_) {
    title_container_->RemoveChildViewT(custom_title_.ExtractAsDangling());
  }
  custom_title_ = title_container_->AddChildViewAt(std::move(title_view), 0);
}

void BubbleFrameView::UpdateSubtitle() {
  if (!subtitle_) {
    return;
  }
  views::BubbleDialogDelegate* const bubble_delegate =
      GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
  if (!bubble_delegate) {
    return;
  }
  // Subtitle anchors and margins rely heavily on Title being visible.
  subtitle_->SetVisible(!bubble_delegate->GetSubtitle().empty() &&
                        default_title_->GetVisible());
  subtitle_->SetText(bubble_delegate->GetSubtitle());
  subtitle_->SetAllowCharacterBreak(
      bubble_delegate->GetSubtitleAllowCharacterBreak());
  InvalidateLayout();
}

void BubbleFrameView::UpdateMainImage() {
  views::BubbleDialogDelegate* const bubble_delegate =
      GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
  if (!bubble_delegate) {
    return;
  }
  const ui::ImageModel& model = bubble_delegate->GetMainImage();
  if (model.IsEmpty()) {
    main_image_->SetVisible(false);
  } else {
    // This max increase is the difference between the 448 and 320 snapping
    // points in ChromeLayoutProvider, but they are not publicly visible in that
    // API. We set the size of the main image so that the dialog increases in
    // size exactly one snapping point.
    // Ideally this would be handled inside the ImageView, but we do cropping
    // and scaling outside (through ScaleAspectRatioAndCropCenter). We should
    // consider moving that functionality into ImageView or ImageModel without
    // having to specify an external size before painting.
    constexpr int kMainImageDialogWidthIncrease = 128;
    constexpr int kBorderStrokeThickness = 1;

    // Use the `title_margins_` for the outer margins between the content and
    // the visible frame border. `border_insets` is the space outside the
    // visible border mask that incorporates the rounded corners. This will
    // ensure that the *perceived* margin will be what is expected since the
    // origin for the view is outside the visible border.
    const int border_margin_left = title_margins_.left();
    const int border_margin_top = title_margins_.top();
    const gfx::Insets border_insets = GetBorder()->GetInsets();
    const int main_image_dimension = kMainImageDialogWidthIncrease -
                                     border_insets.left() - border_margin_left -
                                     kBorderStrokeThickness;
    const int image_inset_left = border_insets.left() + border_margin_left;
    const int image_inset_top = border_insets.top() + border_margin_top;
    const gfx::Insets image_insets =
        gfx::Insets::TLBR(image_inset_top, image_inset_left, border_margin_top,
                          border_margin_left);

    const int border_radius = LayoutProvider::Get()->GetCornerRadiusMetric(
        Emphasis::kHigh, gfx::Size());
    main_image_->SetImage(ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateCroppedCenteredRoundRectImage(
            gfx::Size(main_image_dimension, main_image_dimension),
            border_radius - 2 * kBorderStrokeThickness,
            model.GetImage().AsImageSkia())));
    main_image_->SetBorder(views::CreateRoundedRectBorder(
        kBorderStrokeThickness, border_radius, image_insets,
        GetColorProvider()
            ? GetColorProvider()->GetColor(ui::kColorBubbleBorder)
            : gfx::kPlaceholderColor));

    main_image_->SetVisible(true);
  }
}

void BubbleFrameView::SetProgress(std::optional<double> progress) {
  bool visible = progress.has_value();
  progress_indicator_->SetVisible(visible);
  progress_indicator_->GetViewAccessibility().SetIsIgnored(!visible);
  if (progress) {
    progress_indicator_->SetValue(progress.value());
  }
}

std::optional<double> BubbleFrameView::GetProgress() const {
  if (progress_indicator_->GetVisible()) {
    return progress_indicator_->GetValue();
  }
  return std::nullopt;
}

gfx::Size BubbleFrameView::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  // Get the preferred size of the client area.
  gfx::Size client_size =
      GetWidget()->client_view()->GetPreferredSize(available_size);
  // Expand it to include the bubble border and space for the arrow.
  return GetWindowBoundsForClientBounds(gfx::Rect(client_size)).size();
}

gfx::Size BubbleFrameView::GetMinimumSize() const {
  // Get the minimum size of the client area.
  gfx::Size client_size = GetWidget()->client_view()->GetMinimumSize();
  // Expand it to include the bubble border and space for the arrow.
  return GetWindowBoundsForClientBounds(gfx::Rect(client_size)).size();
}

gfx::Size BubbleFrameView::GetMaximumSize() const {
#if BUILDFLAG(IS_WIN)
  // On Windows, this causes problems, so do not set a maximum size (it doesn't
  // take the drop shadow area into account, resulting in a too-small window;
  // see http://crbug.com/506206). This isn't necessary on Windows anyway, since
  // the OS doesn't give the user controls to resize a bubble.
  return gfx::Size();
#else
#if BUILDFLAG(IS_MAC)
  // Allow BubbleFrameView dialogs to be resizable on Mac.
  if (GetWidget()->widget_delegate()->CanResize()) {
    gfx::Size client_size = GetWidget()->client_view()->GetMaximumSize();
    if (client_size.IsEmpty()) {
      return client_size;
    }
    return GetWindowBoundsForClientBounds(gfx::Rect(client_size)).size();
  }
#endif  // BUILDFLAG(IS_MAC)
  // Non-dialog bubbles should be non-resizable, so its max size is its
  // preferred size.
  return GetPreferredSize({});
#endif
}

void BubbleFrameView::Layout(PassKey) {
  // The title margins may not be set, but make sure that's only the case when
  // there's no title.
  DCHECK(!title_margins_.IsEmpty() ||
         (!custom_title_ && !default_title_->GetVisible()));

  const gfx::Rect contents_bounds = GetContentsBounds();

  // Lay out the progress bar.
  progress_indicator_->SetBounds(contents_bounds.x(), contents_bounds.y(),
                                 contents_bounds.width(),
                                 kProgressIndicatorHeight);

  gfx::Rect bounds = contents_bounds;
  bounds.Inset(title_margins_);
  gfx::Point button_area_top_right = GetButtonAreaTopRight();
  // Position each button according to the top-right corner.
  gfx::Rect button_area_rect(button_area_top_right, gfx::Size());
  for (Button* button : {close_, minimize_}) {
    if (!button->GetVisible()) {
      continue;
    }
    // Add spacing between buttons.
    if (button == minimize_ && close_->GetVisible()) {
      button_area_rect.Outset(
          gfx::Outsets::TLBR(0,
                             LayoutProvider::Get()->GetDistanceMetric(
                                 DISTANCE_RELATED_BUTTON_HORIZONTAL),
                             0, 0));
    }
    button->SetPosition(gfx::Point(button_area_rect.x() - button->width(),
                                   button_area_rect.y()));
    button_area_rect.Union(button->bounds());
  }

  // Add spacing between the title and buttons.
  if (!button_area_rect.IsEmpty()) {
    button_area_rect.Outset(
        gfx::Outsets::TLBR(0,
                           LayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_RELATED_LABEL_HORIZONTAL),
                           0, 0));
  }

  DCHECK_EQ(button_area_rect.size(), GetButtonAreaSize());

  // Lay out the header.
  gfx::Rect header_rect = contents_bounds;
  header_rect.set_height(GetHeaderHeightForFrameWidth(contents_bounds.width()));
  if (header_rect.height() > 0) {
    header_view_->SetBoundsRect(header_rect);
    bounds.Inset(gfx::Insets::TLBR(header_rect.height(), 0, 0, 0));
  }

  // Lay out the title.
  gfx::Size title_icon_pref_size(title_icon_->GetPreferredSize({}));
  const int title_icon_padding =
      title_icon_pref_size.width() > 0 ? title_margins_.left() : 0;
  const int title_label_x = bounds.x() + title_icon_pref_size.width() +
                            title_icon_padding + GetMainImageLeftInsets();
  int title_label_right = bounds.right();
  if (!button_area_rect.IsEmpty() &&
      button_area_rect.bottom() > header_rect.bottom()) {
    title_label_right = std::min(title_label_right, button_area_rect.x());
  }

  // TODO(tapted): Layout() should skip more surrounding code when !HasTitle().
  // Currently DCHECKs fail since title_insets is 0 when there is no title. Skip
  // checking if bounds is empty, as async bounds setting during bubble creation
  // may cause unreliable layout results.
  if (DCHECK_IS_ON() && HasTitle() && !bounds.IsEmpty()) {
    const gfx::Insets title_insets =
        GetTitleLabelInsetsFromFrame() + GetInsets();
    DCHECK_EQ(title_insets.left(), title_label_x);
    DCHECK_EQ(title_insets.right(), width() - title_label_right);
  }

  const int title_available_width =
      std::max(1, title_label_right - title_label_x);
  const int title_preferred_height =
      title_container_->GetHeightForWidth(title_available_width);
  const int title_height =
      std::max(title_icon_pref_size.height(), title_preferred_height);

  title_container_->SetBounds(
      title_label_x, bounds.y() + (title_height - title_preferred_height) / 2,
      title_available_width, title_preferred_height);

  // For default titles, align the icon with the first line of the title if the
  // icon height is equal to or smaller than the line height. Otherwise, align
  // the title with the center of the icon.
  if (default_title_ &&
      title_icon_pref_size.height() <= default_title_->GetLineHeight()) {
    // Offsets the y bounds by half the difference of the first line height and
    // icon height which essentially centers the icon with the middle of the
    // first line.
    title_icon_->SetBounds(
        bounds.x(),
        bounds.y() +
            (default_title_->GetLineHeight() - title_icon_pref_size.height()) /
                2,
        title_icon_pref_size.width(), title_icon_pref_size.height());
  } else {
    title_icon_->SetBounds(bounds.x(), bounds.y(), title_icon_pref_size.width(),
                           title_height);
  }

  main_image_->SetBounds(0, 0, main_image_->GetPreferredSize({}).width(),
                         main_image_->GetPreferredSize({}).height());

  // Lay out the footnote.
  // Only account for footnote_container_'s height if it's visible, because
  // content_margins_ adds extra padding even if all child views are invisible.
  if (footnote_container_ && footnote_container_->GetVisible()) {
    const int width = contents_bounds.width();
    const int height = footnote_container_->GetHeightForWidth(width);
    footnote_container_->SetBounds(
        contents_bounds.x(), contents_bounds.bottom() - height, width, height);
  }

  // Lay out the client view.
  LayoutSuperclass<NonClientFrameView>(this);
}

void BubbleFrameView::OnThemeChanged() {
  NonClientFrameView::OnThemeChanged();
  UpdateWindowTitle();
  UpdateSubtitle();
  ResetWindowControls();
  UpdateWindowIcon();
  UpdateMainImage();
  UpdateClientViewBackground();
  SchedulePaint();
}

void BubbleFrameView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    UpdateClientLayerCornerRadius();
  }

  // We need to update the client view's corner radius whenever the header or
  // footer are added/removed from the bubble frame so that the client view
  // sits flush with both.
  if (details.parent == this) {
    UpdateClientLayerCornerRadius();
  }

  if (!details.is_add && details.parent == footnote_container_ &&
      footnote_container_->children().size() == 1 &&
      details.child == footnote_container_->children().front()) {
    // Setting the footnote_container_ to be hidden and null it. This will
    // remove update the bubble to have no placeholder for the footnote and
    // enable the destructor to delete the footnote_container_ later.
    footnote_container_->SetVisible(false);
    footnote_container_ = nullptr;
  }
}

void BubbleFrameView::VisibilityChanged(View* starting_from, bool is_visible) {
  NonClientFrameView::VisibilityChanged(starting_from, is_visible);
  input_protector_.VisibilityChanged(is_visible);
}

void BubbleFrameView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  // Border comes after children.
}

void BubbleFrameView::PaintChildren(const PaintInfo& paint_info) {
  NonClientFrameView::PaintChildren(paint_info);

  ui::PaintCache paint_cache;
  ui::PaintRecorder recorder(
      paint_info.context(), paint_info.paint_recording_size(),
      paint_info.paint_recording_scale_x(),
      paint_info.paint_recording_scale_y(), &paint_cache);
  OnPaintBorder(recorder.canvas());
}

void BubbleFrameView::SetBubbleBorder(std::unique_ptr<BubbleBorder> border) {
  bubble_border_ = border.get();

  if (footnote_container_)
    footnote_container_->SetCornerRadius(border->corner_radius());

  // Update the background, which relies on the border. First set it to null to
  // avoid dangling pointers, and then update it.
  SetBackground(nullptr);
  SetBorder(std::move(border));
  SetBackground(std::make_unique<views::BubbleBackground>(bubble_border_));
}

void BubbleFrameView::SetContentMargins(const gfx::Insets& content_margins) {
  content_margins_ = content_margins;
  OnPropertyChanged(&content_margins_, kPropertyEffectsPreferredSizeChanged);
}

gfx::Insets BubbleFrameView::GetContentMargins() const {
  return content_margins_;
}

void BubbleFrameView::SetHeaderView(std::unique_ptr<View> view) {
  if (header_view_) {
    RemoveChildViewT(header_view_.ExtractAsDangling());
  }

  if (view) {
    header_view_ = AddChildViewAt(std::move(view), 0);
  }

  InvalidateLayout();
}

void BubbleFrameView::SetFootnoteView(std::unique_ptr<View> view) {
  // Remove the old footnote container.
  if (footnote_container_) {
    RemoveChildViewT(footnote_container_.ExtractAsDangling());
  }
  if (view) {
    int radius = bubble_border_ ? bubble_border_->corner_radius() : 0;
    footnote_container_ = AddChildView(std::make_unique<FootnoteContainerView>(
        footnote_margins_, std::move(view), radius));
  }
  InvalidateLayout();
}

View* BubbleFrameView::GetFootnoteView() const {
  if (!footnote_container_) {
    return nullptr;
  }

  DCHECK_EQ(1u, footnote_container_->children().size());
  return footnote_container_->children()[0];
}

void BubbleFrameView::SetFootnoteMargins(const gfx::Insets& footnote_margins) {
  footnote_margins_ = footnote_margins;
  OnPropertyChanged(&footnote_margins_, kPropertyEffectsLayout);
}

gfx::Insets BubbleFrameView::GetFootnoteMargins() const {
  return footnote_margins_;
}

void BubbleFrameView::SetPreferredArrowAdjustment(
    BubbleFrameView::PreferredArrowAdjustment adjustment) {
  if (preferred_arrow_adjustment_ == adjustment) {
    return;
  }

  preferred_arrow_adjustment_ = adjustment;
  // Changing |preferred_arrow_adjustment| will affect window bounds.
  OnPropertyChanged(&preferred_arrow_adjustment_, kPropertyEffectsLayout);
}

BubbleFrameView::PreferredArrowAdjustment
BubbleFrameView::GetPreferredArrowAdjustment() const {
  return preferred_arrow_adjustment_;
}

void BubbleFrameView::SetCornerRadius(int radius) {
  bubble_border_->SetCornerRadius(radius);
  UpdateClientLayerCornerRadius();
}

int BubbleFrameView::GetCornerRadius() const {
  return bubble_border_ ? bubble_border_->corner_radius() : 0;
}

void BubbleFrameView::SetArrow(BubbleBorder::Arrow arrow) {
  bubble_border_->set_arrow(arrow);
}

BubbleBorder::Arrow BubbleFrameView::GetArrow() const {
  return bubble_border_->arrow();
}

void BubbleFrameView::SetDisplayVisibleArrow(bool display_visible_arrow) {
  bubble_border_->set_visible_arrow(display_visible_arrow);
}

bool BubbleFrameView::GetDisplayVisibleArrow() const {
  return bubble_border_->visible_arrow();
}

void BubbleFrameView::SetBackgroundColor(SkColor color) {
  bubble_border_->SetColor(color);
  UpdateClientViewBackground();
  SchedulePaint();
}

SkColor BubbleFrameView::GetBackgroundColor() const {
  return bubble_border_->color();
}

void BubbleFrameView::UpdateClientViewBackground() {
  DCHECK(GetWidget());
  DCHECK(GetWidget()->client_view());

  // If dealing with a layer backed ClientView we need to update it's color to
  // match that of the frame view.
  View* client_view = GetWidget()->client_view();
  if (client_view->layer()) {
    // If the ClientView's background is transparent this could result in visual
    // artifacts. Make sure this isn't the case.
    DCHECK_EQ(SK_AlphaOPAQUE, SkColorGetA(GetBackgroundColor()));
    client_view->SetBackground(CreateSolidBackground(GetBackgroundColor()));
    client_view->SchedulePaint();
  }
}

gfx::Rect BubbleFrameView::GetUpdatedWindowBounds(
    const gfx::Rect& anchor_rect,
    const BubbleBorder::Arrow delegate_arrow,
    const gfx::Size& client_size,
    bool adjust_to_fit_available_bounds) {
  gfx::Size size(GetFrameSizeForClientSize(client_size));

  // Save these values; if the arrow changes as a result of mirroring (or
  // un-mirroring) the border will need to be repainted.
  const auto old_arrow = bubble_border_->arrow();
  const auto old_offset = bubble_border_->arrow_offset();

  if (adjust_to_fit_available_bounds &&
      BubbleBorder::has_arrow(delegate_arrow)) {
    // Get the desired bubble bounds without adjustment.
    bubble_border_->set_arrow_offset(0);
    bubble_border_->set_arrow(delegate_arrow);
    // Try to mirror the anchoring if the bubble does not fit in the available
    // bounds.
    if (bubble_border_->is_arrow_at_center(delegate_arrow) ||
        preferred_arrow_adjustment_ == PreferredArrowAdjustment::kOffset) {
      const bool mirror_vertical =
          BubbleBorder::is_arrow_on_horizontal(delegate_arrow);
      if (use_anchor_window_bounds_) {
        MirrorArrowIfOutOfBounds(mirror_vertical, anchor_rect, size,
                                 GetAvailableAnchorWindowBounds());
      }
      MirrorArrowIfOutOfBounds(mirror_vertical, anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
      if (use_anchor_window_bounds_) {
        OffsetArrowIfOutOfBounds(anchor_rect, size,
                                 GetAvailableAnchorWindowBounds());
      }
      OffsetArrowIfOutOfBounds(anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
    } else {
      if (use_anchor_window_bounds_) {
        MirrorArrowIfOutOfBounds(true, anchor_rect, size,
                                 GetAvailableAnchorWindowBounds());
      }
      MirrorArrowIfOutOfBounds(true, anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
      if (use_anchor_window_bounds_) {
        MirrorArrowIfOutOfBounds(false, anchor_rect, size,
                                 GetAvailableAnchorWindowBounds());
      }
      MirrorArrowIfOutOfBounds(false, anchor_rect, size,
                               GetAvailableScreenBounds(anchor_rect));
    }
  }

  // Check to see if any of the positioning values have changed.
  if (bubble_border_->arrow() != old_arrow ||
      bubble_border_->arrow_offset() != old_offset) {
    InvalidateLayout();
    SchedulePaint();
  }

  return bubble_border_->GetBounds(anchor_rect, size);
}

void BubbleFrameView::UpdateInputProtectorTimeStamp() {
  input_protector_.MaybeUpdateViewProtectedTimeStamp();
}

void BubbleFrameView::ResetViewShownTimeStampForTesting() {
  input_protector_.ResetForTesting();
}

gfx::Insets BubbleFrameView::GetClientViewInsets() const {
  return GetClientInsetsForFrameWidth(GetContentsBounds().width());
}

gfx::Rect BubbleFrameView::GetAvailableScreenBounds(
    const gfx::Rect& rect) const {
  // The bubble attempts to fit within the current screen bounds.
  return display::Screen::GetScreen()
      ->GetDisplayNearestPoint(rect.CenterPoint())
      .work_area();
}

gfx::Rect BubbleFrameView::GetAvailableAnchorWindowBounds() const {
  views::BubbleDialogDelegate* bubble_delegate_view =
      GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
  if (bubble_delegate_view) {
    views::View* const anchor_view = bubble_delegate_view->GetAnchorView();
    if (anchor_view && anchor_view->GetWidget())
      return anchor_view->GetWidget()->GetWindowBoundsInScreen();
  }
  return gfx::Rect();
}

bool BubbleFrameView::ExtendClientIntoTitle() const {
  return false;
}

bool BubbleFrameView::IsCloseButtonVisible() const {
  return close_->GetVisible();
}

gfx::Rect BubbleFrameView::GetCloseButtonMirroredBounds() const {
  return close_->GetMirroredBounds();
}

gfx::RoundedCornersF BubbleFrameView::GetClientCornerRadii() const {
  DCHECK(bubble_border_);
  const int radius = bubble_border_->corner_radius();
  const gfx::Insets insets =
      GetClientInsetsForFrameWidth(GetContentsBounds().width());

  // Rounded corners do not need to be applied to the client view if the client
  // view is sufficiently inset such that its unclipped bounds will not
  // intersect with the corners of the containing bubble frame view.
  if ((insets.top() > radius && insets.bottom() > radius) ||
      (insets.left() > radius && insets.right() > radius)) {
    return gfx::RoundedCornersF();
  }

  // We want to clip the client view to a rounded rect that's consistent with
  // the bubble's rounded border. However, if there is a header, the top of the
  // client view should be straight and flush with that. Likewise, if there is
  // a footer, the client view should be straight and flush with that. Therefore
  // we set the corner radii separately for top and bottom.
  gfx::RoundedCornersF corner_radii;
  corner_radii.set_upper_left(header_view_ ? 0 : radius);
  corner_radii.set_upper_right(header_view_ ? 0 : radius);
  corner_radii.set_lower_left(footnote_container_ ? 0 : radius);
  corner_radii.set_lower_right(footnote_container_ ? 0 : radius);

  return corner_radii;
}

void BubbleFrameView::MirrorArrowIfOutOfBounds(
    bool vertical,
    const gfx::Rect& anchor_rect,
    const gfx::Size& client_size,
    const gfx::Rect& available_bounds) {
  if (available_bounds.IsEmpty()) {
    return;
  }
  // Check if the bounds don't fit in the available bounds.
  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));
  if (GetOverflowLength(available_bounds, window_bounds, vertical) > 0) {
    BubbleBorder::Arrow arrow = bubble_border_->arrow();
    // Mirror the arrow and get the new bounds.
    bubble_border_->set_arrow(vertical
                                  ? BubbleBorder::vertical_mirror(arrow)
                                  : BubbleBorder::horizontal_mirror(arrow));
    gfx::Rect mirror_bounds =
        bubble_border_->GetBounds(anchor_rect, client_size);
    // Restore the original arrow if mirroring doesn't show more of the bubble.
    // Otherwise it should direct the parent to layout the content based on the
    // new bubble border.
    if (GetOverflowLength(available_bounds, mirror_bounds, vertical) >=
        GetOverflowLength(available_bounds, window_bounds, vertical)) {
      bubble_border_->set_arrow(arrow);
    }
  }
}

void BubbleFrameView::OffsetArrowIfOutOfBounds(
    const gfx::Rect& anchor_rect,
    const gfx::Size& client_size,
    const gfx::Rect& available_bounds) {
  BubbleBorder::Arrow arrow = bubble_border_->arrow();
  DCHECK(BubbleBorder::is_arrow_at_center(arrow) ||
         preferred_arrow_adjustment_ == PreferredArrowAdjustment::kOffset);

  gfx::Rect window_bounds(bubble_border_->GetBounds(anchor_rect, client_size));
  if (available_bounds.IsEmpty() || available_bounds.Contains(window_bounds))
    return;

  // Calculate off-screen adjustment.
  const bool is_horizontal = BubbleBorder::is_arrow_on_horizontal(arrow);
  int offscreen_adjust = 0;
  if (is_horizontal) {
    // If the window bounds are larger than the available bounds then we want to
    // offset the window to fit as much of it in the available bounds as
    // possible without exiting the other side of the available bounds.
    if (window_bounds.width() > available_bounds.width()) {
      if (window_bounds.x() < available_bounds.x())
        offscreen_adjust = available_bounds.right() - window_bounds.right();
      else
        offscreen_adjust = available_bounds.x() - window_bounds.x();
    } else if (window_bounds.x() < available_bounds.x()) {
      offscreen_adjust = available_bounds.x() - window_bounds.x();
    } else if (window_bounds.right() > available_bounds.right()) {
      offscreen_adjust = available_bounds.right() - window_bounds.right();
    }
  } else {
    if (window_bounds.height() > available_bounds.height()) {
      if (window_bounds.y() < available_bounds.y())
        offscreen_adjust = available_bounds.bottom() - window_bounds.bottom();
      else
        offscreen_adjust = available_bounds.y() - window_bounds.y();
    } else if (window_bounds.y() < available_bounds.y()) {
      offscreen_adjust = available_bounds.y() - window_bounds.y();
    } else if (window_bounds.bottom() > available_bounds.bottom()) {
      offscreen_adjust = available_bounds.bottom() - window_bounds.bottom();
    }
  }

  // For center arrows, arrows are moved in the opposite direction of
  // |offscreen_adjust|, e.g. positive |offscreen_adjust| means bubble
  // window needs to be moved to the right and that means we need to move arrow
  // to the left, and that means negative offset.
  bubble_border_->set_arrow_offset(bubble_border_->arrow_offset() -
                                   offscreen_adjust);
}

int BubbleFrameView::GetFrameWidthForClientWidth(int client_width) const {
  // Note that GetMinimumSize() for multiline Labels is typically 0.
  const int title_bar_width =
      GetTitleLabelInsetsFromFrame().width() +
      (HasWindowTitle() ? title()->GetMinimumSize().width() : 0);
  const int client_area_width = client_width + content_margins_.width();
  const int frame_width =
      std::max(title_bar_width, client_area_width) + GetMainImageLeftInsets();

  DialogDelegate* const dialog_delegate =
      GetWidget()->widget_delegate()->AsDialogDelegate();
  bool snapping =
      dialog_delegate && dialog_delegate->buttons() !=
                             static_cast<int>(ui::mojom::DialogButton::kNone);
  return snapping ? LayoutProvider::Get()->GetSnappedDialogWidth(frame_width)
                  : frame_width;
}

gfx::Size BubbleFrameView::GetFrameSizeForClientSize(
    const gfx::Size& client_size) const {
  const int frame_width = GetFrameWidthForClientWidth(client_size.width());
  const gfx::Insets client_insets = GetClientInsetsForFrameWidth(frame_width);
  DCHECK_GE(frame_width, client_size.width());
  gfx::Size size(frame_width, client_size.height() + client_insets.height());

  // Only account for footnote_container_'s height if it's visible, because
  // content_margins_ adds extra padding even if all child views are invisible.
  if (footnote_container_ && footnote_container_->GetVisible())
    size.Enlarge(0, footnote_container_->GetHeightForWidth(size.width()));

  if (main_image_->GetVisible()) {
    size.set_height(
        std::max(size.height(), main_image_->GetPreferredSize({}).height()));
  }

  return size;
}

bool BubbleFrameView::HasTitle() const {
  return (custom_title_ != nullptr &&
          GetWidget()->widget_delegate()->ShouldShowWindowTitle()) ||
         (default_title_ != nullptr &&
          default_title_->GetHeightForWidth(default_title_->width()) > 0) ||
         title_icon_->GetPreferredSize({}).height() > 0;
}

BubbleFrameView::ButtonsPositioning BubbleFrameView::GetButtonsPositioning()
    const {
  // Positions the buttons in the title row when there's no header row.
  return HasTitle() && !(header_view_ && header_view_->GetVisible())
             ? ButtonsPositioning::kInTitleRow
             : ButtonsPositioning::kOnFrameEdge;
}

bool BubbleFrameView::TitleRowHasButtons() const {
  return GetButtonsPositioning() == ButtonsPositioning::kInTitleRow &&
         (GetWidget()->widget_delegate()->ShouldShowCloseButton() ||
          GetWidget()->widget_delegate()->CanMinimize());
}

gfx::Insets BubbleFrameView::GetTitleLabelInsetsFromFrame() const {
  const gfx::Rect content_bounds = GetContentsBounds();
  const int header_height =
      GetHeaderHeightForFrameWidth(content_bounds.width());
  const gfx::Size button_area_size = GetButtonAreaSize();
  const gfx::Rect button_area_bounds(
      GetButtonAreaTopRight() - gfx::Vector2d(button_area_size.width(), 0),
      button_area_size);
  // Only reserve space if the button vertically extends over the header.
  int insets_right = button_area_bounds.bottom() > header_height
                         ? content_bounds.right() - button_area_bounds.x()
                         : 0;

  if (!HasTitle()) {
    return gfx::Insets::TLBR(header_height, 0, 0, insets_right);
  }

  insets_right = std::max(insets_right, title_margins_.right());
  const gfx::Size title_icon_pref_size = title_icon_->GetPreferredSize({});
  const int title_icon_padding =
      title_icon_pref_size.width() > 0 ? title_margins_.left() : 0;
  const int insets_left = GetMainImageLeftInsets() + title_margins_.left() +
                          title_icon_pref_size.width() + title_icon_padding;
  return gfx::Insets::TLBR(header_height + title_margins_.top(), insets_left,
                           title_margins_.bottom(), insets_right);
}

gfx::Insets BubbleFrameView::GetClientInsetsForFrameWidth(
    int frame_width) const {
  int header_height = GetHeaderHeightForFrameWidth(frame_width);
  int close_height = 0;
  if (!ExtendClientIntoTitle() &&
      GetButtonsPositioning() == ButtonsPositioning::kOnFrameEdge &&
      GetWidget()->widget_delegate()->ShouldShowCloseButton()) {
    const int close_margin =
        LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);
    // Note: |close_margin| is not applied on the bottom of the icon.
    close_height = close_margin + close_->height();
  }

  if (!HasTitle()) {
    return content_margins_ +
           gfx::Insets::TLBR(std::max(header_height, close_height),
                             GetMainImageLeftInsets(), 0, 0);
  }

  const int icon_height = title_icon_->GetPreferredSize({}).height();
  const int label_height = title_container_->GetHeightForWidth(
      frame_width - GetTitleLabelInsetsFromFrame().width());
  const int title_height =
      std::max(icon_height, label_height) + title_margins_.height();

  return content_margins_ +
         gfx::Insets::TLBR(std::max(title_height + header_height, close_height),
                           GetMainImageLeftInsets(), 0, 0);
}

int BubbleFrameView::GetHeaderHeightForFrameWidth(int frame_width) const {
  return header_view_ && header_view_->GetVisible()
             ? header_view_->GetHeightForWidth(frame_width)
             : 0;
}

void BubbleFrameView::UpdateClientLayerCornerRadius() {
  // If the ClientView is painted to a layer we need to apply the appropriate
  // corner radius so that the ClientView and all its child layers are masked
  // appropriately to fit within the BubbleFrameView.
  if (GetWidget() && GetWidget()->client_view()->layer()) {
    GetWidget()->client_view()->layer()->SetRoundedCornerRadius(
        GetClientCornerRadii());
  }
}

int BubbleFrameView::GetMainImageLeftInsets() const {
  if (!main_image_->GetVisible()) {
    return 0;
  }
  return main_image_->GetPreferredSize({}).width() -
         main_image_->GetBorder()->GetInsets().right();
}

gfx::Point BubbleFrameView::GetButtonAreaTopRight() const {
  const gfx::Rect contents_bounds = GetContentsBounds();

  // If the buttons are positioned on the upper trailing corner of the bubble.
  if (GetButtonsPositioning() == ButtonsPositioning::kOnFrameEdge) {
    const int distance_to_edge =
        LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);
    return gfx::Point(contents_bounds.right() - distance_to_edge,
                      contents_bounds.y() + distance_to_edge);
  }

  // If the buttons are positioned at the end of the title row.
  DCHECK_EQ(GetButtonsPositioning(), ButtonsPositioning::kInTitleRow);

  gfx::Rect inner_bounds = contents_bounds;
  inner_bounds.Inset(title_margins_);
  // Extend the button rect beyond the inner content bounds by the size
  // of the trailing button's border.
  // This ensures that when the trailing button is not hovered over, it
  // appears vertically aligned with the content's trailing edge.
  return inner_bounds.top_right() +
         gfx::Vector2d(close_->GetInsets().right(), 0);
}

gfx::Size BubbleFrameView::GetButtonAreaSize() const {
  int button_count = 0;
  if (GetWidget()->widget_delegate()->ShouldShowCloseButton()) {
    button_count++;
  }
  if (GetWidget()->widget_delegate()->CanMinimize()) {
    button_count++;
  }
  if (button_count == 0) {
    return gfx::Size();
  }

  int total_width = close_->width() * button_count;
  // Add left padding.
  total_width += LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL);
  // Add spacing between buttons.
  if (button_count == 2) {
    total_width += LayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_BUTTON_HORIZONTAL);
  }
  return gfx::Size(total_width, close_->height());
}

// static
std::unique_ptr<Label> BubbleFrameView::CreateLabelWithContextAndStyle(
    const std::u16string& label_text,
    style::TextContext text_context,
    style::TextStyle text_style) {
  auto label = std::make_unique<Label>(label_text, text_context, text_style);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetCollapseWhenHidden(true);
  label->SetMultiLine(true);
  return label;
}

BEGIN_METADATA(BubbleFrameView)
ADD_PROPERTY_METADATA(std::optional<double>, Progress)
ADD_PROPERTY_METADATA(gfx::Insets, ContentMargins)
ADD_PROPERTY_METADATA(gfx::Insets, FootnoteMargins)
ADD_PROPERTY_METADATA(BubbleFrameView::PreferredArrowAdjustment,
                      PreferredArrowAdjustment)
ADD_PROPERTY_METADATA(int, CornerRadius)
ADD_PROPERTY_METADATA(BubbleBorder::Arrow, Arrow)
ADD_PROPERTY_METADATA(bool, DisplayVisibleArrow)
ADD_PROPERTY_METADATA(SkColor, BackgroundColor, ui::metadata::SkColorConverter)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BubbleFrameView,
                                      kMinimizeButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BubbleFrameView, kCloseButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BubbleFrameView,
                                      kProgressIndicatorElementId);

}  // namespace views
