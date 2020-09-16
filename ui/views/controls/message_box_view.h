// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_
#define UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_

#include <stdint.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/link.h"
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
class VIEWS_EXPORT MessageBoxView : public View {
 public:
  METADATA_HEADER(MessageBoxView);

  // |detect_directionality| indicates whether |message|'s directionality is
  // auto-detected.
  // For a message from a web page (not from Chrome's UI), such as script
  // dialog text, each paragraph's directionality is auto-detected using the
  // directionality of the paragraph's first strong character's. Please refer
  // to HTML5 spec for details.
  // http://dev.w3.org/html5/spec/Overview.html#text-rendered-in-native-user-interfaces:
  // The spec does not say anything about alignment. And we choose to
  // align all paragraphs according to the direction of the first paragraph.
  explicit MessageBoxView(const base::string16& message = base::string16(),
                          bool detect_directionality = false);

  ~MessageBoxView() override;

  // Returns the visible prompt field, returns nullptr otherwise.
  views::Textfield* GetVisiblePromptField();

  // Returns user entered data in the prompt field, returns an empty string if
  // no visible prompt field.
  base::string16 GetInputText();

  // Returns true if this message box has a visible checkbox, false otherwise.
  bool HasVisibleCheckBox() const;

  // Returns true if a checkbox is selected, false otherwise. (And false if
  // the message box has no checkbox.)
  bool IsCheckBoxSelected();

  // Shows a checkbox with the specified label to the message box if this is the
  // first call. Otherwise, it changes the label of the current checkbox. To
  // start, the message box has no visible checkbox until this function is
  // called.
  void SetCheckBoxLabel(const base::string16& label);

  // Sets the state of the check-box if it is visible.
  void SetCheckBoxSelected(bool selected);

  // Sets the text and the callback of the link. |text| must be non-empty.
  void SetLink(const base::string16& text, Link::ClickedCallback callback);

  // View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void SetInterRowVerticalSpacing(int spacing);
  void SetMessageWidth(int width);

  // Adds a prompt field with |default_prompt| as the displayed text.
  void SetPromptField(const base::string16& default_prompt);

 protected:
  // View:
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
  std::vector<Label*> message_labels_;

  // Scrolling view containing the message labels.
  ScrollView* scroll_view_ = nullptr;

  // Input text field for the message box.
  Textfield* prompt_field_ = nullptr;

  // Checkbox for the message box.
  Checkbox* checkbox_ = nullptr;

  // Link displayed at the bottom of the view.
  Link* link_ = nullptr;

  // Spacing between rows in the grid layout.
  int inter_row_vertical_spacing_ = 0;

  // Maximum width of the message label.
  int message_width_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageBoxView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MESSAGE_BOX_VIEW_H_
