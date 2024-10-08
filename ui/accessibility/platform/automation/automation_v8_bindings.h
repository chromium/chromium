// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_BINDINGS_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_BINDINGS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
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
class COMPONENT_EXPORT(AX_PLATFORM) AutomationV8Bindings {
 public:
  AutomationV8Bindings(AutomationTreeManagerOwner* owner,
                       AutomationV8Router* router);
  AutomationV8Bindings(const AutomationV8Bindings&) = delete;
  AutomationV8Bindings& operator=(const AutomationV8Bindings&) = delete;
  virtual ~AutomationV8Bindings();

  //
  // Methods for sending C++ events back to Javascript.
  //
  void SendTreeChangeEvent(int observer_id,
                           const AXTreeID& tree_id,
                           int node_id,
                           ax::mojom::Mutation change_type);
  void SendNodesRemovedEvent(const AXTreeID& tree_id,
                             const std::vector<int>& ids);
  void SendChildTreeIDEvent(const AXTreeID& child_tree_id);
  void SendTreeDestroyedEvent(const AXTreeID& tree_id);
  void SendActionResultEvent(const AXActionData& data, bool result);
  void SendGetTextLocationResult(const AXActionData& data,
                                 const std::optional<gfx::Rect>& rect);
  void SendAutomationEvent(
      const AXTreeID& tree_id,
      const AXEvent& event,
      const gfx::Point& mouse_location,
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type);
  void SendTreeSerializationError(const AXTreeID& tree_id);
  void SendOnAllEventListenersRemoved();

  void AddV8Routes();

 private:
  void RouteNodeIDFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   AutomationAXTreeWrapper* tree_wrapper,
                                   AXNode* node)> callback);

  void RouteTreeIDFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       AutomationAXTreeWrapper* tree_wrapper));

  void RouteNodeIDPlusAttributeFunction(
      const std::string& name,
      void (*callback)(v8::Isolate* isolate,
                       v8::ReturnValue<v8::Value> result,
                       AXTree* tree,
                       AXNode* node,
                       const std::string& attribute_name));
  void RouteNodeIDPlusRangeFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   AutomationAXTreeWrapper* tree_wrapper,
                                   AXNode* node,
                                   int start,
                                   int end,
                                   bool clipped)> callback);
  void RouteNodeIDPlusStringBoolFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   AutomationAXTreeWrapper* tree_wrapper,
                                   AXNode* node,
                                   const std::string& strVal,
                                   bool boolVal)> callback);
  void RouteNodeIDPlusDimensionsFunction(
      const std::string& name,
      base::RepeatingCallback<void(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   AutomationAXTreeWrapper* tree_wrapper,
                                   AXNode* node,
                                   int start,
                                   int end,
                                   int width,
                                   int height)> callback);
  void RouteNodeIDPlusEventFunction(
      const std::string& name,
      base::RepeatingCallback<
          void(v8::Isolate* isolate,
               v8::ReturnValue<v8::Value> result,
               AutomationAXTreeWrapper* tree_wrapper,
               AXNode* node,
               const std::tuple<ax::mojom::Event, AXEventGenerator::Event>&
                   event_type)> callback);

  // Helper functions which reference this and need to be bound.
  void GetParentID(v8::Isolate* isolate,
                   v8::ReturnValue<v8::Value> result,
                   AutomationAXTreeWrapper* tree_wrapper,
                   AXNode* node) const;
  void GetChildCount(v8::Isolate* isolate,
                     v8::ReturnValue<v8::Value> result,
                     AutomationAXTreeWrapper* tree_wrapper,
                     AXNode* node) const;
  void GetLocation(v8::Isolate* isolate,
                   v8::ReturnValue<v8::Value> result,
                   AutomationAXTreeWrapper* tree_wrapper,
                   AXNode* node) const;
  void GetUnclippedLocation(v8::Isolate* isolate,
                            v8::ReturnValue<v8::Value> result,
                            AutomationAXTreeWrapper* tree_wrapper,
                            AXNode* node) const;
  void GetChildIDs(v8::Isolate* isolate,
                   v8::ReturnValue<v8::Value> result,
                   AutomationAXTreeWrapper* tree_wrapper,
                   AXNode* node) const;
  void GetSentenceStartOffsets(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               AutomationAXTreeWrapper* tree_wrapper,
                               AXNode* node) const;
  void GetSentenceEndOffsets(v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) const;
  void GetBoundsForRange(v8::Isolate* isolate,
                         v8::ReturnValue<v8::Value> result,
                         AutomationAXTreeWrapper* tree_wrapper,
                         AXNode* node,
                         int start,
                         int end,
                         bool clipped) const;
  void ComputeGlobalBounds(v8::Isolate* isolate,
                           v8::ReturnValue<v8::Value> result,
                           AutomationAXTreeWrapper* tree_wrapper,
                           AXNode* node,
                           int x,
                           int y,
                           int width,
                           int height) const;
  void GetName(v8::Isolate* isolate,
               v8::ReturnValue<v8::Value> result,
               AutomationAXTreeWrapper* tree_wrapper,
               AXNode* node) const;
  void GetNextTextMatch(v8::Isolate* isolate,
                        v8::ReturnValue<v8::Value> result,
                        AutomationAXTreeWrapper* tree_wrapper,
                        AXNode* node,
                        const std::string& search_str,
                        bool backward) const;
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
  void GetMarkers(v8::Isolate* isolate,
                  v8::ReturnValue<v8::Value> result,
                  AutomationAXTreeWrapper* tree_wrapper,
                  AXNode* node) const;
  void GetFocus(const v8::FunctionCallbackInfo<v8::Value>& args) const;

  //
  // Access the cached accessibility trees and properties of their nodes.
  //

  // Args: string ax_tree_id, int node_id, Returns: int child_id.
  void GetChildIDAtIndex(const v8::FunctionCallbackInfo<v8::Value>& args) const;

  // Returns: string tree_id and int node_id of a node which has global
  // accessibility focus.
  void GetAccessibilityFocus(
      const v8::FunctionCallbackInfo<v8::Value>& args) const;

  // Args: string ax_tree_id.
  // returns: token.high and token.low used to represent an
  // AXTreeID in is unguessable token format. Note that they are returned in
  // string format and later converted to BigInt in JS. This is necessary
  // because when converting uint64_t to JS number format they lose precision,
  // which fails to build the correct AXTreeID.
  void StringAXTreeIDToUnguessableToken(
      const v8::FunctionCallbackInfo<v8::Value>& args) const;

  // Args: string ax_tree_id.
  void SetDesktopID(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Called when an accessibility tree is destroyed and needs to be
  // removed from our cache.
  // Args: string ax_tree_id
  void DestroyAccessibilityTree(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void AddTreeChangeObserver(const v8::FunctionCallbackInfo<v8::Value>& args);

  void RemoveTreeChangeObserver(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Creates the backing AutomationPosition native object given a request from
  // javascript.
  // Args: string ax_tree_id, int node_id, int offset, bool is_downstream
  // Returns: JS object with bindings back to the native AutomationPosition.
  void CreateAutomationPosition(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Args: string ax_tree_id, int node_id
  // Returns: JS object with a string key for each state flag that's set.
  void GetState(const v8::FunctionCallbackInfo<v8::Value>& args) const;

  void GetImageAnnotation(v8::Isolate* isolate,
                          v8::ReturnValue<v8::Value> result,
                          AutomationAXTreeWrapper* tree_wrapper,
                          AXNode* node) const;

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that needs access to accessibility trees.
  void StartCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // This is called by automation_internal_custom_bindings.js to indicate
  // that an API was called that turns off accessibility trees. This clears all
  // existing tree state.
  void StopCachingAccessibilityTrees(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router, DanglingUntriaged> automation_v8_router_;
};
}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_V8_BINDINGS_H_
