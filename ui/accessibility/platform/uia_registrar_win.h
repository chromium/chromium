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

// {FA170AB3-3229-4E7C-827F-DD05EE0481D9}
// This GUID matches Microsoft Word's MathML property for compatibility.
// https://learn.microsoft.com/en-us/office/uia/word/wordcustomproperties
static const GUID kUiaPropertyMathMlGuid = {
    0xFA170AB3,
    0x3229,
    0x4E7C,
    {0x82, 0x7F, 0xDD, 0x05, 0xEE, 0x04, 0x81, 0xD9}};

// {8C787AC3-0405-4C94-AC09-7A56A173F7EF}
static const GUID kUiaPropertyAriaActionsGuid = {
    0x8c787ac3,
    0x0405,
    0x4c94,
    {0xac, 0x09, 0x7a, 0x56, 0xa1, 0x73, 0xf7, 0xef}};

class COMPONENT_EXPORT(AX_PLATFORM) UiaRegistrarWin {
 public:
  UiaRegistrarWin();
  ~UiaRegistrarWin();

  // UIA custom events.
  EVENTID GetTestCompleteEventId() const;

  // UIA custom properties.
  PROPERTYID GetUniqueIdPropertyId() const;

  PROPERTYID GetMathMLPropertyId() const;
  PROPERTYID GetAriaActionsPropertyId() const;

  static const UiaRegistrarWin& GetInstance();

 private:
  // UIA custom events.
  EVENTID test_complete_event_id_ = 0;

  // UIA custom properties.
  PROPERTYID unique_id_property_id_ = 0;

  PROPERTYID mathml_property_id_ = 0;
  PROPERTYID aria_actions_property_id_ = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_UIA_REGISTRAR_WIN_H_
