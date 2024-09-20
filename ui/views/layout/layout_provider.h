// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_PROVIDER_H_
#define UI_VIEWS_LAYOUT_LAYOUT_PROVIDER_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/views_export.h"

namespace views {

enum InsetsMetric {
  // Embedders can extend this enum with additional values that are understood
  // by the LayoutProvider implementation. Embedders define enum values from
  // VIEWS_INSETS_END. Values named beginning with "INSETS_" represent the
  // actual Insets: the rest are markers.
  VIEWS_INSETS_START = 0,

  // Internal border around checkboxes and radio buttons.
  INSETS_CHECKBOX_RADIO_BUTTON = VIEWS_INSETS_START,
  // The margins around the edges of the dialog.
  INSETS_DIALOG,
  // The margins around the button row of a dialog. The top margin is implied
  // by the content insets and the other margins overlap with INSETS_DIALOG.
  INSETS_DIALOG_BUTTON_ROW,
  // The insets to use for a section of a dialog that needs padding around it.
  // For example, the contents of a TabbedPane.
  INSETS_DIALOG_SUBSECTION,
  // The margins around the icon/title of a dialog. The bottom margin is implied
  // by the content insets and the other margins overlap with INSETS_DIALOG.
  INSETS_DIALOG_TITLE,
  // The margins for the dialog footnote content.
  INSETS_DIALOG_FOOTNOTE,
  // The margins around the edges of a tooltip bubble.
  INSETS_TOOLTIP_BUBBLE,
  // Padding to add to vector image buttons to increase their click and touch
  // target size.
  INSETS_VECTOR_IMAGE_BUTTON,
  // Padding used in a label button.
  INSETS_LABEL_BUTTON,
  // Padding used in icon buttons.
  INSETS_ICON_BUTTON,

  // Embedders must start Insets enum values from this value.
  VIEWS_INSETS_END,

  // All Insets enum values must be below this value.
  VIEWS_INSETS_MAX = 0x1000
};

enum DistanceMetric {
  // DistanceMetric enum values must always be greater than any InsetsMetric
  // value. This allows the code to verify at runtime that arguments of the
  // two types have not been interchanged.
  VIEWS_DISTANCE_START = VIEWS_INSETS_MAX,

  // Width of a bubble unless the content is too wide to make that
  // feasible.
  DISTANCE_BUBBLE_PREFERRED_WIDTH = VIEWS_DISTANCE_START,
  // The default padding to add on each side of a button's label.
  DISTANCE_BUTTON_HORIZONTAL_PADDING,
  // The maximum width a button can have and still influence the sizes of
  // other linked buttons.  This allows short buttons to have linked widths
  // without long buttons making things overly wide.
  DISTANCE_BUTTON_MAX_LINKABLE_WIDTH,
  // The distance between a dialog's edge and the close button in the upper
  // trailing corner.
  DISTANCE_CLOSE_BUTTON_MARGIN,
  // The vertical padding applied to text in a control.
  DISTANCE_CONTROL_VERTICAL_TEXT_PADDING,
  // The default minimum width of a dialog button.
  DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH,
  // The distance between the bottom of a dialog's content, when the final
  // content element is a control, and the top of the dialog's button row.
  DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL,
  // The distance between the bottom of a dialog's content, when the final
  // content element is text, and the top of the dialog's button row.
  DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT,
  // The distance between the bottom of a dialog's title and the top of the
  // dialog's content, when the first content element is a control.
  DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL,
  // The distance between the bottom of a dialog's title and the top of the
  // dialog's content, when the first content element is text.
  DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT,
  // Width of the space in a dropdown button between its label and down arrow.
  DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING,
  // Width of the horizontal padding in a dropdown button between the down arrow
  // and the button's border.
  DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN,
  // Width of the horizontal padding in a dropdown button between the button's
  // left border and the label.
  DISTANCE_DROPDOWN_BUTTON_LEFT_MARGIN,
  // Width of modal dialogs unless the content is too wide to make that
  // feasible.
  DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH,
  // The spacing between a pair of related horizontal buttons, used for
  // dialog layout.
  DISTANCE_RELATED_BUTTON_HORIZONTAL,
  // Horizontal spacing between controls that are logically related.
  DISTANCE_RELATED_CONTROL_HORIZONTAL,
  // The spacing between a pair of related vertical controls, used for
  // dialog layout.
  DISTANCE_RELATED_CONTROL_VERTICAL,
  // Horizontal spacing between an item such as an icon or checkbox and a
  // label related to it.
  DISTANCE_RELATED_LABEL_HORIZONTAL,
  // Height to stop at when expanding a scrollable area in a dialog to
  // accommodate its content.
  DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT,
  // Height to stop at when expanding a scrollable area in a modal dialog to
  // accomodate its content.
  DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT,
  // Horizontal margin between a table cell and its contents.
  DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN,
  // Horizontal padding applied to text in a textfield.
  DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING,
  // Horizontal spacing between controls that are logically unrelated.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL,
  // Vertical spacing between controls that are logically unrelated.
  DISTANCE_UNRELATED_CONTROL_VERTICAL,
  // Padding in vector icons. This is a general number for more vector icons.
  DISTANCE_VECTOR_ICON_PADDING,

  // Embedders must start DistanceMetric enum values from here.
  VIEWS_DISTANCE_END,

  // All Distance enum values must be below this value.
  VIEWS_DISTANCE_MAX = 0x2000
};

// The type of a dialog content element. kText should be used for Labels or
// other elements that only show text. Otherwise kControl should be used.
enum class DialogContentType { kControl, kText };

enum class Emphasis {
  // No emphasis needed for shadows, corner radius, etc.
  kNone,
  // Use this to indicate low-emphasis interactive elements such as buttons and
  // text fields.
  kLow,
  // Use this for components with medium emphasis, such the autofill dropdown.
  kMedium,
  // High-emphasis components, such as tabs or dialogs.
  kHigh,
  // Maximum emphasis components like the omnibox or rich suggestions.
  kMaximum,
};

// ShapeContextTokens are enums specific to the context of a Views object.
// This includes components such as Buttons, Labels, Textfields, Dropdowns, etc.
// These context tokens are granular to the entire client and will map to
// sys token values (see below).
enum class ShapeContextTokens {
  kBadgeRadius,
  kButtonRadius,
  kComboboxRadius,
  kDialogRadius,
  kFindBarViewRadius,
  kMenuRadius,
  kMenuAuxRadius,
  kMenuTouchRadius,
  kOmniboxExpandedRadius,
  kTextfieldRadius,
  kSidePanelContentRadius,
  kSidePanelPageContentRadius,
};

// ShapeSysTokens are tokens that map to a fixed value that aligns with UX/UI.
// Different from context tokens that will expand, sys tokens are more selective
// and are not used by the client. Context tokens will be mapped to a
// Sys token which then will fetch the corresponding fixed value.
enum class ShapeSysTokens {
  // Default token should never be used and signals a missing shaping token
  // mapping.
  kDefault,
  kXSmall,
  kSmall,
  kMediumSmall,
  kMedium,
  kLarge,
  kFull,
};

class VIEWS_EXPORT LayoutProvider {
 public:
  LayoutProvider();

  LayoutProvider(const LayoutProvider&) = delete;
  LayoutProvider& operator=(const LayoutProvider&) = delete;

  virtual ~LayoutProvider();

  // This should never return nullptr.
  // TODO(crbug.com/40178332): Replace callers of this with
  // View::GetLayoutProvider().
  static LayoutProvider* Get();

  // Calculates the control height based on the |font|'s reported glyph height,
  // the default line spacing and DISTANCE_CONTROL_VERTICAL_TEXT_PADDING.
  static int GetControlHeightForFont(int context,
                                     int style,
                                     const gfx::FontList& font);

  // Returns the insets metric according to the given enumeration element.
  virtual gfx::Insets GetInsetsMetric(int metric) const;

  // Returns the distance metric between elements according to the given
  // enumeration element.
  virtual int GetDistanceMetric(int metric) const;

  // Returns the TypographyProvider, used to configure text properties such as
  // font, weight, color, size, and line height.
  virtual const TypographyProvider& GetTypographyProvider() const;

  // Returns the actual width to use for a dialog that requires at least
  // |min_width|.
  virtual int GetSnappedDialogWidth(int min_width) const;

  // Returns the insets that should be used around a dialog's content for the
  // given type of content. |leading| is the type (text or control) of the first
  // element in the content  and |trailing| is the type of the final element.
  gfx::Insets GetDialogInsetsForContentType(DialogContentType leading,
                                            DialogContentType trailing) const;

  // TODO(crbug.com/41376600): Possibly combine the following two
  // functions into a single function returning a struct.

  // Returns the corner radius specific to the given emphasis.
  virtual int GetCornerRadiusMetric(Emphasis emphasis,
                                    const gfx::Size& size = gfx::Size()) const;

  // Returns the shadow elevation metric for the given emphasis.
  virtual int GetShadowElevationMetric(Emphasis emphasis) const;

  // Returns the corner radius related to a specific context token.
  // TODO(crbug.com/40255130): Replace GetCornerRadiusMetric(Emphasis...) with
  // context tokens.
  int GetCornerRadiusMetric(ShapeContextTokens token,
                            const gfx::Size& size = gfx::Size()) const;

 protected:
  static constexpr int kSmallDialogWidth = 320;
  static constexpr int kMediumDialogWidth = 448;
  static constexpr int kLargeDialogWidth = 512;

 private:
  TypographyProvider typography_provider_;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_PROVIDER_H_
