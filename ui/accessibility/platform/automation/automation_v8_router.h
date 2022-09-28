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

  // Parses a string representing an event type into an Event tuple.
  virtual std::tuple<ax::mojom::Event, AXEventGenerator::Event> ParseEventType(
      const std::string& event_type) const = 0;

  // Parses a string representing the tree change observer filter.
  virtual TreeChangeObserverFilter ParseTreeChangeObserverFilter(
      const std::string& filter) const = 0;

  // Converts an ax::mojom::MarkerType into a string.
  virtual std::string GetMarkerTypeString(ax::mojom::MarkerType type) const = 0;

  using HandlerFunction =
      base::RepeatingCallback<void(const v8::FunctionCallbackInfo<v8::Value>&)>;
  virtual void RouteHandlerFunction(const std::string& name,
                                    HandlerFunction handler_function) = 0;
  virtual void RouteHandlerFunction(const std::string& name,
                                    const std::string& api_name,
                                    HandlerFunction handler_function) = 0;

  // Dispatches an event with the given name and arguments.
  virtual void DispatchEvent(const std::string& event_name,
                             const base::Value::List& event_args) const = 0;
};
}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_ROUTER_H_
