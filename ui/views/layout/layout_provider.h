// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_PROVIDER_H_
#define UI_VIEWS_LAYOUT_LAYOUT_PROVIDER_H_

#include "base/macros.h"
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
  // The margins around the edges of a tooltip bubble.
  INSETS_TOOLTIP_BUBBLE,
  // Padding to add to vector image buttons to increase their click and touch
  // target size.
  INSETS_VECTOR_IMAGE_BUTTON,
  // Padding used in a label button.
  INSETS_LABEL_BUTTON,

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

  // The default padding to add on each side of a button's label.
  DISTANCE_BUTTON_HORIZONTAL_PADDING = VIEWS_DISTANCE_START,
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
  // Horizontal margin between a table cell and its contents.
  DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN,
  // Horizontal padding applied to text in a textfield.
  DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING,
  // Vertical spacing between controls that are logically unrelated.
  DISTANCE_UNRELATED_CONTROL_VERTICAL,

  // Embedders must start DistanceMetric enum values from here.
  VIEWS_DISTANCE_END,

  // All Distance enum values must be below this value.
  VIEWS_DISTANCE_MAX = 0x2000
};

// The type of a dialog content element. TEXT should be used for Labels or other
// elements that only show text. Otherwise CONTROL should be used.
enum DialogContentType { CONTROL, TEXT };

enum EmphasisMetric {
  // No emphasis needed for shadows, corner radius, etc.
  EMPHASIS_NONE,
  // Use this to indicate low-emphasis interactive elements such as buttons and
  // text fields.
  EMPHASIS_LOW,
  // Use this for components with medium emphasis, such the autofill dropdown.
  EMPHASIS_MEDIUM,
  // High-emphasis components, such as tabs or dialogs.
  EMPHASIS_HIGH,
  // Maximum emphasis components like the omnibox or rich suggestions.
  EMPHASIS_MAXIMUM,
};

class VIEWS_EXPORT LayoutProvider {
 public:
  LayoutProvider();
  virtual ~LayoutProvider();

  // This should never return nullptr.
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
  // font, weight, color, size, and line height. Never null.
  virtual const TypographyProvider& GetTypographyProvider() const;

  // Returns the actual width to use for a dialog that requires at least
  // |min_width|.
  virtual int GetSnappedDialogWidth(int min_width) const;

  // Returns the insets that should be used around a dialog's content for the
  // given type of content. |leading| is the type (text or control) of the first
  // element in the content  and |trailing| is the type of the final element.
  gfx::Insets GetDialogInsetsForContentType(DialogContentType leading,
                                            DialogContentType trailing) const;

  // TODO (https://crbug.com/822000): Possibly combine the following two
  // functions into a single function returning a struct. Keeping them separate
  // for now in case different emphasis is needed for different elements in the
  // same context. Delete this TODO in Q4 2018.

  // Returns the corner radius specific to the given emphasis metric.
  virtual int GetCornerRadiusMetric(EmphasisMetric emphasis_metric,
                                    const gfx::Size& size = gfx::Size()) const;

  // Returns the shadow elevation metric for the given emphasis.
  virtual int GetShadowElevationMetric(EmphasisMetric emphasis_metric) const;

  // Creates shadows for the given elevation. Use GetShadowElevationMetric for
  // the appropriate elevation.
  virtual gfx::ShadowValues MakeShadowValues(int elevation,
                                             SkColor color) const;

 private:
  TypographyProvider typography_provider_;

  DISALLOW_COPY_AND_ASSIGN(LayoutProvider);
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_PROVIDER_H_
