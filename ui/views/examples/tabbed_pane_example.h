// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/examples/example_base.h"

namespace views {
class Button;

namespace examples {

// A TabbedPane example tests adding and selecting tabs.
class VIEWS_EXAMPLES_EXPORT TabbedPaneExample : public ExampleBase,
                                                public TabbedPaneListener {
 public:
  TabbedPaneExample();
  TabbedPaneExample(const TabbedPaneExample&) = delete;
  TabbedPaneExample& operator=(const TabbedPaneExample&) = delete;
  ~TabbedPaneExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // TabbedPaneListener:
  void TabSelectedAt(int index) override;

  void CreateTabbedPane(View* container,
                        TabbedPane::Orientation orientation,
                        TabbedPane::TabStripStyle style);
  void PrintCurrentStatus();
  void SwapLayout();
  void ToggleHighlighted();
  void AddTab(const std::u16string& label);
  void AddAt();
  void SelectAt();

  // The tabbed pane to be tested.
  raw_ptr<TabbedPane> tabbed_pane_;

  // The button that toggles highlighted style.
  raw_ptr<Button> toggle_highlighted_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TABBED_PANE_EXAMPLE_H_
