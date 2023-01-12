// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_
#define UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_

#include <msctf.h>
#include <wrl/client.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/range/range.h"

namespace ui {

class TSFEventRouterObserver {
 public:
  TSFEventRouterObserver() {}

  TSFEventRouterObserver(const TSFEventRouterObserver&) = delete;
  TSFEventRouterObserver& operator=(const TSFEventRouterObserver&) = delete;

  // Called when the number of currently opened candidate windows changes.
  virtual void OnCandidateWindowCountChanged(size_t window_count) {}

  // Called when a composition is started.
  virtual void OnTSFStartComposition() {}

  // Called when the text contents are updated. If there is no composition,
  // gfx::Range::InvalidRange is passed to |composition_range|.
  virtual void OnTextUpdated(const gfx::Range& composition_range) {}

  // Called when a composition is terminated.
  virtual void OnTSFEndComposition() {}

 protected:
  virtual ~TSFEventRouterObserver() {}
};

// This class monitors TSF related events and forwards them to given
// |observer|.
class COMPONENT_EXPORT(UI_BASE_IME_WIN) TSFEventRouter {
 public:
  // Do not pass NULL to |observer|.
  explicit TSFEventRouter(TSFEventRouterObserver* observer);

  TSFEventRouter(const TSFEventRouter&) = delete;
  TSFEventRouter& operator=(const TSFEventRouter&) = delete;

  virtual ~TSFEventRouter();

  // Returns true if the IME is composing text.
  bool IsImeComposing();

  // Callbacks from the TSFEventRouterDelegate:
  void OnCandidateWindowCountChanged(size_t window_count);
  void OnTSFStartComposition();
  void OnTextUpdated(const gfx::Range& composition_range);
  void OnTSFEndComposition();

  // Sets |thread_manager| to be monitored. |thread_manager| can be NULL.
  void SetManager(ITfThreadMgr* thread_manager);

 private:
  class Delegate;

  Microsoft::WRL::ComPtr<Delegate> delegate_;

  raw_ptr<TSFEventRouterObserver> observer_;
};

}  // namespace ui

#endif  // UI_BASE_IME_WIN_TSF_EVENT_ROUTER_H_
