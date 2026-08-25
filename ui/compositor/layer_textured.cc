// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_textured.h"

#include <cstdint>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_mirror.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

LayerTextured::LayerTextured() : Layer(LAYER_TEXTURED) {
  content_layer_ = cc::PictureLayer::Create(this);
  cc_layer_ = content_layer_.get();
  InitializeCcLayer();
}

LayerTextured::~LayerTextured() {
  Destroy();
}

void LayerTextured::AddDeferredPaintRequest() {
  ++deferred_paint_requests_;
  TRACE_COUNTER("ui",
                perfetto::CounterTrack("DeferredPaintRequests",
                                       reinterpret_cast<uintptr_t>(this)),
                deferred_paint_requests_);
}

void LayerTextured::RemoveDeferredPaintRequest() {
  DCHECK_GT(deferred_paint_requests_, 0u);

  --deferred_paint_requests_;
  TRACE_COUNTER("ui",
                perfetto::CounterTrack("DeferredPaintRequests",
                                       reinterpret_cast<uintptr_t>(this)),
                deferred_paint_requests_);
  if (!deferred_paint_requests_ && !damaged_region_.IsEmpty()) {
    ScheduleDraw();
  }
}

bool LayerTextured::IsPaintDeferred() const {
  return deferred_paint_requests_ > 0;
}

std::unique_ptr<Layer> LayerTextured::Clone() const {
  auto clone = Layer::Clone();
  clone->AsTextured()->SetFillsBoundsCompletely(FillsBoundsCompletely());
  return clone;
}

bool LayerTextured::ShouldSchedulePaint() const {
  // LayerTextured only needs to schedule paint if it has a delegate to paint
  // its contents.
  return delegate_ != nullptr;
}

scoped_refptr<cc::DisplayItemList> LayerTextured::PaintContentsToDisplayList() {
  TRACE_EVENT1("ui", "LayerTextured::PaintContentsToDisplayList", "name",
               name_);
  gfx::Rect local_bounds(bounds().size());
  gfx::Rect invalidation(
      gfx::IntersectRects(paint_region_.bounds(), local_bounds));
  paint_region_.Clear();
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  if (delegate_) {
    delegate_->OnPaintLayer(PaintContext(display_list.get(),
                                         device_scale_factor_, invalidation,
                                         GetCompositor()->is_pixel_canvas()));
  }
  display_list->Finalize();
  // TODO(domlaskowski): Move mirror invalidation to Layer::SchedulePaint.
  for (const auto& mirror : mirrors_) {
    mirror->dest()->SchedulePaint(invalidation);
  }

  return display_list;
}

bool LayerTextured::FillsBoundsCompletely() const {
  return fills_bounds_completely_;
}

void LayerTextured::SetFillsBoundsCompletely(bool fills_bounds_completely) {
  fills_bounds_completely_ = fills_bounds_completely;
}

void LayerTextured::OnPaintScheduled() {
  if (IsPaintDeferred()) {
    return;
  }

  ScheduleDraw();
}

void LayerTextured::CommitDamage() {
  // If painting is deferred, or if there is no delegate to paint our contents,
  // do not commit the accumulated damage yet.
  if (deferred_paint_requests_ || !delegate_) {
    return;
  }

  // Record the accumulated damage in `paint_region_` to be used when the
  // compositor is ready to paint the content in PaintContentsToDisplayList().
  paint_region_.Union(damaged_region_);
  Layer::CommitDamage();
}

void LayerTextured::Reset() {
  if (content_layer_) {
    content_layer_->ClearClient();
  }

  content_layer_ = nullptr;
}

}  // namespace ui
