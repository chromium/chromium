// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder_win_uia.h"

#include <numeric>
#include <utility>

#include <psapi.h>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_win.h"
#include "ui/accessibility/platform/uia_registrar_win.h"
#include "ui/base/win/atl_module.h"

namespace ui {

namespace {

#if defined(COMPILER_MSVC)
#define RETURN_ADDRESS() _ReturnAddress()
#elif defined(COMPILER_GCC) && !BUILDFLAG(IS_NACL)
#define RETURN_ADDRESS() \
  __builtin_extract_return_addr(__builtin_return_address(0))
#else
#define RETURN_ADDRESS() nullptr
#endif

static std::pair<uintptr_t, uintptr_t> GetModuleAddressRange(
    const wchar_t* module_name) {
  MODULEINFO info;
  CHECK(GetModuleInformation(GetCurrentProcess(), GetModuleHandle(module_name),
                             &info, sizeof(info)));

  const uintptr_t start = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
  return std::make_pair(start, start + info.SizeOfImage);
}

std::string UiaIdentifierToStringPretty(int32_t id) {
  auto str = base::WideToUTF8(UiaIdentifierToString(id));
  // Remove UIA_ prefix, and EventId/PropertyId suffixes
  if (base::StartsWith(str, "UIA_", base::CompareCase::SENSITIVE))
    str = str.substr(std::size("UIA_") - 1);
  if (base::EndsWith(str, "EventId", base::CompareCase::SENSITIVE))
    str = str.substr(0, str.size() - std::size("EventId") + 1);
  if (base::EndsWith(str, "PropertyId", base::CompareCase::SENSITIVE))
    str = str.substr(0, str.size() - std::size("PropertyId") + 1);
  return str;
}

}  // namespace

// static
volatile base::subtle::Atomic32 AXEventRecorderWinUia::instantiated_ = 0;

AXEventRecorderWinUia::AXEventRecorderWinUia(const AXTreeSelector& selector) {
  CHECK(!base::subtle::NoBarrier_AtomicExchange(&instantiated_, 1))
      << "There can be only one instance at a time.";

  // Find the root content window
  HWND hwnd = GetHWNDBySelector(selector);
  CHECK(hwnd);

  // Create the event thread, and pump messages via |initialization_loop| until
  // initialization is complete.
  base::RunLoop initialization_loop;
  base::PlatformThread::Create(0, &thread_, &thread_handle_);
  thread_.Init(this, hwnd, initialization_loop, shutdown_loop_);
  initialization_loop.Run();
}

AXEventRecorderWinUia::~AXEventRecorderWinUia() {
  base::subtle::NoBarrier_AtomicExchange(&instantiated_, 0);
}

void AXEventRecorderWinUia::WaitForDoneRecording() {
  // Pump messages via |shutdown_loop_| until the thread is complete.
  shutdown_loop_.Run();
  base::PlatformThread::Join(thread_handle_);
}

AXEventRecorderWinUia::Thread::Thread() {}

AXEventRecorderWinUia::Thread::~Thread() {}

void AXEventRecorderWinUia::Thread::Init(AXEventRecorderWinUia* owner,
                                         HWND hwnd,
                                         base::RunLoop& initialization,
                                         base::RunLoop& shutdown) {
  owner_ = owner;
  hwnd_ = hwnd;
  initialization_complete_ = initialization.QuitClosure();
  shutdown_complete_ = shutdown.QuitClosure();
}

void AXEventRecorderWinUia::Thread::ThreadMain() {
  // UIA calls must be made on an MTA thread to prevent random timeouts.
  base::win::ScopedCOMInitializer com_init{
      base::win::ScopedCOMInitializer::kMTA};

  // Create an instance of the CUIAutomation class.
  CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                   IID_IUIAutomation, &uia_);
  CHECK(uia_.Get());

  // Register the custom event to mark the end of the test.
  shutdown_sentinel_ = UiaRegistrarWin::GetInstance().GetTestCompleteEventId();

  // Find the IUIAutomationElement for the root content window
  uia_->ElementFromHandle(hwnd_, &root_);
  CHECK(root_.Get());

  // Create the event handler
  win::CreateATLModuleIfNeeded();
  CHECK(
      SUCCEEDED(CComObject<EventHandler>::CreateInstance(&uia_event_handler_)));
  uia_event_handler_->AddRef();
  uia_event_handler_->Init(this, root_);

  // Create a cache request to avoid cross-thread issues when logging.
  CHECK(SUCCEEDED(uia_->CreateCacheRequest(&cache_request_)));
  CHECK(cache_request_.Get());
  CHECK(SUCCEEDED(cache_request_->AddProperty(UIA_NamePropertyId)));
  CHECK(SUCCEEDED(cache_request_->AddProperty(UIA_AriaRolePropertyId)));

  // Subscribe to the shutdown sentinel event
  uia_->AddAutomationEventHandler(
      shutdown_sentinel_, root_.Get(), TreeScope::TreeScope_Subtree,
      cache_request_.Get(), uia_event_handler_.Get());

  // Subscribe to focus events
  uia_->AddFocusChangedEventHandler(cache_request_.Get(),
                                    uia_event_handler_.Get());

  // Subscribe to all property-change events
  static const PROPERTYID kMinProp = UIA_RuntimeIdPropertyId;
  static const PROPERTYID kMaxProp = UIA_HeadingLevelPropertyId;
  std::array<PROPERTYID, (kMaxProp - kMinProp) + 1> property_list;
  std::iota(property_list.begin(), property_list.end(), kMinProp);
  uia_->AddPropertyChangedEventHandlerNativeArray(
      root_.Get(), TreeScope::TreeScope_Subtree, cache_request_.Get(),
      uia_event_handler_.Get(), &property_list[0], property_list.size());

  // Subscribe to all structure-change events
  uia_->AddStructureChangedEventHandler(root_.Get(), TreeScope_Subtree,
                                        cache_request_.Get(),
                                        uia_event_handler_.Get());

  // Subscribe to all automation events (except structure-change events and
  // live-region events, which are handled elsewhere).
  static const EVENTID kMinEvent = UIA_ToolTipOpenedEventId;
  static const EVENTID kMaxEvent = UIA_ActiveTextPositionChangedEventId;
  for (EVENTID event_id = kMinEvent; event_id <= kMaxEvent; ++event_id) {
    if (event_id != UIA_StructureChangedEventId &&
        event_id != UIA_LiveRegionChangedEventId) {
      uia_->AddAutomationEventHandler(
          event_id, root_.Get(), TreeScope::TreeScope_Subtree,
          cache_request_.Get(), uia_event_handler_.Get());
    }
  }

  // Subscribe to live-region change events.  This must be the last event we
  // subscribe to, because |AXFragmentRootWin| will fire events when advised of
  // the subscription, and this can hang the test-process (on Windows 19H1+) if
  // we're simultaneously trying to subscribe to other events.
  uia_->AddAutomationEventHandler(
      UIA_LiveRegionChangedEventId, root_.Get(), TreeScope::TreeScope_Subtree,
      cache_request_.Get(), uia_event_handler_.Get());

  // Signal that initialization is complete; this will wake the main thread to
  // start executing the test.
  std::move(initialization_complete_).Run();

  // Wait for shutdown signal
  shutdown_signal_.Wait();

  // Cleanup
  uia_->RemoveAllEventHandlers();
  uia_event_handler_->CleanUp();
  uia_event_handler_.Reset();
  cache_request_.Reset();
  root_.Reset();
  uia_.Reset();

  // Signal thread shutdown complete; this will wake the main thread to compile
  // test results and compare against the expected results.
  std::move(shutdown_complete_).Run();
}

void AXEventRecorderWinUia::Thread::SendShutdownSignal() {
  shutdown_signal_.Signal();
}

void AXEventRecorderWinUia::Thread::OnEvent(const std::string& event) {
  // Pass the event to the thread-safe owner_.
  owner_->OnEvent(event);
}

AXEventRecorderWinUia::Thread::EventHandler::EventHandler() {
  // Some events are duplicated between UIAutomationCore.dll and RPCRT4.dll.
  // Before WIN10_19H1, events are mainly sent from RPCRT4.dll, with a few
  // duplicates sent from UIAutomationCore.dll.
  // After WIN10_19H1, events are mainly sent from UIAutomationCore.dll, with a
  // few duplicates sent from RPCRT4.dll.
  allowed_module_address_range_ = GetModuleAddressRange(
      (base::win::GetVersion() >= base::win::Version::WIN10_19H1)
          ? L"UIAutomationCore.dll"
          : L"RPCRT4.dll");
}

AXEventRecorderWinUia::Thread::EventHandler::~EventHandler() {}

void AXEventRecorderWinUia::Thread::EventHandler::Init(
    AXEventRecorderWinUia::Thread* owner,
    Microsoft::WRL::ComPtr<IUIAutomationElement> root) {
  owner_ = owner;
  root_ = root;
}

void AXEventRecorderWinUia::Thread::EventHandler::CleanUp() {
  owner_ = nullptr;
  root_.Reset();
}

IFACEMETHODIMP
AXEventRecorderWinUia::Thread::EventHandler::HandleFocusChangedEvent(
    IUIAutomationElement* sender) {
  if (!owner_ || !IsCallerFromAllowedModule(RETURN_ADDRESS()))
    return S_OK;

  base::win::ScopedSafearray id;
  sender->GetRuntimeId(id.Receive());
  base::win::ScopedVariant id_variant(id.Release());

  Microsoft::WRL::ComPtr<IUIAutomationElement> element_found;
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;

  owner_->uia_->CreatePropertyCondition(UIA_RuntimeIdPropertyId, id_variant,
                                        &condition);
  CHECK(condition);
  root_->FindFirst(TreeScope::TreeScope_Subtree, condition.Get(),
                   &element_found);
  if (!element_found) {
    VLOG(1) << "Ignoring UIA focus event outside our frame";
    return S_OK;
  }

  // Transfer ownership of the RuntimeId SAFEARRAY back into |id| then debounce
  // focus events that are consecutively received for the same |sender|. This
  // needs to happen after determining the |sender| is within the |root_| frame,
  // otherwise a RuntimeId outside the frame may be cached. For example, when
  // receiving a global focus event not related to the |root_| frame.
  {
    VARIANT tmp = id_variant.Release();
    id.Reset(V_ARRAY(&tmp));
  }
  if (auto lock_scope = id.CreateLockScope<VT_I4>()) {
    // Debounce focus events received from the same |sender|.
    if (base::ranges::equal(*lock_scope, last_focused_runtime_id_)) {
      return S_OK;
    }

    last_focused_runtime_id_ = {lock_scope->begin(), lock_scope->end()};
  }

  std::string log = base::StringPrintf("AutomationFocusChanged %s",
                                       GetSenderInfo(sender).c_str());
  owner_->OnEvent(log);

  return S_OK;
}

IFACEMETHODIMP
AXEventRecorderWinUia::Thread::EventHandler::HandlePropertyChangedEvent(
    IUIAutomationElement* sender,
    PROPERTYID property_id,
    VARIANT new_value) {
  if (!owner_ || !IsCallerFromAllowedModule(RETURN_ADDRESS()))
    return S_OK;

  std::string prop_str = UiaIdentifierToStringPretty(property_id);
  if (prop_str.empty()) {
    VLOG(1) << "Ignoring UIA property-changed event " << property_id;
    return S_OK;
  }

  std::string log = base::StringPrintf("%s changed %s", prop_str.c_str(),
                                       GetSenderInfo(sender).c_str());
  owner_->OnEvent(log);
  return S_OK;
}

IFACEMETHODIMP
AXEventRecorderWinUia::Thread::EventHandler::HandleStructureChangedEvent(
    IUIAutomationElement* sender,
    StructureChangeType change_type,
    SAFEARRAY* runtime_id) {
  if (!owner_ || !IsCallerFromAllowedModule(RETURN_ADDRESS()))
    return S_OK;

  std::string type_str;
  switch (change_type) {
    case StructureChangeType_ChildAdded:
      type_str = "ChildAdded";
      break;
    case StructureChangeType_ChildRemoved:
      type_str = "ChildRemoved";
      break;
    case StructureChangeType_ChildrenInvalidated:
      type_str = "ChildrenInvalidated";
      break;
    case StructureChangeType_ChildrenBulkAdded:
      type_str = "ChildrenBulkAdded";
      break;
    case StructureChangeType_ChildrenBulkRemoved:
      type_str = "ChildrenBulkRemoved";
      break;
    case StructureChangeType_ChildrenReordered:
      type_str = "ChildrenReordered";
      break;
  }

  std::string log =
      base::StringPrintf("StructureChanged/%s %s", type_str.c_str(),
                         GetSenderInfo(sender).c_str());
  owner_->OnEvent(log);
  return S_OK;
}

IFACEMETHODIMP
AXEventRecorderWinUia::Thread::EventHandler::HandleAutomationEvent(
    IUIAutomationElement* sender,
    EVENTID event_id) {
  if (!owner_ || !IsCallerFromAllowedModule(RETURN_ADDRESS()))
    return S_OK;

  if (event_id == owner_->shutdown_sentinel_) {
    // This is a sentinel value that tells us the tests are finished.
    owner_->SendShutdownSignal();
  } else {
    std::string event_str = UiaIdentifierToStringPretty(event_id);
    if (event_str.empty()) {
      VLOG(1) << "Ignoring UIA automation event " << event_id;
      return S_OK;
    }

    // Remove duplicate menuclosed events with no event data.
    // The "duplicates" are benign. UIA currently duplicates *all* events for
    // in-process listeners, and the event-recorder tries to eliminate the
    // duplicates... but since the recorder sometimes isn't able to retrieve
    // the role, the duplicate-elimination logic doesn't see them as
    // duplicates in this case.
    std::string sender_info =
        event_id == UIA_MenuClosedEventId ? "" : GetSenderInfo(sender);
    std::string log =
        base::StringPrintf("%s %s", event_str.c_str(), sender_info.c_str());
    owner_->OnEvent(log);
  }
  return S_OK;
}

// Due to a bug in Windows (fixed in Windows 10 19H1, but found broken in 20H2),
// events are raised exactly twice for any in-proc off-thread event listeners.
// To avoid this, in UIA API methods we can pass the RETURN_ADDRESS() to this
// method to determine whether the caller belongs to a specific platform module.
bool AXEventRecorderWinUia::Thread::EventHandler::IsCallerFromAllowedModule(
    void* return_address) {
  const auto address = reinterpret_cast<uintptr_t>(return_address);
  return address >= allowed_module_address_range_.first &&
         address < allowed_module_address_range_.second;
}

std::string AXEventRecorderWinUia::Thread::EventHandler::GetSenderInfo(
    IUIAutomationElement* sender) {
  std::string sender_info;

  auto append_property = [&](const char* name, auto getter) {
    base::win::ScopedBstr bstr;
    (sender->*getter)(bstr.Receive());
    if (bstr.Length() > 0) {
      sender_info +=
          base::StringPrintf("%s%s=%s", sender_info.empty() ? "" : ", ", name,
                             BstrToUTF8(bstr.Get()).c_str());
    }
  };

  append_property("role", &IUIAutomationElement::get_CachedAriaRole);
  append_property("name", &IUIAutomationElement::get_CachedName);

  if (!sender_info.empty())
    sender_info = "on " + sender_info;
  return sender_info;
}

}  // namespace ui
