// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_NOTIFICATION_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_NOTIFICATION_EXAMPLE_H_

#include "ui/views/examples/example_base.h"

namespace views {
namespace examples {

class VIEWS_EXAMPLES_EXPORT NotificationExample : public ExampleBase {
 public:
  NotificationExample();
  NotificationExample(const NotificationExample&) = delete;
  NotificationExample& operator=(const NotificationExample&) = delete;
  ~NotificationExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_NOTIFICATION_EXAMPLE_H_
