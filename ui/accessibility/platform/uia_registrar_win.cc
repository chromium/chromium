// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/uia_registrar_win.h"

#include <wrl/implements.h>

#include "base/no_destructor.h"
#include "ui/accessibility/accessibility_features.h"

namespace ui {

UiaRegistrarWin::UiaRegistrarWin() {
  // Create the registrar object and get the IUIAutomationRegistrar
  // interface pointer.
  Microsoft::WRL::ComPtr<IUIAutomationRegistrar> registrar;
  if (FAILED(CoCreateInstance(CLSID_CUIAutomationRegistrar, nullptr,
                              CLSCTX_INPROC_SERVER, IID_IUIAutomationRegistrar,
                              &registrar)))
    return;

  // Register the custom UIA event that represents the test end event for the
  // UIA test suite.
  UIAutomationEventInfo test_complete_event_info = {
      kUiaEventTestCompleteSentinelGuid, L"kUiaTestCompleteSentinel"};
  registrar->RegisterEvent(&test_complete_event_info, &test_complete_event_id_);

  // Register the custom UIA property that represents the unique id of an UIA
  // element which also matches its corresponding IA2 element's unique id.
  UIAutomationPropertyInfo unique_id_property_info = {
      kUiaPropertyUniqueIdGuid, L"UniqueId", UIAutomationType_String};
  registrar->RegisterProperty(&unique_id_property_info,
                              &unique_id_property_id_);

  if (features::IsAccessibilityAriaVirtualContentEnabled()) {
    // Register the custom UIA property that represents the value for the
    // 'aria-virtualcontent' attribute.
    UIAutomationPropertyInfo virtual_content_property_info = {
        kUiaPropertyVirtualContentGuid, L"VirtualContent",
        UIAutomationType_String};
    registrar->RegisterProperty(&virtual_content_property_info,
                                &virtual_content_property_id_);
  }
}

UiaRegistrarWin::~UiaRegistrarWin() = default;

// UIA custom events.
EVENTID UiaRegistrarWin::GetTestCompleteEventId() const {
  return test_complete_event_id_;
}

// UIA custom properties.
PROPERTYID UiaRegistrarWin::GetUniqueIdPropertyId() const {
  return unique_id_property_id_;
}

PROPERTYID UiaRegistrarWin::GetVirtualContentPropertyId() const {
  if (!features::IsAccessibilityAriaVirtualContentEnabled())
    return 0;
  return virtual_content_property_id_;
}

const UiaRegistrarWin& UiaRegistrarWin::GetInstance() {
  static base::NoDestructor<UiaRegistrarWin> instance;
  return *instance;
}

}  // namespace ui
