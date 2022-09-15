// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_OWNER_H_
#define UI_COMPOSITOR_LAYER_OWNER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Layer;

class COMPOSITOR_EXPORT LayerOwner {
 public:
  class Observer {
   public:
    // Called when the |layer()| of this LayerOwner has been recreated (i.e.
    // RecreateLayer() was called). |old_layer| should not be retained after
    // this as it may be destroyed soon.
    // The new layer can be retrieved from LayerOwner::layer().
    virtual void OnLayerRecreated(ui::Layer* old_layer) = 0;

   protected:
    virtual ~Observer() = default;
  };

  explicit LayerOwner(std::unique_ptr<Layer> layer = nullptr);

  LayerOwner(const LayerOwner&) = delete;
  LayerOwner& operator=(const LayerOwner&) = delete;

  virtual ~LayerOwner();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Releases the owning reference to its layer, and returns it.
  // This is used when you need to animate the presentation of the owner just
  // prior to destroying it. The Owner can be destroyed soon after calling this
  // function, and the caller is then responsible for disposing of the layer
  // once any animation completes. Note that layer() will remain valid until the
  // end of ~LayerOwner().
  std::unique_ptr<Layer> AcquireLayer();

  // Similar to AcquireLayer(), but layer() will be set to nullptr immediately.
  virtual std::unique_ptr<Layer> ReleaseLayer();

  // Releases the ownership of the current layer, and takes ownership of
  // |layer|.
  void Reset(std::unique_ptr<Layer> layer);

  // Asks the owner to recreate the layer, returning the old Layer. nullptr is
  // returned if there is no existing layer, or recreate is not supported.
  //
  // This does not recurse. Existing children of the layer are moved to the new
  // layer.
  virtual std::unique_ptr<Layer> RecreateLayer();

  ui::Layer* layer() { return layer_; }
  const ui::Layer* layer() const { return layer_; }

  bool OwnsLayer() const;

 protected:
  virtual void SetLayer(std::unique_ptr<Layer> layer);
  void DestroyLayer();

 private:
  // The LayerOwner owns its layer unless ownership is relinquished via a call
  // to AcquireLayer(). After that moment |layer_| will still be valid but
  // |layer_owner_| will be nullptr. The reason for releasing ownership is that
  // the client may wish to animate the layer beyond the lifetime of the owner,
  // e.g. fading it out when it is destroyed.
  std::unique_ptr<Layer> layer_owner_;
  raw_ptr<Layer> layer_ = nullptr;

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_OWNER_H_
