// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_BINDINGS_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_BINDINGS_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/platform/automation/automation_ax_tree_wrapper.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-value.h"

namespace ui {

class AutomationV8Router;
class AutomationTreeManagerOwner;

// Class that creates V8 bindings for Automation. This class should contain
// logic about converting to/from V8 values but should not contain logic about
// accessibility.
// TODO(crbug.com/1357889): Refactor logic in V8 routes into
// AutomationTreeManagerOwner so that the above statement is true.
class AX_EXPORT AutomationV8Bindings {
 public:
  AutomationV8Bindings(AutomationTreeManagerOwner* owner,
                       AutomationV8Router* router);
  AutomationV8Bindings(const AutomationV8Bindings&) = delete;
  AutomationV8Bindings& operator=(const AutomationV8Bindings&) = delete;
  virtual ~AutomationV8Bindings();

  void AddV8Routes();

  // TODO(crbug.com/1357889): Move remaining RouteNodeIDFunction usages from
  // AutomationInternalCustomBindings and make this private.
  void RouteNodeIDFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   ui::AutomationAXTreeWrapper* tree_wrapper,
                                   ui::AXNode* node)> callback);

 private:
  void RouteTreeIDFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AutomationAXTreeWrapper* tree_wrapper));

  void RouteNodeIDPlusAttributeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       ui::AXTree* tree,
                       ui::AXNode* node,
                       const std::string& attribute_name));
  void RouteNodeIDPlusRangeFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   ui::AutomationAXTreeWrapper* tree_wrapper,
                                   ui::AXNode* node,
                                   int start,
                                   int end,
                                   bool clipped)> callback);
  void RouteNodeIDPlusStringBoolFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   ui::AutomationAXTreeWrapper* tree_wrapper,
                                   ui::AXNode* node,
                                   const std::string& strVal,
                                   bool boolVal)> callback);
  void RouteNodeIDPlusDimensionsFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   ui::AutomationAXTreeWrapper* tree_wrapper,
                                   ui::AXNode* node,
                                   int start,
                                   int end,
                                   int width,
                                   int height)> callback);
  void RouteNodeIDPlusEventFunction(
      const std::string& name,
      base::RepeatingCallback<
          void(v8::Isolate* isolate,
               v8::ReturnValue<v8::Value> result,
               ui::AutomationAXTreeWrapper* tree_wrapper,
               ui::AXNode* node,
               const std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>&
                   event_type)> callback);

  // Helper functions which reference this and need to be bound.
  void GetParentID(v8::Isolate* isolate,
                   v8::ReturnValue<v8::Value> result,
                   AutomationAXTreeWrapper* tree_wrapper,
                   AXNode* node);
  void GetChildCount(v8::Isolate* isolate,
                     v8::ReturnValue<v8::Value> result,
                     AutomationAXTreeWrapper* tree_wrapper,
                     AXNode* node);
  void GetLocation(v8::Isolate* isolate,
                   v8::ReturnValue<v8::Value> result,
                   AutomationAXTreeWrapper* tree_wrapper,
                   AXNode* node);
  void GetUnclippedLocation(v8::Isolate* isolate,
                            v8::ReturnValue<v8::Value> result,
                            AutomationAXTreeWrapper* tree_wrapper,
                            AXNode* node);
  void GetChildIDs(v8::Isolate* isolate,
                   v8::ReturnValue<v8::Value> result,
                   AutomationAXTreeWrapper* tree_wrapper,
                   AXNode* node);
  void GetSentenceStartOffsets(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               AutomationAXTreeWrapper* tree_wrapper,
                               AXNode* node);
  void GetSentenceEndOffsets(v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node);
  void GetBoundsForRange(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         AutomationAXTreeWrapper* tree_wrapper,
                         AXNode* node,
                         int start,
                         int end,
                         bool clipped);
  void ComputeGlobalBounds(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           AutomationAXTreeWrapper* tree_wrapper,
                           AXNode* node,
                           int x,
                           int y,
                           int width,
                           int height);
  void GetName(v8::Isolate* isolate,
               v8::ReturnValue<v8::Value> result,
               AutomationAXTreeWrapper* tree_wrapper,
               AXNode* node);
  void GetNextTextMatch(v8::Isolate* isolate,
                        v8::ReturnValue<v8::Value> result,
                        AutomationAXTreeWrapper* tree_wrapper,
                        AXNode* node,
                        const std::string& search_str,
                        bool backward);
  void SetAccessibilityFocus(v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node);
  void EventListenerAdded(
      v8::Isolate* isolate,
      v8::ReturnValue<v8::Value> result,
      AutomationAXTreeWrapper* tree_wrapper,
      AXNode* node,
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type);
  void EventListenerRemoved(
      v8::Isolate* isolate,
      v8::ReturnValue<v8::Value> result,
      AutomationAXTreeWrapper* tree_wrapper,
      AXNode* node,
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type);

  AutomationTreeManagerOwner* automation_tree_manager_owner_;
  AutomationV8Router* automation_v8_router_;
};
}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_BINDINGS_H_
