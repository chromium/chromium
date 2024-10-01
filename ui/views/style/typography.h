// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_TYPOGRAPHY_H_
#define UI_VIEWS_STYLE_TYPOGRAPHY_H_

namespace views::style {

// Where a piece of text appears in the UI. This influences size and weight, but
// typically not style or color.
enum TextContext {
  // Embedders can extend this enum with additional values that are understood
  // by the TypographyProvider offered by their ViewsDelegate. Embedders define
  // enum values from VIEWS_TEXT_CONTEXT_END. Values named beginning with
  // "CONTEXT_" represent the actual TextContexts: the rest are markers.
  VIEWS_TEXT_CONTEXT_START = 0,

  // Text that appears on a views::Badge. Always 9pt.
  CONTEXT_BADGE = VIEWS_TEXT_CONTEXT_START,

  // Text that appears over the slightly shaded background of a bubble footer.
  CONTEXT_BUBBLE_FOOTER,

  // Text that appears on a button control. Usually 12pt. This includes controls
  // with button-like behavior, such as Checkbox.
  CONTEXT_BUTTON,

  // Text that appears on an MD-styled dialog button control. Usually 12pt.
  CONTEXT_BUTTON_MD,

  // A title for a dialog window. Usually 15pt. Multi-line OK.
  CONTEXT_DIALOG_TITLE,

  // Text to label a control, usually next to it. "Body 2". Usually 12pt.
  CONTEXT_LABEL,

  // Text used for body text in dialogs.
  CONTEXT_DIALOG_BODY_TEXT,

  // Text in a table row.
  CONTEXT_TABLE_ROW,

  // An editable text field. Usually matches CONTROL_LABEL.
  CONTEXT_TEXTFIELD,

  // Placeholder text in a text field.
  CONTEXT_TEXTFIELD_PLACEHOLDER,

  // Supporting text for a text field, usually below it.
  CONTEXT_TEXTFIELD_SUPPORTING_TEXT,

  // Text in a menu.
  CONTEXT_MENU,

  // Text for the menu items that appear in the touch-selection context menu.
  CONTEXT_TOUCH_MENU,

  // Embedders must start TextContext enum values from this value.
  VIEWS_TEXT_CONTEXT_END,

  // All TextContext enum values must be below this value.
  TEXT_CONTEXT_MAX = 0x1000
};

// How a piece of text should be presented. This influences color and style, but
// typically not size.
enum TextStyle {
  // TextStyle enum values must always be greater than any TextContext value.
  // This allows the code to verify at runtime that arguments of the two types
  // have not been swapped.
  VIEWS_TEXT_STYLE_START = TEXT_CONTEXT_MAX,

  // Primary text: solid black, normal weight. Converts to DISABLED in some
  // contexts (e.g. BUTTON_TEXT, FIELD).
  STYLE_PRIMARY = VIEWS_TEXT_STYLE_START,

  // Secondary text: Appears near the primary text.
  STYLE_SECONDARY,

  // "Hint" text, usually a line that gives context to something more important.
  STYLE_HINT,

  // Style for text that is displayed in a selection.
  STYLE_SELECTED,

  // Style for text is part of a static highlight.
  STYLE_HIGHLIGHTED,

  // Style for the default button on a dialog.
  STYLE_DIALOG_BUTTON_DEFAULT,

  // Style for the tonal button on a dialog.
  STYLE_DIALOG_BUTTON_TONAL,

  // Disabled "greyed out" text.
  STYLE_DISABLED,

  // Used to draw attention to a section of body text such as an extension name
  // or hostname.
  STYLE_EMPHASIZED,

  // Emphasized secondary style. Like STYLE_EMPHASIZED but styled to match
  // surrounding STYLE_SECONDARY text.
  STYLE_EMPHASIZED_SECONDARY,

  // Style for invalid text. Can be either primary or solid red color.
  STYLE_INVALID,

  // The style used for links. Usually a solid shade of blue.
  STYLE_LINK,
  // Active tab in a tabbed pane.
  STYLE_TAB_ACTIVE,

  // Similar to STYLE_PRIMARY but with a monospaced typeface.
  // It is currently expected to be overridden by `ChromeTypographyProvider`,
  // and the default implementation is not actually monospaced.
  // TODO(crbug.com/367623931): Add proper default implementation.
  STYLE_PRIMARY_MONOSPACED,

  // Similar to views::style::STYLE_SECONDARY but with a monospaced typeface.
  // It is currently expected to be overridden by `ChromeTypographyProvider`,
  // and the default implementation is not actually monospaced.
  // TODO(crbug.com/367623931): Add proper default implementation.
  STYLE_SECONDARY_MONOSPACED,

  // CR2023 typography tokens.
  // These styles override the style specified by TextContext.
  STYLE_OVERRIDE_TYPOGRAPHY_START,
  STYLE_HEADLINE_1,
  STYLE_HEADLINE_2,
  STYLE_HEADLINE_3,
  STYLE_HEADLINE_4,
  STYLE_HEADLINE_4_BOLD,
  STYLE_HEADLINE_5,
  STYLE_BODY_1,
  STYLE_BODY_1_EMPHASIS,
  STYLE_BODY_1_MEDIUM = STYLE_BODY_1_EMPHASIS,
  STYLE_BODY_1_BOLD,
  STYLE_BODY_2,
  STYLE_BODY_2_EMPHASIS,
  STYLE_BODY_2_MEDIUM = STYLE_BODY_2_EMPHASIS,
  STYLE_BODY_2_BOLD,
  STYLE_BODY_3,
  STYLE_BODY_3_EMPHASIS,
  STYLE_BODY_3_MEDIUM = STYLE_BODY_3_EMPHASIS,
  STYLE_BODY_3_BOLD,
  STYLE_BODY_4,
  STYLE_BODY_4_EMPHASIS,
  STYLE_BODY_4_MEDIUM = STYLE_BODY_4_EMPHASIS,
  STYLE_BODY_4_BOLD,
  STYLE_BODY_5,
  STYLE_BODY_5_EMPHASIS,
  STYLE_BODY_5_MEDIUM = STYLE_BODY_5_EMPHASIS,
  STYLE_BODY_5_BOLD,
  STYLE_CAPTION,
  STYLE_CAPTION_EMPHASIS,
  STYLE_CAPTION_MEDIUM = STYLE_CAPTION_EMPHASIS,
  STYLE_CAPTION_BOLD,
  // The style used for links within blocks of STYLE_BODY_3 text.
  STYLE_LINK_3,
  // The style used for links within blocks of STYLE_BODY_5 text.
  STYLE_LINK_5,
  STYLE_OVERRIDE_TYPOGRAPHY_END,

  // Embedders must start TextStyle enum values from here.
  VIEWS_TEXT_STYLE_END
};

}  // namespace views::style

#endif  // UI_VIEWS_STYLE_TYPOGRAPHY_H_
