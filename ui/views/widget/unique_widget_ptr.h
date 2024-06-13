// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_UNIQUE_WIDGET_PTR_H_
#define UI_VIEWS_WIDGET_UNIQUE_WIDGET_PTR_H_

#include <memory>

#include "base/scoped_observation.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

// A weak owning pointer to a Widget.
// This smart pointer acts like a unique_ptr: it ensures the Widget is
// properly closed when it goes out of scope.
// It also acts like a WeakPtr: the Widget may be deleted by its native
// widget and `get()` becomes nullptr when this happens.
// Caller may check the validity of this pointer before dereferencing it if
// the widget lifetime is in doubt.
class VIEWS_EXPORT UniqueWidgetPtr {
 public:
  UniqueWidgetPtr();
  // This class acts like a std::unique_ptr<Widget>, so this constructor is
  // deliberately implicit.
  UniqueWidgetPtr(std::unique_ptr<Widget> widget);  // NOLINT

  // Construct from a subclass instance of Widget. Note that ~Widget() is
  // virtual, so the downcasting is safe. This constructor is deliberately
  // implicit.
  template <class U>
  UniqueWidgetPtr(std::unique_ptr<U> widget) {  // NOLINT
    Init(std::unique_ptr<Widget>(widget.release()));
  }

  UniqueWidgetPtr(UniqueWidgetPtr&&);
  UniqueWidgetPtr& operator=(UniqueWidgetPtr&&);
  ~UniqueWidgetPtr();

  explicit operator bool() const;
  Widget& operator*() const;
  Widget* operator->() const;
  void reset();
  Widget* get() const;

 private:
  class Impl : public WidgetObserver {
   public:
    Impl();
    explicit Impl(std::unique_ptr<Widget> widget);
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() override;

    Widget* Get() const;

    // WidgetObserver overrides.
    void OnWidgetDestroying(Widget* widget) override;

   private:
    struct WidgetAutoCloser {
      void operator()(Widget* widget);
    };

    bool received_widget_destruction_signal_ = false;
    base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
    std::unique_ptr<Widget, WidgetAutoCloser> widget_closer_;
  };
  void Init(std::unique_ptr<Widget> widget);

  std::unique_ptr<Impl> unique_widget_ptr_impl_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_UNIQUE_WIDGET_PTR_H_
