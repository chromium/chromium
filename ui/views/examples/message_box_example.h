// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/examples/example_base.h"

namespace views {
class MessageBoxView;

namespace examples {

// A MessageBoxView example. This tests some of checkbox features as well.
class VIEWS_EXAMPLES_EXPORT MessageBoxExample : public ExampleBase {
 public:
  MessageBoxExample();

  MessageBoxExample(const MessageBoxExample&) = delete;
  MessageBoxExample& operator=(const MessageBoxExample&) = delete;

  ~MessageBoxExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void StatusButtonPressed();

  // The MessageBoxView to be tested.
  raw_ptr<MessageBoxView> message_box_view_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_MESSAGE_BOX_EXAMPLE_H_
