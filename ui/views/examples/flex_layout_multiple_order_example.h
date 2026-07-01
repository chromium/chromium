// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_FLEX_LAYOUT_MULTIPLE_ORDER_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_FLEX_LAYOUT_MULTIPLE_ORDER_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/view.h"

namespace views {
class Label;

namespace examples {

class VIEWS_EXAMPLES_EXPORT FlexLayoutMultipleOrderExample
    : public ExampleBase {
 public:
  FlexLayoutMultipleOrderExample();
  FlexLayoutMultipleOrderExample(const FlexLayoutMultipleOrderExample&) =
      delete;
  FlexLayoutMultipleOrderExample& operator=(
      const FlexLayoutMultipleOrderExample&) = delete;
  ~FlexLayoutMultipleOrderExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // A view that calls UpdateViews() on its parents after each layout.
  class UpdateChildrenOnLayoutView : public View {
    METADATA_HEADER(UpdateChildrenOnLayoutView, View)
   public:
    explicit UpdateChildrenOnLayoutView(
        base::WeakPtr<FlexLayoutMultipleOrderExample> parent);
    ~UpdateChildrenOnLayoutView() override;

    void Layout(PassKey) override;

   private:
    base::WeakPtr<FlexLayoutMultipleOrderExample> parent_;
  };

  // Updates labels to reflect the current size of each view in
  // `flex_container_`.
  void UpdateViews();

  raw_ptr<Label> status_label_ = nullptr;
  raw_ptr<View> flex_container_ = nullptr;
  raw_ptr<View> view1_ = nullptr;
  raw_ptr<View> view2_ = nullptr;
  raw_ptr<View> view_a_ = nullptr;
  raw_ptr<View> view_b_ = nullptr;
  raw_ptr<View> view_c_ = nullptr;
  raw_ptr<View> view_d_ = nullptr;
  raw_ptr<Label> view_a_label_ = nullptr;
  raw_ptr<Label> view_b_label_ = nullptr;
  raw_ptr<Label> view_c_label_ = nullptr;
  raw_ptr<Label> view_d_label_ = nullptr;
  raw_ptr<View> spacer_ = nullptr;
  raw_ptr<Label> spacer_label_ = nullptr;

  base::WeakPtrFactory<FlexLayoutMultipleOrderExample> weak_ptr_factory_{this};
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_FLEX_LAYOUT_MULTIPLE_ORDER_EXAMPLE_H_
