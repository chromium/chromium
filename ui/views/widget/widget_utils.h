// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_UTILS_H_
#define UI_VIEWS_WIDGET_WIDGET_UTILS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class Layer;
}

namespace views {
class Widget;

class VIEWS_EXPORT WidgetOpenTimer : public WidgetObserver {
 public:
  using Callback = base::RepeatingCallback<void(base::TimeDelta)>;

  explicit WidgetOpenTimer(Callback callback);
  WidgetOpenTimer(const WidgetOpenTimer&) = delete;
  const WidgetOpenTimer& operator=(const WidgetOpenTimer&) = delete;
  ~WidgetOpenTimer() override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  // Called to start the |open_timer_|.
  void Reset(Widget* widget);

 private:
  // Callback run when the passed in Widget is destroyed.
  Callback callback_;

  // Time the bubble has been open. Used for UMA metrics collection.
  std::optional<base::ElapsedTimer> open_timer_;

  base::ScopedObservation<Widget, WidgetObserver> observed_widget_{this};
};

// Returns the root window for |widget|.  On non-Aura, this is equivalent to
// widget->GetNativeWindow().
VIEWS_EXPORT gfx::NativeWindow GetRootWindow(const Widget* widget);

struct VIEWS_EXPORT LayerRelation {
  enum class Type {
    // First layer is a descendant of second layer (inclusive).
    kFirstIsChildOfSecond,
    // Second layer is a descendant of first layer (exclusive).
    kSecondIsChildOfFirst,
    // First and second layers are in sibling subtrees under a common parent.
    kSiblings,
    // First and second layers do not share a common parent.
    kDisjoint
  };

  Type type = Type::kDisjoint;
  raw_ptr<ui::Layer> common_parent = nullptr;
  raw_ptr<ui::Layer> ancestor_of_first = nullptr;
  raw_ptr<ui::Layer> ancestor_of_second = nullptr;
};

// Analyzes the relationship between two layers in the layer tree.
VIEWS_EXPORT LayerRelation GetLayerRelation(ui::Layer* first,
                                            ui::Layer* second);

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_UTILS_H_
