// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_ROUTER_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_ROUTER_H_

#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-value.h"

namespace ui {

// Pure virtual class that allows Automation to route information in and
// out of V8. This should be implemented by each V8 version of Automation,
// for example in Extensions and in the AccessibilityService.
class AX_EXPORT AutomationV8Router {
 public:
  // Throws an invalid argument exception in V8.
  virtual void ThrowInvalidArgumentsException(bool is_fatal = true) const = 0;

  // Gets the V8 isolate.
  virtual v8::Isolate* GetIsolate() const = 0;

  // Gets the V8 script context.
  virtual v8::Local<v8::Context> GetContext() const = 0;

  // Returns whether this extension has the "interact" permission set (either
  // explicitly or implicitly after manifest parsing).
  // TODO(crbug.com/1357889): This is specific to the extensions system and
  // should be removed from this more generic location.
  virtual bool IsInteractPermitted() const = 0;

  virtual void StartCachingAccessibilityTrees() = 0;

  virtual void StopCachingAccessibilityTrees() = 0;

  //
  // Methods converting to and from strings
  //

  // Parses a string representing the tree change observer filter.
  virtual TreeChangeObserverFilter ParseTreeChangeObserverFilter(
      const std::string& filter) const = 0;

  // Converts an ax::mojom::MarkerType into a string.
  virtual std::string GetMarkerTypeString(ax::mojom::MarkerType type) const = 0;
  virtual std::string GetFocusedStateString() const = 0;
  virtual std::string GetOffscreenStateString() const = 0;
  virtual std::string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const = 0;
  virtual std::string GetTreeChangeTypeString(
      ax::mojom::Mutation change_type) const = 0;
  virtual std::string GetEventTypeString(
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type)
      const = 0;

  //
  // Methods for routing Javascript methods to C++.
  //

  using HandlerFunction =
      base::RepeatingCallback<void(const v8::FunctionCallbackInfo<v8::Value>&)>;
  virtual void RouteHandlerFunction(const std::string& name,
                                    HandlerFunction handler_function) = 0;
  virtual void RouteHandlerFunction(const std::string& name,
                                    const std::string& api_name,
                                    HandlerFunction handler_function) = 0;

  //
  // Methods for passing information from C++ to Javascript.
  //

  // Dispatches an event with the given name and arguments.
  virtual void DispatchEvent(const std::string& event_name,
                             const base::Value::List& event_args) const = 0;
};
}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_ROUTER_H_
