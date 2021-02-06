// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/examples/example_base.h"

namespace views {
class Button;
class TabbedPane;

namespace examples {

// A TabbedPane example tests adding and selecting tabs.
class VIEWS_EXAMPLES_EXPORT TabbedPaneExample : public ExampleBase,
                                                public TabbedPaneListener {
 public:
  TabbedPaneExample();
  ~TabbedPaneExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // TabbedPaneListener:
  void TabSelectedAt(int index) override;

  // Print the status of the tab in the status area.
  void PrintCurrentStatus();

  void AddButton(const base::string16& label);

  void AddAtButtonPressed();

  // The tabbed pane to be tested.
  TabbedPane* tabbed_pane_;

  // Control buttons to add and select tabs.
  Button* add_;
  Button* add_at_;
  Button* select_at_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPaneExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
