// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_WIN_UIA_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_WIN_UIA_H_

#include <ole2.h>

#include <stdint.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <atomic>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

#include <uiautomation.h>

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXEventRecorderWinUia
    : public AXEventRecorder {
 public:
  AXEventRecorderWinUia(const AXTreeSelector& selector);

  AXEventRecorderWinUia(const AXEventRecorderWinUia&) = delete;
  AXEventRecorderWinUia& operator=(const AXEventRecorderWinUia&) = delete;

  ~AXEventRecorderWinUia() override;

  // Called to ensure the event recorder has finished recording async events.
  void WaitForDoneRecording() override;

 private:
  // Used to prevent creation of multiple instances simultaneously
  static std::atomic<bool> instantiated_;

  // All UIA calls need to be made on a secondary MTA thread to avoid sporadic
  // test hangs / timeouts.
  class Thread : public base::PlatformThread::Delegate {
   public:
    Thread();
    ~Thread() override;

    void Init(AXEventRecorderWinUia* owner,
              HWND hwnd,
              base::RunLoop& initialization_loop,
              base::RunLoop& shutdown_loop);

    void SendShutdownSignal();

    void ThreadMain() override;

   private:
    raw_ptr<AXEventRecorderWinUia> owner_ = nullptr;
    HWND hwnd_ = NULL;
    EVENTID shutdown_sentinel_ = 0;

    Microsoft::WRL::ComPtr<IUIAutomation4> uia_;
    Microsoft::WRL::ComPtr<IUIAutomationElement> root_;
    Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request_;

    // Thread synchronization members
    base::OnceClosure initialization_complete_;
    base::OnceClosure shutdown_complete_;
    base::WaitableEvent shutdown_signal_;

    void OnEvent(const std::string& event);

    // An implementation of various UIA interfaces that forward event
    // notifications to the owning event recorder.
    class EventHandler final
        : public Microsoft::WRL::RuntimeClass<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
              IUIAutomationFocusChangedEventHandler,
              IUIAutomationPropertyChangedEventHandler,
              IUIAutomationStructureChangedEventHandler,
              IUIAutomationEventHandler,
              IUIAutomationChangesEventHandler> {
     public:
      EventHandler(AXEventRecorderWinUia::Thread* owner,
                   Microsoft::WRL::ComPtr<IUIAutomationElement> root);

      EventHandler(const EventHandler&) = delete;
      EventHandler& operator=(const EventHandler&) = delete;

      ~EventHandler() override;

      void CleanUp();

      // IUIAutomationFocusChangedEventHandler interface.
      IFACEMETHODIMP HandleFocusChangedEvent(
          IUIAutomationElement* sender) override;

      // IUIAutomationPropertyChangedEventHandler interface.
      IFACEMETHODIMP HandlePropertyChangedEvent(IUIAutomationElement* sender,
                                                PROPERTYID property_id,
                                                VARIANT new_value) override;

      // IUIAutomationStructureChangedEventHandler interface.
      IFACEMETHODIMP HandleStructureChangedEvent(
          IUIAutomationElement* sender,
          StructureChangeType change_type,
          SAFEARRAY* runtime_id) override;

      // IUIAutomationEventHandler interface.
      IFACEMETHODIMP HandleAutomationEvent(IUIAutomationElement* sender,
                                           EVENTID event_id) override;

      // IUIAutomationChangesEventHandler interface.
      IFACEMETHODIMP HandleChangesEvent(IUIAutomationElement* sender,
                                        UiaChangeInfo* uia_changes,
                                        int changes_count) override;

      // Points to the event recorder to receive notifications.
      raw_ptr<AXEventRecorderWinUia::Thread> owner_ = nullptr;

     private:
      std::pair<uintptr_t, uintptr_t> allowed_module_address_range_;
      bool IsCallerFromAllowedModule(void* return_address);

      std::string GetSenderInfo(IUIAutomationElement* sender);

      Microsoft::WRL::ComPtr<IUIAutomationElement> root_;

      std::vector<int32_t> last_focused_runtime_id_;
    };
    Microsoft::WRL::ComPtr<EventHandler> uia_event_handler_;
  };

  Thread thread_;
  base::RunLoop shutdown_loop_;
  base::PlatformThreadHandle thread_handle_;
  bool thread_joined_ = false;
  bool wait_for_done_recording_called_ = false;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_WIN_UIA_H_
