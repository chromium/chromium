// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TEST_API_H_
#define UI_VIEWS_VIEW_TEST_API_H_

#include "ui/views/view.h"

namespace views {

class VIEWS_EXPORT ViewTestApi {
 public:
  explicit ViewTestApi(View* view) : view_(view) {}
  ~ViewTestApi() = default;

  bool needs_layout() { return view_->needs_layout(); }

 private:
  View* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewTestApi);
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TEST_API_H_
