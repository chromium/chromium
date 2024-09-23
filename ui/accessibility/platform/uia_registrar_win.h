// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_UIA_REGISTRAR_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_UIA_REGISTRAR_WIN_H_

#include <objbase.h>

#include "base/component_export.h"

#include <uiautomation.h>

namespace ui {

// UIA custom events.

// {3761326A-34B2-465A-835D-7A3D8F4EFB92}
static const GUID kUiaEventTestCompleteSentinelGuid = {
    0x3761326a,
    0x34b2,
    0x465a,
    {0x83, 0x5d, 0x7a, 0x3d, 0x8f, 0x4e, 0xfb, 0x92}};

// UIA custom properties.

// {cc7eeb32-4b62-4f4c-aff6-1c2e5752ad8e}
static const GUID kUiaPropertyUniqueIdGuid = {
    0xcc7eeb32,
    0x4b62,
    0x4f4c,
    {0xaf, 0xf6, 0x1c, 0x2e, 0x57, 0x52, 0xad, 0x8e}};

// {28A68D78-3EA6-4FE4-B7C6-1E0F089A72A5}
static const GUID kUiaPropertyVirtualContentGuid = {
    0x28A68D78,
    0x3EA6,
    0x4FE4,
    {0xB7, 0xC6, 0x1E, 0x0F, 0x08, 0x9A, 0x72, 0xA5}};

class COMPONENT_EXPORT(AX_PLATFORM) UiaRegistrarWin {
 public:
  UiaRegistrarWin();
  ~UiaRegistrarWin();

  // UIA custom events.
  EVENTID GetTestCompleteEventId() const;

  // UIA custom properties.
  PROPERTYID GetUniqueIdPropertyId() const;
  PROPERTYID GetVirtualContentPropertyId() const;

  static const UiaRegistrarWin& GetInstance();

 private:
  // UIA custom events.
  EVENTID test_complete_event_id_ = 0;

  // UIA custom properties.
  PROPERTYID unique_id_property_id_ = 0;
  PROPERTYID virtual_content_property_id_ = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_UIA_REGISTRAR_WIN_H_
