// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_NOTIFICATION_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_NOTIFICATION_EXAMPLE_H_

#include "base/scoped_observation.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views::examples {

class VIEWS_EXAMPLES_EXPORT NotificationExample : public ExampleBase,
                                                  public ViewObserver {
 public:
  NotificationExample();
  NotificationExample(const NotificationExample&) = delete;
  NotificationExample& operator=(const NotificationExample&) = delete;
  ~NotificationExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // ViewObserver:
  void OnViewAddedToWidget(View* observed_view) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  base::ScopedObservation<View, ViewObserver> observer_{this};
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_NOTIFICATION_EXAMPLE_H_
