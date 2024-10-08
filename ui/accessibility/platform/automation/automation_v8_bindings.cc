// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/automation/automation_v8_bindings.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"
#include "ui/accessibility/platform/automation/automation_position.h"
#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "v8/include/v8-function-callback.h"

namespace ui {

namespace {

v8::Local<v8::String> CreateV8String(v8::Isolate* isolate,
                                     std::string_view str) {
  return gin::StringToSymbol(isolate, str);
}

v8::Local<v8::Object> RectToV8Object(v8::Isolate* isolate,
                                     const gfx::Rect& rect) {
  return gin::DataObjectBuilder(isolate)
      .Set("left", rect.x())
      .Set("top", rect.y())
      .Set("width", rect.width())
      .Set("height", rect.height())
      .Build();
}

// Helper class that wraps a V8 handler function to run with V8 or gin
// arguments.
class GenericHandlerFunctionWrapper : public V8HandlerFunctionWrapper {
 public:
  explicit GenericHandlerFunctionWrapper(
      base::RepeatingCallback<void(const v8::FunctionCallbackInfo<v8::Value>&)>
          handler_function)
      : handler_function_(handler_function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert callback to use gin::Arguments.
    const v8::FunctionCallbackInfo<v8::Value>* args =
        arguments->GetFunctionCallbackInfo();
    DCHECK(args);
    handler_function_.Run(*args);
  }

 private:
  ~GenericHandlerFunctionWrapper() override = default;
  base::RepeatingCallback<void(const v8::FunctionCallbackInfo<v8::Value>&)>
      handler_function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes a single input argument consisting of a Tree ID. Looks up
// the AutomationAXTreeWrapper and passes it to the function passed to the
// constructor.
//

typedef void (*TreeIDFunction)(v8::Isolate* isolate,
                               v8::ReturnValue<v8::Value> result,
                               AutomationAXTreeWrapper* tree_wrapper);

class TreeIDWrapper : public V8HandlerFunctionWrapper {
 public:
  TreeIDWrapper(AutomationTreeManagerOwner* automation_tree_manager_owner,
                AutomationV8Router* automation_router,
                TreeIDFunction function)
      : automation_tree_manager_owner_(automation_tree_manager_owner),
        automation_router_(automation_router),
        function_(function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert to use gin::Arguments.
    DCHECK(arguments->GetFunctionCallbackInfo());
    const v8::FunctionCallbackInfo<v8::Value>& args =
        *arguments->GetFunctionCallbackInfo();
    v8::Isolate* isolate = automation_router_->GetIsolate();
    if (args.Length() != 1 || !args[0]->IsString())
      automation_router_->ThrowInvalidArgumentsException();

    AXTreeID tree_id =
        AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    AutomationAXTreeWrapper* tree_wrapper =
        automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
            tree_id);
    if (!tree_wrapper)
      return;

    // The root can be null if this is called from an onTreeChange callback.
    if (!tree_wrapper->ax_tree()->root())
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper);
  }

 private:
  ~TreeIDWrapper() override = default;

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router> automation_router_;
  TreeIDFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes two input arguments: a tree ID and node ID. Looks up the
// AutomationAXTreeWrapper and the AXNode and passes them to the function
// passed to the constructor.
//
typedef base::RepeatingCallback<void(v8::Isolate* isolate,
                                     v8::ReturnValue<v8::Value> result,
                                     AutomationAXTreeWrapper* tree_wrapper,
                                     AXNode* node)>
    NodeIDFunction;

class NodeIDWrapper : public V8HandlerFunctionWrapper {
 public:
  NodeIDWrapper(AutomationTreeManagerOwner* automation_tree_manager_owner,
                AutomationV8Router* automation_router,
                NodeIDFunction function)
      : automation_tree_manager_owner_(automation_tree_manager_owner),
        automation_router_(automation_router),
        function_(function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert to use gin::Arguments.
    DCHECK(arguments->GetFunctionCallbackInfo());
    const v8::FunctionCallbackInfo<v8::Value>& args =
        *arguments->GetFunctionCallbackInfo();
    v8::Isolate* isolate = automation_router_->GetIsolate();
    if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber())
      automation_router_->ThrowInvalidArgumentsException();

    v8::Local<v8::Context> context = automation_router_->GetContext();
    AXTreeID tree_id =
        AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);

    AutomationAXTreeWrapper* tree_wrapper =
        automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
            tree_id);
    if (!tree_wrapper)
      return;

    AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_.Run(isolate, args.GetReturnValue(), tree_wrapper, node);
  }

 private:
  ~NodeIDWrapper() override = default;

  friend class base::RefCountedThreadSafe<NodeIDWrapper>;

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router> automation_router_;
  NodeIDFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes three input arguments: a tree ID, node ID, and string
// argument. Looks up the AutomationAXTreeWrapper and the AXNode and passes
// them to the function passed to the constructor.
//

typedef void (*NodeIDPlusAttributeFunction)(v8::Isolate* isolate,
                                            v8::ReturnValue<v8::Value> result,
                                            AXTree* tree,
                                            AXNode* node,
                                            const std::string& attribute);

class NodeIDPlusAttributeWrapper : public V8HandlerFunctionWrapper {
 public:
  NodeIDPlusAttributeWrapper(
      AutomationTreeManagerOwner* automation_tree_manager_owner,
      AutomationV8Router* automation_router,
      NodeIDPlusAttributeFunction function)
      : automation_tree_manager_owner_(automation_tree_manager_owner),
        automation_router_(automation_router),
        function_(function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert to use gin::Arguments.
    DCHECK(arguments->GetFunctionCallbackInfo());
    const v8::FunctionCallbackInfo<v8::Value>& args =
        *arguments->GetFunctionCallbackInfo();
    v8::Isolate* isolate = automation_router_->GetIsolate();
    if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsNumber() ||
        !args[2]->IsString()) {
      automation_router_->ThrowInvalidArgumentsException();
    }

    v8::Local<v8::Context> context = automation_router_->GetContext();
    AXTreeID tree_id =
        AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    std::string attribute = *v8::String::Utf8Value(isolate, args[2]);

    AutomationAXTreeWrapper* tree_wrapper =
        automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
            tree_id);
    if (!tree_wrapper)
      return;

    AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_(isolate, args.GetReturnValue(), tree_wrapper->ax_tree(), node,
              attribute);
  }

 private:
  ~NodeIDPlusAttributeWrapper() override = default;

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router> automation_router_;
  NodeIDPlusAttributeFunction function_;
};

//
// Helper class that helps implement bindings for a JavaScript function
// that takes four input arguments: a tree ID, node ID, and integer start
// and end indices. Looks up the AutomationAXTreeWrapper and the AXNode and
// passes them to the function passed to the constructor.
//

typedef base::RepeatingCallback<void(v8::Isolate* isolate,
                                     v8::ReturnValue<v8::Value> result,
                                     AutomationAXTreeWrapper* tree_wrapper,
                                     AXNode* node,
                                     int start,
                                     int end,
                                     bool clipped)>
    NodeIDPlusRangeFunction;

class NodeIDPlusRangeWrapper : public V8HandlerFunctionWrapper {
 public:
  NodeIDPlusRangeWrapper(
      AutomationTreeManagerOwner* automation_tree_manager_owner,
      AutomationV8Router* automation_router,
      NodeIDPlusRangeFunction function)
      : automation_tree_manager_owner_(automation_tree_manager_owner),
        automation_router_(automation_router),
        function_(function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert to use gin::Arguments.
    DCHECK(arguments->GetFunctionCallbackInfo());
    const v8::FunctionCallbackInfo<v8::Value>& args =
        *arguments->GetFunctionCallbackInfo();
    v8::Isolate* isolate = automation_router_->GetIsolate();
    if (args.Length() < 5 || !args[0]->IsString() || !args[1]->IsNumber() ||
        !args[2]->IsNumber() || !args[3]->IsNumber() || !args[4]->IsBoolean()) {
      automation_router_->ThrowInvalidArgumentsException();
    }

    v8::Local<v8::Context> context = automation_router_->GetContext();
    AXTreeID tree_id =
        AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    int start = args[2]->Int32Value(context).FromMaybe(0);
    int end = args[3]->Int32Value(context).FromMaybe(0);
    bool clipped = args[4]->BooleanValue(isolate);

    AutomationAXTreeWrapper* tree_wrapper =
        automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
            tree_id);
    if (!tree_wrapper)
      return;

    AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_.Run(isolate, args.GetReturnValue(), tree_wrapper, node, start,
                  end, clipped);
  }

 private:
  ~NodeIDPlusRangeWrapper() override = default;

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router> automation_router_;
  NodeIDPlusRangeFunction function_;
};

typedef base::RepeatingCallback<void(v8::Isolate* isolate,
                                     v8::ReturnValue<v8::Value> result,
                                     AutomationAXTreeWrapper* tree_wrapper,
                                     AXNode* node,
                                     const std::string& strVal,
                                     bool boolVal)>
    NodeIDPlusStringBoolFunction;

class NodeIDPlusStringBoolWrapper : public V8HandlerFunctionWrapper {
 public:
  NodeIDPlusStringBoolWrapper(
      AutomationTreeManagerOwner* automation_tree_manager_owner,
      AutomationV8Router* automation_router,
      NodeIDPlusStringBoolFunction function)
      : automation_tree_manager_owner_(automation_tree_manager_owner),
        automation_router_(automation_router),
        function_(function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert to use gin::Arguments.
    DCHECK(arguments->GetFunctionCallbackInfo());
    const v8::FunctionCallbackInfo<v8::Value>& args =
        *arguments->GetFunctionCallbackInfo();
    v8::Isolate* isolate = automation_router_->GetIsolate();
    if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsNumber() ||
        !args[2]->IsString() || !args[3]->IsBoolean()) {
      automation_router_->ThrowInvalidArgumentsException();
    }

    v8::Local<v8::Context> context = automation_router_->GetContext();
    AXTreeID tree_id =
        AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1]->Int32Value(context).FromMaybe(0);
    std::string str_val = *v8::String::Utf8Value(isolate, args[2]);
    bool bool_val = args[3].As<v8::Boolean>()->Value();

    AutomationAXTreeWrapper* tree_wrapper =
        automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
            tree_id);
    if (!tree_wrapper)
      return;

    AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_.Run(isolate, args.GetReturnValue(), tree_wrapper, node, str_val,
                  bool_val);
  }

 private:
  ~NodeIDPlusStringBoolWrapper() override = default;

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router> automation_router_;
  NodeIDPlusStringBoolFunction function_;
};

using NodeIDPlusDimensionsFunction =
    base::RepeatingCallback<void(v8::Isolate* isolate,
                                 v8::ReturnValue<v8::Value> result,
                                 AutomationAXTreeWrapper* tree_wrapper,
                                 AXNode* node,
                                 int x,
                                 int y,
                                 int width,
                                 int height)>;

class NodeIDPlusDimensionsWrapper : public V8HandlerFunctionWrapper {
 public:
  NodeIDPlusDimensionsWrapper(
      AutomationTreeManagerOwner* automation_tree_manager_owner,
      AutomationV8Router* automation_router,
      NodeIDPlusDimensionsFunction function)
      : automation_tree_manager_owner_(automation_tree_manager_owner),
        automation_router_(automation_router),
        function_(function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert to use gin::Arguments.
    DCHECK(arguments->GetFunctionCallbackInfo());
    const v8::FunctionCallbackInfo<v8::Value>& args =
        *arguments->GetFunctionCallbackInfo();
    v8::Isolate* isolate = automation_router_->GetIsolate();
    if (args.Length() < 6 || !args[0]->IsString() || !args[1]->IsInt32() ||
        !args[2]->IsInt32() || !args[3]->IsInt32() || !args[4]->IsInt32() ||
        !args[5]->IsInt32()) {
      automation_router_->ThrowInvalidArgumentsException(/*is_fatal=*/true);
    }

    AXTreeID tree_id =
        AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1].As<v8::Int32>()->Value();
    int x = args[2].As<v8::Int32>()->Value();
    int y = args[3].As<v8::Int32>()->Value();
    int width = args[4].As<v8::Int32>()->Value();
    int height = args[5].As<v8::Int32>()->Value();

    AutomationAXTreeWrapper* tree_wrapper =
        automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
            tree_id);
    if (!tree_wrapper)
      return;

    AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_.Run(isolate, args.GetReturnValue(), tree_wrapper, node, x, y,
                  width, height);
  }

 private:
  ~NodeIDPlusDimensionsWrapper() override = default;

  friend class base::RefCountedThreadSafe<NodeIDPlusDimensionsWrapper>;

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router> automation_router_;
  NodeIDPlusDimensionsFunction function_;
};

typedef base::RepeatingCallback<void(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type)>
    NodeIDPlusEventFunction;

class NodeIDPlusEventWrapper : public V8HandlerFunctionWrapper {
 public:
  NodeIDPlusEventWrapper(
      AutomationTreeManagerOwner* automation_tree_manager_owner,
      AutomationV8Router* automation_router,
      NodeIDPlusEventFunction function)
      : automation_tree_manager_owner_(automation_tree_manager_owner),
        automation_router_(automation_router),
        function_(function) {}

  void Run(gin::Arguments* arguments) override {
    // TODO: Convert to use gin::Arguments.
    DCHECK(arguments->GetFunctionCallbackInfo());
    const v8::FunctionCallbackInfo<v8::Value>& args =
        *arguments->GetFunctionCallbackInfo();
    v8::Isolate* isolate = automation_router_->GetIsolate();
    if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsInt32() ||
        !args[2]->IsString()) {
      // The extension system does not perform argument validation in js, so an
      // extension author can do something like node.addEventListener(undefined)
      // and reach here. Do not crash the process.
      automation_router_->ThrowInvalidArgumentsException(/*is_fatal=*/false);
      return;
    }

    AXTreeID tree_id =
        AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
    int node_id = args[1].As<v8::Int32>()->Value();

    std::tuple<ax::mojom::Event, AXEventGenerator::Event> event_type =
        AutomationEventTypeToAXEventTuple(
            *v8::String::Utf8Value(isolate, args[2]));

    // Check this is an event type we don't use in automation:
    // either type none or ignored type.
    const ax::mojom::Event ax_event = std::get<0>(event_type);
    const AXEventGenerator::Event generated_event = std::get<1>(event_type);
    if ((ax_event == ax::mojom::Event::kNone &&
         generated_event == AXEventGenerator::Event::NONE)) {
      automation_router_->ThrowInvalidArgumentsException(/*is_fatal=*/false);
      return;
    }

    AutomationAXTreeWrapper* tree_wrapper =
        automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
            tree_id);
    if (!tree_wrapper)
      return;

    AXNode* node = tree_wrapper->GetUnignoredNodeFromId(node_id);
    if (!node)
      return;

    function_.Run(isolate, args.GetReturnValue(), tree_wrapper, node,
                  event_type);
  }

 private:
  ~NodeIDPlusEventWrapper() override = default;

  raw_ptr<AutomationTreeManagerOwner> automation_tree_manager_owner_;
  raw_ptr<AutomationV8Router> automation_router_;
  NodeIDPlusEventFunction function_;
};

}  // namespace

AutomationV8Bindings::AutomationV8Bindings(AutomationTreeManagerOwner* owner,
                                           AutomationV8Router* router)
    : automation_tree_manager_owner_(owner), automation_v8_router_(router) {}

AutomationV8Bindings::~AutomationV8Bindings() = default;

void AutomationV8Bindings::SendTreeChangeEvent(
    int observer_id,
    const AXTreeID& tree_id,
    int node_id,
    ax::mojom::Mutation change_type) {
  base::Value::List args;
  args.Append(observer_id);
  args.Append(tree_id.ToString());
  args.Append(node_id);
  args.Append(automation_v8_router_->GetTreeChangeTypeString(change_type));
  automation_v8_router_->DispatchEvent("automationInternal.onTreeChange", args);
}

void AutomationV8Bindings::SendNodesRemovedEvent(const AXTreeID& tree_id,
                                                 const std::vector<int>& ids) {
  base::Value::List args;
  args.Append(tree_id.ToString());
  {
    base::Value::List nodes;
    for (auto id : ids)
      nodes.Append(id);
    args.Append(std::move(nodes));
  }

  automation_v8_router_->DispatchEvent("automationInternal.onNodesRemoved",
                                       args);
}

void AutomationV8Bindings::SendChildTreeIDEvent(const AXTreeID& child_tree_id) {
  base::Value::List args;
  args.Append(child_tree_id.ToString());
  automation_v8_router_->DispatchEvent("automationInternal.onChildTreeID",
                                       args);
}

void AutomationV8Bindings::SendTreeDestroyedEvent(const AXTreeID& tree_id) {
  base::Value::List args;
  args.Append(tree_id.ToString());
  automation_v8_router_->DispatchEvent(
      "automationInternal.onAccessibilityTreeDestroyed", args);
}

void AutomationV8Bindings::SendGetTextLocationResult(
    const AXActionData& data,
    const std::optional<gfx::Rect>& rect) {
  base::Value::Dict params;
  params.Set("treeID", data.target_tree_id.ToString());
  params.Set("childTreeID", data.child_tree_id.ToString());
  params.Set("nodeID", data.target_node_id);
  params.Set("result", false);
  if (rect) {
    params.Set("left", rect.value().x());
    params.Set("top", rect.value().y());
    params.Set("width", rect.value().width());
    params.Set("height", rect.value().height());
    params.Set("result", true);
  }
  params.Set("requestID", data.request_id);

  base::Value::List args;
  args.Append(std::move(params));
  automation_v8_router_->DispatchEvent(
      "automationInternal.onGetTextLocationResult", args);
}

void AutomationV8Bindings::SendActionResultEvent(const AXActionData& data,
                                                 bool result) {
  base::Value::List args;
  args.Append(data.target_tree_id.ToString());
  args.Append(data.request_id);
  args.Append(result);
  automation_v8_router_->DispatchEvent("automationInternal.onActionResult",
                                       args);
}

void AutomationV8Bindings::SendAutomationEvent(
    const AXTreeID& tree_id,
    const AXEvent& event,
    const gfx::Point& mouse_location,
    const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type) {
  const std::string automation_event_type_str =
      automation_v8_router_->GetEventTypeString(event_type);

  base::Value::Dict event_params;
  event_params.Set("treeID", base::Value(tree_id.ToString()));
  event_params.Set("targetID", base::Value(event.id));
  event_params.Set("eventType", base::Value(automation_event_type_str));

  event_params.Set("eventFrom", base::Value(ToString(event.event_from)));
  event_params.Set("eventFromAction",
                   base::Value(ToString(event.event_from_action)));
  event_params.Set("actionRequestID", base::Value(event.action_request_id));
  event_params.Set("mouseX", base::Value(mouse_location.x()));
  event_params.Set("mouseY", base::Value(mouse_location.y()));

  // Populate intents.
  base::Value::List value_intents;
  for (const auto& intent : event.event_intents) {
    base::Value::Dict dict;
    dict.Set("command", base::Value(ToString(intent.command)));
    dict.Set("inputEventType", base::Value(ToString(intent.input_event_type)));
    dict.Set("textBoundary", base::Value(ToString(intent.text_boundary)));
    dict.Set("moveDirection", base::Value(ToString(intent.move_direction)));
    value_intents.Append(std::move(dict));
  }

  event_params.Set("intents", std::move(value_intents));

  base::Value::List args;
  args.Append(std::move(event_params));
  automation_v8_router_->DispatchEvent(
      "automationInternal.onAccessibilityEvent", args);
}

void AutomationV8Bindings::SendTreeSerializationError(const AXTreeID& tree_id) {
  base::Value::List args;
  args.Append(tree_id.ToString());
  automation_v8_router_->DispatchEvent(
      "automationInternal.onAccessibilityTreeSerializationError", args);
}

void AutomationV8Bindings::SendOnAllEventListenersRemoved() {
  automation_v8_router_->DispatchEvent(
      "automationInternal.onAllAutomationEventListenersRemoved",
      base::Value::List());
}

void AutomationV8Bindings::AddV8Routes() {
  // It's safe to use base::Unretained(this) here because these bindings
  // will only be called on a valid AutomationV8Bindings instance
  // and none of the functions have any side effects.
  scoped_refptr<GenericHandlerFunctionWrapper> wrapper;
#define ROUTE_FUNCTION(FN)                                                     \
  wrapper = base::MakeRefCounted<GenericHandlerFunctionWrapper>(               \
      base::BindRepeating(&AutomationV8Bindings::FN, base::Unretained(this))); \
  automation_v8_router_->RouteHandlerFunction(#FN, wrapper);
  ROUTE_FUNCTION(GetChildIDAtIndex);
  ROUTE_FUNCTION(GetFocus);
  ROUTE_FUNCTION(CreateAutomationPosition);
  ROUTE_FUNCTION(GetAccessibilityFocus);
  ROUTE_FUNCTION(StringAXTreeIDToUnguessableToken);
  ROUTE_FUNCTION(SetDesktopID);
  ROUTE_FUNCTION(DestroyAccessibilityTree);
  ROUTE_FUNCTION(AddTreeChangeObserver);
  ROUTE_FUNCTION(RemoveTreeChangeObserver);
  ROUTE_FUNCTION(GetState);
  ROUTE_FUNCTION(StartCachingAccessibilityTrees);
  ROUTE_FUNCTION(StopCachingAccessibilityTrees);

  // Bindings that take a Tree ID and return a property of the tree.

  RouteTreeIDFunction(
      "GetRootID", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(
            v8::Integer::New(isolate, tree_wrapper->ax_tree()->root()->id()));
      });
  RouteTreeIDFunction(
      "GetPublicRoot",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        tree_wrapper = tree_wrapper->GetTreeWrapperWithUnignoredRoot();
        if (!tree_wrapper)
          return;

        gin::DataObjectBuilder response(isolate);
        response.Set("treeId", tree_wrapper->GetTreeID().ToString());
        response.Set("nodeId", tree_wrapper->ax_tree()->root()->id());
        result.Set(response.Build());
      });
  RouteTreeIDFunction(
      "GetDocURL", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                      AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::String::NewFromUtf8(
                       isolate, tree_wrapper->ax_tree()->data().url.c_str())
                       .ToLocalChecked());
      });
  RouteTreeIDFunction(
      "GetDocTitle", [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
                        AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::String::NewFromUtf8(
                       isolate, tree_wrapper->ax_tree()->data().title.c_str())
                       .ToLocalChecked());
      });
  RouteTreeIDFunction(
      "GetDocLoaded",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(tree_wrapper->ax_tree()->data().loaded);
      });
  RouteTreeIDFunction(
      "GetDocLoadingProgress",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->ax_tree()->data().loading_progress));
      });
  RouteTreeIDFunction(
      "GetIsSelectionBackward",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        const AXNode* anchor = tree_wrapper->GetNodeFromTree(
            tree_wrapper->GetTreeID(),
            tree_wrapper->GetUnignoredSelection().anchor_object_id);
        if (!anchor)
          return;

        result.Set(tree_wrapper->ax_tree()->data().sel_is_backward);
      });
  RouteTreeIDFunction(
      "GetAnchorObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().anchor_object_id));
      });
  RouteTreeIDFunction(
      "GetAnchorOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().anchor_offset));
      });
  RouteTreeIDFunction(
      "GetAnchorAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ToString(tree_wrapper->GetUnignoredSelection().anchor_affinity)));
      });
  RouteTreeIDFunction(
      "GetFocusObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().focus_object_id));
      });
  RouteTreeIDFunction(
      "GetFocusOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(v8::Number::New(
            isolate, tree_wrapper->GetUnignoredSelection().focus_offset));
      });
  RouteTreeIDFunction(
      "GetFocusAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        result.Set(CreateV8String(
            isolate,
            ToString(tree_wrapper->GetUnignoredSelection().focus_affinity)));
      });
  RouteTreeIDFunction(
      "GetSelectionStartObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        AXSelection unignored_selection = tree_wrapper->GetUnignoredSelection();
        int32_t start_object_id = unignored_selection.is_backward
                                      ? unignored_selection.focus_object_id
                                      : unignored_selection.anchor_object_id;
        result.Set(v8::Number::New(isolate, start_object_id));
      });
  RouteTreeIDFunction(
      "GetSelectionStartOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        AXSelection unignored_selection = tree_wrapper->GetUnignoredSelection();
        int start_offset = unignored_selection.is_backward
                               ? unignored_selection.focus_offset
                               : unignored_selection.anchor_offset;
        result.Set(v8::Number::New(isolate, start_offset));
      });
  RouteTreeIDFunction(
      "GetSelectionStartAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        AXSelection unignored_selection = tree_wrapper->GetUnignoredSelection();
        ax::mojom::TextAffinity start_affinity =
            unignored_selection.is_backward
                ? unignored_selection.focus_affinity
                : unignored_selection.anchor_affinity;
        result.Set(CreateV8String(isolate, ToString(start_affinity)));
      });
  RouteTreeIDFunction(
      "GetSelectionEndObjectID",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        AXSelection unignored_selection = tree_wrapper->GetUnignoredSelection();
        int32_t end_object_id = unignored_selection.is_backward
                                    ? unignored_selection.anchor_object_id
                                    : unignored_selection.focus_object_id;
        result.Set(v8::Number::New(isolate, end_object_id));
      });
  RouteTreeIDFunction(
      "GetSelectionEndOffset",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        AXSelection unignored_selection = tree_wrapper->GetUnignoredSelection();
        int end_offset = unignored_selection.is_backward
                             ? unignored_selection.anchor_offset
                             : unignored_selection.focus_offset;
        result.Set(v8::Number::New(isolate, end_offset));
      });
  RouteTreeIDFunction(
      "GetSelectionEndAffinity",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
         AutomationAXTreeWrapper* tree_wrapper) {
        AXSelection unignored_selection = tree_wrapper->GetUnignoredSelection();
        ax::mojom::TextAffinity end_affinity =
            unignored_selection.is_backward
                ? unignored_selection.anchor_affinity
                : unignored_selection.focus_affinity;
        result.Set(CreateV8String(isolate, ToString(end_affinity)));
      });

  // Bindings that take a Tree ID and Node ID and return a property of the node.

  RouteNodeIDFunction("GetParentID",
                      base::BindRepeating(&AutomationV8Bindings::GetParentID,
                                          base::Unretained(this)));
  RouteNodeIDFunction("GetChildCount",
                      base::BindRepeating(&AutomationV8Bindings::GetChildCount,
                                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetIndexInParent",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        result.Set(v8::Integer::New(
            isolate, static_cast<int32_t>(node->GetUnignoredIndexInParent())));
      }));
  RouteNodeIDFunction(
      "GetRole",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            const std::string& role_name = ToString(node->GetRole());
            result.Set(v8::String::NewFromUtf8(isolate, role_name.c_str())
                           .ToLocalChecked());
          }));
  RouteNodeIDFunction("GetLocation",
                      base::BindRepeating(&AutomationV8Bindings::GetLocation,
                                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetUnclippedLocation",
      base::BindRepeating(&AutomationV8Bindings::GetUnclippedLocation,
                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetLineStartOffsets",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        const std::vector<int> line_starts =
            node->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, line_starts.size()));
        for (size_t i = 0; i < line_starts.size(); ++i) {
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, line_starts[i]))
              .Check();
        }
        result.Set(array_result);
      }));
  RouteNodeIDFunction("GetChildIDs",
                      base::BindRepeating(&AutomationV8Bindings::GetChildIDs,
                                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetWordStartOffsets",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            std::vector<int> word_starts = GetWordStartOffsets(
                node->GetString16Attribute(ax::mojom::StringAttribute::kName));
            result.Set(gin::ConvertToV8(isolate, word_starts));
          }));
  RouteNodeIDFunction(
      "GetWordEndOffsets",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            std::vector<int> word_ends = GetWordEndOffsets(
                node->GetString16Attribute(ax::mojom::StringAttribute::kName));
            result.Set(gin::ConvertToV8(isolate, word_ends));
          }));
  RouteNodeIDFunction(
      "GetSentenceStartOffsets",
      base::BindRepeating(&AutomationV8Bindings::GetSentenceStartOffsets,
                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetSentenceEndOffsets",
      base::BindRepeating(&AutomationV8Bindings::GetSentenceEndOffsets,
                          base::Unretained(this)));

  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusRangeFunction(
      "GetBoundsForRange",
      base::BindRepeating(&AutomationV8Bindings::GetBoundsForRange,
                          base::Unretained(this)));

  RouteNodeIDPlusDimensionsFunction(
      "ComputeGlobalBounds",
      base::BindRepeating(&AutomationV8Bindings::ComputeGlobalBounds,
                          base::Unretained(this)));

  // Bindings that take a Tree ID and Node ID and string attribute name
  // and return a property of the node.

  RouteNodeIDPlusAttributeFunction(
      "GetStringAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result, AXTree* tree,
         AXNode* node, const std::string& attribute_name) {
        auto attribute =
            ParseAXEnum<ax::mojom::StringAttribute>(attribute_name.c_str());
        const char* attr_value;
        if (attribute == ax::mojom::StringAttribute::kFontFamily ||
            attribute == ax::mojom::StringAttribute::kLanguage) {
          attr_value = node->GetInheritedStringAttribute(attribute).c_str();
        } else if (!node->HasStringAttribute(attribute)) {
          return;
        } else {
          attr_value = node->GetStringAttribute(attribute).c_str();
        }

        result.Set(
            v8::String::NewFromUtf8(isolate, attr_value).ToLocalChecked());
      });
  RouteNodeIDPlusAttributeFunction(
      "GetBoolAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result, AXTree* tree,
         AXNode* node, const std::string& attribute_name) {
        auto attribute =
            ParseAXEnum<ax::mojom::BoolAttribute>(attribute_name.c_str());
        if (!node->HasBoolAttribute(attribute)) {
          return;
        }
        result.Set(node->GetBoolAttribute(attribute));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result, AXTree* tree,
         AXNode* node, const std::string& attribute_name) {
        auto attribute =
            ParseAXEnum<ax::mojom::IntAttribute>(attribute_name.c_str());
        int attr_value;

        if (attribute == ax::mojom::IntAttribute::kPosInSet &&
            node->GetPosInSet()) {
          attr_value = *node->GetPosInSet();
        } else if (attribute == ax::mojom::IntAttribute::kSetSize &&
                   node->GetSetSize()) {
          attr_value = *node->GetSetSize();
        } else if (!node->HasIntAttribute(attribute)) {
          return;
        } else {
          attr_value = node->GetIntAttribute(attribute);
        }

        result.Set(v8::Integer::New(isolate, attr_value));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntAttributeReverseRelations",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result, AXTree* tree,
         AXNode* node, const std::string& attribute_name) {
        auto attribute =
            ParseAXEnum<ax::mojom::IntAttribute>(attribute_name.c_str());
        std::set<int32_t> ids =
            tree->GetReverseRelations(attribute, node->id());
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(v8::Array::New(isolate, ids.size()));
        size_t count = 0;
        for (int32_t id : ids) {
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(count++),
                                   v8::Integer::New(isolate, id))
              .Check();
        }
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetFloatAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result, AXTree* tree,
         AXNode* node, const std::string& attribute_name) {
        auto attribute =
            ParseAXEnum<ax::mojom::FloatAttribute>(attribute_name.c_str());

        if (!node->HasFloatAttribute(attribute)) {
          return;
        }

        float attr_value = node->GetFloatAttribute(attribute);

        double intpart, fracpart;
        fracpart = modf(attr_value, &intpart);
        double value_precision_2 =
            intpart + std::round(fracpart * 100) / 100.0f;
        result.Set(v8::Number::New(isolate, value_precision_2));
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntListAttribute",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result, AXTree* tree,
         AXNode* node, const std::string& attribute_name) {
        auto attribute =
            ParseAXEnum<ax::mojom::IntListAttribute>(attribute_name.c_str());
        if (!node->HasIntListAttribute(attribute))
          return;
        const std::vector<int32_t>& attr_value =
            node->GetIntListAttribute(attribute);

        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, attr_value.size()));
        for (size_t i = 0; i < attr_value.size(); ++i)
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, attr_value[i]))
              .Check();
        result.Set(array_result);
      });
  RouteNodeIDPlusAttributeFunction(
      "GetIntListAttributeReverseRelations",
      [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result, AXTree* tree,
         AXNode* node, const std::string& attribute_name) {
        auto attribute =
            ParseAXEnum<ax::mojom::IntListAttribute>(attribute_name.c_str());
        std::set<int32_t> ids =
            tree->GetReverseRelations(attribute, node->id());
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(v8::Array::New(isolate, ids.size()));
        size_t count = 0;
        for (int32_t id : ids) {
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(count++),
                                   v8::Integer::New(isolate, id))
              .Check();
        }
        result.Set(array_result);
      });
  RouteNodeIDFunction(
      "GetNameFrom",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            ax::mojom::NameFrom name_from = node->data().GetNameFrom();
            const std::string& name_from_str = ToString(name_from);
            result.Set(v8::String::NewFromUtf8(isolate, name_from_str.c_str())
                           .ToLocalChecked());
          }));
  RouteNodeIDFunction("GetName",
                      base::BindRepeating(&AutomationV8Bindings::GetName,
                                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetDescriptionFrom",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            ax::mojom::DescriptionFrom description_from =
                static_cast<ax::mojom::DescriptionFrom>(node->GetIntAttribute(
                    ax::mojom::IntAttribute::kDescriptionFrom));
            std::string description_from_str = ToString(description_from);
            result.Set(
                v8::String::NewFromUtf8(isolate, description_from_str.c_str())
                    .ToLocalChecked());
          }));

  RouteNodeIDFunction(
      "GetSubscript",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value =
                node->GetIntAttribute(ax::mojom::IntAttribute::kTextPosition) ==
                static_cast<int32_t>(ax::mojom::TextPosition::kSubscript);
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetSuperscript",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value =
                node->GetIntAttribute(ax::mojom::IntAttribute::kTextPosition) ==
                static_cast<int32_t>(ax::mojom::TextPosition::kSuperscript);
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetBold",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value = node->data().HasTextStyle(ax::mojom::TextStyle::kBold);
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetItalic", base::BindRepeating([](v8::Isolate* isolate,
                                          v8::ReturnValue<v8::Value> result,
                                          AutomationAXTreeWrapper* tree_wrapper,
                                          AXNode* node) {
        bool value = node->data().HasTextStyle(ax::mojom::TextStyle::kItalic);
        result.Set(value);
      }));
  RouteNodeIDFunction(
      "GetUnderline",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value =
                node->data().HasTextStyle(ax::mojom::TextStyle::kUnderline);
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetLineThrough",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value =
                node->data().HasTextStyle(ax::mojom::TextStyle::kLineThrough);
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetDetectedLanguage",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        const std::string& detectedLanguage = node->GetLanguage();
        result.Set(v8::String::NewFromUtf8(isolate, detectedLanguage.c_str())
                       .ToLocalChecked());
      }));

  RouteNodeIDFunction(
      "GetCustomActions",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            const std::vector<int32_t>& custom_action_ids =
                node->GetIntListAttribute(
                    ax::mojom::IntListAttribute::kCustomActionIds);
            if (custom_action_ids.empty()) {
              result.SetUndefined();
              return;
            }

            const std::vector<std::string>& custom_action_descriptions =
                node->GetStringListAttribute(
                    ax::mojom::StringListAttribute::kCustomActionDescriptions);
            if (custom_action_ids.size() != custom_action_descriptions.size()) {
              NOTREACHED_IN_MIGRATION();
              return;
            }

            v8::Local<v8::Context> context = isolate->GetCurrentContext();
            v8::Local<v8::Array> custom_actions(
                v8::Array::New(isolate, custom_action_ids.size()));
            for (size_t i = 0; i < custom_action_ids.size(); i++) {
              gin::DataObjectBuilder custom_action(isolate);
              custom_action.Set("id", custom_action_ids[i]);
              custom_action.Set("description", custom_action_descriptions[i]);
              custom_actions
                  ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                       custom_action.Build())
                  .Check();
            }
            result.Set(custom_actions);
          }));
  RouteNodeIDFunction(
      "GetStandardActions",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        std::vector<std::string> standard_actions;
        for (uint32_t action = static_cast<uint32_t>(ax::mojom::Action::kNone);
             action <= static_cast<uint32_t>(ax::mojom::Action::kMaxValue);
             ++action) {
          if (node->data().HasAction(static_cast<ax::mojom::Action>(action))) {
            standard_actions.push_back(
                ToString(static_cast<ax::mojom::Action>(action)));
          }
        }

        // TODO(crbug.com/41454524): Set doDefault, increment, and decrement
        // directly
        //     on the AXNode.
        // The doDefault action is implied by having a default action verb.
        int default_action_verb = static_cast<int>(
            node->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));
        if (node->HasIntAttribute(
                ax::mojom::IntAttribute::kDefaultActionVerb) &&
            default_action_verb !=
                static_cast<int>(ax::mojom::DefaultActionVerb::kNone)) {
          standard_actions.push_back(ToString(
              static_cast<ax::mojom::Action>(ax::mojom::Action::kDoDefault)));
        }

        // Increment and decrement are available when the role is a slider or
        // spin button.
        auto role = node->GetRole();
        if (role == ax::mojom::Role::kSlider ||
            role == ax::mojom::Role::kSpinButton) {
          standard_actions.push_back(ToString(
              static_cast<ax::mojom::Action>(ax::mojom::Action::kIncrement)));
          standard_actions.push_back(ToString(
              static_cast<ax::mojom::Action>(ax::mojom::Action::kDecrement)));
        }

        auto actions_result = v8::Array::New(isolate, standard_actions.size());
        for (size_t i = 0; i < standard_actions.size(); i++) {
          const v8::Maybe<bool>& did_set_value = actions_result->Set(
              isolate->GetCurrentContext(), i,
              v8::String::NewFromUtf8(isolate, standard_actions[i].c_str())
                  .ToLocalChecked());

          bool did_set_value_result = false;
          if (!did_set_value.To(&did_set_value_result) || !did_set_value_result)
            return;
        }
        result.Set(actions_result);
      }));
  RouteNodeIDFunction(
      "GetChecked",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            const ax::mojom::CheckedState checked_state =
                static_cast<ax::mojom::CheckedState>(node->GetIntAttribute(
                    ax::mojom::IntAttribute::kCheckedState));
            if (checked_state != ax::mojom::CheckedState::kNone) {
              const std::string& checked_str = ToString(checked_state);
              result.Set(v8::String::NewFromUtf8(isolate, checked_str.c_str())
                             .ToLocalChecked());
            }
          }));
  RouteNodeIDFunction(
      "GetRestriction",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        const ax::mojom::Restriction restriction =
            node->data().GetRestriction();
        if (restriction != ax::mojom::Restriction::kNone) {
          const std::string& restriction_str = ToString(restriction);
          result.Set(v8::String::NewFromUtf8(isolate, restriction_str.c_str())
                         .ToLocalChecked());
        }
      }));
  RouteNodeIDFunction(
      "GetDefaultActionVerb",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            ax::mojom::DefaultActionVerb default_action_verb =
                static_cast<ax::mojom::DefaultActionVerb>(node->GetIntAttribute(
                    ax::mojom::IntAttribute::kDefaultActionVerb));
            if (default_action_verb == ax::mojom::DefaultActionVerb::kNone)
              return;

            const std::string& default_action_verb_str =
                ToString(default_action_verb);
            result.Set(v8::String::NewFromUtf8(isolate,
                                               default_action_verb_str.c_str())
                           .ToLocalChecked());
          }));
  RouteNodeIDFunction(
      "GetHasPopup",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            ax::mojom::HasPopup has_popup = node->data().GetHasPopup();
            const std::string& has_popup_str = ToString(has_popup);
            result.Set(v8::String::NewFromUtf8(isolate, has_popup_str.c_str())
                           .ToLocalChecked());
          }));
  RouteNodeIDFunction(
      "GetAriaCurrentState",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            ax::mojom::AriaCurrentState current_state =
                static_cast<ax::mojom::AriaCurrentState>(node->GetIntAttribute(
                    ax::mojom::IntAttribute::kAriaCurrentState));
            if (current_state == ax::mojom::AriaCurrentState::kNone)
              return;
            const std::string& current_state_string = ToString(current_state);
            result.Set(
                v8::String::NewFromUtf8(isolate, current_state_string.c_str())
                    .ToLocalChecked());
          }));
  RouteNodeIDFunction(
      "GetInvalidState",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            ax::mojom::InvalidState invalid_state = node->GetInvalidState();
            if (invalid_state == ax::mojom::InvalidState::kNone)
              return;
            const std::string& invalid_state_string = ToString(invalid_state);
            result.Set(
                v8::String::NewFromUtf8(isolate, invalid_state_string.c_str())
                    .ToLocalChecked());
          }));
  RouteNodeIDFunction(
      "GetIsButton",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value = IsButton(node->GetRole());
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetIsCheckBox",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value = IsCheckBox(node->GetRole());
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetIsComboBox",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value = IsComboBox(node->GetRole());
            result.Set(value);
          }));
  RouteNodeIDFunction(
      "GetIsImage",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            bool value = IsImage(node->GetRole());
            result.Set(value);
          }));
  RouteNodeIDPlusStringBoolFunction(
      "GetNextTextMatch",
      base::BindRepeating(&AutomationV8Bindings::GetNextTextMatch,
                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetTableColumnCount",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            if (node->GetTableColCount())
              result.Set(*node->GetTableColCount());
          }));
  RouteNodeIDFunction(
      "GetTableRowCount",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            if (node->GetTableRowCount())
              result.Set(*node->GetTableRowCount());
          }));
  RouteNodeIDFunction(
      "GetTableCellColumnHeaders",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        std::vector<int32_t> col_headers = node->GetTableCellColHeaderNodeIds();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, col_headers.size()));
        for (size_t i = 0; i < col_headers.size(); ++i)
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, col_headers[i]))
              .Check();
        result.Set(array_result);
      }));
  RouteNodeIDFunction(
      "GetTableCellRowHeaders",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        std::vector<int32_t> row_headers = node->GetTableCellRowHeaderNodeIds();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> array_result(
            v8::Array::New(isolate, row_headers.size()));
        for (size_t i = 0; i < row_headers.size(); ++i)
          array_result
              ->CreateDataProperty(context, static_cast<uint32_t>(i),
                                   v8::Integer::New(isolate, row_headers[i]))
              .Check();
        result.Set(array_result);
      }));
  RouteNodeIDFunction(
      "GetTableCellColumnIndex",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            if (node->GetTableCellColIndex())
              result.Set(*node->GetTableCellColIndex());
          }));
  RouteNodeIDFunction(
      "GetTableCellRowIndex",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            if (node->GetTableCellRowIndex())
              result.Set(*node->GetTableCellRowIndex());
          }));
  RouteNodeIDFunction(
      "GetTableCellAriaColumnIndex",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            if (node->GetTableCellAriaColIndex())
              result.Set(*node->GetTableCellAriaColIndex());
          }));
  RouteNodeIDFunction(
      "GetTableCellAriaRowIndex",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            if (node->GetTableCellAriaRowIndex())
              result.Set(*node->GetTableCellAriaRowIndex());
          }));
  RouteNodeIDFunction(
      "SetAccessibilityFocus",
      base::BindRepeating(&AutomationV8Bindings::SetAccessibilityFocus,
                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetSortDirection",
      base::BindRepeating([](v8::Isolate* isolate,
                             v8::ReturnValue<v8::Value> result,
                             AutomationAXTreeWrapper* tree_wrapper,
                             AXNode* node) {
        if (node->HasIntAttribute(ax::mojom::IntAttribute::kSortDirection)) {
          const std::string& sort_direction_str = ToString(
              static_cast<ax::mojom::SortDirection>(node->GetIntAttribute(
                  ax::mojom::IntAttribute::kSortDirection)));
          result.Set(
              v8::String::NewFromUtf8(isolate, sort_direction_str.c_str())
                  .ToLocalChecked());
        }
      }));
  RouteNodeIDFunction(
      "GetValue",
      base::BindRepeating(
          [](v8::Isolate* isolate, v8::ReturnValue<v8::Value> result,
             AutomationAXTreeWrapper* tree_wrapper, AXNode* node) {
            const std::string value_str = node->GetValueForControl();
            result.Set(v8::String::NewFromUtf8(isolate, value_str.c_str())
                           .ToLocalChecked());
          }));
  RouteNodeIDPlusEventFunction(
      "EventListenerAdded",
      base::BindRepeating(&AutomationV8Bindings::EventListenerAdded,
                          base::Unretained(this)));
  RouteNodeIDPlusEventFunction(
      "EventListenerRemoved",
      base::BindRepeating(&AutomationV8Bindings::EventListenerRemoved,
                          base::Unretained(this)));
  RouteNodeIDFunction("GetMarkers",
                      base::BindRepeating(&AutomationV8Bindings::GetMarkers,
                                          base::Unretained(this)));
  RouteNodeIDFunction(
      "GetImageAnnotation",
      base::BindRepeating(&AutomationV8Bindings::GetImageAnnotation,
                          base::Unretained(this)));
}

void AutomationV8Bindings::RouteTreeIDFunction(const std::string& name,
                                               TreeIDFunction callback) {
  auto wrapper = base::MakeRefCounted<TreeIDWrapper>(
      automation_tree_manager_owner_, automation_v8_router_, callback);
  automation_v8_router_->RouteHandlerFunction(name, wrapper);
}

void AutomationV8Bindings::RouteNodeIDFunction(const std::string& name,
                                               NodeIDFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDWrapper>(
      automation_tree_manager_owner_, automation_v8_router_, callback);
  automation_v8_router_->RouteHandlerFunction(name, wrapper);
}

void AutomationV8Bindings::RouteNodeIDPlusAttributeFunction(
    const std::string& name,
    NodeIDPlusAttributeFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDPlusAttributeWrapper>(
      automation_tree_manager_owner_, automation_v8_router_, callback);
  automation_v8_router_->RouteHandlerFunction(name, wrapper);
}

void AutomationV8Bindings::RouteNodeIDPlusRangeFunction(
    const std::string& name,
    NodeIDPlusRangeFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDPlusRangeWrapper>(
      automation_tree_manager_owner_, automation_v8_router_, callback);
  automation_v8_router_->RouteHandlerFunction(name, wrapper);
}

void AutomationV8Bindings::RouteNodeIDPlusStringBoolFunction(
    const std::string& name,
    NodeIDPlusStringBoolFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDPlusStringBoolWrapper>(
      automation_tree_manager_owner_, automation_v8_router_, callback);
  automation_v8_router_->RouteHandlerFunction(name, wrapper);
}

void AutomationV8Bindings::RouteNodeIDPlusDimensionsFunction(
    const std::string& name,
    NodeIDPlusDimensionsFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDPlusDimensionsWrapper>(
      automation_tree_manager_owner_, automation_v8_router_, callback);
  automation_v8_router_->RouteHandlerFunction(name, wrapper);
}

void AutomationV8Bindings::RouteNodeIDPlusEventFunction(
    const std::string& name,
    NodeIDPlusEventFunction callback) {
  auto wrapper = base::MakeRefCounted<NodeIDPlusEventWrapper>(
      automation_tree_manager_owner_, automation_v8_router_, callback);
  automation_v8_router_->RouteHandlerFunction(name, wrapper);
}

void AutomationV8Bindings::GetFocus(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  if (args.Length() != 0) {
    automation_v8_router_->ThrowInvalidArgumentsException();
    return;
  }

  int node_id;
  AXTreeID focused_tree_id;
  if (!automation_tree_manager_owner_->GetFocus(&focused_tree_id, &node_id))
    return;

  args.GetReturnValue().Set(
      gin::DataObjectBuilder(automation_v8_router_->GetIsolate())
          .Set("treeId", focused_tree_id.ToString())
          .Set("nodeId", node_id)
          .Build());
}

void AutomationV8Bindings::GetAccessibilityFocus(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  AXTreeID tree_id;
  int node_id;
  if (!automation_tree_manager_owner_->GetAccessibilityFocus(&tree_id,
                                                             &node_id))
    return;

  args.GetReturnValue().Set(
      gin::DataObjectBuilder(automation_v8_router_->GetIsolate())
          .Set("treeId", tree_id.ToString())
          .Set("nodeId", node_id)
          .Build());
}

void AutomationV8Bindings::StringAXTreeIDToUnguessableToken(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  if (args.Length() != 1 || !args[0]->IsString()) {
    automation_v8_router_->ThrowInvalidArgumentsException();
    return;
  }

  const AXTreeID tree_id =
      AXTreeID::FromString(*v8::String::Utf8Value(args.GetIsolate(), args[0]));
  const std::optional<base::UnguessableToken>& token = tree_id.token();
  if (!token || token->is_empty()) {
    return;
  }

  const std::string high_str =
      base::NumberToString(token->GetHighForSerialization());
  const std::string low_str =
      base::NumberToString(token->GetLowForSerialization());
  gin::DataObjectBuilder response(automation_v8_router_->GetIsolate());
  response.Set("high", high_str);
  response.Set("low", low_str);
  args.GetReturnValue().Set(response.Build());
}

void AutomationV8Bindings::SetDesktopID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    automation_v8_router_->ThrowInvalidArgumentsException();
    return;
  }

  automation_tree_manager_owner_->SetDesktopTreeId(
      AXTreeID::FromString(*v8::String::Utf8Value(args.GetIsolate(), args[0])));
}

void AutomationV8Bindings::GetChildIDAtIndex(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  if (args.Length() < 3 || !args[2]->IsNumber()) {
    automation_v8_router_->ThrowInvalidArgumentsException();
    return;
  }

  AXTreeID tree_id =
      AXTreeID::FromString(*v8::String::Utf8Value(args.GetIsolate(), args[0]));
  int node_id =
      args[1]->Int32Value(automation_v8_router_->GetContext()).FromMaybe(0);
  int index =
      args[2]->Int32Value(automation_v8_router_->GetContext()).FromMaybe(0);

  int child_node_id;
  AXTreeID child_tree_id;
  if (!automation_tree_manager_owner_->GetChildIDAtIndex(
          tree_id, node_id, index, &child_tree_id, &child_node_id))
    return;

  gin::DataObjectBuilder response(automation_v8_router_->GetIsolate());
  response.Set("treeId", child_tree_id.ToString());
  response.Set("nodeId", child_node_id);
  args.GetReturnValue().Set(response.Build());
}

void AutomationV8Bindings::CreateAutomationPosition(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = automation_v8_router_->GetIsolate();
  if (args.Length() < 5 || !args[0]->IsString() /* tree id */ ||
      !args[1]->IsInt32() /* node id */ || !args[2]->IsString() /* type */ ||
      !args[3]->IsInt32() /* offset */ ||
      !args[4]->IsBoolean() /* is upstream affinity */) {
    automation_v8_router_->ThrowInvalidArgumentsException();
  }

  AXTreeID tree_id =
      AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
  int node_id =
      args[1]->Int32Value(automation_v8_router_->GetContext()).ToChecked();

  AutomationAXTreeWrapper* tree_wrapper =
      automation_tree_manager_owner_->GetAutomationAXTreeWrapperFromTreeID(
          tree_id);
  if (!tree_wrapper)
    return;

  AXNode* node = tree_wrapper->ax_tree()->GetFromId(node_id);
  if (!node)
    return;

  AXPositionKind kind =
      StringToAXPositionKind(*v8::String::Utf8Value(isolate, args[2]));
  int offset =
      args[3]->Int32Value(automation_v8_router_->GetContext()).ToChecked();
  bool is_upstream = args[3]->BooleanValue(isolate);

  gin::Handle<AutomationPosition> handle = gin::CreateHandle(
      isolate, new AutomationPosition(*node, kind, offset, is_upstream));
  args.GetReturnValue().Set(handle.ToV8().As<v8::Object>());
}

void AutomationV8Bindings::DestroyAccessibilityTree(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    automation_v8_router_->ThrowInvalidArgumentsException();
    return;
  }

  AXTreeID tree_id =
      AXTreeID::FromString(*v8::String::Utf8Value(args.GetIsolate(), args[0]));
  automation_tree_manager_owner_->DestroyAccessibilityTree(tree_id);
}

void AutomationV8Bindings::AddTreeChangeObserver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2 || !args[0]->IsNumber() || !args[1]->IsString()) {
    automation_v8_router_->ThrowInvalidArgumentsException();
    return;
  }

  int id =
      args[0]->Int32Value(automation_v8_router_->GetContext()).FromMaybe(0);
  std::string filter_str = *v8::String::Utf8Value(args.GetIsolate(), args[1]);
  automation_tree_manager_owner_->AddTreeChangeObserver(
      id, automation_v8_router_->ParseTreeChangeObserverFilter(filter_str));
}

void AutomationV8Bindings::RemoveTreeChangeObserver(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // The argument is an integer key for an object which is automatically
  // converted to a string.
  if (args.Length() != 1 || !args[0]->IsString()) {
    automation_v8_router_->ThrowInvalidArgumentsException();
    return;
  }

  int observer_id =
      args[0]->Int32Value(automation_v8_router_->GetContext()).FromMaybe(0);
  automation_tree_manager_owner_->RemoveTreeChangeObserver(observer_id);
}

void AutomationV8Bindings::GetParentID(v8::Isolate* isolate,
                                       v8::ReturnValue<v8::Value> result,
                                       AutomationAXTreeWrapper* tree_wrapper,
                                       AXNode* node) const {
  AXNode* parent =
      automation_tree_manager_owner_->GetParent(node, &tree_wrapper);
  if (parent) {
    gin::DataObjectBuilder response(isolate);
    response.Set("treeId", tree_wrapper->GetTreeID().ToString());
    response.Set("nodeId", parent->id());
    result.Set(response.Build());
  }
}

void AutomationV8Bindings::GetChildCount(v8::Isolate* isolate,
                                         v8::ReturnValue<v8::Value> result,
                                         AutomationAXTreeWrapper* tree_wrapper,
                                         AXNode* node) const {
  size_t child_count = automation_tree_manager_owner_->GetChildCount(node);
  result.Set(v8::Integer::New(isolate, static_cast<int32_t>(child_count)));
}
void AutomationV8Bindings::GetLocation(v8::Isolate* isolate,
                                       v8::ReturnValue<v8::Value> result,
                                       AutomationAXTreeWrapper* tree_wrapper,
                                       AXNode* node) const {
  gfx::Rect global_clipped_bounds =
      automation_tree_manager_owner_->ComputeGlobalNodeBounds(tree_wrapper,
                                                              node);
  result.Set(RectToV8Object(isolate, global_clipped_bounds));
}
void AutomationV8Bindings::GetUnclippedLocation(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node) const {
  bool offscreen = false;
  gfx::Rect global_unclipped_bounds =
      automation_tree_manager_owner_->ComputeGlobalNodeBounds(
          tree_wrapper, node, gfx::RectF(), &offscreen,
          false /* clip_bounds */);
  result.Set(RectToV8Object(isolate, global_unclipped_bounds));
}

void AutomationV8Bindings::GetChildIDs(v8::Isolate* isolate,
                                       v8::ReturnValue<v8::Value> result,
                                       AutomationAXTreeWrapper* tree_wrapper,
                                       AXNode* node) const {
  AXTreeID tree_id;
  std::vector<int> child_ids =
      automation_tree_manager_owner_->GetChildIDs(node, &tree_id);
  gin::DataObjectBuilder response(isolate);
  response.Set("treeId", tree_id.ToString());
  response.Set("nodeIds", child_ids);
  result.Set(response.Build());
}

void AutomationV8Bindings::GetSentenceStartOffsets(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node) const {
  const std::vector<int>& sentence_starts =
      automation_tree_manager_owner_->CalculateSentenceBoundary(
          tree_wrapper, node, true /* start_boundary */);
  result.Set(gin::ConvertToV8(isolate, sentence_starts));
}

void AutomationV8Bindings::GetSentenceEndOffsets(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node) const {
  const std::vector<int>& sentence_ends =
      automation_tree_manager_owner_->CalculateSentenceBoundary(
          tree_wrapper, node, false /* start_boundary */);
  result.Set(gin::ConvertToV8(isolate, sentence_ends));
}
void AutomationV8Bindings::GetBoundsForRange(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    int start,
    int end,
    bool clipped) const {
  gfx::Rect global_bounds;
  if (!automation_tree_manager_owner_->GetBoundsForRange(
          tree_wrapper, node, start, end, clipped, &global_bounds)) {
    return;
  }
  result.Set(RectToV8Object(isolate, global_bounds));
}
void AutomationV8Bindings::ComputeGlobalBounds(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    int x,
    int y,
    int width,
    int height) const {
  gfx::RectF local_bounds(x, y, width, height);

  // Convert from local coordinates in Android window, to global
  // coordinates spanning entire screen.
  gfx::Rect global_bounds =
      automation_tree_manager_owner_->ComputeGlobalNodeBounds(
          tree_wrapper, node, local_bounds, nullptr, false /* clip_bounds */);
  result.Set(RectToV8Object(isolate, global_bounds));
}

void AutomationV8Bindings::GetName(v8::Isolate* isolate,
                                   v8::ReturnValue<v8::Value> result,
                                   AutomationAXTreeWrapper* tree_wrapper,
                                   AXNode* node) const {
  const char* name = automation_tree_manager_owner_->GetName(node);
  if (name)
    result.Set(v8::String::NewFromUtf8(isolate, name).ToLocalChecked());
}

void AutomationV8Bindings::GetNextTextMatch(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    const std::string& search_str,
    bool backward) const {
  AXTreeID tree_id;
  int node_id;
  if (!automation_tree_manager_owner_->GetNextTextMatch(
          tree_wrapper, node, search_str, backward, &tree_id, &node_id)) {
    return;
  }
  gin::DataObjectBuilder response(isolate);
  response.Set("treeId", tree_id.ToString());
  response.Set("nodeId", node_id);
  result.Set(response.Build());
}

void AutomationV8Bindings::SetAccessibilityFocus(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node) {
  AXTreeID tree_id = tree_wrapper->GetTreeID();
  automation_tree_manager_owner_->SetAccessibilityFocus(tree_id);
  tree_wrapper->SetAccessibilityFocus(node->id());
}
void AutomationV8Bindings::EventListenerAdded(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type) {
  tree_wrapper->EventListenerAdded(event_type, node);
  automation_tree_manager_owner_->TreeEventListenersChanged(tree_wrapper);
}

void AutomationV8Bindings::EventListenerRemoved(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node,
    const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type) {
  tree_wrapper->EventListenerRemoved(event_type, node);
  automation_tree_manager_owner_->TreeEventListenersChanged(tree_wrapper);
}

void AutomationV8Bindings::GetMarkers(v8::Isolate* isolate,
                                      v8::ReturnValue<v8::Value> result,
                                      AutomationAXTreeWrapper* tree_wrapper,
                                      AXNode* node) const {
  if (!node->HasIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts) ||
      !node->HasIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds) ||
      !node->HasIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes)) {
    return;
  }

  const std::vector<int32_t>& marker_starts =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
  const std::vector<int32_t>& marker_ends =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
  const std::vector<int32_t>& marker_types =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);

  v8::LocalVector<v8::Object> markers(isolate);
  for (size_t i = 0; i < marker_types.size(); ++i) {
    gin::DataObjectBuilder marker_obj(isolate);
    marker_obj.Set("startOffset", marker_starts[i]);
    marker_obj.Set("endOffset", marker_ends[i]);

    gin::DataObjectBuilder flags(isolate);
    int32_t marker_type = marker_types[i];
    int32_t marker_pos = 1;
    while (marker_type) {
      if (marker_type & 1) {
        flags.Set(automation_v8_router_->GetMarkerTypeString(
                      static_cast<ax::mojom::MarkerType>(marker_pos)),
                  true);
      }
      marker_type = marker_type >> 1;
      marker_pos = marker_pos << 1;
    }

    marker_obj.Set("flags", flags.Build());
    markers.push_back(marker_obj.Build());
  }

  result.Set(gin::ConvertToV8(isolate, markers));
}

void AutomationV8Bindings::GetState(
    const v8::FunctionCallbackInfo<v8::Value>& args) const {
  v8::Isolate* isolate = automation_v8_router_->GetIsolate();
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber())
    automation_v8_router_->ThrowInvalidArgumentsException();

  AXTreeID tree_id =
      AXTreeID::FromString(*v8::String::Utf8Value(isolate, args[0]));
  int node_id =
      args[1]->Int32Value(automation_v8_router_->GetContext()).FromMaybe(0);

  bool offscreen = false;
  bool focused = false;
  uint32_t node_state;
  if (!automation_tree_manager_owner_->CalculateNodeState(
          tree_id, node_id, &node_state, &offscreen, &focused)) {
    return;
  }

  gin::DataObjectBuilder state(isolate);
  uint32_t state_pos = 0, state_shifter = node_state;
  while (state_shifter) {
    if (state_shifter & 1)
      state.Set(ToString(static_cast<ax::mojom::State>(state_pos)), true);
    state_shifter = state_shifter >> 1;
    state_pos++;
  }

  if (focused) {
    state.Set(automation_v8_router_->GetFocusedStateString(), true);
  }
  if (offscreen)
    state.Set(automation_v8_router_->GetOffscreenStateString(), true);

  args.GetReturnValue().Set(state.Build());
}

void AutomationV8Bindings::GetImageAnnotation(
    v8::Isolate* isolate,
    v8::ReturnValue<v8::Value> result,
    AutomationAXTreeWrapper* tree_wrapper,
    AXNode* node) const {
  std::string status_string = std::string();
  auto status = node->data().GetImageAnnotationStatus();
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      break;

    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      status_string =
          automation_v8_router_->GetLocalizedStringForImageAnnotationStatus(
              status);
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      status_string = node->GetStringAttribute(
          ax::mojom::StringAttribute::kImageAnnotation);
      break;
  }
  if (status_string.empty())
    return;
  result.Set(
      v8::String::NewFromUtf8(isolate, status_string.c_str()).ToLocalChecked());
}

void AutomationV8Bindings::StartCachingAccessibilityTrees(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  automation_v8_router_->StartCachingAccessibilityTrees();
}

void AutomationV8Bindings::StopCachingAccessibilityTrees(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  automation_v8_router_->StopCachingAccessibilityTrees();
  automation_tree_manager_owner_->ClearCachedAccessibilityTrees();
}

}  // namespace ui
