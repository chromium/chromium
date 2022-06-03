// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_UNIQUE_WIDGET_PTR_H_
#define UI_VIEWS_WIDGET_UNIQUE_WIDGET_PTR_H_

#include <memory>

#include "ui/views/views_export.h"

namespace views {

class Widget;

// Ensures the Widget is properly closed when this special
// auto pointer goes out of scope.

class VIEWS_EXPORT UniqueWidgetPtr {
 public:
  UniqueWidgetPtr();
  // This class acts like a std::unique_ptr<Widget>, so this constructor is
  // deliberately implicit.
  UniqueWidgetPtr(std::unique_ptr<Widget> widget);  // NOLINT
  UniqueWidgetPtr(UniqueWidgetPtr&&);
  UniqueWidgetPtr& operator=(UniqueWidgetPtr&&);
  ~UniqueWidgetPtr();

  explicit operator bool() const;
  Widget& operator*() const;
  Widget* operator->() const;
  void reset();
  Widget* get() const;

 private:
  class UniqueWidgetPtrImpl;

  std::unique_ptr<UniqueWidgetPtrImpl> unique_widget_ptr_impl_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_UNIQUE_WIDGET_PTR_H_
