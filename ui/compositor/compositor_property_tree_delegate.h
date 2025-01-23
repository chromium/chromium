// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_PROPERTY_TREE_DELEGATE_H_
#define UI_COMPOSITOR_COMPOSITOR_PROPERTY_TREE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "cc/trees/property_tree_delegate.h"
#include "ui/compositor/compositor_export.h"

namespace cc {
class LayerTreeHost;
}

namespace ui {

// TODO(crbug.com/389771428): This class exists to gradually move the
// Compositor from using the cc::Compositor in legacy layer tree mode
// to using it in property tree / layer list mode. This class should be
// removed once that migration is done and we can just use the
// cc::Compositor's default logic for property tree / layer list mode.
class COMPOSITOR_EXPORT CompositorPropertyTreeDelegate
    : public cc::PropertyTreeDelegate {
 public:
  // This class exists for testing purposes, so that tests can probe the
  // property trees once they've been updated.
  class Observer {
   public:
    // Called when UpdatePropertyTreesIfNeeded is finished.
    virtual void OnUpdateCalled(cc::LayerTreeHost* host) = 0;

   protected:
    virtual ~Observer() = default;
  };

  CompositorPropertyTreeDelegate() = default;
  CompositorPropertyTreeDelegate(const CompositorPropertyTreeDelegate&) =
      delete;
  CompositorPropertyTreeDelegate& operator=(
      const CompositorPropertyTreeDelegate&) = delete;
  ~CompositorPropertyTreeDelegate() override = default;

  void SetObserverForTesting(Observer*);

  // PropertyTreeDelegate overrides.
  void UpdatePropertyTreesIfNeeded(cc::LayerTreeHost*) override;

 private:
  raw_ptr<Observer> observer_ = nullptr;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_PROPERTY_TREE_DELEGATE_H_
