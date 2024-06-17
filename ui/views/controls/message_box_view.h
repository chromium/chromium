// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_
#define UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {

class Checkbox;
class Label;
class LayoutProvider;
class ScrollView;
class Textfield;

// This class displays the contents of a message box. It is intended for use
// within a constrained window, and has options for a message, prompt, OK
// and Cancel buttons.
class VIEWS_EXPORT MessageBoxView : public BoxLayoutView {
  METADATA_HEADER(MessageBoxView, BoxLayoutView)

 public:
  // |detect_directionality| indicates whether |message|'s directionality is
  // auto-detected.
  // For a message from a web page (not from Chrome's UI), such as script
  // dialog text, each paragraph's directionality is auto-detected using the
  // directionality of the paragraph's first strong character's. Please refer
  // to HTML5 spec for details.
  // http://dev.w3.org/html5/spec/Overview.html#text-rendered-in-native-user-interfaces:
  // The spec does not say anything about alignment. And we choose to
  // align all paragraphs according to the direction of the first paragraph.
  explicit MessageBoxView(const std::u16string& message = std::u16string(),
                          bool detect_directionality = false);

  MessageBoxView(const MessageBoxView&) = delete;
  MessageBoxView& operator=(const MessageBoxView&) = delete;

  ~MessageBoxView() override;

  // Returns the visible prompt field, returns nullptr otherwise.
  views::Textfield* GetVisiblePromptField();

  // Returns user entered data in the prompt field, returns an empty string if
  // no visible prompt field.
  std::u16string GetInputText();

  // Returns true if this message box has a visible checkbox, false otherwise.
  bool HasVisibleCheckBox() const;

  // Returns true if a checkbox is selected, false otherwise. (And false if
  // the message box has no checkbox.)
  bool IsCheckBoxSelected();

  // Shows a checkbox with the specified label to the message box if this is the
  // first call. Otherwise, it changes the label of the current checkbox. To
  // start, the message box has no visible checkbox until this function is
  // called.
  void SetCheckBoxLabel(const std::u16string& label);

  // Sets the state of the check-box if it is visible.
  void SetCheckBoxSelected(bool selected);

  // Sets the text and the callback of the link. |text| must be non-empty.
  void SetLink(const std::u16string& text, Link::ClickedCallback callback);

  void SetInterRowVerticalSpacing(int spacing);
  void SetMessageWidth(int width);

  // Adds a prompt field with |default_prompt| as the displayed text.
  void SetPromptField(const std::u16string& default_prompt);

 protected:
  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  // Handles Ctrl-C and writes the message in the system clipboard.
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MessageBoxViewTest, CheckMessageOnlySize);
  FRIEND_TEST_ALL_PREFIXES(MessageBoxViewTest, CheckWithOptionalViewsSize);
  FRIEND_TEST_ALL_PREFIXES(MessageBoxViewTest, CheckInterRowHeightChange);

  // Sets up the layout manager based on currently initialized views and layout
  // parameters. Should be called when a view is initialized or changed.
  void ResetLayoutManager();

  // Return the proper horizontal insets based on the given layout provider.
  gfx::Insets GetHorizontalInsets(const LayoutProvider* provider);

  // Message for the message box.
  std::vector<raw_ptr<Label, VectorExperimental>> message_labels_;

  // Scrolling view containing the message labels.
  raw_ptr<ScrollView> scroll_view_ = nullptr;

  // Input text field for the message box.
  raw_ptr<Textfield> prompt_field_ = nullptr;

  // Checkbox for the message box.
  raw_ptr<Checkbox> checkbox_ = nullptr;

  // Link displayed at the bottom of the view.
  raw_ptr<Link> link_ = nullptr;

  // Spacing between rows in the grid layout.
  int inter_row_vertical_spacing_ = 0;

  // Maximum width of the message label.
  int message_width_ = 0;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, MessageBoxView, BoxLayoutView)
VIEW_BUILDER_PROPERTY(const std::u16string&, CheckBoxLabel)
VIEW_BUILDER_PROPERTY(bool, CheckBoxSelected)
VIEW_BUILDER_METHOD(SetLink, const std::u16string&, Link::ClickedCallback)
VIEW_BUILDER_PROPERTY(int, InterRowVerticalSpacing)
VIEW_BUILDER_PROPERTY(int, MessageWidth)
VIEW_BUILDER_PROPERTY(const std::u16string&, PromptField)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, views::MessageBoxView)

#endif  // UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_
