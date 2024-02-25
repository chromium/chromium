// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_TRANSIENT_WINDOW_CONTROLLER_H_
#define UI_WM_CORE_TRANSIENT_WINDOW_CONTROLLER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/client/transient_window_client.h"

namespace wm {

class TransientWindowManager;

// TransientWindowClient implementation. Uses TransientWindowManager to handle
// tracking transient per window.
class COMPONENT_EXPORT(UI_WM) TransientWindowController
    : public aura::client::TransientWindowClient {
 public:
  TransientWindowController();

  TransientWindowController(const TransientWindowController&) = delete;
  TransientWindowController& operator=(const TransientWindowController&) =
      delete;

  ~TransientWindowController() override;

  // Returns the single TransientWindowController instance.
  static TransientWindowController* Get() { return instance_; }

  // TransientWindowClient:
  void AddTransientChild(aura::Window* parent, aura::Window* child) override;
  void RemoveTransientChild(aura::Window* parent, aura::Window* child) override;
  aura::Window* GetTransientParent(aura::Window* window) override;
  const aura::Window* GetTransientParent(const aura::Window* window) override;
  std::vector<raw_ptr<aura::Window, VectorExperimental>> GetTransientChildren(
      const aura::Window* parent) override;
  void AddObserver(
      aura::client::TransientWindowClientObserver* observer) override;
  void RemoveObserver(
      aura::client::TransientWindowClientObserver* observer) override;

 private:
  friend class TransientWindowManager;

  static TransientWindowController* instance_;

  base::ObserverList<aura::client::TransientWindowClientObserver>::Unchecked
      observers_;
};

}  // namespace wm

#endif  // UI_WM_CORE_TRANSIENT_WINDOW_CONTROLLER_H_
