// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_SHADOW_H_
#define UI_VIEWS_VIEW_SHADOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace ui {
class Shadow;
}

namespace views {

// Manages the shadow for a view. This forces |view| to paint to layer if it's
// not.
class VIEWS_EXPORT ViewShadow : public ViewObserver,
                                public ui::LayerOwner::Observer {
 public:
  ViewShadow(View* view, int elevation);

  ViewShadow(const ViewShadow&) = delete;
  ViewShadow& operator=(const ViewShadow&) = delete;

  ~ViewShadow() override;

  // Update the corner radius of the view along with the shadow.
  void SetRoundedCornerRadius(int corner_radius);

  // ui::LayerOwner::Observer:
  void OnLayerRecreated(ui::Layer* old_layer) override;

  ui::Shadow* shadow() { return shadow_.get(); }
  const ui::Shadow* shadow() const { return shadow_.get(); }

 private:
  // ViewObserver:
  void OnViewLayerBoundsSet(View* view) override;
  void OnViewIsDeleting(View* view) override;

  raw_ptr<View> view_;
  std::unique_ptr<ui::Shadow> shadow_;

  base::ScopedObservation<View, ViewObserver> view_observation_{this};
  base::ScopedObservation<ui::Shadow, ui::LayerOwner::Observer>
      shadow_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_SHADOW_H_
