// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_CAPTURE_CONTROLLER_H_
#define UI_WM_CORE_CAPTURE_CONTROLLER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window_observer.h"

namespace aura {
namespace client {
class CaptureDelegate;
}
}

namespace wm {

// Internal CaptureClient implementation. See ScopedCaptureClient for details.
class COMPONENT_EXPORT(UI_WM) CaptureController
    : public aura::client::CaptureClient {
 public:
  CaptureController();

  CaptureController(const CaptureController&) = delete;
  CaptureController& operator=(const CaptureController&) = delete;

  ~CaptureController() override;

  static CaptureController* Get() { return instance_; }

  // Adds |root| to the list of root windows notified when capture changes.
  void Attach(aura::Window* root);

  // Removes |root| from the list of root windows notified when capture changes.
  void Detach(aura::Window* root);

  // Resets the current capture window and prevents it from being set again.
  void PrepareForShutdown();

  // Returns true if this CaptureController is installed on at least one
  // root window.
  bool is_active() const { return !delegates_.empty(); }

  // Overridden from aura::client::CaptureClient:
  void SetCapture(aura::Window* window) override;
  void ReleaseCapture(aura::Window* window) override;
  aura::Window* GetCaptureWindow() override;
  aura::Window* GetGlobalCaptureWindow() override;
  void AddObserver(aura::client::CaptureClientObserver* observer) override;
  void RemoveObserver(aura::client::CaptureClientObserver* observer) override;

 private:
  friend class ScopedCaptureClient;

  static CaptureController* instance_;

  // Set to true by `PrepareForShutdown()`. If this is true, `SetCapture()` will
  // have no effect.
  bool destroying_ = false;

  // The current capture window. NULL if there is no capture window.
  raw_ptr<aura::Window> capture_window_;

  // The capture delegate for the root window with native capture. The root
  // window with native capture may not contain |capture_window_|. This occurs
  // if |capture_window_| is reparented to a different root window while it has
  // capture.
  raw_ptr<aura::client::CaptureDelegate> capture_delegate_;

  // The delegates notified when capture changes.
  std::map<aura::Window*,
           raw_ptr<aura::client::CaptureDelegate, CtnExperimental>>
      delegates_;

  base::ObserverList<aura::client::CaptureClientObserver>::Unchecked observers_;
};

// ScopedCaptureClient is responsible for creating a CaptureClient for a
// RootWindow. Specifically it creates a single CaptureController that is shared
// among all ScopedCaptureClients and adds the RootWindow to it.
class COMPONENT_EXPORT(UI_WM) ScopedCaptureClient
    : public aura::WindowObserver {
 public:
  class COMPONENT_EXPORT(UI_WM) TestApi {
   public:
    explicit TestApi(ScopedCaptureClient* client) : client_(client) {}

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    ~TestApi() {}

    // Sets the delegate.
    void SetDelegate(aura::client::CaptureDelegate* delegate);

   private:
    // Not owned.
    raw_ptr<ScopedCaptureClient> client_;
  };

  explicit ScopedCaptureClient(aura::Window* root);

  ScopedCaptureClient(const ScopedCaptureClient&) = delete;
  ScopedCaptureClient& operator=(const ScopedCaptureClient&) = delete;

  ~ScopedCaptureClient() override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  // Invoked from destructor and OnWindowDestroyed() to cleanup.
  void Shutdown();

  // RootWindow this ScopedCaptureClient was create for.
  raw_ptr<aura::Window> root_window_;
};

}  // namespace wm

#endif  // UI_WM_CORE_CAPTURE_CONTROLLER_H_
