// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_

#include <array>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button_image_container.h"
#include "ui/views/controls/button/label_button_label.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/native_theme_delegate.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace actions {
class ActionItem;
}

namespace views {

class InkDropContainerView;
class LabelButtonBorder;

// LabelButton is a button with text and an icon.
class VIEWS_EXPORT LabelButton : public Button,
                                 public NativeThemeDelegate,
                                 public LayoutDelegate {
  METADATA_HEADER(LabelButton, Button)

 public:
  // Creates a LabelButton with pressed events sent to `callback` and label
  // `text`. `button_context` is a value from views::style::TextContext and
  // determines the appearance of `text`.
  explicit LabelButton(
      PressedCallback callback = PressedCallback(),
      const std::u16string& text = std::u16string(),
      int button_context = style::CONTEXT_BUTTON,
      std::unique_ptr<LabelButtonImageContainer> image_container =
          std::make_unique<SingleImageContainer>());
  LabelButton(const LabelButton&) = delete;
  LabelButton& operator=(const LabelButton&) = delete;
  ~LabelButton() override;

  // Gets the image shown for state.
  virtual gfx::ImageSkia GetImage(ButtonState state) const;

  // Gets the image model set for `state`. This may not be the image shown,
  // since a nullopt image model will use the STATE_NORMAL image.
  const std::optional<ui::ImageModel>& GetImageModel(ButtonState state) const;

  // Sets the image show for `state`. If `image_model` is nullopt, it will use
  // the STATE_NORMAL image. If `image_model` is empty, image won't be shown in
  // the button.
  virtual void SetImageModel(ButtonState state,
                             const std::optional<ui::ImageModel>& image_model);

  // Returns whether the image shown for `state` is not empty.
  bool HasImage(ButtonState state) const;

  // Gets or sets the text shown on the button.
  const std::u16string& GetText() const;
  virtual void SetText(const std::u16string& text);

  // Set the text style of the label.
  void SetLabelStyle(views::style::TextStyle text_style);

  // Makes the button report its preferred size without the label. This lets
  // AnimatingLayoutManager gradually shrink the button until the text is
  // invisible, at which point the text gets cleared. Think of this as
  // transitioning from the current text to SetText("").
  // Note that the layout manager (or manual SetBounds calls) need to be
  // configured to eventually hit the the button's preferred size (or smaller)
  // or the text will never be cleared.
  void ShrinkDownThenClearText();

  // Sets the text color shown for the specified button |for_state| to |color|.
  // TODO(crbug.com/40259212): Get rid of SkColor versions of these functions in
  // favor of the ColorId versions.
  void SetTextColor(ButtonState for_state, SkColor color);

  // Sets the text color as above but using ColorId.
  void SetTextColorId(ButtonState for_state, ui::ColorId color_id);

  // Sets the text colors shown for the non-disabled states to |color|.
  // TODO(crbug.com/40259212): Get rid of SkColor versions of these functions in
  // favor of the ColorId versions.
  virtual void SetEnabledTextColors(std::optional<SkColor> color);

  // Sets the text colors shown for the non-disabled states to |color_id|.
  void SetEnabledTextColorIds(ui::ColorId color_id);

  // Gets the current state text color.
  SkColor GetCurrentTextColor() const;

  // Sets drop shadows underneath the text.
  void SetTextShadows(const gfx::ShadowValues& shadows);

  // Sets whether subpixel rendering is used on the label.
  void SetTextSubpixelRenderingEnabled(bool enabled);

  // Sets the elide behavior of this button.
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);

  // Sets the horizontal alignment used for the button; reversed in RTL. The
  // optional image will lead the text, unless the button is right-aligned.
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);
  gfx::HorizontalAlignment GetHorizontalAlignment() const;

  gfx::Size GetMinSize() const;
  void SetMinSize(const gfx::Size& min_size);

  gfx::Size GetMaxSize() const;
  void SetMaxSize(const gfx::Size& max_size);

  // Gets or sets the option to handle the return key; false by default.
  bool GetIsDefault() const;
  void SetIsDefault(bool is_default);

  // Sets the spacing between the image and the text.
  int GetImageLabelSpacing() const;
  void SetImageLabelSpacing(int spacing);

  // Gets or sets the option to place the image aligned with the center of the
  // the label. The image is not centered for CheckBox and RadioButton only.
  bool GetImageCentered() const;
  void SetImageCentered(bool image_centered);

  // Sets the corner radius of the focus ring around the button.
  float GetFocusRingCornerRadius() const;
  void SetFocusRingCornerRadius(float radius);

  // Creates the default border for this button. This can be overridden by
  // subclasses.
  virtual std::unique_ptr<LabelButtonBorder> CreateDefaultBorder() const;

  // Normally a LabelButton only appears as disabled in an inactive widget if
  // PlatformStyle::kInactiveWidgetControlsAppearDisabled is true. This method
  // overrides the default PlatformStyle behavior for this button.
  void SetAppearDisabledInInactiveWidget(bool appear_disabled);

  // Button:
  void SetBorder(std::unique_ptr<Border> border) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void AddLayerToRegion(ui::Layer* new_layer,
                        views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* old_layer) override;
  std::unique_ptr<ActionViewInterface> GetActionViewInterface() override;

  // NativeThemeDelegate:
  ui::NativeTheme::Part GetThemePart() const override;
  gfx::Rect GetThemePaintRect() const override;
  ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  const gfx::Animation* GetThemeAnimation() const override;
  ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;
  ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const override;

  // LayoutDelegate:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

  // Returns the current visual appearance of the button. This takes into
  // account both the button's underlying state, the state of the containing
  // widget, and the parent of the containing widget.
  ButtonState GetVisualState() const;

 protected:
  LabelButtonImageContainer* image_container() {
    return image_container_.get();
  }
  const LabelButtonImageContainer* image_container() const {
    return image_container_.get();
  }
  const View* image_container_view() const {
    return image_container_->GetView();
  }
  View* image_container_view() { return image_container_->GetView(); }
  Label* label() const { return label_; }
  InkDropContainerView* ink_drop_container() const {
    return ink_drop_container_;
  }

  bool explicitly_set_normal_color() const {
    return explicitly_set_colors_[STATE_NORMAL];
  }

  const std::array<bool, STATE_COUNT>& explicitly_set_colors() const {
    return explicitly_set_colors_;
  }
  void set_explicitly_set_colors(const std::array<bool, STATE_COUNT>& colors) {
    explicitly_set_colors_ = colors;
  }

  // Updates the image view to contain the appropriate button state image.
  void UpdateImage();

  // Updates the background color, if the background color is state-sensitive.
  virtual void UpdateBackgroundColor() {}

  // Fills |params| with information about the button.
  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

  // Changes the visual styling to match changes in the default state.  Returns
  // the PropertyEffects triggered as a result.
  virtual PropertyEffects UpdateStyleToIndicateDefaultStatus();

  // Button:
  void ChildPreferredSizeChanged(View* child) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnFocus() override;
  void OnBlur() override;
  void OnThemeChanged() override;
  void StateChanged(ButtonState old_state) override;

 private:
  void SetTextInternal(const std::u16string& text);

  void ClearTextIfShrunkDown();

  // Gets the preferred size (without respecting min_size_ or max_size_), but
  // does not account for the label. This is shared between GetHeightForWidth
  // and CalculatePreferredSize. GetHeightForWidth will subtract the width
  // returned from this method to get width available for the label while
  // CalculatePreferredSize will just add the preferred width from the label.
  // Both methods will then use the max of inset height + label height and this
  // height as total height, and clamp to min/max sizes as appropriate.
  gfx::Size GetUnclampedSizeWithoutLabel() const;

  // Updates the portions of the object that might change in response to a
  // change in the value returned by GetVisualState().
  void VisualStateChanged();

  // Resets colors from the NativeTheme, explicitly set colors are unchanged.
  void ResetColorsFromNativeTheme();

  // Updates additional state related to focus or default status, rather than
  // merely the Button::state(). E.g. ensures the label text color is
  // correct for the current background.
  void ResetLabelEnabledColor();

  // Returns the state whose image is shown for `for_state`, by falling back to
  // STATE_NORMAL when `for_state` has not been set.
  ButtonState ImageStateForState(ButtonState for_state) const;

  void FlipCanvasOnPaintForRTLUIChanged();

  // The image container and label shown in the button.
  std::unique_ptr<LabelButtonImageContainer> image_container_;
  raw_ptr<internal::LabelButtonLabel> label_;

  // A separate view is necessary to hold the ink drop layer so that it can
  // be stacked below |image_| and on top of |label_|, without resorting to
  // drawing |label_| on a layer (which can mess with subpixel anti-aliasing).
  raw_ptr<InkDropContainerView> ink_drop_container_;

  // The cached font lists in the normal and default button style. The latter
  // may be bold.
  gfx::FontList cached_normal_font_list_;
  gfx::FontList cached_default_button_font_list_;

  // The image models and colors for each button state.
  std::array<std::optional<ui::ImageModel>, STATE_COUNT>
      button_state_image_models_;
  std::array<absl::variant<SkColor, ui::ColorId>, STATE_COUNT>
      button_state_colors_;

  // Used to track whether SetTextColor() has been invoked.
  std::array<bool, STATE_COUNT> explicitly_set_colors_ = {};

  // |min_size_| and |max_size_| may be set to clamp the preferred size.
  gfx::Size min_size_;
  gfx::Size max_size_;

  // A flag indicating that this button should not include the label in its
  // desired size. Furthermore, once the bounds of the button adapt to this
  // desired size, the text in the label should get cleared.
  bool shrinking_down_label_ = false;

  // Flag indicating default handling of the return key via an accelerator.
  // Whether or not the button appears or behaves as the default button in its
  // current context;
  bool is_default_ = false;

  // True if current border was set by SetBorder.
  bool explicitly_set_border_ = false;

  // A flag indicating that this button's image should be aligned with the
  // center of the label when multiline is enabled. This shouldn't be the case
  // for a CheckBox or a RadioButton.
  bool image_centered_ = true;

  // Spacing between the image and the text.
  int image_label_spacing_ = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL);

  // Alignment of the button. This can be different from the alignment of the
  // text; for example, the label may be set to ALIGN_TO_HEAD (alignment matches
  // text direction) while |this| is laid out as ALIGN_LEFT (alignment matches
  // UI direction).
  gfx::HorizontalAlignment horizontal_alignment_ = gfx::ALIGN_LEFT;

  // Corner radius of the focus ring.
  float focus_ring_corner_radius_ = FocusRing::kDefaultCornerRadiusDp;

  base::CallbackListSubscription paint_as_active_subscription_;

  bool appear_disabled_in_inactive_widget_ = false;

  base::CallbackListSubscription flip_canvas_on_paint_subscription_ =
      AddFlipCanvasOnPaintForRTLUIChangedCallback(
          base::BindRepeating(&LabelButton::FlipCanvasOnPaintForRTLUIChanged,
                              base::Unretained(this)));
};

class VIEWS_EXPORT LabelButtonActionViewInterface
    : public ButtonActionViewInterface {
 public:
  explicit LabelButtonActionViewInterface(LabelButton* action_view);
  ~LabelButtonActionViewInterface() override = default;

  // ButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<LabelButton> action_view_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, LabelButton, Button)
VIEW_BUILDER_PROPERTY(std::u16string, Text)
VIEW_BUILDER_PROPERTY(gfx::HorizontalAlignment, HorizontalAlignment)
VIEW_BUILDER_PROPERTY(views::style::TextStyle, LabelStyle)
VIEW_BUILDER_PROPERTY(gfx::Size, MinSize)
VIEW_BUILDER_PROPERTY(gfx::Size, MaxSize)
VIEW_BUILDER_PROPERTY(std::optional<SkColor>, EnabledTextColors)
VIEW_BUILDER_PROPERTY(ui::ColorId, EnabledTextColorIds)
VIEW_BUILDER_PROPERTY(bool, IsDefault)
VIEW_BUILDER_PROPERTY(int, ImageLabelSpacing)
VIEW_BUILDER_PROPERTY(bool, ImageCentered)
VIEW_BUILDER_METHOD(SetImageModel, Button::ButtonState, const ui::ImageModel&)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, LabelButton)

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
