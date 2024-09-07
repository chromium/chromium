// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LABEL_H_
#define UI_VIEWS_CONTROLS_LABEL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/buildflags.h"
#include "ui/views/cascading_property.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/selection_controller_delegate.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"
#include "ui/views/word_lookup_client.h"

namespace views {
class LabelSelectionTest;
class MenuRunner;
class SelectionController;

VIEWS_EXPORT extern const ui::ClassProperty<CascadingProperty<SkColor>*>* const
    kCascadingLabelEnabledColor;

// A view subclass that can display a string.
class VIEWS_EXPORT Label : public View,
                           public ContextMenuController,
                           public WordLookupClient,
                           public SelectionControllerDelegate,
                           public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(Label, View)

 public:
  enum MenuCommands {
    kCopy = 1,
    kSelectAll,
    kLastCommandId = kSelectAll,
  };

  // Helper to construct a Label that doesn't use the views typography spec.
  // Using this causes Label to obtain colors from ui::NativeTheme and line
  // spacing from gfx::FontList::GetHeight().
  // TODO(tapted): Audit users of this class when MD is default. Then add
  // foreground/background colors, line spacing and everything else that
  // views::TextContext abstracts away so the separate setters can be removed.
  struct CustomFont {
    // TODO(tapted): Change this to a size delta and font weight since that's
    // typically all the callers really care about, and would allow Label to
    // guarantee caching of the FontList in ResourceBundle.
    //
    // Exclude from `raw_ref` rewriter because there are usages (e.g.
    // `indexed_suggestion_candidate_button.cc` that attempt to bind
    // temporaries (`T&&`) to `font_list`, which `raw_ref` forbids.
    RAW_PTR_EXCLUSION const gfx::FontList& font_list;
  };

  // Create Labels with style::CONTEXT_CONTROL_LABEL and style::STYLE_PRIMARY.
  // TODO(tapted): Remove these. Callers must specify a context or use the
  // constructor taking a CustomFont.
  Label();
  explicit Label(const std::u16string& text);

  // Construct a Label in the given |text_context|. The |text_style| can change
  // later, so provide a default. The |text_context| is fixed.
  // By default text directionality will be derived from the label text, however
  // it can be overriden with |directionality_mode|.
  Label(const std::u16string& text,
        int text_context,
        int text_style = style::STYLE_PRIMARY,
        gfx::DirectionalityMode directionality_mode =
            gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT);

  // Construct a Label with the given |font| description.
  Label(const std::u16string& text, const CustomFont& font);

  Label(const Label&) = delete;
  Label& operator=(const Label&) = delete;

  ~Label() override;

  static const gfx::FontList& GetDefaultFontList();

  // Gets or sets the fonts used by this label.
  const gfx::FontList& font_list() const { return full_text_->font_list(); }

  // TODO(tapted): Replace this with a private method, e.g., OnFontChanged().
  virtual void SetFontList(const gfx::FontList& font_list);

  // Get or set the label text.
  const std::u16string& GetText() const;
  virtual void SetText(const std::u16string& text);

  void AdjustAccessibleName(std::u16string& new_name,
                            ax::mojom::NameFrom& name_from) override;

  // Where the label appears in the UI. Passed in from the constructor. This is
  // a value from views::style::TextContext or an enum that extends it.
  int GetTextContext() const;
  void SetTextContext(int text_context);

  // The style of the label.  This is a value from views::style::TextStyle or an
  // enum that extends it.
  int GetTextStyle() const;
  void SetTextStyle(int style);

  // Applies |style| to a specific |range|.  This is unimplemented for styles
  // that vary from the global text style by anything besides weight.
  void SetTextStyleRange(int style, const gfx::Range& range);

  // Apply the baseline style range across the entire label.
  void ApplyBaselineTextStyle();

  // Enables or disables auto-color-readability (enabled by default).  If this
  // is enabled, then calls to set any foreground or background color will
  // trigger an automatic mapper that uses color_utils::BlendForMinContrast()
  // to ensure that the foreground colors are readable over the background
  // color.
  bool GetAutoColorReadabilityEnabled() const;
  void SetAutoColorReadabilityEnabled(bool enabled);

  // Gets/Sets the color.  This will automatically force the color to be
  // readable over the current background color, if auto color readability is
  // enabled.
  SkColor GetEnabledColor() const;
  virtual void SetEnabledColor(SkColor color);
  std::optional<ui::ColorId> GetEnabledColorId() const;
  void SetEnabledColorId(std::optional<ui::ColorId> enabled_color_id);

  // Gets/Sets the background color. This won't be explicitly drawn, but the
  // label will force the text color to be readable over it.
  SkColor GetBackgroundColor() const;
  void SetBackgroundColor(SkColor color);
  void SetBackgroundColorId(std::optional<ui::ColorId> background_color_id);

  // Gets/Sets the selection text color. This will automatically force the color
  // to be readable over the selection background color, if auto color
  // readability is enabled. Initialized with system default.
  SkColor GetSelectionTextColor() const;
  void SetSelectionTextColor(SkColor color);

  // Gets/Sets the selection background color. Initialized with system default.
  SkColor GetSelectionBackgroundColor() const;
  void SetSelectionBackgroundColor(SkColor color);

  // Get/Set drop shadows underneath the text.
  const gfx::ShadowValues& GetShadows() const;
  void SetShadows(const gfx::ShadowValues& shadows);

  // Gets/Sets whether subpixel rendering is used; the default is true, but this
  // feature also requires an opaque background color.
  // TODO(mukai): rename this as SetSubpixelRenderingSuppressed() to keep the
  // consistency with RenderText field name.
  bool GetSubpixelRenderingEnabled() const;
  void SetSubpixelRenderingEnabled(bool subpixel_rendering_enabled);

  // Gets/Sets whether the DCHECK() checking that subpixel-rendered text is
  // only drawn onto opaque layers is skipped. Use this to suppress false
  // positives - for example, if the label is drawn onto an opaque region of a
  // non-opaque layer. If possible, prefer making the layer opaque or painting
  // onto an opaque views::Background, as those cases are detected and
  // excluded by this DCHECK automatically.
  bool GetSkipSubpixelRenderingOpacityCheck() const;
  void SetSkipSubpixelRenderingOpacityCheck(
      bool skip_subpixel_rendering_opacity_check);

  // Gets/Sets the horizontal alignment; the argument value is mirrored in RTL
  // UI.
  gfx::HorizontalAlignment GetHorizontalAlignment() const;
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Gets/Sets the vertical alignment. Affects how whitespace is distributed
  // vertically around the label text, or if the label is not tall enough to
  // render all of the text, what gets cut off. ALIGN_MIDDLE is default and is
  // strongly suggested for single-line labels because it produces a consistent
  // baseline even when rendering with mixed fonts.
  gfx::VerticalAlignment GetVerticalAlignment() const;
  void SetVerticalAlignment(gfx::VerticalAlignment alignment);

  // Get or set the distance in pixels between baselines of multi-line text.
  // Default is the height of the default font.
  int GetLineHeight() const;
  void SetLineHeight(int line_height);

  // Get or set if the label text can wrap on multiple lines; default is false.
  bool GetMultiLine() const;
  void SetMultiLine(bool multi_line);

  // If multi-line, a non-zero value will cap the number of lines rendered, and
  // elide the rest (currently only ELIDE_TAIL supported). See gfx::RenderText.
  size_t GetMaxLines() const;
  void SetMaxLines(size_t max_lines);

  // If single-line, a non-zero value will help determine the amount of space
  // needed *after* elision, which may be less than the passed |max_width|.
  void SetMaximumWidthSingleLine(int max_width);

  // Returns the number of lines required to render all text. The actual number
  // of rendered lines might be limited by |max_lines_| which elides the rest.
  size_t GetRequiredLines() const;

  // Get or set if the label text should be obscured before rendering (e.g.
  // should "Password!" display as "*********"); default is false.
  bool GetObscured() const;
  void SetObscured(bool obscured);

  // Returns true if some portion of the text is not displayed because of
  // clipping.
  bool IsDisplayTextClipped() const;

  // Returns true if some portion of the text is not displayed, either because
  // of eliding or clipping.
  bool IsDisplayTextTruncated() const;

  // Gets/Sets whether multi-line text can wrap mid-word; the default is false.
  // TODO(mukai): allow specifying WordWrapBehavior.
  bool GetAllowCharacterBreak() const;
  void SetAllowCharacterBreak(bool allow_character_break);

  // For the provided line index, gets the corresponding rendered line and
  // returns the text position of the first character of that line.
  size_t GetTextIndexOfLine(size_t line) const;

  // Set the truncate length of the |full_text_|.
  // NOTE: This does not affect the |display_text_|, since right now the only
  // consumer does not need that; if you need this function, you may need to
  // implement this.
  void SetTruncateLength(size_t truncate_length);

  // Gets/Sets the eliding or fading behavior, applied as necessary. The default
  // is to elide at the end. Eliding is not well-supported for multi-line
  // labels.
  gfx::ElideBehavior GetElideBehavior() const;
  void SetElideBehavior(gfx::ElideBehavior elide_behavior);

  // Gets/Sets the tooltip text.  Default behavior for a label (single-line) is
  // to show the full text if it is wider than its bounds.  Calling this
  // overrides the default behavior and lets you set a custom tooltip.  To
  // revert to default behavior, call this with an empty string.
  std::u16string GetTooltipText() const;
  void SetTooltipText(const std::u16string& tooltip_text);

  // Get or set whether this label can act as a tooltip handler; the default is
  // true.  Set to false whenever an ancestor view should handle tooltips
  // instead.
  bool GetHandlesTooltips() const;
  void SetHandlesTooltips(bool enabled);

  // Resizes the label so its width is set to the fixed width and its height
  // deduced accordingly. Even if all widths of the lines are shorter than
  // |fixed_width|, the given value is applied to the element's width.
  // This is only intended for multi-line labels and is useful when the label's
  // text contains several lines separated with \n.
  // |fixed_width| is the fixed width that will be used (longer lines will be
  // wrapped).  If 0, no fixed width is enforced.
  int GetFixedWidth() const;
  void SizeToFit(int fixed_width);

  // Like SizeToFit, but uses a smaller width if possible.
  int GetMaximumWidth() const;
  void SetMaximumWidth(int max_width);

  // Gets/Sets whether the preferred size is empty when the label is not
  // visible.
  bool GetCollapseWhenHidden() const;
  void SetCollapseWhenHidden(bool value);

  // Get the text as displayed to the user, respecting the obscured flag.
  const std::u16string GetDisplayTextForTesting() const;

  // Get the text direction, as displayed to the user.
  base::i18n::TextDirection GetTextDirectionForTesting();

  // Returns true if the label can be made selectable. For example, links do not
  // support text selection.
  // Subclasses should override this function in case they want to selectively
  // support text selection. If a subclass stops supporting text selection, it
  // should call SetSelectable(false).
  virtual bool IsSelectionSupported() const;

  // Returns true if the label is selectable. Default is false.
  bool GetSelectable() const;

  // Sets whether the label is selectable. False is returned if the call fails,
  // i.e. when selection is not supported but |selectable| is true. For example,
  // obscured labels do not support text selection.
  bool SetSelectable(bool selectable);

  // Returns true if the label has a selection.
  bool HasSelection() const;

  // Returns true if the label has the whole text selected.
  bool HasFullSelection() const;

  // Selects the entire text. NO-OP if the label is not selectable.
  void SelectAll();

  // Clears any active selection.
  void ClearSelection();

  // Selects the given text range. NO-OP if the label is not selectable or the
  // |range| endpoints don't lie on grapheme boundaries.
  void SelectRange(const gfx::Range& range);

  // Get the visual bounds containing the logical substring of the full text
  // within the |range|. See gfx::RenderText.
  std::vector<gfx::Rect> GetSubstringBounds(const gfx::Range& range);

  [[nodiscard]] base::CallbackListSubscription AddTextChangedCallback(
      views::PropertyChangedCallback callback);

  [[nodiscard]] base::CallbackListSubscription AddTextContextChangedCallback(
      PropertyChangedCallback callback);

  // View:
  int GetBaseline() const override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool GetCanProcessEventsWithinSubtree() const override;
  WordLookupClient* GetWordLookupClient() override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 protected:
  // Create a single RenderText instance to actually be painted.
  virtual std::unique_ptr<gfx::RenderText> CreateRenderText() const;

  // Returns the preferred size and position of the text in local coordinates,
  // which may exceed the local bounds of the label.
  gfx::Rect GetTextBounds() const;

  // Returns the Y coordinate the font_list() will actually be drawn at, in
  // local coordinates.  This may differ from GetTextBounds().y() since the font
  // is positioned inside the display rect.
  int GetFontListY() const;

  void PaintText(gfx::Canvas* canvas);

  // View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnThemeChanged() override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LabelTest, ResetRenderTextData);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, MultilineSupportedRenderText);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, TextChangeWithoutLayout);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, EmptyLabel);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, FocusBounds);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, MultiLineSizingWithElide);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, IsDisplayTextTruncated);
  FRIEND_TEST_ALL_PREFIXES(LabelTest, ChecksSubpixelRenderingOntoOpaqueSurface);
  FRIEND_TEST_ALL_PREFIXES(ViewAXPlatformNodeDelegateWinInnerTextRangeTest,
                           Label_LTR);
  FRIEND_TEST_ALL_PREFIXES(ViewAXPlatformNodeDelegateWinInnerTextRangeTest,
                           Label_RTL);
  friend class LabelSelectionTest;

  // ContextMenuController overrides:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // WordLookupClient overrides:
  bool GetWordLookupDataAtPoint(const gfx::Point& point,
                                gfx::DecoratedText* decorated_word,
                                gfx::Rect* rect) override;

  bool GetWordLookupDataFromSelection(gfx::DecoratedText* decorated_text,
                                      gfx::Rect* rect) override;

  // SelectionControllerDelegate overrides:
  gfx::RenderText* GetRenderTextForSelectionController() override;
  bool IsReadOnly() const override;
  bool SupportsDrag() const override;
  bool HasTextBeingDragged() const override;
  void SetTextBeingDragged(bool value) override;
  int GetViewHeight() const override;
  int GetViewWidth() const override;
  int GetDragSelectionDelay() const override;
  void OnBeforePointerAction() override;
  void OnAfterPointerAction(bool text_changed, bool selection_changed) override;
  bool PasteSelectionClipboard() override;
  void UpdateSelectionClipboard() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  const gfx::RenderText* GetRenderTextForSelectionController() const;

  void Init(const std::u16string& text,
            const gfx::FontList& font_list,
            gfx::DirectionalityMode directionality_mode);

  // Set up |display_text_| to actually be painted.
  void MaybeBuildDisplayText() const;

  // Get the text size for the current layout.
  gfx::Size GetTextSize() const;

  // Get the text size that ignores the current layout and respects
  // `available_size`.
  gfx::Size GetBoundedTextSize(const SizeBounds& available_size) const;

  // Returns the height of the Label given the width `w`.
  int GetLabelHeightForWidth(int w) const;

  // Returns the appropriate foreground color to use given the proposed
  // |foreground| and |background| colors.
  SkColor GetForegroundColor(SkColor foreground, SkColor background) const;

  // Updates text and selection colors from requested colors.
  void RecalculateColors();

  // Applies the foreground color to |display_text_|.
  void ApplyTextColors() const;

  // Updates any colors that have not been explicitly set from the theme.
  void UpdateColorsFromTheme();

  bool ShouldShowDefaultTooltip() const;

  // Clears |display_text_| and updates |stored_selection_range_|.
  // TODO(crbug.com/40704805) Most uses of this function are inefficient; either
  // replace with setting attributes on both RenderTexts or collapse them to one
  // RenderText.
  void ClearDisplayText();

  // Returns the currently selected text.
  std::u16string GetSelectedText() const;

  // Updates the clipboard with the currently selected text.
  void CopyToClipboard();

  // Builds |context_menu_contents_|.
  void BuildContextMenuContents();

  // Updates the elide behavior used by |full_text_|.
  void UpdateFullTextElideBehavior();

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // Calculate widths for each grapheme and word starts and ends. Used for
  // accessibility. Currently only on Windows when UIA is enabled.
  bool RefreshAccessibleTextOffsets();

  // The string used to compute the text offsets for accessibility. This is used
  // to determine if the offsets need to be recomputed.
  std::u16string ax_name_used_to_compute_offsets_;
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

  int text_context_;
  int text_style_;
  std::optional<int> line_height_;

  // An un-elided and single-line RenderText object used for preferred sizing.
  std::unique_ptr<gfx::RenderText> full_text_;

  // The RenderText instance used for drawing.
  mutable std::unique_ptr<gfx::RenderText> display_text_;

  // Persists the current selection range between the calls to
  // ClearDisplayText() and MaybeBuildDisplayText(). Holds an InvalidRange when
  // not in use.
  mutable gfx::Range stored_selection_range_ = gfx::Range::InvalidRange();

  SkColor requested_enabled_color_ = gfx::kPlaceholderColor;
  SkColor actual_enabled_color_ = gfx::kPlaceholderColor;
  SkColor background_color_ = gfx::kPlaceholderColor;
  SkColor requested_selection_text_color_ = gfx::kPlaceholderColor;
  SkColor actual_selection_text_color_ = gfx::kPlaceholderColor;
  SkColor selection_background_color_ = gfx::kPlaceholderColor;

  std::optional<ui::ColorId> enabled_color_id_;
  std::optional<ui::ColorId> background_color_id_;

  // Set to true once the corresponding setter is invoked.
  bool enabled_color_set_ = false;
  bool background_color_set_ = false;
  bool selection_text_color_set_ = false;
  bool selection_background_color_set_ = false;

  gfx::ElideBehavior elide_behavior_ = gfx::ELIDE_TAIL;

  bool subpixel_rendering_enabled_ = true;
  bool skip_subpixel_rendering_opacity_check_ = false;
  bool auto_color_readability_enabled_ = true;
  // TODO(mukai): remove |multi_line_| when all RenderText can render multiline.
  bool multi_line_ = false;
  size_t max_lines_ = 0;
  std::u16string tooltip_text_;
  bool handles_tooltips_ = true;
  // Whether to collapse the label when it's not visible.
  bool collapse_when_hidden_ = false;
  int fixed_width_ = 0;
  // This is used only for multi-line mode.
  int max_width_ = 0;
  // This is used in single-line mode.
  int max_width_single_line_ = 0;

  std::unique_ptr<SelectionController> selection_controller_;

  // Context menu related members.
  ui::SimpleMenuModel context_menu_contents_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Label, View)
VIEW_BUILDER_PROPERTY(const gfx::FontList&, FontList)
VIEW_BUILDER_PROPERTY(const std::u16string&, Text)
VIEW_BUILDER_PROPERTY(int, TextStyle)
VIEW_BUILDER_PROPERTY(int, TextContext)
VIEW_BUILDER_PROPERTY(bool, AutoColorReadabilityEnabled)
VIEW_BUILDER_PROPERTY(SkColor, EnabledColor)
VIEW_BUILDER_PROPERTY(SkColor, BackgroundColor)
VIEW_BUILDER_PROPERTY(SkColor, SelectionTextColor)
VIEW_BUILDER_PROPERTY(SkColor, SelectionBackgroundColor)
VIEW_BUILDER_PROPERTY(ui::ColorId, EnabledColorId)
VIEW_BUILDER_PROPERTY(ui::ColorId, BackgroundColorId)
VIEW_BUILDER_PROPERTY(const gfx::ShadowValues&, Shadows)
VIEW_BUILDER_PROPERTY(bool, SubpixelRenderingEnabled)
VIEW_BUILDER_PROPERTY(bool, SkipSubpixelRenderingOpacityCheck)
VIEW_BUILDER_PROPERTY(gfx::HorizontalAlignment, HorizontalAlignment)
VIEW_BUILDER_PROPERTY(gfx::VerticalAlignment, VerticalAlignment)
VIEW_BUILDER_PROPERTY(int, LineHeight)
VIEW_BUILDER_PROPERTY(bool, MultiLine)
VIEW_BUILDER_PROPERTY(int, MaxLines)
VIEW_BUILDER_PROPERTY(bool, Obscured)
VIEW_BUILDER_PROPERTY(bool, AllowCharacterBreak)
VIEW_BUILDER_PROPERTY(size_t, TruncateLength)
VIEW_BUILDER_PROPERTY(gfx::ElideBehavior, ElideBehavior)
VIEW_BUILDER_PROPERTY(const std::u16string&, TooltipText)
VIEW_BUILDER_PROPERTY(bool, HandlesTooltips)
VIEW_BUILDER_PROPERTY(int, MaximumWidth)
VIEW_BUILDER_PROPERTY(int, MaximumWidthSingleLine)
VIEW_BUILDER_PROPERTY(bool, CollapseWhenHidden)
VIEW_BUILDER_PROPERTY(bool, Selectable)
VIEW_BUILDER_METHOD(SizeToFit, int)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Label)

#endif  // UI_VIEWS_CONTROLS_LABEL_H_
