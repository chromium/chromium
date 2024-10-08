// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/ax_platform_node_win.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "base/values.h"
#include "base/win/enum_variant.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "base/win/variant_vector.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_constants.mojom.h"
#include "ui/accessibility/ax_enum_localization_util.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_utils_win.h"
#include "ui/accessibility/platform/ax_platform_node_textchildprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"
#include "ui/accessibility/platform/ax_platform_relation_win.h"
#include "ui/accessibility/platform/child_iterator.h"
#include "ui/accessibility/platform/compute_attributes.h"
#include "ui/accessibility/platform/uia_registrar_win.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/atl_module.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/strings/grit/ax_strings.h"

//
// Macros to use at the top of any AXPlatformNodeWin function that implements
// a non-UIA COM interface. Because COM objects are reference counted and
// clients are completely untrusted, it's important to always first check that
// our object is still valid, and then check that all pointer arguments are not
// NULL.
//
#define COM_OBJECT_VALIDATE() \
  if (!GetDelegate())         \
    return E_FAIL;
#define COM_OBJECT_VALIDATE_1_ARG(arg) \
  if (!GetDelegate())                  \
    return E_FAIL;                     \
  if (!arg)                            \
    return E_INVALIDARG;               \
  *arg = {};
#define COM_OBJECT_VALIDATE_2_ARGS(arg1, arg2) \
  if (!GetDelegate())                          \
    return E_FAIL;                             \
  if (!arg1)                                   \
    return E_INVALIDARG;                       \
  *arg1 = {};                                  \
  if (!arg2)                                   \
    return E_INVALIDARG;                       \
  *arg2 = {};
#define COM_OBJECT_VALIDATE_3_ARGS(arg1, arg2, arg3) \
  if (!GetDelegate())                                \
    return E_FAIL;                                   \
  if (!arg1)                                         \
    return E_INVALIDARG;                             \
  *arg1 = {};                                        \
  if (!arg2)                                         \
    return E_INVALIDARG;                             \
  *arg2 = {};                                        \
  if (!arg3)                                         \
    return E_INVALIDARG;                             \
  *arg3 = {};
#define COM_OBJECT_VALIDATE_4_ARGS(arg1, arg2, arg3, arg4) \
  if (!GetDelegate())                                      \
    return E_FAIL;                                         \
  if (!arg1)                                               \
    return E_INVALIDARG;                                   \
  *arg1 = {};                                              \
  if (!arg2)                                               \
    return E_INVALIDARG;                                   \
  *arg2 = {};                                              \
  if (!arg3)                                               \
    return E_INVALIDARG;                                   \
  *arg3 = {};                                              \
  if (!arg4)                                               \
    return E_INVALIDARG;                                   \
  *arg4 = {};
#define COM_OBJECT_VALIDATE_5_ARGS(arg1, arg2, arg3, arg4, arg5) \
  if (!GetDelegate())                                            \
    return E_FAIL;                                               \
  if (!arg1)                                                     \
    return E_INVALIDARG;                                         \
  *arg1 = {};                                                    \
  if (!arg2)                                                     \
    return E_INVALIDARG;                                         \
  *arg2 = {};                                                    \
  if (!arg3)                                                     \
    return E_INVALIDARG;                                         \
  *arg3 = {};                                                    \
  if (!arg4)                                                     \
    return E_INVALIDARG;                                         \
  *arg4 = {};                                                    \
  if (!arg5)                                                     \
    return E_INVALIDARG;                                         \
  *arg5 = {};
#define COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_id, target) \
  if (!GetDelegate())                                             \
    return E_FAIL;                                                \
  target = GetTargetFromChildID(var_id);                          \
  if (!target)                                                    \
    return E_INVALIDARG;                                          \
  if (!target->GetDelegate())                                     \
    return E_INVALIDARG;
#define COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, arg, target) \
  if (!GetDelegate())                                                        \
    return E_FAIL;                                                           \
  if (!arg)                                                                  \
    return E_INVALIDARG;                                                     \
  *arg = {};                                                                 \
  target = GetTargetFromChildID(var_id);                                     \
  if (!target)                                                               \
    return E_INVALIDARG;                                                     \
  if (!target->GetDelegate())                                                \
    return E_INVALIDARG;
#define COM_OBJECT_VALIDATE_VAR_ID_2_ARGS_AND_GET_TARGET(var_id, arg1, arg2, \
                                                         target)             \
  if (!GetDelegate())                                                        \
    return E_FAIL;                                                           \
  if (!arg1)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg1 = {};                                                                \
  if (!arg2)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg2 = {};                                                                \
  target = GetTargetFromChildID(var_id);                                     \
  if (!target)                                                               \
    return E_INVALIDARG;                                                     \
  if (!target->GetDelegate())                                                \
    return E_INVALIDARG;
#define COM_OBJECT_VALIDATE_VAR_ID_3_ARGS_AND_GET_TARGET(var_id, arg1, arg2, \
                                                         arg3, target)       \
  if (!GetDelegate())                                                        \
    return E_FAIL;                                                           \
  if (!arg1)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg1 = {};                                                                \
  if (!arg2)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg2 = {};                                                                \
  if (!arg3)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg3 = {};                                                                \
  target = GetTargetFromChildID(var_id);                                     \
  if (!target)                                                               \
    return E_INVALIDARG;                                                     \
  if (!target->GetDelegate())                                                \
    return E_INVALIDARG;
#define COM_OBJECT_VALIDATE_VAR_ID_4_ARGS_AND_GET_TARGET(var_id, arg1, arg2, \
                                                         arg3, arg4, target) \
  if (!GetDelegate())                                                        \
    return E_FAIL;                                                           \
  if (!arg1)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg1 = {};                                                                \
  if (!arg2)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg2 = {};                                                                \
  if (!arg3)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg3 = {};                                                                \
  if (!arg4)                                                                 \
    return E_INVALIDARG;                                                     \
  *arg4 = {};                                                                \
  target = GetTargetFromChildID(var_id);                                     \
  if (!target)                                                               \
    return E_INVALIDARG;                                                     \
  if (!target->GetDelegate())                                                \
    return E_INVALIDARG;

namespace ui {

namespace {

typedef std::unordered_set<AXPlatformNodeWin*> AXPlatformNodeWinSet;
// Set of all AXPlatformNodeWin objects that were the target of an
// alert event.
base::LazyInstance<AXPlatformNodeWinSet>::Leaky g_alert_targets =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<
    base::ObserverList<WinAccessibilityAPIUsageObserver>::Unchecked>::Leaky
    g_win_accessibility_api_usage_observer_list = LAZY_INSTANCE_INITIALIZER;

// Sets the multiplier by which large changes to a RangeValueProvider are
// greater than small changes.
constexpr int kLargeChangeScaleFactor = 10;

// Sets the default small change amount for a RangeValueProvider when no
// step was set on the element. Note: This should be in-sync with the native
// default value, defined by the default constructor of:
//
//   third_party/blink/renderer/core/html/forms/step_range.cc
//
constexpr float kDefaultSmallChangeValue = 1.0f;

// The amount to scroll when UI Automation asks to scroll by a small increment.
// Value is in device independent pixels and is the same used by Blink when
// cursor keys are used to scroll a webpage.
constexpr float kSmallScrollIncrement = 40.0f;

// Helper function to GetPatternProviderFactoryMethod that, given a node,
// will return a pattern interface through result based on the provided type T.
template <typename T>
void PatternProvider(AXPlatformNodeWin* node, IUnknown** result) {
  node->AddRef();
  *result = static_cast<T*>(node);
}

}  // namespace

void AXPlatformNodeWin::AddAttributeToList(const char* name,
                                           const char* value,
                                           PlatformAttributeList* attributes) {
  std::string str_value = value;
  SanitizeStringAttribute(str_value, &str_value);
  attributes->push_back(base::UTF8ToWide(name) + L":" +
                        base::UTF8ToWide(str_value));
}

// This also sets kNativeAPIs and kWebContents to ensure we don't have an
// incorrect combination of AXModes.
const uint32_t kScreenReaderAccessibilityMode =
    AXMode::kNativeAPIs | AXMode::kWebContents | AXMode::kScreenReader;

//
// WinAccessibilityAPIUsageObserver
//

WinAccessibilityAPIUsageObserver::WinAccessibilityAPIUsageObserver() {}

WinAccessibilityAPIUsageObserver::~WinAccessibilityAPIUsageObserver() {}

// static
base::ObserverList<WinAccessibilityAPIUsageObserver>::Unchecked&
GetWinAccessibilityAPIUsageObserverList() {
  return g_win_accessibility_api_usage_observer_list.Get();
}

// Used to simplify calling StartFiringUIAEvents and EndFiringEvents
WinAccessibilityAPIUsageScopedUIAEventsNotifier::
    WinAccessibilityAPIUsageScopedUIAEventsNotifier() {
  GetWinAccessibilityAPIUsageObserverList().Notify(
      &WinAccessibilityAPIUsageObserver::StartFiringUIAEvents);
}
WinAccessibilityAPIUsageScopedUIAEventsNotifier::
    ~WinAccessibilityAPIUsageScopedUIAEventsNotifier() {
  GetWinAccessibilityAPIUsageObserverList().Notify(
      &WinAccessibilityAPIUsageObserver::EndFiringUIAEvents);
}

//
// AXPlatformNode::Create
//

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  // Make sure ATL is initialized in this module.
  win::CreateATLModuleIfNeeded();

  CComObject<AXPlatformNodeWin>* instance = nullptr;
  HRESULT hr = CComObject<AXPlatformNodeWin>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  instance->Init(delegate);
  instance->AddRef();
  return instance;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  if (!accessible)
    return nullptr;
  Microsoft::WRL::ComPtr<AXPlatformNodeWin> ax_platform_node;
  accessible->QueryInterface(IID_PPV_ARGS(&ax_platform_node));
  return ax_platform_node.Get();
}

//
// AXPlatformNodeWin
//

AXPlatformNodeWin::AXPlatformNodeWin() {}

AXPlatformNodeWin::~AXPlatformNodeWin() {
  ClearOwnRelations();
}

void AXPlatformNodeWin::Init(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeBase::Init(delegate);
}

void AXPlatformNodeWin::ClearOwnRelations() {
  for (size_t i = 0; i < relations_.size(); ++i)
    relations_[i]->Invalidate();
  relations_.clear();
}

// Static
void AXPlatformNodeWin::SanitizeStringAttributeForUIAAriaProperty(
    const std::wstring& input,
    std::wstring* output) {
  DCHECK(output);
  // According to the UIA Spec, these characters need to be escaped with a
  // backslash in an AriaProperties string: backslash, equals and semicolon.
  // Note that backslash must be replaced first.
  base::ReplaceChars(input, L"\\", L"\\\\", output);
  base::ReplaceChars(*output, L"=", L"\\=", output);
  base::ReplaceChars(*output, L";", L"\\;", output);
}

void AXPlatformNodeWin::StringAttributeToUIAAriaProperty(
    std::vector<std::wstring>& properties,
    ax::mojom::StringAttribute attribute,
    const char* uia_aria_property) {
  std::string value;
  if (GetStringAttribute(attribute, &value)) {
    std::wstring wide_value = base::UTF8ToWide(value);
    SanitizeStringAttributeForUIAAriaProperty(wide_value, &wide_value);
    properties.push_back(base::ASCIIToWide(uia_aria_property) + L"=" +
                         wide_value);
  }
}

void AXPlatformNodeWin::BoolAttributeToUIAAriaProperty(
    std::vector<std::wstring>& properties,
    ax::mojom::BoolAttribute attribute,
    const char* uia_aria_property) {
  bool value;
  if (GetBoolAttribute(attribute, &value)) {
    properties.push_back(base::ASCIIToWide(uia_aria_property) + L"=" +
                         (value ? L"true" : L"false"));
  }
}

void AXPlatformNodeWin::IntAttributeToUIAAriaProperty(
    std::vector<std::wstring>& properties,
    ax::mojom::IntAttribute attribute,
    const char* uia_aria_property) {
  int value;
  if (GetIntAttribute(attribute, &value)) {
    properties.push_back(base::ASCIIToWide(uia_aria_property) + L"=" +
                         base::NumberToWString(value));
  }
}

void AXPlatformNodeWin::FloatAttributeToUIAAriaProperty(
    std::vector<std::wstring>& properties,
    ax::mojom::FloatAttribute attribute,
    const char* uia_aria_property) {
  float value;
  if (GetFloatAttribute(attribute, &value)) {
    properties.push_back(base::ASCIIToWide(uia_aria_property) + L"=" +
                         base::NumberToWString(value));
  }
}

void AXPlatformNodeWin::StateToUIAAriaProperty(
    std::vector<std::wstring>& properties,
    ax::mojom::State state,
    const char* uia_aria_property) {
  bool value = HasState(state);
  properties.push_back(base::ASCIIToWide(uia_aria_property) + L"=" +
                       (value ? L"true" : L"false"));
}

std::vector<AXPlatformNodeWin*>
AXPlatformNodeWin::CreatePlatformNodeVectorFromRelationIdVector(
    const std::vector<int32_t>& relation_id_list) {
  std::vector<AXPlatformNodeWin*> platform_node_list;

  for (int32_t id : relation_id_list) {
    AXPlatformNode* platform_node = GetDelegate()->GetFromNodeID(id);
    if (IsValidUiaRelationTarget(platform_node)) {
      platform_node_list.push_back(
          static_cast<AXPlatformNodeWin*>(platform_node));
    }
  }

  return platform_node_list;
}

SAFEARRAY* AXPlatformNodeWin::CreateUIAElementsSafeArray(
    const std::vector<AXPlatformNodeWin*>& platform_node_list) {
  if (platform_node_list.empty())
    return nullptr;

  SAFEARRAY* uia_array =
      SafeArrayCreateVector(VT_UNKNOWN, 0, platform_node_list.size());
  LONG i = 0;

  for (AXPlatformNodeWin* platform_node : platform_node_list) {
    // All incoming ids should already be validated to have a valid relation
    // targets so that this function does not need to re-check before allocating
    // the SAFEARRAY.
    DCHECK(IsValidUiaRelationTarget(platform_node));
    SafeArrayPutElement(uia_array, &i,
                        static_cast<IRawElementProviderSimple*>(platform_node));
    ++i;
  }

  return uia_array;
}

SAFEARRAY* AXPlatformNodeWin::CreateUIAControllerForArray() {
  std::vector<int32_t> relation_id_list =
      GetIntListAttribute(ax::mojom::IntListAttribute::kControlsIds);

  std::vector<AXPlatformNodeWin*> platform_node_list =
      CreatePlatformNodeVectorFromRelationIdVector(relation_id_list);

  if (GetActivePopupAxUniqueId() != std::nullopt) {
    AXPlatformNodeWin* view_popup_node_win = static_cast<AXPlatformNodeWin*>(
        GetFromUniqueId(GetActivePopupAxUniqueId().value()));

    if (IsValidUiaRelationTarget(view_popup_node_win))
      platform_node_list.push_back(view_popup_node_win);
  }

  // The aria-errormessage attribute (mapped to the kErrormessageIds) is
  // expected to be exposed through the ControllerFor property on UIA:
  // https://w3c.github.io/core-aam/#ariaErrorMessage
  if (HasIntListAttribute(ax::mojom::IntListAttribute::kErrormessageIds)) {
    std::vector<int32_t> error_message_node_ids =
        GetIntListAttribute(ax::mojom::IntListAttribute::kErrormessageIds);
    for (int32_t id : error_message_node_ids) {
      AXPlatformNodeWin* error_message_node_win =
          static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(id));
      if (IsValidUiaRelationTarget(error_message_node_win)) {
        platform_node_list.push_back(error_message_node_win);
      }
    }
  }

  return CreateUIAElementsSafeArray(platform_node_list);
}

SAFEARRAY* AXPlatformNodeWin::CreateUIAElementsArrayForRelation(
    const ax::mojom::IntListAttribute& attribute) {
  return CreateUIAElementsSafeArray(
      CreatePlatformNodeVectorFromRelationIdVector(
          GetIntListAttribute(attribute)));
}

SAFEARRAY* AXPlatformNodeWin::CreateUIAElementsArrayForReverseRelation(
    const ax::mojom::IntListAttribute& attribute) {
  std::vector<AXPlatformNode*> reverse_relations =
      GetDelegate()->GetSourceNodesForReverseRelations(attribute);

  std::vector<int32_t> id_list;
  base::ranges::transform(
      reverse_relations, std::back_inserter(id_list),
      [](AXPlatformNode* platform_node) {
        return static_cast<AXPlatformNodeWin*>(platform_node)->GetData().id;
      });

  return CreateUIAElementsSafeArray(
      CreatePlatformNodeVectorFromRelationIdVector(id_list));
}

SAFEARRAY* AXPlatformNodeWin::CreateClickablePointArray() {
  SAFEARRAY* clickable_point_array = SafeArrayCreateVector(VT_R8, 0, 2);
  gfx::Point center = GetDelegate()
                          ->GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                                          AXClippingBehavior::kUnclipped)
                          .CenterPoint();

  double* double_array;
  SafeArrayAccessData(clickable_point_array,
                      reinterpret_cast<void**>(&double_array));
  double_array[0] = center.x();
  double_array[1] = center.y();
  SafeArrayUnaccessData(clickable_point_array);

  return clickable_point_array;
}

gfx::Vector2d AXPlatformNodeWin::CalculateUIAScrollPoint(
    const ScrollAmount horizontal_amount,
    const ScrollAmount vertical_amount) const {
  if (!GetDelegate() || !IsScrollable())
    return {};

  const gfx::Rect bounds = GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreenDIPs, AXClippingBehavior::kClipped);
  const int large_horizontal_change = bounds.width();
  const int large_vertical_change = bounds.height();

  const HWND hwnd = GetDelegate()->GetTargetForNativeAccessibilityEvent();
  DCHECK(hwnd);
  const float scale_factor =
      display::win::ScreenWin::GetScaleFactorForHWND(hwnd);
  const int small_change =
      base::ClampRound(kSmallScrollIncrement * scale_factor);

  const int x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  const int x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  const int y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  const int y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);

  int x = GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  int y = GetIntAttribute(ax::mojom::IntAttribute::kScrollY);

  switch (horizontal_amount) {
    case ScrollAmount_LargeDecrement:
      x -= large_horizontal_change;
      break;
    case ScrollAmount_LargeIncrement:
      x += large_horizontal_change;
      break;
    case ScrollAmount_NoAmount:
      break;
    case ScrollAmount_SmallDecrement:
      x -= small_change;
      break;
    case ScrollAmount_SmallIncrement:
      x += small_change;
      break;
  }
  x = std::min(x, x_max);
  x = std::max(x, x_min);

  switch (vertical_amount) {
    case ScrollAmount_LargeDecrement:
      y -= large_vertical_change;
      break;
    case ScrollAmount_LargeIncrement:
      y += large_vertical_change;
      break;
    case ScrollAmount_NoAmount:
      break;
    case ScrollAmount_SmallDecrement:
      y -= small_change;
      break;
    case ScrollAmount_SmallIncrement:
      y += small_change;
      break;
  }
  y = std::min(y, y_max);
  y = std::max(y, y_min);

  return {x, y};
}

//
// AXPlatformNodeBase implementation.
//

void AXPlatformNodeWin::Dispose() {
  Release();
}

void AXPlatformNodeWin::Destroy() {
  RemoveAlertTarget();

  // This will end up calling Dispose() which may result in deleting this object
  // if there are no more outstanding references.
  AXPlatformNodeBase::Destroy();
}

//
// AXPlatformNode implementation.
//

gfx::NativeViewAccessible AXPlatformNodeWin::GetNativeViewAccessible() {
  return this;
}

void AXPlatformNodeWin::NotifyAccessibilityEvent(ax::mojom::Event event_type) {
  TRACE_EVENT("accessibility", "NotifyAccessibilityEvent",
              perfetto::Flow::FromPointer(this), "event_type",
              base::NumberToString(static_cast<int32_t>(event_type)));
  AXPlatformNodeBase::NotifyAccessibilityEvent(event_type);
  // Menu items fire selection events but Windows screen readers work reliably
  // with focus events. Remap here.
  if (event_type == ax::mojom::Event::kSelection) {
    // A menu item could have something other than a role of
    // |ROLE_SYSTEM_MENUITEM|. Zoom modification controls for example have a
    // role of button.
    if (int role = MSAARole(); role == ROLE_SYSTEM_MENUITEM) {
      event_type = ax::mojom::Event::kFocus;
    } else if (role == ROLE_SYSTEM_LISTITEM) {
      if (const AXPlatformNodeBase* container = GetSelectionContainer()) {
        if (container->GetRole() == ax::mojom::Role::kListBox &&
            !container->HasState(ax::mojom::State::kMultiselectable) &&
            GetDelegate()->GetFocus() == GetNativeViewAccessible()) {
          event_type = ax::mojom::Event::kFocus;
        }
      }
    } else if (auto* parent = GetParentPlatformNodeWin(); parent) {
      if (int parent_role = parent->MSAARole();
          parent_role == ROLE_SYSTEM_MENUPOPUP ||
          parent_role == ROLE_SYSTEM_LIST) {
        event_type = ax::mojom::Event::kFocus;
      }
    }
  }

  // TODO(benjamin.beaudry): Uncomment DCHECK once https://crbug.com/331840469
  // is fixed.
  // DCHECK(event_type != ax::mojom::Event::kLiveRegionChanged ||
  //        GetDelegate()->IsWebContent() || IsUIAControl())
  //     << "For views, the LiveRegionChanged event should only be fired on
  //     nodes that are UIA controls.";

  if (event_type == ax::mojom::Event::kValueChanged ||
      event_type == ax::mojom::Event::kLiveRegionCreated ||
      event_type == ax::mojom::Event::kLiveRegionChanged) {
    // For the IAccessibleText interface to work on non-web content nodes, we
    // need to update the nodes' hypertext
    // when the value changes. Otherwise, for web and PDF content, this is
    // handled by "BrowserAccessibilityComWin".
    if (!GetDelegate()->IsWebContent())
      UpdateComputedHypertext();
  }

  if (std::optional<DWORD> native_event = MojoEventToMSAAEvent(event_type)) {
    HWND hwnd = GetDelegate()->GetTargetForNativeAccessibilityEvent();
    if (!hwnd)
      return;

    TRACE_EVENT("accessibility", "NotifyWinEvent", "native_event",
                base::StringPrintf("0x%04lX", native_event.value()));
    ::NotifyWinEvent((*native_event), hwnd, OBJID_CLIENT, -GetUniqueId());
  }

  if (std::optional<PROPERTYID> uia_property =
          MojoEventToUIAProperty(event_type)) {
    // For this event, we're not concerned with the old value.
    base::win::ScopedVariant old_value;
    ::VariantInit(old_value.Receive());
    base::win::ScopedVariant new_value;
    ::VariantInit(new_value.Receive());
    GetPropertyValueImpl((*uia_property), new_value.Receive());
    ::UiaRaiseAutomationPropertyChangedEvent(this, (*uia_property), old_value,
                                             new_value);
  }

  if (std::optional<EVENTID> uia_event = MojoEventToUIAEvent(event_type)) {
    ::UiaRaiseAutomationEvent(this, (*uia_event));
  }

  // Keep track of objects that are a target of an alert event.
  if (event_type == ax::mojom::Event::kAlert)
    AddAlertTarget();
}

bool AXPlatformNodeWin::HasActiveComposition() const {
  return active_composition_range_.end() > active_composition_range_.start();
}

gfx::Range AXPlatformNodeWin::GetActiveCompositionOffsets() const {
  return active_composition_range_;
}

void AXPlatformNodeWin::OnActiveComposition(
    const gfx::Range& range,
    const std::u16string& active_composition_text,
    bool is_composition_committed) {
  // Cache the composition range that will be used when
  // GetActiveComposition and GetConversionTarget is called in
  // AXPlatformNodeTextProviderWin
  active_composition_range_ = range;
  // Fire the UiaTextEditTextChangedEvent
  FireUiaTextEditTextChangedEvent(range,
                                  base::UTF16ToWide(active_composition_text),
                                  is_composition_committed);
}

void AXPlatformNodeWin::FireUiaTextEditTextChangedEvent(
    const gfx::Range& range,
    const std::wstring& active_composition_text,
    bool is_composition_committed) {
  if (!AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    return;
  }

  // This API is only supported from Win8.1 onwards
  // Check if the function pointer is valid or not
  using UiaRaiseTextEditTextChangedEventFunction = HRESULT(WINAPI*)(
      IRawElementProviderSimple*, TextEditChangeType, SAFEARRAY*);
  UiaRaiseTextEditTextChangedEventFunction text_edit_text_changed_func =
      reinterpret_cast<UiaRaiseTextEditTextChangedEventFunction>(
          ::GetProcAddress(GetModuleHandle(L"uiautomationcore.dll"),
                           "UiaRaiseTextEditTextChangedEvent"));
  if (!text_edit_text_changed_func) {
    return;
  }

  TextEditChangeType text_edit_change_type =
      is_composition_committed ? TextEditChangeType_CompositionFinalized
                               : TextEditChangeType_Composition;
  // Composition has been finalized by TSF
  base::win::ScopedBstr composition_text(active_composition_text.c_str());
  base::win::ScopedSafearray changed_data(
      SafeArrayCreateVector(VT_BSTR /* element type */, 0 /* lower bound */,
                            1 /* number of elements */));
  if (!changed_data.Get()) {
    return;
  }

  LONG index = 0;
  HRESULT hr =
      SafeArrayPutElement(changed_data.Get(), &index, composition_text.Get());

  if (FAILED(hr)) {
    return;
  } else {
    // Fire the UiaRaiseTextEditTextChangedEvent
    text_edit_text_changed_func(this, text_edit_change_type,
                                changed_data.Release());
  }
}

bool AXPlatformNodeWin::IsValidUiaRelationTarget(
    AXPlatformNode* ax_platform_node) {
  if (!ax_platform_node)
    return false;
  if (!ax_platform_node->GetDelegate())
    return false;

  // This is needed for get_FragmentRoot.
  if (!ax_platform_node->GetDelegate()->GetTargetForNativeAccessibilityEvent())
    return false;

  return true;
}

AXPlatformNodeWin::UIARoleProperties AXPlatformNodeWin::GetUIARoleProperties() {
  // If this is a web area for a presentational iframe, give it a role of
  // something other than document so that the fact that it's a separate doc
  // is not exposed to AT.
  if (GetDelegate()->IsRootWebAreaForPresentationalIframe())
    return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId, L"group"};

  // See UIARoleProperties for descriptions of the properties.
  switch (GetRole()) {
    case ax::mojom::Role::kAlert:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_TextControlTypeId,
              L"alert"};
    case ax::mojom::Role::kAlertDialog:
      // Our MSAA implementation suggests the use of
      // "alert", not "alertdialog" because some
      // Windows screen readers are not compatible with
      // |ax::mojom::Role::kAlertDialog| yet.
      return {UIALocalizationStrategy::kSupply, UIA_WindowControlTypeId,
              L"alert"};

    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kSuggestion:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kApplication:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_PaneControlTypeId,
              L"application"};

    case ax::mojom::Role::kArticle:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_GroupControlTypeId,
              L"article"};

    case ax::mojom::Role::kAudio:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"banner"};

    case ax::mojom::Role::kBlockquote:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kButton:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ButtonControlTypeId, L"button"};

    case ax::mojom::Role::kCanvas:
      return {UIALocalizationStrategy::kSupply, UIA_ImageControlTypeId, L"img"};

    case ax::mojom::Role::kCaption:
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
              L"description"};

    case ax::mojom::Role::kCaret:
      return {UIALocalizationStrategy::kSupply, UIA_PaneControlTypeId,
              L"region"};

    case ax::mojom::Role::kCell:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_DataItemControlTypeId, L"gridcell"};

    case ax::mojom::Role::kCode:
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId, L"code"};

    case ax::mojom::Role::kCheckBox:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_CheckBoxControlTypeId, L"checkbox"};

    case ax::mojom::Role::kClient:
      return {UIALocalizationStrategy::kSupply, UIA_PaneControlTypeId,
              L"region"};

    case ax::mojom::Role::kColorWell:
      return {UIALocalizationStrategy::kSupply, UIA_ButtonControlTypeId,
              L"textbox"};

    case ax::mojom::Role::kColumn:
      return {UIALocalizationStrategy::kSupply, UIA_PaneControlTypeId,
              L"region"};

    case ax::mojom::Role::kColumnHeader:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_DataItemControlTypeId, L"columnheader"};

    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
      return {UIALocalizationStrategy::kSupply, UIA_ComboBoxControlTypeId,
              L"combobox"};

    case ax::mojom::Role::kComboBoxSelect:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ComboBoxControlTypeId, L"combobox"};

    case ax::mojom::Role::kComplementary:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_GroupControlTypeId,
              L"complementary"};

    case ax::mojom::Role::kContentDeletion:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"deletion"};

    case ax::mojom::Role::kContentInsertion:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"insertion"};

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"contentinfo"};

    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      return {UIALocalizationStrategy::kSupply, UIA_EditControlTypeId,
              L"textbox"};

    case ax::mojom::Role::kDefinition:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_GroupControlTypeId,
              L"definition"};

    case ax::mojom::Role::kDescriptionList:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ListControlTypeId, L"list"};

    case ax::mojom::Role::kDesktop:
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"document"};

    case ax::mojom::Role::kDetails:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kDialog:
      return {UIALocalizationStrategy::kDeferToAriaRole,
              UIA_WindowControlTypeId, L"dialog"};

    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kDisclosureTriangleGrouped:
      if (::features::IsAccessibilityExposeSummaryAsHeadingEnabled()) {
        return {UIALocalizationStrategy::kSupply, UIA_ButtonControlTypeId,
                L"heading"};
      } else {
        return {UIALocalizationStrategy::kSupply, UIA_ButtonControlTypeId,
                L"button"};
      }
    case ax::mojom::Role::kDocCover:
      return {UIALocalizationStrategy::kSupply, UIA_ImageControlTypeId, L"img"};

    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return {UIALocalizationStrategy::kSupply, UIA_HyperlinkControlTypeId,
              L"link"};

    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
    case ax::mojom::Role::kDocFootnote:
      return {UIALocalizationStrategy::kSupply, UIA_ListItemControlTypeId,
              L"listitem"};

    case ax::mojom::Role::kDocPageBreak:
      return {UIALocalizationStrategy::kSupply, UIA_SeparatorControlTypeId,
              L"separator"};

    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIndex:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocNotice:
    case ax::mojom::Role::kDocPageFooter:
    case ax::mojom::Role::kDocPageHeader:
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
    case ax::mojom::Role::kDocSubtitle:
    case ax::mojom::Role::kDocTip:
    case ax::mojom::Role::kDocToc:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kDocument:
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"document"};
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kRootWebArea:
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"document"};

    case ax::mojom::Role::kEmbeddedObject:
      if (GetDelegate()->GetChildCount()) {
        return {UIALocalizationStrategy::kSupply, UIA_PaneControlTypeId,
                L"group"};
      }
      return {UIALocalizationStrategy::kSupply, UIA_PaneControlTypeId,
              L"document"};

    case ax::mojom::Role::kEmphasis:
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
              L"emphasis"};

    case ax::mojom::Role::kFeed:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"feed"};

    case ax::mojom::Role::kFigcaption:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"description"};

    case ax::mojom::Role::kFigure:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kForm:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"form"};

    case ax::mojom::Role::kGenericContainer:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kGraphicsDocument:
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"document"};

    case ax::mojom::Role::kGraphicsObject:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kGraphicsSymbol:
      return {UIALocalizationStrategy::kSupply, UIA_ImageControlTypeId, L"img"};

    case ax::mojom::Role::kGrid:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_DataGridControlTypeId, L"grid"};

    case ax::mojom::Role::kGridCell:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_DataItemControlTypeId, L"gridcell"};

    case ax::mojom::Role::kGroup:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_GroupControlTypeId, L"group"};

    case ax::mojom::Role::kHeading:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_TextControlTypeId,
              L"heading"};

    case ax::mojom::Role::kIframe:
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"document"};

    case ax::mojom::Role::kIframePresentational:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kImage:
      // We may want to expose additional details in the localized string such
      // as 'Unlabeled'
      if (IsImageWithMap()) {
        return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
                L"img"};
      }
      return {UIALocalizationStrategy::kSupply, UIA_ImageControlTypeId, L"img"};

    case ax::mojom::Role::kInputTime:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kInlineTextBox:
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"textbox"};

    case ax::mojom::Role::kLabelText:
          return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"description"};

    case ax::mojom::Role::kLegend:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"description"};

    case ax::mojom::Role::kLayoutTable:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_GroupControlTypeId, L"group"};

    case ax::mojom::Role::kLayoutTableCell:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_GroupControlTypeId, L"group"};

    case ax::mojom::Role::kLayoutTableRow:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kLink:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_HyperlinkControlTypeId, L"link"};

    case ax::mojom::Role::kList:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ListControlTypeId, L"list"};

    case ax::mojom::Role::kListBox:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_ListControlTypeId,
              L"listbox"};

    case ax::mojom::Role::kListBoxOption:
      return {UIALocalizationStrategy::kDeferToAriaRole,
              UIA_ListItemControlTypeId, L"option"};

    case ax::mojom::Role::kListGrid:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_DataGridControlTypeId, L"listview"};

    case ax::mojom::Role::kListItem:
      return {UIALocalizationStrategy::kSupply, UIA_ListItemControlTypeId,
              L"listitem"};

    case ax::mojom::Role::kListMarker:
      // Regular list markers only expose their alternative text, but do not
      // expose their descendants; and the descendants should be ignored. This
      // is because the alternative text depends on the counter style and can be
      // different from the actual (visual) marker text, and hence, inconsistent
      // with the descendants. We treat a list marker as non-text only if it
      // still has non-ignored descendants, which happens only when:
      // - The list marker itself is ignored but the descendants are not
      // - Or the list marker contains images
      if (!GetDelegate()->GetChildCount()) {
        return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
                L"description"};
      }
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
              L"group"};

    case ax::mojom::Role::kLog:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_GroupControlTypeId,
              L"log"};

    case ax::mojom::Role::kMain:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_GroupControlTypeId,
              L"main"};

    case ax::mojom::Role::kMark:
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
              L"description"};

    case ax::mojom::Role::kMarquee:
      return {UIALocalizationStrategy::kDeferToAriaRole, UIA_TextControlTypeId,
              L"marquee"};

    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMathMLMath:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    // TODO(http://crbug.com/1260585): Refine this if/when a UIA API exists for
    // properly exposing MathML content.
    case ax::mojom::Role::kMathMLFraction:
    case ax::mojom::Role::kMathMLIdentifier:
    case ax::mojom::Role::kMathMLMultiscripts:
    case ax::mojom::Role::kMathMLNoneScript:
    case ax::mojom::Role::kMathMLNumber:
    case ax::mojom::Role::kMathMLOperator:
    case ax::mojom::Role::kMathMLOver:
    case ax::mojom::Role::kMathMLPrescriptDelimiter:
    case ax::mojom::Role::kMathMLRoot:
    case ax::mojom::Role::kMathMLRow:
    case ax::mojom::Role::kMathMLSquareRoot:
    case ax::mojom::Role::kMathMLStringLiteral:
    case ax::mojom::Role::kMathMLSub:
    case ax::mojom::Role::kMathMLSubSup:
    case ax::mojom::Role::kMathMLSup:
    case ax::mojom::Role::kMathMLTable:
    case ax::mojom::Role::kMathMLTableCell:
    case ax::mojom::Role::kMathMLTableRow:
    case ax::mojom::Role::kMathMLText:
    case ax::mojom::Role::kMathMLUnder:
    case ax::mojom::Role::kMathMLUnderOver:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kMenu:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_MenuControlTypeId, L"menu"};

    case ax::mojom::Role::kMenuBar:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_MenuBarControlTypeId, L"menubar"};

    case ax::mojom::Role::kMenuItem:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_MenuItemControlTypeId, L"menuitem"};

    case ax::mojom::Role::kMenuItemCheckBox:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_CheckBoxControlTypeId, L"menuitemcheckbox"};

    case ax::mojom::Role::kMenuItemRadio:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_RadioButtonControlTypeId, L"menuitemradio"};

    case ax::mojom::Role::kMenuListPopup:
      return {UIALocalizationStrategy::kSupply, UIA_ListControlTypeId, L"list"};

    case ax::mojom::Role::kMenuListOption:
      return {UIALocalizationStrategy::kSupply, UIA_ListItemControlTypeId,
              L"listitem"};

    case ax::mojom::Role::kMeter:
      return {UIALocalizationStrategy::kSupply, UIA_ProgressBarControlTypeId,
              L"progressbar"};

    case ax::mojom::Role::kNavigation:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"navigation"};

    case ax::mojom::Role::kNote:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"note"};

    case ax::mojom::Role::kParagraph:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kPdfActionableHighlight:
      return {UIALocalizationStrategy::kSupply, UIA_CustomControlTypeId,
              L"button"};

    case ax::mojom::Role::kPluginObject:
      // UIA_DocumentControlTypeId
      if (GetDelegate()->GetChildCount()) {
        return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
                L"group"};
      }
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"document"};

    case ax::mojom::Role::kPopUpButton:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ButtonControlTypeId, L"button"};

    case ax::mojom::Role::kProgressIndicator:
      return {UIALocalizationStrategy::kSupply, UIA_ProgressBarControlTypeId,
              L"progressbar"};

    case ax::mojom::Role::kRadioButton:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_RadioButtonControlTypeId, L"radio"};

    case ax::mojom::Role::kRadioGroup:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"radiogroup"};

    case ax::mojom::Role::kRegion:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"region"};

    case ax::mojom::Role::kRow:
      // Role changes depending on whether row is inside a treegrid
      // https://www.w3.org/TR/core-aam-1.1/#role-map-row
      if (IsInTreeGrid()) {
        return {UIALocalizationStrategy::kDeferToControlType,
                UIA_TreeItemControlTypeId, L"treeitem"};
      }
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_DataItemControlTypeId, L"row"};

    case ax::mojom::Role::kRowGroup:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"rowgroup"};

    case ax::mojom::Role::kRowHeader:
      return {UIALocalizationStrategy::kSupply, UIA_DataItemControlTypeId,
              L"rowheader"};

    case ax::mojom::Role::kRuby:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kRubyAnnotation:
      // Generally exposed as description on <ruby> (Role::kRuby) element, not
      // as its own object in the tree.
      // However, it's possible to make a kRubyAnnotation element show up in the
      // AX tree, for example by adding tabindex="0" to the source <rp> or <rt>
      // element or making the source element the target of an aria-owns.
      // Therefore, browser side needs to gracefully handle it if it actually
      // shows up in the tree.
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
              L"description"};

    case ax::mojom::Role::kSection:
    case ax::mojom::Role::kSectionFooter:
    case ax::mojom::Role::kSectionHeader:
    case ax::mojom::Role::kSectionWithoutName:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kScrollBar:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ScrollBarControlTypeId, L"scrollbar"};

    case ax::mojom::Role::kScrollView:
      return {UIALocalizationStrategy::kSupply, UIA_PaneControlTypeId,
              L"region"};

    case ax::mojom::Role::kSearch:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"search"};

    case ax::mojom::Role::kSlider:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_SliderControlTypeId, L"slider"};

    case ax::mojom::Role::kSpinButton:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_SpinnerControlTypeId, L"spinbutton"};

    case ax::mojom::Role::kStrong:
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
              L"strong"};

    case ax::mojom::Role::kSwitch:
      return {UIALocalizationStrategy::kSupply, UIA_ButtonControlTypeId,
              L"switch"};

    case ax::mojom::Role::kStaticText:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_TextControlTypeId, L"description"};

    case ax::mojom::Role::kStatus:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_StatusBarControlTypeId, L"status"};

    case ax::mojom::Role::kSplitter:
      return {
          UIALocalizationStrategy::kDeferToControlType,
          IsFocusable() ? UIA_ThumbControlTypeId : UIA_SeparatorControlTypeId,
          L"separator"};

    case ax::mojom::Role::kSubscript:
    case ax::mojom::Role::kSuperscript:
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId,
              L"group"};

    case ax::mojom::Role::kSvgRoot:
      return {UIALocalizationStrategy::kSupply, UIA_ImageControlTypeId, L"img"};

    case ax::mojom::Role::kTab:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_TabItemControlTypeId, L"tab"};

    case ax::mojom::Role::kTable:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_TableControlTypeId, L"grid"};

    case ax::mojom::Role::kTableHeaderContainer:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kTabList:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_TabControlTypeId, L"tablist"};

    case ax::mojom::Role::kTabPanel:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_PaneControlTypeId, L"tabpanel"};

    case ax::mojom::Role::kTerm:
      return {UIALocalizationStrategy::kSupply, UIA_ListItemControlTypeId,
              L"listitem"};

    case ax::mojom::Role::kTitleBar:
      return {UIALocalizationStrategy::kSupply, UIA_TitleBarControlTypeId,
              L"group"};

    case ax::mojom::Role::kToggleButton:
      return {UIALocalizationStrategy::kSupply, UIA_ButtonControlTypeId,
              L"button"};

    case ax::mojom::Role::kTextField:
      return {UIALocalizationStrategy::kDeferToControlType, UIA_EditControlTypeId,
              L"textbox"};

    case ax::mojom::Role::kSearchBox:
      return {UIALocalizationStrategy::kDeferToControlType, UIA_EditControlTypeId,
              L"searchbox"};

    case ax::mojom::Role::kTextFieldWithComboBox:
      return {UIALocalizationStrategy::kSupply, UIA_ComboBoxControlTypeId,
              L"combobox"};

    case ax::mojom::Role::kAbbr:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"description"};

    case ax::mojom::Role::kTime:
      return {UIALocalizationStrategy::kSupply, UIA_TextControlTypeId, L"time"};

    case ax::mojom::Role::kTimer:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_PaneControlTypeId, L"timer"};

    case ax::mojom::Role::kToolbar:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ToolBarControlTypeId, L"toolbar"};

    case ax::mojom::Role::kTooltip:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_ToolTipControlTypeId, L"tooltip"};

    case ax::mojom::Role::kTree:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_TreeControlTypeId, L"tree"};

    case ax::mojom::Role::kTreeGrid:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_DataGridControlTypeId, L"treegrid"};

    case ax::mojom::Role::kTreeItem:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_TreeItemControlTypeId, L"treeitem"};

    case ax::mojom::Role::kLineBreak:
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_TextControlTypeId, L"description"};

    case ax::mojom::Role::kVideo:
      return {UIALocalizationStrategy::kSupply, UIA_GroupControlTypeId,
              L"group"};

    case ax::mojom::Role::kWebView:
      return {UIALocalizationStrategy::kSupply, UIA_DocumentControlTypeId,
              L"document"};

    case ax::mojom::Role::kImeCandidate:  // Internal role, not used on Windows.
      return {UIALocalizationStrategy::kDeferToControlType,
              UIA_PaneControlTypeId, L"group"};

    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kWindow:
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kUnknown:
      return {UIALocalizationStrategy::kSupply, UIA_PaneControlTypeId,
              L"region"};

    case ax::mojom::Role::kDescriptionListTermDeprecated:
    case ax::mojom::Role::kDescriptionListDetailDeprecated:
    case ax::mojom::Role::kDirectoryDeprecated:
    case ax::mojom::Role::kPreDeprecated:
    case ax::mojom::Role::kPortalDeprecated:
      NOTREACHED();
  }
}

//
// IAccessible implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::accHitTest(LONG screen_physical_pixel_x,
                                             LONG screen_physical_pixel_y,
                                             VARIANT* child) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("accHitTest");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_HIT_TEST);
  COM_OBJECT_VALIDATE_1_ARG(child);

  gfx::Point point(screen_physical_pixel_x, screen_physical_pixel_y);
  if (!GetDelegate()
           ->GetBoundsRect(AXCoordinateSystem::kScreenPhysicalPixels,
                           AXClippingBehavior::kClipped)
           .Contains(point)) {
    // Return S_FALSE and VT_EMPTY when outside the object's boundaries.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  AXPlatformNode* current_result = this;
  while (true) {
    gfx::NativeViewAccessible hit_child =
        current_result->GetDelegate()->HitTestSync(screen_physical_pixel_x,
                                                   screen_physical_pixel_y);
    if (!hit_child) {
      child->vt = VT_EMPTY;
      return S_FALSE;
    }

    AXPlatformNode* hit_child_node =
        AXPlatformNode::FromNativeViewAccessible(hit_child);
    if (!hit_child_node)
      break;

    // If we get the same node, we're done.
    if (hit_child_node == current_result)
      break;

    // Prevent cycles / loops.
    //
    // This is a workaround for a bug where a hit test in web content might
    // return a node that's not a strict descendant. To catch that case
    // without disallowing other valid cases of hit testing, add the
    // following check:
    //
    // If the hit child comes from the same HWND, but it's not a descendant,
    // just ignore the result and stick with the current result. Note that
    // GetTargetForNativeAccessibilityEvent returns a node's owning HWND.
    //
    // Ideally this shouldn't happen - see http://crbug.com/1061323
    bool is_descendant = hit_child_node->IsDescendantOf(current_result);
    bool is_same_hwnd =
        hit_child_node->GetDelegate()->GetTargetForNativeAccessibilityEvent() ==
        current_result->GetDelegate()->GetTargetForNativeAccessibilityEvent();
    if (!is_descendant && is_same_hwnd)
      break;

    // Continue to check recursively. That's because HitTestSync may have
    // returned the best result within a particular accessibility tree,
    // but we might need to recurse further in a tree of a different type
    // (for example, from Views to Web).
    current_result = hit_child_node;
  }

  if (current_result == this) {
    // This object is the best match, so return CHILDID_SELF. It's tempting to
    // simplify the logic and use VT_DISPATCH everywhere, but the Windows
    // call AccessibleObjectFromPoint will keep calling accHitTest until some
    // object returns CHILDID_SELF.
    child->vt = VT_I4;
    child->lVal = CHILDID_SELF;
    return S_OK;
  }

  child->vt = VT_DISPATCH;
  child->pdispVal = static_cast<AXPlatformNodeWin*>(current_result);
  // Always increment ref when returning a reference to a COM object.
  child->pdispVal->AddRef();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::accDoDefaultAction(VARIANT var_id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("accDoDefaultAction");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_DO_DEFAULT_ACTION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_id, target);

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;

  if (target->GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::accLocation(LONG* physical_pixel_left,
                                              LONG* physical_pixel_top,
                                              LONG* width,
                                              LONG* height,
                                              VARIANT var_id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("accLocation");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_LOCATION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_4_ARGS_AND_GET_TARGET(
      var_id, physical_pixel_left, physical_pixel_top, width, height, target);
  NotifyObserverForMSAAUsage();

  gfx::Rect bounds = target->GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreenPhysicalPixels,
      AXClippingBehavior::kUnclipped);
  *physical_pixel_left = bounds.x();
  *physical_pixel_top = bounds.y();
  *width = bounds.width();
  *height = bounds.height();

  if (bounds.IsEmpty())
    return S_FALSE;

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::accNavigate(LONG nav_dir,
                                              VARIANT start,
                                              VARIANT* end) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("accNavigate");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_NAVIGATE);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(start, end, target);

  end->vt = VT_EMPTY;
  if ((nav_dir == NAVDIR_FIRSTCHILD || nav_dir == NAVDIR_LASTCHILD) &&
      V_VT(&start) == VT_I4 && V_I4(&start) != CHILDID_SELF) {
    // MSAA states that navigating to first/last child can only be from self.
    return E_INVALIDARG;
  }

  IAccessible* result = nullptr;
  switch (nav_dir) {
    case NAVDIR_FIRSTCHILD:
      if (GetDelegate()->GetChildCount() > 0)
        result = GetDelegate()->GetFirstChild();
      break;

    case NAVDIR_LASTCHILD:
      if (GetDelegate()->GetChildCount() > 0)
        result = GetDelegate()->GetLastChild();
      break;

    case NAVDIR_NEXT: {
      AXPlatformNodeBase* next = target->GetNextSibling();
      if (next)
        result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_PREVIOUS: {
      AXPlatformNodeBase* previous = target->GetPreviousSibling();
      if (previous)
        result = previous->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_DOWN: {
      // This direction is not implemented except in tables.
      if (!GetTableRow() || !GetTableRowSpan() || !GetTableColumn())
        return E_NOTIMPL;

      AXPlatformNodeBase* next = target->GetTableCell(
          *GetTableRow() + *GetTableRowSpan(), *GetTableColumn());
      if (!next)
        return S_OK;

      result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_UP: {
      // This direction is not implemented except in tables.
      if (!GetTableRow() || !GetTableColumn())
        return E_NOTIMPL;

      AXPlatformNodeBase* next =
          target->GetTableCell(*GetTableRow() - 1, *GetTableColumn());
      if (!next)
        return S_OK;

      result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_LEFT: {
      // This direction is not implemented except in tables.
      if (!GetTableRow() || !GetTableColumn())
        return E_NOTIMPL;

      AXPlatformNodeBase* next =
          target->GetTableCell(*GetTableRow(), *GetTableColumn() - 1);
      if (!next)
        return S_OK;

      result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_RIGHT: {
      // This direction is not implemented except in tables.

      if (!GetTableRow() || !GetTableColumn() || !GetTableColumnSpan())
        return E_NOTIMPL;

      AXPlatformNodeBase* next = target->GetTableCell(
          *GetTableRow(), *GetTableColumn() + *GetTableColumnSpan());
      if (!next)
        return S_OK;

      result = next->GetNativeViewAccessible();
      break;
    }
  }

  if (!result)
    return S_FALSE;

  end->vt = VT_DISPATCH;
  end->pdispVal = result;
  // Always increment ref when returning a reference to a COM object.
  end->pdispVal->AddRef();

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accChild(VARIANT var_child,
                                               IDispatch** disp_child) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accChild");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_CHILD);

  *disp_child = nullptr;
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_child, target);

  *disp_child = target;
  (*disp_child)->AddRef();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accChildCount(LONG* child_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accChildCount");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_CHILD_COUNT);
  COM_OBJECT_VALIDATE_1_ARG(child_count);

  *child_count = GetDelegate()->GetChildCount();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accDefaultAction(VARIANT var_id,
                                                       BSTR* def_action) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accDefaultAction");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_DEFAULT_ACTION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, def_action, target);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  int action;
  if (!target->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                               &action)) {
    *def_action = nullptr;
    return S_FALSE;
  }

  std::wstring action_verb = base::UTF8ToWide(
      ToLocalizedString(static_cast<ax::mojom::DefaultActionVerb>(action)));
  if (action_verb.empty()) {
    *def_action = nullptr;
    return S_FALSE;
  }

  *def_action = SysAllocString(action_verb.c_str());
  DCHECK(def_action);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accDescription(VARIANT var_id,
                                                     BSTR* desc) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accDescription");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_DESCRIPTION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, desc, target);

  return target->GetStringAttributeAsBstr(
      ax::mojom::StringAttribute::kDescription, desc);
}

IFACEMETHODIMP AXPlatformNodeWin::get_accFocus(VARIANT* focus_child) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accFocus");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_FOCUS);
  COM_OBJECT_VALIDATE_1_ARG(focus_child);

  gfx::NativeViewAccessible focus_accessible = GetDelegate()->GetFocus();
  if (focus_accessible == this) {
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else if (focus_accessible) {
    Microsoft::WRL::ComPtr<IDispatch> focus_idispatch;
    if (FAILED(
            focus_accessible->QueryInterface(IID_PPV_ARGS(&focus_idispatch)))) {
      focus_child->vt = VT_EMPTY;
      return E_FAIL;
    }

    focus_child->vt = VT_DISPATCH;
    focus_child->pdispVal = focus_idispatch.Detach();
  } else {
    focus_child->vt = VT_EMPTY;
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accKeyboardShortcut(VARIANT var_id,
                                                          BSTR* acc_key) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accKeyboardShortcut");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_KEYBOARD_SHORTCUT);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, acc_key, target);

  return target->GetStringAttributeAsBstr(
      ax::mojom::StringAttribute::kKeyShortcuts, acc_key);
}

IFACEMETHODIMP AXPlatformNodeWin::get_accName(VARIANT var_id, BSTR* name_bstr) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accName");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_NAME);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, name_bstr, target);
  NotifyObserverForMSAAUsage();

  GetWinAccessibilityAPIUsageObserverList().Notify(
      &WinAccessibilityAPIUsageObserver::OnAccNameCalled);

  if (!IsNameExposed())
    return S_FALSE;

  bool has_name = target->HasStringAttribute(ax::mojom::StringAttribute::kName);
  std::wstring name = base::UTF8ToWide(target->GetName());

  if (name.empty() && !has_name)
    return S_FALSE;

  *name_bstr = SysAllocString(name.c_str());
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accParent(IDispatch** disp_parent) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accParent");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_PARENT);
  COM_OBJECT_VALIDATE_1_ARG(disp_parent);
  *disp_parent = GetParent();
  if (*disp_parent) {
    (*disp_parent)->AddRef();
    return S_OK;
  }

  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accRole(VARIANT var_id, VARIANT* role) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accRole");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_ROLE);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, role, target);
  NotifyObserverForMSAAUsage();

  role->vt = VT_I4;
  role->lVal = target->MSAARole();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accState(VARIANT var_id, VARIANT* state) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accState");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_STATE);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, state, target);
  NotifyObserverForMSAAUsage();

  state->vt = VT_I4;
  state->lVal = target->MSAAState();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accHelp(VARIANT var_id, BSTR* help) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accHelp");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_HELP);
  COM_OBJECT_VALIDATE_1_ARG(help);
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accValue(VARIANT var_id, BSTR* value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_VALUE);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, value, target);

  // Special case for indeterminate progressbar.
  if (GetRole() == ax::mojom::Role::kProgressIndicator &&
      !HasStringAttribute(ax::mojom::StringAttribute::kValue) &&
      !HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange)) {
    // The MIXED state is also exposed for an indeterminate value.
    // However, without some value here, NVDA/JAWS 2022 will ignore the
    // progress indicator in the virtual buffer.
    *value = SysAllocString(L"0");
    return S_OK;
  }

  *value = GetValueAttributeAsBstr(target);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::put_accValue(VARIANT var_id, BSTR new_value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("put_accValue");
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_id, target);
  if (!new_value)
    return E_INVALIDARG;

  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = base::WideToUTF8(new_value);
  if (target->GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accSelection(VARIANT* selected) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accSelection");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_SELECTION);
  COM_OBJECT_VALIDATE_1_ARG(selected);

  std::vector<Microsoft::WRL::ComPtr<IDispatch>> selected_nodes;
  for (size_t i = 0; i < GetDelegate()->GetChildCount(); ++i) {
    auto* node = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(GetDelegate()->ChildAtIndex(i)));
    if (node && node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
      Microsoft::WRL::ComPtr<IDispatch> node_idispatch;
      if (SUCCEEDED(node->QueryInterface(IID_PPV_ARGS(&node_idispatch))))
        selected_nodes.push_back(node_idispatch);
    }
  }

  if (selected_nodes.empty()) {
    selected->vt = VT_EMPTY;
    return S_OK;
  }

  if (selected_nodes.size() == 1) {
    selected->vt = VT_DISPATCH;
    selected->pdispVal = selected_nodes[0].Detach();
    return S_OK;
  }

  // Multiple items are selected.
  LONG selected_count = static_cast<LONG>(selected_nodes.size());
  Microsoft::WRL::ComPtr<base::win::EnumVariant> enum_variant =
      Microsoft::WRL::Make<base::win::EnumVariant>(selected_count);
  for (LONG i = 0; i < selected_count; ++i) {
    enum_variant->ItemAt(i)->vt = VT_DISPATCH;
    enum_variant->ItemAt(i)->pdispVal = selected_nodes[i].Detach();
  }
  selected->vt = VT_UNKNOWN;
  return enum_variant.CopyTo(IID_PPV_ARGS(&V_UNKNOWN(selected)));
}

IFACEMETHODIMP AXPlatformNodeWin::accSelect(LONG flagsSelect, VARIANT var_id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("accSelect");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_SELECT);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_id, target);

  if (flagsSelect & SELFLAG_TAKEFOCUS) {
    AXActionData action_data;
    action_data.action = ax::mojom::Action::kFocus;
    target->GetDelegate()->AccessibilityPerformAction(action_data);
    return S_OK;
  }

  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accHelpTopic(BSTR* help_file,
                                                   VARIANT var_id,
                                                   LONG* topic_id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accHelpTopic");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_HELP_TOPIC);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_2_ARGS_AND_GET_TARGET(var_id, help_file, topic_id,
                                                   target);

  if (help_file) {
    *help_file = nullptr;
  }
  if (topic_id) {
    *topic_id = static_cast<LONG>(-1);
  }
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::put_accName(VARIANT var_id, BSTR put_name) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("put_accName");
  // Deprecated by Microsoft and no good use cases; no need to implement.
  return E_NOTIMPL;
}

//
// IAccessible2 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::role(LONG* role) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("role");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ROLE);
  COM_OBJECT_VALIDATE_1_ARG(role);

  *role = ComputeIA2Role();
  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!*role)
    *role = MSAARole();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_states(AccessibleStates* states) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_states");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_STATES);
  COM_OBJECT_VALIDATE_1_ARG(states);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  *states = ComputeIA2State();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_uniqueID(LONG* id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_uniqueID");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_UNIQUE_ID);
  COM_OBJECT_VALIDATE_1_ARG(id);
  // We want to negate the unique id for it to be consistent across different
  // Windows accessiblity APIs. The negative unique id convention originated
  // from ::NotifyWinEvent() takes an hwnd and a child id. A 0 child id means
  // self, and a positive child id means child #n. In order to fire an event for
  // an arbitrary descendant of the window, Firefox started the practice of
  // using a negative unique id. We follow the same negative unique id
  // convention here and when we fire events via
  // ::NotifyWinEvent().
  *id = -GetUniqueId();
  DCHECK(*id < 0);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_windowHandle(HWND* window_handle) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_windowHandle");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_WINDOW_HANDLE);
  COM_OBJECT_VALIDATE_1_ARG(window_handle);
  *window_handle = GetDelegate()->GetTargetForNativeAccessibilityEvent();
  return *window_handle ? S_OK : S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_relationTargetsOfType(BSTR type_bstr,
                                                            LONG max_targets,
                                                            IUnknown*** targets,
                                                            LONG* n_targets) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_relationTargetsOfType");
  COM_OBJECT_VALIDATE_2_ARGS(targets, n_targets);
  if (!type_bstr)
    return E_INVALIDARG;

  *n_targets = 0;
  *targets = nullptr;

  // Special case for relations of type "alerts".
  std::wstring type(type_bstr);
  if (type == L"alerts") {
    // Collect all of the objects that have had an alert fired on them that
    // are a descendant of this object.
    std::vector<AXPlatformNodeWin*> alert_targets;
    for (auto iter = g_alert_targets.Get().begin();
         iter != g_alert_targets.Get().end(); ++iter) {
      AXPlatformNodeWin* target = *iter;
      if (IsDescendant(target))
        alert_targets.push_back(target);
    }

    LONG count = static_cast<LONG>(alert_targets.size());
    if (count == 0)
      return S_FALSE;

    // Don't return more targets than max_targets - but note that the caller
    // is allowed to specify max_targets=0 to mean no limit.
    if (max_targets > 0 && count > max_targets)
      count = max_targets;

    // Return the number of targets.
    *n_targets = count;

    // Allocate COM memory for the result array and populate it.
    *targets =
        static_cast<IUnknown**>(CoTaskMemAlloc(count * sizeof(IUnknown*)));
    for (LONG i = 0; i < count; ++i) {
      (*targets)[i] = static_cast<IAccessible*>(alert_targets[i]);
      (*targets)[i]->AddRef();
    }
    return S_OK;
  }

  std::wstring relation_type;
  std::vector<AXPlatformNode*> enumerated_targets;
  int found = AXPlatformRelationWin::EnumerateRelationships(
      this, 0, type, &relation_type, &enumerated_targets);
  if (found == 0)
    return S_FALSE;

  // Don't return more targets than max_targets - but note that the caller
  // is allowed to specify max_targets=0 to mean no limit.
  int count = static_cast<int>(enumerated_targets.size());
  if (max_targets > 0 && count > max_targets)
    count = max_targets;

  // Allocate COM memory for the result array and populate it.
  *targets = static_cast<IUnknown**>(CoTaskMemAlloc(count * sizeof(IUnknown*)));
  int index = 0;
  for (AXPlatformNode* target : enumerated_targets) {
    if (target) {
      AXPlatformNodeWin* win_target = static_cast<AXPlatformNodeWin*>(target);
      (*targets)[index] = static_cast<IAccessible*>(win_target);
      (*targets)[index]->AddRef();
      if (++index >= count) {
        break;
      }
    }
  }
  *n_targets = index;
  return index > 0 ? S_OK : S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_attributes(BSTR* attributes) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_attributes");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IA2_GET_ATTRIBUTES);
  COM_OBJECT_VALIDATE_1_ARG(attributes);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);
  *attributes = nullptr;

  std::wstring attributes_str;
  std::vector<std::wstring> computed_attributes = ComputeIA2Attributes();
  for (const std::wstring& attribute : computed_attributes)
    attributes_str += attribute + L';';

  if (attributes_str.empty())
    return S_FALSE;

  *attributes = SysAllocString(attributes_str.c_str());
  DCHECK(*attributes);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_indexInParent(LONG* index_in_parent) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_indexInParent");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_INDEX_IN_PARENT);
  COM_OBJECT_VALIDATE_1_ARG(index_in_parent);
  std::optional<int> index = GetIndexInParent();
  if (!index.has_value())
    return E_FAIL;

  *index_in_parent = index.value();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nRelations(LONG* n_relations) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nRelations");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_RELATIONS);
  COM_OBJECT_VALIDATE_1_ARG(n_relations);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  int count = AXPlatformRelationWin::EnumerateRelationships(
      this, -1, std::wstring(), nullptr, nullptr);
  *n_relations = count;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_relation(LONG relation_index,
                                               IAccessibleRelation** relation) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_relation");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_RELATION);
  COM_OBJECT_VALIDATE_1_ARG(relation);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::wstring relation_type;
  std::vector<AXPlatformNode*> targets;
  int found = AXPlatformRelationWin::EnumerateRelationships(
      this, relation_index, std::wstring(), &relation_type, &targets);
  if (found == 0)
    return E_INVALIDARG;

  CComObject<AXPlatformRelationWin>* relation_obj;
  HRESULT hr = CComObject<AXPlatformRelationWin>::CreateInstance(&relation_obj);
  DCHECK(SUCCEEDED(hr));
  relation_obj->AddRef();
  relation_obj->Initialize(relation_type);
  for (AXPlatformNode* target : targets) {
    if (target)
      relation_obj->AddTarget(static_cast<AXPlatformNodeWin*>(target));
  }

  // Maintain references to all relations returned by this object.
  // Every time this object changes state, invalidate them.
  relations_.push_back(relation_obj);
  *relation = relation_obj;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_relations(LONG max_relations,
                                                IAccessibleRelation** relations,
                                                LONG* n_relations) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_relations");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_RELATIONS);
  COM_OBJECT_VALIDATE_2_ARGS(relations, n_relations);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  LONG count;
  HRESULT hr = get_nRelations(&count);
  if (!SUCCEEDED(hr))
    return hr;
  count = std::min(count, max_relations);
  *n_relations = count;
  for (LONG i = 0; i < count; i++) {
    hr = get_relation(i, &relations[i]);
    if (!SUCCEEDED(hr))
      return hr;
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_groupPosition(
    LONG* group_level,
    LONG* similar_items_in_group,
    LONG* position_in_group) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_groupPosition");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_GROUP_POSITION);
  COM_OBJECT_VALIDATE_3_ARGS(group_level, similar_items_in_group,
                             position_in_group);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  *group_level = GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
  *similar_items_in_group = GetSetSize().value_or(0);
  *position_in_group = GetPosInSet().value_or(0);

  if (!*group_level && !*similar_items_in_group && !*position_in_group)
    return S_FALSE;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_localizedExtendedRole(
    BSTR* localized_extended_role) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_localizedExtendedRole");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALIZED_EXTENDED_ROLE);
  COM_OBJECT_VALIDATE_1_ARG(localized_extended_role);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::u16string role_description =
      GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute();
  if (base::ContainsOnlyChars(role_description, base::kWhitespaceUTF16))
    return S_FALSE;

  *localized_extended_role = SysAllocString(base::as_wcstr(role_description));
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_attribute(BSTR name, VARIANT* attribute) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_attribute");
  COM_OBJECT_VALIDATE_1_ARG(attribute);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::wstring desired_attribute(name);

  // Each computed attribute from ComputeIA2Attributes is a string of
  // the form "key:value". Search for strings that start with the
  // attribute name plus a colon.
  std::wstring prefix = desired_attribute + L":";

  // Let's accept any case.
  const auto compare_case = base::CompareCase::INSENSITIVE_ASCII;

  const std::vector<std::wstring> computed_attributes = ComputeIA2Attributes();
  for (const std::wstring& computed_attribute : computed_attributes) {
    if (base::StartsWith(computed_attribute, prefix, compare_case)) {
      std::wstring value = computed_attribute.substr(prefix.size());
      attribute->vt = VT_BSTR;
      attribute->bstrVal = SysAllocString(value.c_str());
      return S_OK;
    }
  }

  return S_FALSE;
}

//
// IAccessible2 methods not implemented.
//

IFACEMETHODIMP AXPlatformNodeWin::get_extendedRole(BSTR* extended_role) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_extendedRole");
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::scrollTo(enum IA2ScrollType ia2_scroll_type) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("scrollTo");
  COM_OBJECT_VALIDATE();
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IA2_SCROLL_TO);

  switch (ia2_scroll_type) {
    case IA2_SCROLL_TYPE_TOP_LEFT:
      ScrollToNode(ScrollType::TopLeft);
      break;
    case IA2_SCROLL_TYPE_BOTTOM_RIGHT:
      ScrollToNode(ScrollType::BottomRight);
      break;
    case IA2_SCROLL_TYPE_TOP_EDGE:
      ScrollToNode(ScrollType::TopEdge);
      break;
    case IA2_SCROLL_TYPE_BOTTOM_EDGE:
      ScrollToNode(ScrollType::BottomEdge);
      break;
    case IA2_SCROLL_TYPE_LEFT_EDGE:
      ScrollToNode(ScrollType::LeftEdge);
      break;
    case IA2_SCROLL_TYPE_RIGHT_EDGE:
      ScrollToNode(ScrollType::RightEdge);
      break;
    case IA2_SCROLL_TYPE_ANYWHERE:
      ScrollToNode(ScrollType::Anywhere);
      break;
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::scrollToPoint(
    enum IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("scrollToPoint");
  COM_OBJECT_VALIDATE();
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_TO_POINT);

  // Convert to screen-relative coordinates if necessary.
  gfx::Point scroll_to(x, y);
  if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    if (GetParent()) {
      AXPlatformNodeBase* base = FromNativeViewAccessible(GetParent());
      scroll_to += base->GetDelegate()
                       ->GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                                       AXClippingBehavior::kUnclipped)
                       .OffsetFromOrigin();
    }
  } else if (coordinate_type != IA2_COORDTYPE_SCREEN_RELATIVE) {
    return E_INVALIDARG;
  }

  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = scroll_to;
  GetDelegate()->AccessibilityPerformAction(action_data);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nExtendedStates(LONG* n_extended_states) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nExtendedStates");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_EXTENDED_STATES);
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_extendedStates(LONG max_extended_states,
                                                     BSTR** extended_states,
                                                     LONG* n_extended_states) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_extendedStates");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_EXTENDED_STATES);
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_localizedExtendedStates(
    LONG max_localized_extended_states,
    BSTR** localized_extended_states,
    LONG* n_localized_extended_states) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_localizedExtendedStates");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALIZED_EXTENDED_STATES);

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_locale(IA2Locale* locale) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_locale");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALE);
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accessibleWithCaret(IUnknown** accessible,
                                                          LONG* caret_offset) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accessibleWithCaret");
  return E_NOTIMPL;
}

//
// IAccessible2_3 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::get_selectionRanges(IA2Range** ranges,
                                                      LONG* nRanges) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selectionRanges");
  COM_OBJECT_VALIDATE_2_ARGS(ranges, nRanges);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);
  AXSelection unignored_selection = GetDelegate()->GetUnignoredSelection();

  AXNodeID anchor_id = unignored_selection.anchor_object_id;
  auto* anchor_node =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(anchor_id));
  if (!anchor_node)
    return E_FAIL;

  AXNodeID focus_id = unignored_selection.focus_object_id;
  auto* focus_node =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(focus_id));
  if (!focus_node)
    return E_FAIL;

  if (!IsDescendant(anchor_node) || !IsDescendant(focus_node))
    return S_FALSE;  // No selection within this subtree.

  *ranges = reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  anchor_node->AddRef();
  ranges[0]->anchor = static_cast<IAccessible*>(anchor_node);
  ranges[0]->anchorOffset = unignored_selection.anchor_offset;
  focus_node->AddRef();
  ranges[0]->active = static_cast<IAccessible*>(focus_node);
  ranges[0]->activeOffset = unignored_selection.focus_offset;
  *nRanges = 1;
  return S_OK;
}

//
// IAccessible2_4 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::setSelectionRanges(LONG nRanges,
                                                     IA2Range* ranges) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("setSelectionRanges");
  COM_OBJECT_VALIDATE();
  // Blink supports only one selection range for now.
  if (nRanges != 1)
    return E_INVALIDARG;
  if (!ranges)
    return E_INVALIDARG;
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  if (!ranges->anchor)
    return E_INVALIDARG;
  if (!ranges->active)
    return E_INVALIDARG;

  Microsoft::WRL::ComPtr<IAccessible> anchor;
  if (FAILED(ranges->anchor->QueryInterface(IID_PPV_ARGS(&anchor))))
    return E_INVALIDARG;

  Microsoft::WRL::ComPtr<IAccessible> focus;
  if (FAILED(ranges->active->QueryInterface(IID_PPV_ARGS(&focus))))
    return E_INVALIDARG;

  const auto* anchor_node =
      static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(anchor.Get()));
  const auto* focus_node =
      static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(focus.Get()));
  if (!anchor_node || !focus_node)
    return E_INVALIDARG;

  // Blink only supports selections within a single tree.
  AXTreeID anchor_tree_id = anchor_node->GetDelegate()->GetTreeData().tree_id;
  AXTreeID focus_tree_id = focus_node->GetDelegate()->GetTreeData().tree_id;
  if (anchor_tree_id != focus_tree_id) {
    return E_INVALIDARG;
  }

  if (ranges->anchorOffset < 0 || ranges->activeOffset < 0)
    return E_INVALIDARG;

  if (anchor_node->IsLeaf()) {
    if (static_cast<size_t>(ranges->anchorOffset) >
        anchor_node->GetHypertext().length()) {
      return E_INVALIDARG;
    }
  } else {
    if (static_cast<size_t>(ranges->anchorOffset) >
        anchor_node->GetChildCount())
      return E_INVALIDARG;
  }

  if (focus_node->IsLeaf()) {
    if (static_cast<size_t>(ranges->activeOffset) >
        focus_node->GetHypertext().length())
      return E_INVALIDARG;
  } else {
    if (static_cast<size_t>(ranges->activeOffset) > focus_node->GetChildCount())
      return E_INVALIDARG;
  }

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = anchor_node->GetData().id;
  action_data.target_tree_id = anchor_tree_id;
  action_data.anchor_offset = int32_t{ranges->anchorOffset};
  action_data.focus_node_id = focus_node->GetData().id;
  action_data.focus_offset = int32_t{ranges->activeOffset};
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return S_FALSE;
}

//
// IAccessibleEx implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::GetObjectForChild(LONG child_id,
                                                    IAccessibleEx** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetObjectForChild");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_OBJECT_FOR_CHILD);
  // No support for child IDs in this implementation.
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = nullptr;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetIAccessiblePair(IAccessible** accessible,
                                                     LONG* child_id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetIAccessiblePair");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_IACCESSIBLE_PAIR);
  COM_OBJECT_VALIDATE_2_ARGS(accessible, child_id);
  *accessible = static_cast<IAccessible*>(this);
  (*accessible)->AddRef();
  *child_id = CHILDID_SELF;
  return S_OK;
}

//
// IAnnotationProvider methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_AnnotationTypeId(int* type_id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_AnnotationTypeId");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ANNOTATION_GET_ANNOTATIONTYPEID);
  UIA_VALIDATE_CALL_1_ARG(type_id);

  *type_id = GetAnnotationTypeImpl();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_AnnotationTypeName(BSTR* type_name) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_AnnotationTypeName");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ANNOTATION_GET_ANNOTATIONTYPENAME);
  UIA_VALIDATE_CALL_1_ARG(type_name);

  // According to UIA spec, for well known AnnotationType, we do not need to
  // implement the type name, since UI Automation can provide a default name, so
  // we do not set |type_name|.
  // But for unknown type, we should provide a name, so we set |type_name| to
  // the localized role description.
  // https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingannotation
  if (AnnotationType_Unknown == GetAnnotationTypeImpl())
    *type_name = SysAllocString(base::as_wcstr(GetRoleDescription()));

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Author(BSTR* author) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Author");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ANNOTATION_GET_AUTHOR);
  UIA_VALIDATE_CALL_1_ARG(author);
  // This method is optional, and currently does not have a mapping. So we
  // return S_OK with empty string.
  *author = SysAllocString(L"");
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_DateTime(BSTR* date_time) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_DateTime");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ANNOTATION_GET_DATETIME);
  UIA_VALIDATE_CALL_1_ARG(date_time);
  // This method is optional, and currently does not have a mapping. So we
  // return S_OK with empty string.
  *date_time = SysAllocString(L"");
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Target(
    IRawElementProviderSimple** target) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Target");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ANNOTATION_GET_TARGET);
  UIA_VALIDATE_CALL_1_ARG(target);
  std::vector<AXPlatformNode*> reverse_relations =
      GetDelegate()->GetSourceNodesForReverseRelations(
          ax::mojom::IntListAttribute::kDetailsIds);

  // If there is no reverse relation target, IAnnotationProvider
  // should not be exposed in the first place.
  DCHECK_GT(reverse_relations.size(), 0u);
  AXPlatformNodeWin* target_node;
  auto iter = reverse_relations.begin();
  target_node = static_cast<AXPlatformNodeWin*>(*iter);

  // Since this method is expected to return only one target, if the node has
  // multiple targets, we default to return the first one.
  // Since |reverse_relations| does not guarantee the order of the nodes. We
  // have to compare the nodes to find the first target node.
  ++iter;
  while (iter != reverse_relations.end()) {
    AXPlatformNodeWin* lhs = static_cast<AXPlatformNodeWin*>(*iter);
    if (lhs->CompareTo(*target_node) < 0)
      target_node = lhs;
    ++iter;
  }

  return target_node->QueryInterface(IID_PPV_ARGS(target));
}

int AXPlatformNodeWin::GetAnnotationTypeImpl() const {
  switch (GetRole()) {
    case ax::mojom::Role::kComment:
      return AnnotationType_Comment;
    case ax::mojom::Role::kDocEndnote:
      return AnnotationType_Endnote;
    case ax::mojom::Role::kDocFootnote:
      return AnnotationType_Footnote;
    case ax::mojom::Role::kMark:
      return AnnotationType_Highlighted;
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kRegion: {
      if (DescendantHasComment(this))
        return AnnotationType_Comment;

      [[fallthrough]];
    }
    default:
      return AnnotationType_Unknown;
  }
}

//
// IExpandCollapseProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Collapse() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Collapse");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_EXPANDCOLLAPSE_COLLAPSE);
  UIA_VALIDATE_CALL();
  if (GetData().GetRestriction() == ax::mojom::Restriction::kDisabled)
    return UIA_E_ELEMENTNOTAVAILABLE;

  if (HasState(ax::mojom::State::kCollapsed))
    return UIA_E_INVALIDOPERATION;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kCollapse;
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::Expand() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Expand");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_EXPANDCOLLAPSE_EXPAND);
  UIA_VALIDATE_CALL();
  if (GetData().GetRestriction() == ax::mojom::Restriction::kDisabled)
    return UIA_E_ELEMENTNOTAVAILABLE;

  if (HasState(ax::mojom::State::kExpanded))
    return UIA_E_INVALIDOPERATION;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kExpand;
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

ExpandCollapseState AXPlatformNodeWin::ComputeExpandCollapseState() const {
  if (HasState(ax::mojom::State::kExpanded)) {
    return ExpandCollapseState_Expanded;
  } else if (HasState(ax::mojom::State::kCollapsed)) {
    return ExpandCollapseState_Collapsed;
  } else if (GetData().IsMenuButton()) {
    // Since a menu button implies there is a popup and it is either expanded or
    // collapsed, it should not support ExpandCollapseState_LeafNode.
    // According to the UIA spec, ExpandCollapseState_LeafNode indicates that
    // the element neither expands nor collapses.
    if (GetData().IsButtonPressed()) {
      return ExpandCollapseState_Expanded;
    } else {
      return ExpandCollapseState_Collapsed;
    }
  } else {
    return ExpandCollapseState_LeafNode;
  }
}

IFACEMETHODIMP AXPlatformNodeWin::get_ExpandCollapseState(
    ExpandCollapseState* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_ExpandCollapseState");
  WIN_ACCESSIBILITY_API_HISTOGRAM(
      UMA_API_EXPANDCOLLAPSE_GET_EXPANDCOLLAPSESTATE);
  UIA_VALIDATE_CALL_1_ARG(result);

  *result = ComputeExpandCollapseState();

  return S_OK;
}

//
// IGridItemProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::get_Column(int* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Column");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_COLUMN);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::optional<int> column = GetDelegate()->GetTableCellAriaColIndex();

  // |aria-colindex| starts at 1, where as IGridItemProvider::get_Column's index
  // starts at 0, so we need to subtract by 1 if |aria-colindex| attribute is
  // present.
  //  https://www.w3.org/TR/core-aam-1.2/#aria-colindex
  if (column)
    *column = *column - 1;
  else
    column = GetTableColumn();

  if (!column)
    return E_FAIL;
  *result = *column;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ColumnSpan(int* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_ColumnSpan");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_COLUMNSPAN);
  UIA_VALIDATE_CALL_1_ARG(result);
  std::optional<int> column_span = GetTableColumnSpan();
  if (!column_span)
    return E_FAIL;
  *result = *column_span;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ContainingGrid(
    IRawElementProviderSimple** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_ContainingGrid");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_CONTAININGGRID);
  UIA_VALIDATE_CALL_1_ARG(result);

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return E_FAIL;

  auto* node_win = static_cast<AXPlatformNodeWin*>(table);
  node_win->AddRef();
  *result = static_cast<IRawElementProviderSimple*>(node_win);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Row(int* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Row");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_ROW);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::optional<int> row = GetDelegate()->GetTableCellAriaRowIndex();

  // |aria-rowindex| starts at 1, where as IGridItemProvider::get_Row's index
  // starts at 0, so we need to subtract by 1 if |aria-rowindex| attribute is
  // present.
  //  https://www.w3.org/TR/core-aam-1.2/#aria-rowindex
  if (row)
    *row = *row - 1;
  else
    row = GetTableRow();

  if (!row)
    return E_FAIL;
  *result = *row;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowSpan(int* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_RowSpan");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_ROWSPAN);
  UIA_VALIDATE_CALL_1_ARG(result);
  std::optional<int> row_span = GetTableRowSpan();
  if (!row_span)
    return E_FAIL;
  *result = *row_span;
  return S_OK;
}

//
// IGridProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::GetItem(int row,
                                          int column,
                                          IRawElementProviderSimple** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetItem");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRID_GETITEM);
  UIA_VALIDATE_CALL_1_ARG(result);

  // While UIA is 0-based, aria is 1-based, so correct here.
  AXPlatformNodeBase* cell = GetAriaTableCell(row + 1, column + 1);
  if (!cell) {
    cell = GetTableCell(row, column);
  }

  if (!cell) {
    return E_INVALIDARG;
  }

  auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
  node_win->AddRef();
  *result = static_cast<IRawElementProviderSimple*>(node_win);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowCount(int* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_RowCount");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRID_GET_ROWCOUNT);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::optional<int> row_count = GetTableAriaRowCount();
  if (!row_count)
    row_count = GetTableRowCount();

  if (!row_count || *row_count == ax::mojom::kUnknownAriaColumnOrRowCount)
    return E_UNEXPECTED;
  *result = *row_count;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ColumnCount(int* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_ColumnCount");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRID_GET_COLUMNCOUNT);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::optional<int> column_count = GetTableAriaColumnCount();
  if (!column_count)
    column_count = GetTableColumnCount();

  if (!column_count ||
      *column_count == ax::mojom::kUnknownAriaColumnOrRowCount) {
    return E_UNEXPECTED;
  }
  *result = *column_count;
  return S_OK;
}

//
// IInvokeProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Invoke() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Invoke");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_INVOKE_INVOKE);
  UIA_VALIDATE_CALL();

  if (GetData().GetRestriction() == ax::mojom::Restriction::kDisabled)
    return UIA_E_ELEMENTNOTENABLED;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  GetDelegate()->AccessibilityPerformAction(action_data);

  return S_OK;
}

//
// IScrollItemProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::ScrollIntoView() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("ScrollIntoView");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLLITEM_SCROLLINTOVIEW);
  UIA_VALIDATE_CALL();
  gfx::Rect r = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
  r -= r.OffsetFromOrigin();

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_node_id = GetData().id;
  action_data.target_rect = r;
  action_data.horizontal_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.vertical_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.scroll_behavior =
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible;
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

//
// IScrollProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Scroll(ScrollAmount horizontal_amount,
                                         ScrollAmount vertical_amount) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Scroll");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_SCROLL);
  UIA_VALIDATE_CALL();
  if (!IsScrollable())
    return E_FAIL;

  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kSetScrollOffset;
  action_data.target_point = gfx::PointAtOffsetFromOrigin(
      CalculateUIAScrollPoint(horizontal_amount, vertical_amount));
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::SetScrollPercent(double horizontal_percent,
                                                   double vertical_percent) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("SetScrollPercent");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_SETSCROLLPERCENT);
  UIA_VALIDATE_CALL();
  if (!IsScrollable())
    return E_FAIL;

  const double x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  const double x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  const double y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  const double y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
  const int x =
      base::ClampRound(horizontal_percent / 100.0 * (x_max - x_min) + x_min);
  const int y =
      base::ClampRound(vertical_percent / 100.0 * (y_max - y_min) + y_min);
  const gfx::Point scroll_to(x, y);

  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kSetScrollOffset;
  action_data.target_point = scroll_to;
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_HorizontallyScrollable(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_HorizontallyScrollable");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_HORIZONTALLYSCROLLABLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = IsHorizontallyScrollable();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_HorizontalScrollPercent(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_HorizontalScrollPercent");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_HORIZONTALSCROLLPERCENT);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetHorizontalScrollPercent();
  return S_OK;
}

// Horizontal size of the viewable region as a percentage of the total content
// area.
IFACEMETHODIMP AXPlatformNodeWin::get_HorizontalViewSize(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_HorizontalViewSize");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_HORIZONTALVIEWSIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  if (!IsHorizontallyScrollable()) {
    *result = 100.;
    return S_OK;
  }

  gfx::RectF clipped_bounds(GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreenDIPs, AXClippingBehavior::kClipped));
  float x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  float x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  float total_width = clipped_bounds.width() + x_max - x_min;
  DCHECK_LE(clipped_bounds.width(), total_width);
  *result = 100.0 * clipped_bounds.width() / total_width;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_VerticallyScrollable(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_VerticallyScrollable");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_VERTICALLYSCROLLABLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = IsVerticallyScrollable();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_VerticalScrollPercent(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_VerticalScrollPercent");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_VERTICALSCROLLPERCENT);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetVerticalScrollPercent();
  return S_OK;
}

// Vertical size of the viewable region as a percentage of the total content
// area.
IFACEMETHODIMP AXPlatformNodeWin::get_VerticalViewSize(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_VerticalViewSize");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_VERTICALVIEWSIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  if (!IsVerticallyScrollable()) {
    *result = 100.0;
    return S_OK;
  }

  gfx::RectF clipped_bounds(GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreenDIPs, AXClippingBehavior::kClipped));
  float y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  float y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
  float total_height = clipped_bounds.height() + y_max - y_min;
  DCHECK_LE(clipped_bounds.height(), total_height);
  *result = 100.0 * clipped_bounds.height() / total_height;
  return S_OK;
}

//
// ISelectionItemProvider implementation.
//

HRESULT AXPlatformNodeWin::ISelectionItemProviderSetSelected(
    bool selected) const {
  UIA_VALIDATE_CALL();
  if (GetData().GetRestriction() == ax::mojom::Restriction::kDisabled)
    return UIA_E_ELEMENTNOTENABLED;

  // The platform implements selection follows focus for single-selection
  // container elements. Focus action can change a node's accessibility selected
  // state, but does not cause the actual control to be selected.
  // https://www.w3.org/TR/wai-aria-practices-1.1/#kbd_selection_follows_focus
  // https://www.w3.org/TR/core-aam-1.2/#mapping_events_selection
  //
  // We don't want to perform |Action::kDoDefault| for an ax node that has
  // |kSelected=true| and |kSelectedFromFocus=false|, because perform
  // |Action::kDoDefault| may cause the control to be unselected. However, if an
  // ax node is selected due to focus, i.e. |kSelectedFromFocus=true|, we need
  // to perform |Action::kDoDefault| on the ax node, since focus action only
  // changes an ax node's accessibility selected state to |kSelected=true| and
  // no |Action::kDoDefault| was performed on that node yet. So we need to
  // perform |Action::kDoDefault| on the ax node to cause its associated control
  // to be selected.
  if (selected == ISelectionItemProviderIsSelected() &&
      !GetBoolAttribute(ax::mojom::BoolAttribute::kSelectedFromFocus))
    return S_OK;

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return UIA_E_INVALIDOPERATION;
}

bool AXPlatformNodeWin::ISelectionItemProviderIsSelected() const {
  // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
  // SelectionItem.IsSelected is set according to the True or False value of
  // aria-checked for 'radio' and 'menuitemradio' roles.
  if (GetRole() == ax::mojom::Role::kRadioButton ||
      GetRole() == ax::mojom::Role::kMenuItemRadio)
    return GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue;

  // https://www.w3.org/TR/wai-aria-1.1/#aria-selected
  // SelectionItem.IsSelected is set according to the True or False value of
  // aria-selected.
  return GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

ToggleState AXPlatformNodeWin::GetToggleStateImpl() {
  const auto checked_state = GetData().GetCheckedState();
  if (checked_state == ax::mojom::CheckedState::kTrue) {
    return ToggleState_On;
  } else if (checked_state == ax::mojom::CheckedState::kMixed) {
    return ToggleState_Indeterminate;
  } else {
    return ToggleState_Off;
  }
}

bool AXPlatformNodeWin::IsNodeInaccessibleForUIA() const {
  // Ignored nodes and those that are descendants of a leaf node shouldn't be
  // exposed to UIA. For example, an atomic text field can have text children
  // in our internal AX tree but isn't supposed to have any in UIA's AX tree.
  // We also don't expose inline text boxes to UIA.
  return GetData().IsIgnored() ||
         GetRole() == ax::mojom::Role::kInlineTextBox || IsChildOfLeaf();
}

IFACEMETHODIMP AXPlatformNodeWin::AddToSelection() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("AddToSelection");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_ADDTOSELECTION);
  return ISelectionItemProviderSetSelected(true);
}

IFACEMETHODIMP AXPlatformNodeWin::RemoveFromSelection() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("RemoveFromSelection");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_REMOVEFROMSELECTION);
  return ISelectionItemProviderSetSelected(false);
}

IFACEMETHODIMP AXPlatformNodeWin::Select() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Select");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_SELECT);
  return ISelectionItemProviderSetSelected(true);
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsSelected(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_IsSelected");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_GET_ISSELECTED);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = ISelectionItemProviderIsSelected();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_SelectionContainer(
    IRawElementProviderSimple** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_SelectionContainer");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_GET_SELECTIONCONTAINER);
  UIA_VALIDATE_CALL_1_ARG(result);

  auto* node_win = static_cast<AXPlatformNodeWin*>(GetSelectionContainer());
  if (!node_win)
    return E_FAIL;

  node_win->AddRef();
  *result = static_cast<IRawElementProviderSimple*>(node_win);
  return S_OK;
}

//
// ISelectionProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::GetSelection(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetSelection");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTION_GETSELECTION);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::vector<AXPlatformNodeBase*> selected_children;
  int max_items = GetMaxSelectableItems();
  if (max_items)
    GetSelectedItems(max_items, &selected_children);

  LONG selected_children_count = selected_children.size();
  *result = SafeArrayCreateVector(VT_UNKNOWN, 0, selected_children_count);
  if (!*result)
    return E_OUTOFMEMORY;

  for (LONG i = 0; i < selected_children_count; ++i) {
    AXPlatformNodeWin* children =
        static_cast<AXPlatformNodeWin*>(selected_children[i]);
    HRESULT hr = SafeArrayPutElement(
        *result, &i, static_cast<IRawElementProviderSimple*>(children));
    if (FAILED(hr)) {
      SafeArrayDestroy(*result);
      *result = nullptr;
      return hr;
    }
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_CanSelectMultiple(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_CanSelectMultiple");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTION_GET_CANSELECTMULTIPLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = HasState(ax::mojom::State::kMultiselectable);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsSelectionRequired(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_IsSelectionRequired");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTION_GET_ISSELECTIONREQUIRED);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = HasState(ax::mojom::State::kRequired);
  return S_OK;
}

//
// ITableItemProvider methods.
//

IFACEMETHODIMP AXPlatformNodeWin::GetColumnHeaderItems(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetColumnHeaderItems");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLEITEM_GETCOLUMNHEADERITEMS);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::optional<int> column = GetTableColumn();
  if (!column)
    return E_FAIL;

  *result =
      CreateUIAElementsSafeArray(CreatePlatformNodeVectorFromRelationIdVector(
          GetDelegate()->GetColHeaderNodeIds(*column)));
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetRowHeaderItems(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetRowHeaderItems");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLEITEM_GETROWHEADERITEMS);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::optional<int> row = GetTableRow();
  if (!row)
    return E_FAIL;

  *result =
      CreateUIAElementsSafeArray(CreatePlatformNodeVectorFromRelationIdVector(
          GetDelegate()->GetRowHeaderNodeIds(*row)));
  return S_OK;
}

//
// ITableProvider methods.
//

IFACEMETHODIMP AXPlatformNodeWin::GetColumnHeaders(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetColumnHeaders");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLE_GETCOLUMNHEADERS);
  UIA_VALIDATE_CALL_1_ARG(result);

  *result =
      CreateUIAElementsSafeArray(CreatePlatformNodeVectorFromRelationIdVector(
          GetDelegate()->GetColHeaderNodeIds()));
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetRowHeaders(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetRowHeaders");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLE_GETROWHEADERS);
  UIA_VALIDATE_CALL_1_ARG(result);

  *result =
      CreateUIAElementsSafeArray(CreatePlatformNodeVectorFromRelationIdVector(
          GetDelegate()->GetRowHeaderNodeIds()));
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowOrColumnMajor(
    RowOrColumnMajor* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_RowOrColumnMajor");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLE_GET_ROWORCOLUMNMAJOR);
  UIA_VALIDATE_CALL_1_ARG(result);

  // Tables and ARIA grids are always in row major order
  // see AXPlatformNodeBase::GetTableCell
  *result = RowOrColumnMajor_RowMajor;
  return S_OK;
}

//
// IToggleProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Toggle() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Toggle");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TOGGLE_TOGGLE);
  UIA_VALIDATE_CALL();
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;

  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ToggleState(ToggleState* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_ToggleState");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TOGGLE_GET_TOGGLESTATE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetToggleStateImpl();
  return S_OK;
}

//
// IValueProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::SetValue(LPCWSTR value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("SetValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_VALUE_SETVALUE);
  UIA_VALIDATE_CALL();
  if (!value)
    return E_INVALIDARG;

  if (GetDelegate()->IsReadOnlyOrDisabled())
    return UIA_E_ELEMENTNOTENABLED;

  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = base::WideToUTF8(value);
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsReadOnly(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_IsReadOnly");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_VALUE_GET_ISREADONLY);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetDelegate()->IsReadOnlyOrDisabled();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Value(BSTR* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Value");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_VALUE_GET_VALUE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetValueAttributeAsBstr(this);
  return S_OK;
}

//
// IWindowProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::SetVisualState(
    WindowVisualState window_visual_state) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("SetVisualState");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_SETVISUALSTATE);
  UIA_VALIDATE_CALL();
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::Close() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Close");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_CLOSE);
  UIA_VALIDATE_CALL();
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::WaitForInputIdle(int milliseconds,
                                                   BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("WaitForInputIdle");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_WAITFORINPUTIDLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_CanMaximize(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_CanMaximize");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_CANMAXIMIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_CanMinimize(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_CanMinimize");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_CANMINIMIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsModal(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_IsModal");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_ISMODAL);
  UIA_VALIDATE_CALL_1_ARG(result);

  *result = GetBoolAttribute(ax::mojom::BoolAttribute::kModal);

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_WindowVisualState(
    WindowVisualState* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_WindowVisualState");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_WINDOWVISUALSTATE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_WindowInteractionState(
    WindowInteractionState* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_WindowInteractionState");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_WINDOWINTERACTIONSTATE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsTopmost(BOOL* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_IsTopmost");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_ISTOPMOST);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

//
// IRangeValueProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::SetValue(double value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("SetValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_SETVALUE);
  UIA_VALIDATE_CALL();
  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = base::NumberToString(value);
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_LargeChange(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_LargeChange");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_GET_LARGECHANGE);
  UIA_VALIDATE_CALL_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange,
                        &attribute)) {
    *result = attribute * kLargeChangeScaleFactor;
    return S_OK;
  } else {
    // For native sliders and spin buttons, when no explicit step value was
    // set, use the default value instead.
    std::string html_input_type =
        GetStringAttribute(ax::mojom::StringAttribute::kInputType);
    if (html_input_type == "range" || html_input_type == "number") {
      *result = kDefaultSmallChangeValue * kLargeChangeScaleFactor;
      return S_OK;
    }
    return E_FAIL;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Maximum(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Maximum");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_GET_MAXIMUM);
  UIA_VALIDATE_CALL_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Minimum(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Minimum");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_GET_MINIMUM);
  UIA_VALIDATE_CALL_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_SmallChange(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_SmallChange");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_GET_SMALLCHANGE);
  UIA_VALIDATE_CALL_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  } else {
    // For native sliders and spin buttons, when no explicit step value was
    // set, use the default value instead.
    std::string html_input_type =
        GetStringAttribute(ax::mojom::StringAttribute::kInputType);
    if (html_input_type == "range" || html_input_type == "number") {
      *result = kDefaultSmallChangeValue;
      return S_OK;
    }
    return E_FAIL;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Value(double* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_Value");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_GET_VALUE);
  UIA_VALIDATE_CALL_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

// IAccessibleEx methods not implemented.
IFACEMETHODIMP
AXPlatformNodeWin::ConvertReturnedElement(IRawElementProviderSimple* element,
                                          IAccessibleEx** acc) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("ConvertReturnedElement");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_CONVERT_RETURNED_ELEMENT);
  return E_NOTIMPL;
}

//
// IAccessibleTable methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_accessibleAt(LONG row,
                                                   LONG column,
                                                   IUnknown** accessible) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_accessibleAt");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACCESSIBLE_AT);
  COM_OBJECT_VALIDATE_1_ARG(accessible);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
  return node_win->QueryInterface(IID_PPV_ARGS(accessible));
}

IFACEMETHODIMP AXPlatformNodeWin::get_caption(IUnknown** accessible) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_caption");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CAPTION);
  COM_OBJECT_VALIDATE_1_ARG(accessible);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  AXPlatformNodeBase* caption = GetTableCaption();
  if (!caption)
    return S_FALSE;

  auto* node_win = static_cast<AXPlatformNodeWin*>(caption);
  return node_win->QueryInterface(IID_PPV_ARGS(accessible));
}

IFACEMETHODIMP AXPlatformNodeWin::get_childIndex(LONG row,
                                                 LONG column,
                                                 LONG* cell_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_childIndex");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CHILD_INDEX);
  COM_OBJECT_VALIDATE_1_ARG(cell_index);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  std::optional<int> index = cell->GetTableCellIndex();
  if (!index)
    return E_FAIL;

  *cell_index = LONG{*index};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnDescription(LONG column,
                                                        BSTR* description) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_columnDescription");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_DESCRIPTION);
  COM_OBJECT_VALIDATE_1_ARG(description);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  if (!columns)
    return E_FAIL;

  if (column < 0 || column >= *columns)
    return E_INVALIDARG;

  std::vector<int32_t> column_header_ids =
      GetDelegate()->GetColHeaderNodeIds(int{column});
  for (int32_t node_id : column_header_ids) {
    AXPlatformNodeWin* cell =
        static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(node_id));
    if (!cell)
      continue;

    std::wstring cell_name = base::UTF8ToWide(cell->GetName());
    if (!cell_name.empty()) {
      *description = SysAllocString(cell_name.c_str());
      return S_OK;
    }

    cell_name = base::UTF8ToWide(
        cell->GetStringAttribute(ax::mojom::StringAttribute::kDescription));
    if (!cell_name.empty()) {
      *description = SysAllocString(cell_name.c_str());
      return S_OK;
    }
  }

  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnExtentAt(LONG row,
                                                     LONG column,
                                                     LONG* n_columns_spanned) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_columnExtentAt");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_EXTENT_AT);
  COM_OBJECT_VALIDATE_1_ARG(n_columns_spanned);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  std::optional<int> column_span = cell->GetTableColumnSpan();
  if (!column_span)
    return E_FAIL;
  *n_columns_spanned = LONG{*column_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnHeader(
    IAccessibleTable** accessible_table,
    LONG* starting_row_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_columnHeader");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_HEADER);
  COM_OBJECT_VALIDATE_2_ARGS(accessible_table, starting_row_index);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  // Currently unimplemented.
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnIndex(LONG cell_index,
                                                  LONG* column_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_columnIndex");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_INDEX);
  COM_OBJECT_VALIDATE_1_ARG(column_index);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  AXPlatformNodeBase* cell = GetTableCell(cell_index);
  if (!cell)
    return E_INVALIDARG;

  std::optional<int> cell_column = cell->GetTableColumn();
  if (!cell_column)
    return E_FAIL;
  *column_index = LONG{*cell_column};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nColumns(LONG* column_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nColumns");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_COLUMNS);
  COM_OBJECT_VALIDATE_1_ARG(column_count);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);
  std::optional<int> columns = GetTableColumnCount();
  if (!columns)
    return E_FAIL;
  *column_count = LONG{*columns};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nRows(LONG* row_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nRows");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_ROWS);
  COM_OBJECT_VALIDATE_1_ARG(row_count);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);
  std::optional<int> rows = GetTableRowCount();
  if (!rows)
    return E_FAIL;
  *row_count = LONG{*rows};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedChildren(LONG* cell_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nSelectedChildren");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_CHILDREN);
  COM_OBJECT_VALIDATE_1_ARG(cell_count);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  LONG result = 0;
  for (int r = 0; r < *rows; ++r) {
    for (int c = 0; c < *columns; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell && cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
        ++result;
    }
  }
  *cell_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedColumns(LONG* column_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nSelectedColumns");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_COLUMNS);
  COM_OBJECT_VALIDATE_1_ARG(column_count);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  // If every cell in a column is selected, then that column is selected.
  LONG result = 0;
  for (int c = 0; c < *columns; ++c) {
    bool selected = true;
    for (int r = 0; r < *rows && selected == true; ++r) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell ||
          !(cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      ++result;
  }

  *column_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedRows(LONG* row_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nSelectedRows");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_ROWS);
  COM_OBJECT_VALIDATE_1_ARG(row_count);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  // If every cell in a row is selected, then that row is selected.
  LONG result = 0;
  for (int r = 0; r < *rows; ++r) {
    bool selected = true;
    for (int c = 0; c < *columns && selected == true; ++c) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell ||
          !(cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      ++result;
  }

  *row_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowDescription(LONG row,
                                                     BSTR* description) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowDescription");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_DESCRIPTION);
  COM_OBJECT_VALIDATE_1_ARG(description);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> rows = GetTableRowCount();
  if (!rows)
    return E_FAIL;

  if (row < 0 || row >= *rows)
    return E_INVALIDARG;

  std::vector<int32_t> row_header_ids =
      GetDelegate()->GetRowHeaderNodeIds(int{row});
  for (int32_t node_id : row_header_ids) {
    AXPlatformNodeWin* cell =
        static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(node_id));
    if (!cell)
      continue;

    std::wstring cell_name = base::UTF8ToWide(cell->GetName());
    if (!cell_name.empty()) {
      *description = SysAllocString(cell_name.c_str());
      return S_OK;
    }

    cell_name = base::UTF8ToWide(
        cell->GetStringAttribute(ax::mojom::StringAttribute::kDescription));
    if (!cell_name.empty()) {
      *description = SysAllocString(cell_name.c_str());
      return S_OK;
    }
  }

  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowExtentAt(LONG row,
                                                  LONG column,
                                                  LONG* n_rows_spanned) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowExtentAt");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_EXTENT_AT);
  COM_OBJECT_VALIDATE_1_ARG(n_rows_spanned);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  const AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  std::optional<int> cell_row_span = cell->GetTableRowSpan();
  if (!cell_row_span)
    return E_FAIL;
  *n_rows_spanned = LONG{*cell_row_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowHeader(
    IAccessibleTable** accessible_table,
    LONG* starting_column_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowHeader");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_HEADER);
  COM_OBJECT_VALIDATE_2_ARGS(accessible_table, starting_column_index);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  // Currently unimplemented.
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowIndex(LONG cell_index,
                                               LONG* row_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowIndex");
  COM_OBJECT_VALIDATE_1_ARG(row_index);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  const AXPlatformNodeBase* cell = GetTableCell(cell_index);
  if (!cell)
    return E_INVALIDARG;

  std::optional<int> cell_row = cell->GetTableRow();
  if (!cell_row)
    return E_FAIL;
  *row_index = LONG{*cell_row};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedChildren(LONG max_children,
                                                       LONG** children,
                                                       LONG* n_children) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selectedChildren");
  COM_OBJECT_VALIDATE_2_ARGS(children, n_children);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  if (max_children <= 0)
    return E_INVALIDARG;

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  std::vector<LONG> results;
  for (int r = 0; r < *rows; ++r) {
    for (int c = 0; c < *columns; ++c) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell && cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
        std::optional<int> cell_index = cell->GetTableCellIndex();
        if (!cell_index)
          return E_FAIL;

        results.push_back(*cell_index);
      }
    }
  }

  return AllocateComArrayFromVector(results, max_children, children,
                                    n_children);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedColumns(LONG max_columns,
                                                      LONG** columns,
                                                      LONG* n_columns) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selectedColumns");
  COM_OBJECT_VALIDATE_2_ARGS(columns, n_columns);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  if (max_columns <= 0)
    return E_INVALIDARG;

  std::optional<int> column_count = GetTableColumnCount();
  std::optional<int> row_count = GetTableRowCount();
  if (!column_count || !row_count)
    return E_FAIL;

  std::vector<LONG> results;
  for (int c = 0; c < *column_count; ++c) {
    bool selected = true;
    for (int r = 0; r < *row_count && selected == true; ++r) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell ||
          !(cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      results.push_back(c);
  }

  return AllocateComArrayFromVector(results, max_columns, columns, n_columns);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedRows(LONG max_rows,
                                                   LONG** rows,
                                                   LONG* n_rows) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selectedRows");
  COM_OBJECT_VALIDATE_2_ARGS(rows, n_rows);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  if (max_rows <= 0)
    return E_INVALIDARG;

  std::optional<int> column_count = GetTableColumnCount();
  std::optional<int> row_count = GetTableRowCount();
  if (!column_count || !row_count)
    return E_FAIL;

  std::vector<LONG> results;
  for (int r = 0; r < *row_count; ++r) {
    bool selected = true;
    for (int c = 0; c < *column_count && selected == true; ++c) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell ||
          !(cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      results.push_back(r);
  }

  return AllocateComArrayFromVector(results, max_rows, rows, n_rows);
}

IFACEMETHODIMP AXPlatformNodeWin::get_summary(IUnknown** accessible) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_summary");
  COM_OBJECT_VALIDATE_1_ARG(accessible);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  // Current unimplemented.
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isColumnSelected(LONG column,
                                                       boolean* is_selected) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_isColumnSelected");
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  if (column < 0 || column >= *columns)
    return E_INVALIDARG;

  for (int r = 0; r < *rows; ++r) {
    const AXPlatformNodeBase* cell = GetTableCell(r, column);
    if (!cell || !(cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)))
      return S_OK;
  }

  *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isRowSelected(LONG row,
                                                    boolean* is_selected) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_isRowSelected");
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  if (row < 0 || row >= *rows)
    return E_INVALIDARG;

  for (int c = 0; c < *columns; ++c) {
    const AXPlatformNodeBase* cell = GetTableCell(row, c);
    if (!cell || !(cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)))
      return S_OK;
  }

  *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isSelected(LONG row,
                                                 LONG column,
                                                 boolean* is_selected) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_isSelected");
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  const AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  if (cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    *is_selected = true;

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowColumnExtentsAtIndex(
    LONG index,
    LONG* row,
    LONG* column,
    LONG* row_extents,
    LONG* column_extents,
    boolean* is_selected) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowColumnExtentsAtIndex");
  COM_OBJECT_VALIDATE_5_ARGS(row, column, row_extents, column_extents,
                             is_selected);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  const AXPlatformNodeBase* cell = GetTableCell(index);
  if (!cell)
    return E_INVALIDARG;

  std::optional<int> row_index = cell->GetTableRow();
  std::optional<int> column_index = cell->GetTableColumn();
  std::optional<int> row_span = cell->GetTableRowSpan();
  std::optional<int> column_span = cell->GetTableColumnSpan();
  if (!row_index || !column_index || !row_span || !column_span)
    return E_FAIL;

  *row = LONG{*row_index};
  *column = LONG{*column_index};
  *row_extents = LONG{*row_span};
  *column_extents = LONG{*column_span};
  if (cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::selectRow(LONG row) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("selectRow");
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> rows = GetTableRowCount();
  if (!rows)
    return E_FAIL;

  if (row < 0 || row >= *rows)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::selectColumn(LONG column) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("selectColumn");
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  if (!columns)
    return E_FAIL;

  if (column < 0 || column >= *columns)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::unselectRow(LONG row) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("unselectRow");
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> rows = GetTableRowCount();
  if (!rows)
    return E_FAIL;

  if (row < 0 || row >= *rows)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::unselectColumn(LONG column) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("unselectColumn");
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  if (!columns)
    return E_FAIL;

  if (column < 0 || column >= *columns)
    return E_INVALIDARG;

  // Currently unimplemented.
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_modelChange(
    IA2TableModelChange* model_change) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_modelChange");
  COM_OBJECT_VALIDATE_1_ARG(model_change);

  // Currently unimplemented.
  return E_NOTIMPL;
}

//
// IAccessibleTable2 methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_cellAt(LONG row,
                                             LONG column,
                                             IUnknown** cell) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_cellAt");
  COM_OBJECT_VALIDATE_1_ARG(cell);
  NotifyAddAXModeFlagsForIA2(AXMode::kScreenReader);

  AXPlatformNodeBase* table_cell = GetTableCell(int{row}, int{column});
  if (!table_cell)
    return E_INVALIDARG;

  auto* node_win = static_cast<AXPlatformNodeWin*>(table_cell);
  return node_win->QueryInterface(IID_PPV_ARGS(cell));
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedCells(LONG* cell_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nSelectedCells");
  // Note that this method does not need to set any ax mode since it
  // calls into get_nSelectedChildren() which does.
  return get_nSelectedChildren(cell_count);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedCells(IUnknown*** cells,
                                                    LONG* n_selected_cells) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selectedCells");
  COM_OBJECT_VALIDATE_2_ARGS(cells, n_selected_cells);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> columns = GetTableColumnCount();
  std::optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  std::vector<AXPlatformNodeBase*> selected;
  for (int r = 0; r < *rows; ++r) {
    for (int c = 0; c < *columns; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell && cell->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
        selected.push_back(cell);
    }
  }

  *n_selected_cells = static_cast<LONG>(selected.size());
  *cells = static_cast<IUnknown**>(
      CoTaskMemAlloc(selected.size() * sizeof(IUnknown*)));

  for (size_t i = 0; i < selected.size(); ++i) {
    auto* node_win = static_cast<AXPlatformNodeWin*>(selected[i]);
    node_win->QueryInterface(IID_PPV_ARGS(&(*cells)[i]));
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedColumns(LONG** columns,
                                                      LONG* n_columns) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selectedColumns");
  return get_selectedColumns(INT_MAX, columns, n_columns);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedRows(LONG** rows, LONG* n_rows) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selectedRows");
  return get_selectedRows(INT_MAX, rows, n_rows);
}

//
// IAccessibleTableCell methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_columnExtent(LONG* n_columns_spanned) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_columnExtent");
  COM_OBJECT_VALIDATE_1_ARG(n_columns_spanned);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> column_span = GetTableColumnSpan();
  if (!column_span)
    return E_FAIL;
  *n_columns_spanned = LONG{*column_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnHeaderCells(
    IUnknown*** cell_accessibles,
    LONG* n_column_header_cells) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_columnHeaderCells");
  COM_OBJECT_VALIDATE_2_ARGS(cell_accessibles, n_column_header_cells);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> column = GetTableColumn();
  if (!column)
    return E_FAIL;

  std::vector<int32_t> column_header_ids =
      GetDelegate()->GetColHeaderNodeIds(*column);
  *cell_accessibles = static_cast<IUnknown**>(
      CoTaskMemAlloc(column_header_ids.size() * sizeof(IUnknown*)));
  int index = 0;
  for (int32_t node_id : column_header_ids) {
    AXPlatformNodeWin* node_win =
        static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(node_id));
    if (node_win) {
      node_win->QueryInterface(IID_PPV_ARGS(&(*cell_accessibles)[index]));
      ++index;
    }
  }

  *n_column_header_cells = static_cast<LONG>(column_header_ids.size());
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnIndex(LONG* column_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_columnIndex");
  COM_OBJECT_VALIDATE_1_ARG(column_index);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> column = GetTableColumn();
  if (!column)
    return E_FAIL;
  *column_index = LONG{*column};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowExtent(LONG* n_rows_spanned) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowExtent");
  COM_OBJECT_VALIDATE_1_ARG(n_rows_spanned);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> row_span = GetTableRowSpan();
  if (!row_span)
    return E_FAIL;
  *n_rows_spanned = LONG{*row_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowHeaderCells(
    IUnknown*** cell_accessibles,
    LONG* n_row_header_cells) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowHeaderCells");
  COM_OBJECT_VALIDATE_2_ARGS(cell_accessibles, n_row_header_cells);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> row = GetTableRow();
  if (!row)
    return E_FAIL;

  std::vector<int32_t> row_header_ids =
      GetDelegate()->GetRowHeaderNodeIds(*row);
  *cell_accessibles = static_cast<IUnknown**>(
      CoTaskMemAlloc(row_header_ids.size() * sizeof(IUnknown*)));
  int index = 0;
  for (int32_t node_id : row_header_ids) {
    AXPlatformNodeWin* node_win =
        static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(node_id));
    if (node_win) {
      node_win->QueryInterface(IID_PPV_ARGS(&(*cell_accessibles)[index]));
      ++index;
    }
  }

  *n_row_header_cells = static_cast<LONG>(row_header_ids.size());
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowIndex(LONG* row_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowIndex");
  COM_OBJECT_VALIDATE_1_ARG(row_index);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> row = GetTableRow();
  if (!row)
    return E_FAIL;
  *row_index = LONG{*row};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isSelected(boolean* is_selected) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_isSelected");
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  if (GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowColumnExtents(LONG* row_index,
                                                       LONG* column_index,
                                                       LONG* row_extents,
                                                       LONG* column_extents,
                                                       boolean* is_selected) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_rowColumnExtents");
  COM_OBJECT_VALIDATE_5_ARGS(row_index, column_index, row_extents,
                             column_extents, is_selected);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  std::optional<int> row = GetTableRow();
  std::optional<int> column = GetTableColumn();
  std::optional<int> row_span = GetTableRowSpan();
  std::optional<int> column_span = GetTableColumnSpan();
  if (!row || !column || !row_span || !column_span)
    return E_FAIL;

  *row_index = LONG{*row};
  *column_index = LONG{*column};
  *row_extents = LONG{*row_span};
  *column_extents = LONG{*column_span};
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_table(IUnknown** table) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_table");
  COM_OBJECT_VALIDATE_1_ARG(table);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  AXPlatformNodeBase* table_node = GetTable();
  if (!table_node)
    return E_FAIL;

  auto* node_win = static_cast<AXPlatformNodeWin*>(table_node);
  return node_win->QueryInterface(IID_PPV_ARGS(table));
}

//
// IAccessibleText
//

IFACEMETHODIMP AXPlatformNodeWin::get_nCharacters(LONG* n_characters) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nCharacters");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_CHARACTERS);
  COM_OBJECT_VALIDATE_1_ARG(n_characters);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode |
                             AXMode::kInlineTextBoxes);

  std::u16string text = GetHypertext();
  *n_characters = static_cast<LONG>(text.size());

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_caretOffset(LONG* offset) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_caretOffset");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CARET_OFFSET);
  COM_OBJECT_VALIDATE_1_ARG(offset);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);
  *offset = 0;

  if (!HasVisibleCaretOrSelection())
    return S_FALSE;

  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  // The caret is always at the end of the selection.
  *offset = selection_end;
  if (*offset < 0)
    return S_FALSE;

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelections(LONG* n_selections) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nSelections");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTIONS);
  COM_OBJECT_VALIDATE_1_ARG(n_selections);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  *n_selections = 0;
  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  if (selection_start >= 0 && selection_end >= 0 &&
      selection_start != selection_end) {
    *n_selections = 1;
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_selection(LONG selection_index,
                                                LONG* start_offset,
                                                LONG* end_offset) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selection");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_SELECTION);
  COM_OBJECT_VALIDATE_2_ARGS(start_offset, end_offset);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  if (!start_offset || !end_offset || selection_index != 0)
    return E_INVALIDARG;

  *start_offset = 0;
  *end_offset = 0;
  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  if (selection_start >= 0 && selection_end >= 0 &&
      selection_start != selection_end) {
    // We should ignore the direction of the selection when exposing start and
    // end offsets. According to the IA2 Spec the end offset is always
    // increased by one past the end of the selection. This wouldn't make
    // sense if end < start.
    if (selection_end < selection_start)
      std::swap(selection_start, selection_end);

    *start_offset = selection_start;
    *end_offset = selection_end;
    return S_OK;
  }

  return E_INVALIDARG;
}

IFACEMETHODIMP AXPlatformNodeWin::get_text(LONG start_offset,
                                           LONG end_offset,
                                           BSTR* text) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_text");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT);
  COM_OBJECT_VALIDATE_1_ARG(text);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);
  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset)
    std::swap(start_offset, end_offset);

  const std::u16string str = GetHypertext();
  LONG str_len = static_cast<LONG>(str.length());
  if (start_offset < 0 || start_offset > str_len)
    return E_INVALIDARG;
  if (end_offset < 0 || end_offset > str_len)
    return E_INVALIDARG;

  std::u16string substr = str.substr(start_offset, end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(base::as_wcstr(substr));
  DCHECK(*text);
  return S_OK;
}

HRESULT AXPlatformNodeWin::IAccessibleTextGetTextForOffsetType(
    TextOffsetType text_offset_type,
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  COM_OBJECT_VALIDATE_3_ARGS(start_offset, end_offset, text);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode |
                             AXMode::kInlineTextBoxes);

  HandleSpecialTextOffset(&offset);
  if (offset < 0)
    return E_INVALIDARG;

  const std::u16string& text_str = GetHypertext();
  LONG text_len = text_str.length();

  // https://accessibility.linuxfoundation.org/a11yspecs/ia2/docs/html/interface_i_accessible_text.html
  // All methods that operate on particular characters use character indices
  // (e.g. IAccessibleText::textAtOffset) from 0 to length-1.
  if (offset >= text_len) {
    // We aren't strictly following the spec here by allowing offset to be equal
    // to the text length for IA2_TEXT_BOUNDARY_LINE in case screen readers
    // expect this behavior which has existed since Feb. 2015,
    // commit: 6baff46f520e31ff92669890207be5708064d16e.
    const bool offset_for_line_text_len =
        offset == text_len && boundary_type == IA2_TEXT_BOUNDARY_LINE;
    if (!offset_for_line_text_len)
      return E_INVALIDARG;
  }

  LONG start, end;
  switch (text_offset_type) {
    case TextOffsetType::kAtOffset: {
      end = FindBoundary(boundary_type, offset,
                         ax::mojom::MoveDirection::kForward);
      // Early return if the range will be degenerate containing no text.
      if (end <= 0)
        return S_FALSE;
      start = FindBoundary(boundary_type, offset,
                           ax::mojom::MoveDirection::kBackward);
      break;
    }
    case TextOffsetType::kBeforeOffset: {
      // Find the start of the boundary at |offset| and assign to |end|,
      // then find the start of the preceding boundary and assign to |start|.
      end = FindBoundary(boundary_type, offset,
                         ax::mojom::MoveDirection::kBackward);
      // Early return if the range will be degenerate containing no text,
      // or the range is after |offset|. Because the character at |offset| must
      // be excluded, |end| and |offset| may be equal.
      if (end <= 0 || end > offset)
        return S_FALSE;
      start = FindBoundary(boundary_type, end - 1,
                           ax::mojom::MoveDirection::kBackward);
      break;
    }
    case TextOffsetType::kAfterOffset: {
      // Find the end of the boundary at |offset| and assign to |start|,
      // then find the end of the following boundary and assign to |end|.
      start = FindBoundary(boundary_type, offset,
                           ax::mojom::MoveDirection::kForward);
      // Early return if the range will be degenerate containing no text,
      // or the range is before or includes|offset|. Because the character at
      // |offset| must be excluded, |start| and |offset| cannot be equal.
      if (start >= text_len || start <= offset)
        return S_FALSE;
      end = FindBoundary(boundary_type, start,
                         ax::mojom::MoveDirection::kForward);
      break;
    }
  }

  DCHECK_LE(start, end);
  if (start >= end)
    return S_FALSE;

  *start_offset = start;
  *end_offset = end;
  return get_text(start, end, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_textAtOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_textAtOffset");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT_AT_OFFSET);
  return IAccessibleTextGetTextForOffsetType(TextOffsetType::kAtOffset, offset,
                                             boundary_type, start_offset,
                                             end_offset, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_textBeforeOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_textBeforeOffset");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT_BEFORE_OFFSET);
  return IAccessibleTextGetTextForOffsetType(TextOffsetType::kBeforeOffset,
                                             offset, boundary_type,
                                             start_offset, end_offset, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_textAfterOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_textAfterOffset");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT_AFTER_OFFSET);
  return IAccessibleTextGetTextForOffsetType(TextOffsetType::kAfterOffset,
                                             offset, boundary_type,
                                             start_offset, end_offset, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_offsetAtPoint(
    LONG x,
    LONG y,
    enum IA2CoordinateType coord_type,
    LONG* offset) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_offsetAtPoint");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_OFFSET_AT_POINT);
  COM_OBJECT_VALIDATE_1_ARG(offset);

  *offset = -1;

  if (coord_type == IA2CoordinateType::IA2_COORDTYPE_PARENT_RELATIVE) {
    // We don't support when the IA2 coordinate type is parent relative, but
    // we have to return something rather than E_NOTIMPL or screen readers
    // will complain.
    // See http://crbug.com/1010726
    NOTIMPLEMENTED_LOG_ONCE();
    return S_FALSE;
  }

  // We currently only handle IA2 screen relative coord type.
  DCHECK_EQ(coord_type, IA2_COORDTYPE_SCREEN_RELATIVE);

  const AXPlatformNodeWin* hit_child = static_cast<AXPlatformNodeWin*>(
      FromNativeViewAccessible(GetDelegate()->HitTestSync(x, y)));

  if (!hit_child || !hit_child->IsText()) {
    return S_FALSE;
  }

  for (int i = 0, text_length = hit_child->GetTextContentUTF16().length();
       i < text_length; ++i) {
    gfx::Rect char_bounds =
        hit_child->GetDelegate()->GetInnerTextRangeBoundsRect(
            i, i + 1, AXCoordinateSystem::kScreenDIPs,
            AXClippingBehavior::kUnclipped);
    if (char_bounds.Contains(x, y)) {
      *offset = i;
      break;
    }
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::addSelection(LONG start_offset,
                                               LONG end_offset) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("addSelection");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ADD_SELECTION);
  COM_OBJECT_VALIDATE();
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  // We only support one selection.
  return setSelection(0, start_offset, end_offset);
}

IFACEMETHODIMP AXPlatformNodeWin::removeSelection(LONG selection_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("removeSelection");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_REMOVE_SELECTION);
  COM_OBJECT_VALIDATE();
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  if (selection_index != 0)
    return E_INVALIDARG;
  // Simply collapse the selection to the position of the caret if a caret is
  // visible, otherwise set the selection to 0.
  return setCaretOffset(GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));
}

IFACEMETHODIMP AXPlatformNodeWin::setCaretOffset(LONG offset) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("setCaretOffset");
  return setSelection(0, offset, offset);
}

IFACEMETHODIMP AXPlatformNodeWin::setSelection(LONG selection_index,
                                               LONG start_offset,
                                               LONG end_offset) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("setSelection");
  if (selection_index != 0)
    return E_INVALIDARG;

  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);
  if (start_offset < 0 ||
      start_offset > static_cast<LONG>(GetHypertext().length())) {
    return E_INVALIDARG;
  }
  if (end_offset < 0 ||
      end_offset > static_cast<LONG>(GetHypertext().length())) {
    return E_INVALIDARG;
  }

  if (SetHypertextSelection(int{start_offset}, int{end_offset})) {
    return S_OK;
  }
  return E_FAIL;
}

//
// IAccessibleTextSelectionContainer methods.
//

IFACEMETHODIMP
AXPlatformNodeWin::get_selections(IA2TextSelection** selections,
                                  LONG* nSelections) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_selections");
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  COM_OBJECT_VALIDATE_2_ARGS(selections, nSelections);

  AXSelection unignored_selection = GetDelegate()->GetUnignoredSelection();

  AXNodeID anchor_id = unignored_selection.anchor_object_id;
  AXPlatformNodeWin* anchor_node =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(anchor_id));
  if (!anchor_node)
    return E_FAIL;

  // If the selection endpoint is inside this object and therefore, at least
  // from this side, we do not need to crop the selection. Simply convert
  // selection anchor/focus offset to a hypertext offset within anchor/focus
  // object. Otherwise per the IA2 Spec, we need to crop the selection to be
  // within this object. `AXPlatformNodeBase::GetHypertextOffsetFromEndpoint`
  // can correctly handle endpoint offsets that are outside this object by
  // returning either 0 or hypertext length; see the related comment in
  // the method's declaration.

  int anchor_offset = unignored_selection.anchor_offset;
  if (anchor_node->IsDescendant(this)) {
    anchor_offset =
        anchor_node->GetHypertextOffsetFromEndpoint(anchor_node, anchor_offset);
  } else {
    anchor_offset = GetHypertextOffsetFromEndpoint(anchor_node, anchor_offset);
    anchor_node = this;
  }
  DCHECK_GE(anchor_offset, 0)
      << "This value is unexpected here, since we have already determined in "
         "this method that anchor_object is in the accessibility tree.";

  AXNodeID focus_id = unignored_selection.focus_object_id;
  AXPlatformNodeWin* focus_node =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(focus_id));
  if (!focus_node)
    return E_FAIL;

  int focus_offset = unignored_selection.focus_offset;
  if (focus_node->IsDescendant(this)) {
    focus_offset =
        focus_node->GetHypertextOffsetFromEndpoint(focus_node, focus_offset);
  } else {
    focus_offset = GetHypertextOffsetFromEndpoint(focus_node, focus_offset);
    focus_node = this;
  }
  DCHECK_GE(focus_offset, 0)
      << "This value is unexpected here, since we have already determined in "
         "this method that focus_object is in the accessibility tree.";

  if (anchor_node == focus_node && anchor_offset == focus_offset)
    return S_FALSE;  // No selection within this subtree.

  Microsoft::WRL::ComPtr<IAccessibleText> anchor_text_node;
  if (FAILED(anchor_node->QueryInterface(IID_PPV_ARGS(&anchor_text_node))))
    return E_FAIL;

  Microsoft::WRL::ComPtr<IAccessibleText> focus_text_node;
  if (FAILED(focus_node->QueryInterface(IID_PPV_ARGS(&focus_text_node))))
    return E_FAIL;

  *selections = reinterpret_cast<IA2TextSelection*>(
      CoTaskMemAlloc(sizeof(IA2TextSelection)));

  if (unignored_selection.is_backward) {
    selections[0]->startObj = focus_text_node.Detach();
    selections[0]->startOffset = focus_offset;
    selections[0]->endObj = anchor_text_node.Detach();
    selections[0]->endOffset = anchor_offset;
    selections[0]->startIsActive = true;
  } else {
    selections[0]->startObj = anchor_text_node.Detach();
    selections[0]->startOffset = anchor_offset;
    selections[0]->endObj = focus_text_node.Detach();
    selections[0]->endOffset = focus_offset;
    selections[0]->startIsActive = false;
  }

  *nSelections = 1;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::setSelections(LONG nSelections,
                                                IA2TextSelection* selections) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("setSelections");
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  COM_OBJECT_VALIDATE();

  // Chromium does not currently support more than one selection.
  if (nSelections != 1 || !selections)
    return E_INVALIDARG;

  if (!selections->startObj || !selections->endObj)
    return E_INVALIDARG;

  Microsoft::WRL::ComPtr<IAccessible> start_obj;
  if (FAILED(selections->startObj->QueryInterface(IID_PPV_ARGS(&start_obj))))
    return E_INVALIDARG;

  Microsoft::WRL::ComPtr<IAccessible> end_obj;
  if (FAILED(selections->endObj->QueryInterface(IID_PPV_ARGS(&end_obj))))
    return E_INVALIDARG;

  const auto* start_node = static_cast<AXPlatformNodeWin*>(
      FromNativeViewAccessible(start_obj.Get()));
  const auto* end_node =
      static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(end_obj.Get()));
  if (!start_node || !end_node)
    return E_INVALIDARG;

  AXPosition start_position =
      start_node->HypertextOffsetToEndpoint(selections->startOffset)
          ->AsDomSelectionPosition();
  AXPosition end_position =
      end_node->HypertextOffsetToEndpoint(selections->endOffset)
          ->AsDomSelectionPosition();
  if (start_position->IsNullPosition() || end_position->IsNullPosition())
    return E_INVALIDARG;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.target_tree_id = start_position->tree_id();
  int start_offset = start_position->IsTextPosition()
                         ? start_position->text_offset()
                         : start_position->child_index();
  int end_offset = end_position->IsTextPosition() ? end_position->text_offset()
                                                  : end_position->child_index();
  if (selections->startIsActive) {
    action_data.focus_node_id = start_position->anchor_id();
    action_data.focus_offset = start_offset;
    action_data.anchor_node_id = end_position->anchor_id();
    action_data.anchor_offset = end_offset;
  } else {
    action_data.anchor_node_id = start_position->anchor_id();
    action_data.anchor_offset = start_offset;
    action_data.focus_node_id = end_position->anchor_id();
    action_data.focus_offset = end_offset;
  }

  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;

  return S_FALSE;
}

//
// IAccessibleHypertext methods not implemented.
//

IFACEMETHODIMP AXPlatformNodeWin::get_nHyperlinks(LONG* hyperlink_count) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_nHyperlinks");
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_hyperlink(
    LONG index,
    IAccessibleHyperlink** hyperlink) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_hyperlink");
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_hyperlinkIndex(LONG char_index,
                                                     LONG* hyperlink_index) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_hyperlinkIndex");
  return E_NOTIMPL;
}

//
// IAccessibleText methods not implemented.
//

IFACEMETHODIMP AXPlatformNodeWin::get_newText(IA2TextSegment* new_text) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_newText");
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::get_oldText(IA2TextSegment* old_text) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_oldText");
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::get_characterExtents(
    LONG offset,
    enum IA2CoordinateType coord_type,
    LONG* x,
    LONG* y,
    LONG* width,
    LONG* height) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_characterExtents");
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::scrollSubstringTo(
    LONG start_index,
    LONG end_index,
    enum IA2ScrollType scroll_type) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("scrollSubstringTo");
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::scrollSubstringToPoint(
    LONG start_index,
    LONG end_index,
    enum IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("scrollSubstringToPoint");
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::get_attributes(LONG offset,
                                                 LONG* start_offset,
                                                 LONG* end_offset,
                                                 BSTR* text_attributes) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_attributes");
  return E_NOTIMPL;
}

//
// IAccessibleValue methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_currentValue(VARIANT* value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_currentValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CURRENT_VALUE);
  COM_OBJECT_VALIDATE_1_ARG(value);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  float float_val;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_minimumValue(VARIANT* value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_minimumValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_MINIMUM_VALUE);
  COM_OBJECT_VALIDATE_1_ARG(value);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  float float_val;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_maximumValue(VARIANT* value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_maximumValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_MAXIMUM_VALUE);
  COM_OBJECT_VALIDATE_1_ARG(value);
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  float float_val;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::setCurrentValue(VARIANT new_value) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("setCurrentValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SET_CURRENT_VALUE);
  COM_OBJECT_VALIDATE();
  NotifyAddAXModeFlagsForIA2(kScreenReaderAccessibilityMode);

  double double_value = 0.0;
  if (V_VT(&new_value) == VT_R8)
    double_value = V_R8(&new_value);
  else if (V_VT(&new_value) == VT_R4)
    double_value = V_R4(&new_value);
  else if (V_VT(&new_value) == VT_I4)
    double_value = V_I4(&new_value);
  else
    return E_INVALIDARG;

  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = base::NumberToString(double_value);
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

//
// IRawElementProviderFragment implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Navigate(
    NavigateDirection direction,
    IRawElementProviderFragment** element_provider) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("Navigate");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_NAVIGATE);
  WIN_ACCESSIBILITY_API_PERF_HISTOGRAM(UMA_API_NAVIGATE);
  UIA_VALIDATE_CALL_1_ARG(element_provider);

  *element_provider = nullptr;

  //
  // Navigation to a fragment root node:
  //
  // In order for the platform-neutral accessibility tree to support IA2 and UIA
  // simultaneously, we handle navigation to and from fragment roots in UIA
  // specific code. Consider the following platform-neutral tree:
  //
  //         N1
  //   _____/ \_____
  //  /             \
  // N2---N3---N4---N5
  //     / \       / \
  //   N6---N7   N8---N9
  //
  // N3 and N5 are nodes for which we need a fragment root. This will correspond
  // to the following tree in UIA:
  //
  //         U1
  //   _____/ \_____
  //  /             \
  // U2---R3---U4---R5
  //      |         |
  //      U3        U5
  //     / \       / \
  //   U6---U7   U8---U9
  //
  // Ux is the platform node for Nx.
  // R3 and R5 are the fragment root nodes for U3 and U5 respectively.
  //
  // Navigation has the following behaviors:
  //
  // 1. Parent navigation: If source node Ux is the child of a fragment root,
  //    return Rx. Otherwise, consult the platform-neutral tree.
  // 2. First/last child navigation: If target node Ux is the child of a
  //    fragment root and the source node isn't Rx, return Rx. Otherwise, return
  //    Ux.
  // 3. Next/previous sibling navigation:
  //    a. If source node Ux is the child of a fragment root, return nullptr.
  //    b. If target node Ux is the child of a fragment root, return Rx.
  //       Otherwise, return Ux.
  //
  // Note that the condition in 3b is a special case of the condition in 2. In
  // 3b, the source node is never Rx. So in the code, we collapse them to a
  // common implementation.
  //
  // Navigation from an Rx node is set up by delegate APIs on AXFragmentRootWin.
  //
  gfx::NativeViewAccessible neighbor = nullptr;
  switch (direction) {
    case NavigateDirection_Parent: {
      // 1. If source node Ux is the child of a fragment root, return Rx.
      // Otherwise, consult the platform-neutral tree.
      AXFragmentRootWin* fragment_root =
          AXFragmentRootWin::GetFragmentRootParentOf(GetNativeViewAccessible());
      if (fragment_root) [[unlikely]] {
        neighbor = fragment_root->GetNativeViewAccessible();
      } else {
        neighbor = GetParent();
      }
    } break;

    case NavigateDirection_FirstChild:
      if (GetChildCount() > 0)
        neighbor = GetFirstChild()->GetNativeViewAccessible();
      break;

    case NavigateDirection_LastChild:
      if (GetChildCount() > 0)
        neighbor = GetLastChild()->GetNativeViewAccessible();
      break;

    case NavigateDirection_NextSibling:
      // 3a. If source node Ux is the child of a fragment root, return nullptr.
      if (AXFragmentRootWin::GetFragmentRootParentOf(
              GetNativeViewAccessible()) == nullptr) {
        AXPlatformNodeBase* neighbor_node = GetNextSibling();
        if (neighbor_node)
          neighbor = neighbor_node->GetNativeViewAccessible();
      }
      break;

    case NavigateDirection_PreviousSibling:
      // 3a. If source node Ux is the child of a fragment root, return nullptr.
      if (AXFragmentRootWin::GetFragmentRootParentOf(
              GetNativeViewAccessible()) == nullptr) {
        AXPlatformNodeBase* neighbor_node = GetPreviousSibling();
        if (neighbor_node)
          neighbor = neighbor_node->GetNativeViewAccessible();
      }
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (neighbor) {
    if (direction != NavigateDirection_Parent) {
      // 2 / 3b. If target node Ux is the child of a fragment root and the
      // source node isn't Rx, return Rx.
      AXFragmentRootWin* fragment_root =
          AXFragmentRootWin::GetFragmentRootParentOf(neighbor);
      if (fragment_root && fragment_root != GetDelegate()) [[unlikely]] {
        neighbor = fragment_root->GetNativeViewAccessible();
      }
    }
    neighbor->QueryInterface(IID_PPV_ARGS(element_provider));
  }

  return S_OK;
}

void AXPlatformNodeWin::GetRuntimeIdArray(
    AXPlatformNodeWin::RuntimeIdArray& runtime_id) {
  runtime_id[0] = UiaAppendRuntimeId;

  // The combination of tree/frame id and Blink (DOM) id is unique and gives
  // nodes stable ids across layouts/tree movement. If there's a valid tree
  // id, use that, otherwise fall back to the globally unique id.
  int dom_id = GetData().GetDOMNodeId();
  if (dom_id) {
    AXTreeID tree_id = GetDelegate()->GetTreeData().tree_id;
    if (tree_id != AXTreeIDUnknown()) {
      AXActionHandlerRegistry::FrameID frame_id =
          AXActionHandlerRegistry::GetInstance()->GetFrameID(tree_id);
      runtime_id[1] = frame_id.first;
      runtime_id[2] = frame_id.second;
      runtime_id[3] = dom_id;
      return;
    }
  }

  runtime_id[1] = 0;
  runtime_id[2] = 0;
  runtime_id[3] = GetUniqueId();
}

IFACEMETHODIMP AXPlatformNodeWin::GetRuntimeId(SAFEARRAY** runtime_id) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetRuntimeId");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_RUNTIME_ID);
  UIA_VALIDATE_CALL_1_ARG(runtime_id);

  RuntimeIdArray id_array;
  GetRuntimeIdArray(id_array);
  *runtime_id = ::SafeArrayCreateVector(VT_I4, 0, id_array.size());

  int* array_data = nullptr;
  ::SafeArrayAccessData(*runtime_id, reinterpret_cast<void**>(&array_data));

  size_t runtime_id_byte_count = id_array.size() * sizeof(int);
  memcpy_s(array_data, runtime_id_byte_count, id_array.data(),
           runtime_id_byte_count);

  ::SafeArrayUnaccessData(*runtime_id);

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_BoundingRectangle(
    UiaRect* screen_physical_pixel_bounds) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_BoundingRectangle");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_BOUNDINGRECTANGLE);
  WIN_ACCESSIBILITY_API_PERF_HISTOGRAM(UMA_API_GET_BOUNDINGRECTANGLE);

  UIA_VALIDATE_CALL_1_ARG(screen_physical_pixel_bounds);

  gfx::Rect bounds =
      delegate_->GetBoundsRect(AXCoordinateSystem::kScreenPhysicalPixels,
                               AXClippingBehavior::kUnclipped);
  screen_physical_pixel_bounds->left = bounds.x();
  screen_physical_pixel_bounds->top = bounds.y();
  screen_physical_pixel_bounds->width = bounds.width();
  screen_physical_pixel_bounds->height = bounds.height();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetEmbeddedFragmentRoots(
    SAFEARRAY** embedded_fragment_roots) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetEmbeddedFragmentRoots");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GETEMBEDDEDFRAGMENTROOTS);
  UIA_VALIDATE_CALL_1_ARG(embedded_fragment_roots);

  *embedded_fragment_roots = nullptr;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::SetFocus() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("SetFocus");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SETFOCUS);
  UIA_VALIDATE_CALL();

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  delegate_->AccessibilityPerformAction(action_data);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_FragmentRoot(
    IRawElementProviderFragmentRoot** fragment_root) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_FragmentRoot");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_FRAGMENTROOT);
  UIA_VALIDATE_CALL_1_ARG(fragment_root);

  gfx::AcceleratedWidget widget =
      delegate_->GetTargetForNativeAccessibilityEvent();
  if (widget) {
    AXFragmentRootWin* root =
        AXFragmentRootWin::GetForAcceleratedWidget(widget);
    if (root != nullptr) {
      root->GetNativeViewAccessible()->QueryInterface(
          IID_PPV_ARGS(fragment_root));
      DCHECK(*fragment_root);
      return S_OK;
    }
  }

  *fragment_root = nullptr;
  return UIA_E_ELEMENTNOTAVAILABLE;
}

//
// IRawElementProviderSimple implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::GetPatternProvider(PATTERNID pattern_id,
                                                     IUnknown** result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetPatternProvider");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PATTERN_PROVIDER);
  WIN_ACCESSIBILITY_API_PERF_HISTOGRAM(UMA_API_GET_PATTERN_PROVIDER);
  NotifyAPIObserverForPatternRequest(pattern_id);
  return GetPatternProviderImpl(pattern_id, result);
}

HRESULT AXPlatformNodeWin::GetPatternProviderImpl(PATTERNID pattern_id,
                                                  IUnknown** result) {
  UIA_VALIDATE_CALL_1_ARG(result);

  *result = nullptr;

  PatternProviderFactoryMethod factory_method =
      GetPatternProviderFactoryMethod(pattern_id);
  if (factory_method)
    (*factory_method)(this, result);

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetPropertyValue(PROPERTYID property_id,
                                                   VARIANT* result) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("GetPropertyValue");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PROPERTY_VALUE);
  WIN_ACCESSIBILITY_API_PERF_HISTOGRAM(UMA_API_GET_PROPERTY_VALUE);

  constexpr LONG kFirstKnownUiaPropertyId = UIA_RuntimeIdPropertyId;
  constexpr LONG kLastKnownUiaPropertyId = UIA_IsDialogPropertyId;
  if (property_id >= kFirstKnownUiaPropertyId &&
      property_id <= kLastKnownUiaPropertyId) {
    base::UmaHistogramSparse("Accessibility.WinAPIs.GetPropertyValue",
                             property_id);
  } else {
    // Collapse all unknown property IDs into a single bucket.
    base::UmaHistogramSparse("Accessibility.WinAPIs.GetPropertyValue", 0);
  }

  NotifyAPIObserverForPropertyRequest(property_id);

  return GetPropertyValueImpl(property_id, result);
}

HRESULT AXPlatformNodeWin::GetPropertyValueImpl(PROPERTYID property_id,
                                                VARIANT* result) {
  UIA_VALIDATE_CALL_1_ARG(result);

  result->vt = VT_EMPTY;
  int int_attribute;
  // Default UIA Property Ids.
  switch (property_id) {
    case UIA_AnnotationObjectsPropertyId: {
      result->vt = VT_ARRAY | VT_UNKNOWN;
      result->parray = CreateUIAElementsArrayForRelation(
          ax::mojom::IntListAttribute::kDetailsIds);
      break;
    }
    case UIA_AriaPropertiesPropertyId:
      result->vt = VT_BSTR;
      result->bstrVal = SysAllocString(ComputeUIAProperties().c_str());
      break;

    case UIA_AriaRolePropertyId:
      result->vt = VT_BSTR;
      result->bstrVal = SysAllocString(GetUIARoleProperties().aria_role);
      break;

    case UIA_AutomationIdPropertyId: {
      // The kRootWebArea is the only element in a web page that cannot have
      // an author provided id. In this case, we return a constant string
      // that needs to be the same for all locales.
      std::u16string automation_id = GetRole() == ax::mojom::Role::kRootWebArea
                                         ? u"RootWebArea"
                                         : GetDelegate()->GetAuthorUniqueId();
      V_VT(result) = VT_BSTR;
      V_BSTR(result) = SysAllocString(base::as_wcstr(automation_id));
      break;
    }
    case UIA_ClassNamePropertyId:
      result->vt = VT_BSTR;
      GetStringAttributeAsBstr(ax::mojom::StringAttribute::kClassName,
                               &result->bstrVal);
      break;

    case UIA_ClickablePointPropertyId:
      if (!GetDelegate()->IsOffscreen()) {
        result->vt = VT_ARRAY | VT_R8;
        result->parray = CreateClickablePointArray();
      }
      break;

    case UIA_ControllerForPropertyId:
      result->vt = VT_ARRAY | VT_UNKNOWN;
      result->parray = CreateUIAControllerForArray();
      break;

    case UIA_ControlTypePropertyId:
      result->vt = VT_I4;
      result->lVal = GetUIARoleProperties().control_type;
      break;

    case UIA_CulturePropertyId: {
      std::optional<LCID> lcid = GetCultureAttributeAsLCID();
      if (!lcid)
        return E_FAIL;
      result->vt = VT_I4;
      result->lVal = lcid.value();
      break;
    }

    case UIA_DescribedByPropertyId:
      result->vt = VT_ARRAY | VT_UNKNOWN;
      result->parray = CreateUIAElementsArrayForRelation(
          ax::mojom::IntListAttribute::kDetailsIds);
      break;

    case UIA_FlowsFromPropertyId:
      V_VT(result) = VT_ARRAY | VT_UNKNOWN;
      V_ARRAY(result) = CreateUIAElementsArrayForReverseRelation(
          ax::mojom::IntListAttribute::kFlowtoIds);
      break;

    case UIA_FlowsToPropertyId:
      result->vt = VT_ARRAY | VT_UNKNOWN;
      result->parray = CreateUIAElementsArrayForRelation(
          ax::mojom::IntListAttribute::kFlowtoIds);
      break;

    case UIA_FrameworkIdPropertyId:
      V_VT(result) = VT_BSTR;
      V_BSTR(result) = SysAllocString(FRAMEWORK_ID);
      break;

    case UIA_HasKeyboardFocusPropertyId:
      result->vt = VT_BOOL;
      result->boolVal = (delegate_->GetFocus() == GetNativeViewAccessible())
                            ? VARIANT_TRUE
                            : VARIANT_FALSE;
      break;

    case UIA_FullDescriptionPropertyId:
      result->vt = VT_BSTR;
      GetStringAttributeAsBstr(ax::mojom::StringAttribute::kDescription,
                               &result->bstrVal);
      break;

    case UIA_HelpTextPropertyId:
      if (HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder)) {
        V_VT(result) = VT_BSTR;
        GetStringAttributeAsBstr(ax::mojom::StringAttribute::kPlaceholder,
                                 &V_BSTR(result));
      } else if (GetNameFrom() == ax::mojom::NameFrom::kPlaceholder ||
                 GetNameFrom() == ax::mojom::NameFrom::kTitle) {
        V_VT(result) = VT_BSTR;
        GetNameAsBstr(&V_BSTR(result));
      } else if (HasStringAttribute(ax::mojom::StringAttribute::kTooltip)) {
        V_VT(result) = VT_BSTR;
        GetStringAttributeAsBstr(ax::mojom::StringAttribute::kTooltip,
                                 &V_BSTR(result));
      }
      break;

    case UIA_IsContentElementPropertyId:
    case UIA_IsControlElementPropertyId:
      result->vt = VT_BOOL;
      result->boolVal = IsUIAControl() ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_IsDataValidForFormPropertyId:
      if (GetIntAttribute(ax::mojom::IntAttribute::kInvalidState,
                          &int_attribute)) {
        result->vt = VT_BOOL;
        result->boolVal =
            (static_cast<int>(ax::mojom::InvalidState::kFalse) == int_attribute)
                ? VARIANT_TRUE
                : VARIANT_FALSE;
      }
      break;

    case UIA_IsDialogPropertyId:
      result->vt = VT_BOOL;
      result->boolVal = IsDialog(GetRole()) ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_IsKeyboardFocusablePropertyId:
      result->vt = VT_BOOL;
      result->boolVal = IsFocusable() ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_IsOffscreenPropertyId:
      result->vt = VT_BOOL;
      result->boolVal =
          GetDelegate()->IsOffscreen() ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_IsRequiredForFormPropertyId:
      result->vt = VT_BOOL;
      if (HasState(ax::mojom::State::kRequired)) {
        result->boolVal = VARIANT_TRUE;
      } else {
        result->boolVal = VARIANT_FALSE;
      }
      break;

    case UIA_ItemStatusPropertyId: {
      // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
      // aria-sort='ascending|descending|other' is mapped for the
      // HeaderItem Control Type.
      int32_t sort_direction;
      if (IsTableHeader(GetRole()) &&
          GetIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                          &sort_direction)) {
        switch (static_cast<ax::mojom::SortDirection>(sort_direction)) {
          case ax::mojom::SortDirection::kNone:
          case ax::mojom::SortDirection::kUnsorted:
            break;
          case ax::mojom::SortDirection::kAscending:
            V_VT(result) = VT_BSTR;
            V_BSTR(result) = SysAllocString(L"ascending");
            break;
          case ax::mojom::SortDirection::kDescending:
            V_VT(result) = VT_BSTR;
            V_BSTR(result) = SysAllocString(L"descending");
            break;
          case ax::mojom::SortDirection::kOther:
            V_VT(result) = VT_BSTR;
            V_BSTR(result) = SysAllocString(L"other");
            break;
        }
      }
      break;
    }

    case UIA_LabeledByPropertyId:
      if (AXPlatformNodeWin* node = ComputeUIALabeledBy()) {
        result->vt = VT_UNKNOWN;
        result->punkVal = node->GetNativeViewAccessible();
        result->punkVal->AddRef();
      }
      break;

    case UIA_LocalizedControlTypePropertyId: {
      // Always favor the explicitly set aria-roledescription value if there's
      // one.
      std::u16string role_description;
      if (GetString16Attribute(ax::mojom::StringAttribute::kRoleDescription,
                               &role_description)) {
        result->vt = VT_BSTR;
        result->bstrVal = SysAllocString(base::as_wcstr(role_description));
        break;
      }

      // UIA core handles Localized Control type for some built-in types and
      // also has a mapping for ARIA roles. To get these defaults, we need to
      // have returned VT_EMPTY.
      // With this in mind, we should return VT_EMPTY if the internal role is
      // accurately described by its UIA Control Type or aria role. Conversely,
      // if the internal role cannot be accurately described by its UIA Control
      // Type or aria role, we should instead provide our own localized
      // description.
      if (GetUIARoleProperties().localization_strategy ==
          UIALocalizationStrategy::kSupply) {
        // According to the HTML-AAM, UIA expects <output> to have a
        // Localized Control Type of "output" whereas the Core-AAM states
        // the Localized Control Type of the ARIA status role should be
        // "status".
        const std::string& html_tag =
            GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
        std::u16string localized_control_type =
            html_tag == "output" ? l10n_util::GetStringUTF16(IDS_AX_ROLE_OUTPUT)
                                 : GetRoleDescription();

        if (!localized_control_type.empty()) {
          result->vt = VT_BSTR;
          result->bstrVal =
              SysAllocString(base::as_wcstr(localized_control_type));
        }
      }  // If a role description has not been provided, leave as VT_EMPTY.
      break;
    }
    case UIA_NamePropertyId:
      if (IsNameExposed()) {
        result->vt = VT_BSTR;

        // We need to handle listitems name property differently because UIA
        // expects a name for listitems, whereas other APIs do not.
        if (GetRole() == ax::mojom::Role::kListItem &&
            !HasStringAttribute(ax::mojom::StringAttribute::kName)) {
          ComputeListItemNameAsBstr(&result->bstrVal);
        } else {
          GetNameAsBstr(&result->bstrVal);
        }
      }
      break;

    case UIA_OrientationPropertyId:
      if (SupportsOrientation(GetRole())) {
        if (HasState(ax::mojom::State::kHorizontal) &&
            HasState(ax::mojom::State::kVertical)) {
          NOTREACHED_IN_MIGRATION()
              << "An accessibility object cannot have a horizontal "
                 "and a vertical orientation at the same time.";
        }
        if (HasState(ax::mojom::State::kHorizontal)) {
          result->vt = VT_I4;
          result->intVal = OrientationType_Horizontal;
        }
        if (HasState(ax::mojom::State::kVertical)) {
          result->vt = VT_I4;
          result->intVal = OrientationType_Vertical;
        }
      } else {
        result->vt = VT_I4;
        result->intVal = OrientationType_None;
      }
      break;

    case UIA_IsEnabledPropertyId:
      V_VT(result) = VT_BOOL;
      switch (GetData().GetRestriction()) {
        case ax::mojom::Restriction::kDisabled:
          V_BOOL(result) = VARIANT_FALSE;
          break;

        case ax::mojom::Restriction::kNone:
        case ax::mojom::Restriction::kReadOnly:
          V_BOOL(result) = VARIANT_TRUE;
          break;
      }
      break;

    case UIA_IsPasswordPropertyId:
      result->vt = VT_BOOL;
      result->boolVal =
          HasState(ax::mojom::State::kProtected) ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_AcceleratorKeyPropertyId:
      if (HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts)) {
        result->vt = VT_BSTR;
        GetStringAttributeAsBstr(ax::mojom::StringAttribute::kKeyShortcuts,
                                 &result->bstrVal);
      }
      break;

    case UIA_AccessKeyPropertyId:
      if (HasStringAttribute(ax::mojom::StringAttribute::kAccessKey)) {
        result->vt = VT_BSTR;
        GetStringAttributeAsBstr(ax::mojom::StringAttribute::kAccessKey,
                                 &result->bstrVal);
      }
      break;

    case UIA_IsPeripheralPropertyId:
      result->vt = VT_BOOL;
      result->boolVal = VARIANT_FALSE;
      break;

    case UIA_LevelPropertyId:
      if (GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                          &int_attribute)) {
        result->vt = VT_I4;
        result->intVal = int_attribute;
      }
      break;

    case UIA_LiveSettingPropertyId: {
      result->vt = VT_I4;
      result->intVal = LiveSetting::Off;

      std::string string_attribute;
      if (GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                             &string_attribute)) {
        if (string_attribute == "polite")
          result->intVal = LiveSetting::Polite;
        else if (string_attribute == "assertive")
          result->intVal = LiveSetting::Assertive;
      }
      break;
    }

    case UIA_OptimizeForVisualContentPropertyId:
      result->vt = VT_BOOL;
      result->boolVal = VARIANT_FALSE;
      break;

    case UIA_PositionInSetPropertyId: {
      std::optional<int> pos_in_set = GetPosInSet();
      if (pos_in_set) {
        result->vt = VT_I4;
        result->intVal = *pos_in_set;
      }
    } break;

    case UIA_ScrollHorizontalScrollPercentPropertyId: {
      V_VT(result) = VT_R8;
      V_R8(result) = GetHorizontalScrollPercent();
      break;
    }

    case UIA_ScrollVerticalScrollPercentPropertyId: {
      V_VT(result) = VT_R8;
      V_R8(result) = GetVerticalScrollPercent();
      break;
    }

    case UIA_SelectionItemIsSelectedPropertyId: {
      result->vt = VT_BOOL;
      result->boolVal =
          GetDelegate()->IsUIANodeSelected() ? VARIANT_TRUE : VARIANT_FALSE;
      break;
    }

    case UIA_SizeOfSetPropertyId: {
      std::optional<int> set_size = GetSetSize();
      if (set_size) {
        result->vt = VT_I4;
        result->intVal = *set_size;
      }
      break;
    }

    case UIA_ToggleToggleStatePropertyId: {
      result->vt = VT_I4;
      result->intVal = GetToggleStateImpl();
      break;
    }

    case UIA_LandmarkTypePropertyId: {
      std::optional<LONG> landmark_type = ComputeUIALandmarkType();
      if (landmark_type) {
        result->vt = VT_I4;
        result->intVal = landmark_type.value();
      }
      break;
    }

    case UIA_LocalizedLandmarkTypePropertyId: {
      std::u16string localized_landmark_type =
          GetDelegate()->GetLocalizedStringForLandmarkType();
      if (!localized_landmark_type.empty()) {
        result->vt = VT_BSTR;
        result->bstrVal =
            SysAllocString(base::as_wcstr(localized_landmark_type));
      }
      break;
    }

    case UIA_ExpandCollapseExpandCollapseStatePropertyId:
      result->vt = VT_I4;
      result->intVal = static_cast<int>(ComputeExpandCollapseState());
      break;

    // Not currently implemented.
    case UIA_AnnotationTypesPropertyId:
    case UIA_CenterPointPropertyId:
    case UIA_FillColorPropertyId:
    case UIA_FillTypePropertyId:
    case UIA_HeadingLevelPropertyId:
    case UIA_ItemTypePropertyId:
    case UIA_OutlineColorPropertyId:
    case UIA_OutlineThicknessPropertyId:
    case UIA_RotationPropertyId:
    case UIA_SizePropertyId:
    case UIA_VisualEffectsPropertyId:
      break;

    // Provided by UIA Core; we should not implement.
    case UIA_BoundingRectanglePropertyId:
    case UIA_NativeWindowHandlePropertyId:
    case UIA_ProcessIdPropertyId:
    case UIA_ProviderDescriptionPropertyId:
    case UIA_RuntimeIdPropertyId:
      break;
    default:
      // We can't simply add these custom properties ids to the switch case
      // because they are not constant expressions.
      //
      // Custom UIA Property Ids.
      if (property_id ==
          UiaRegistrarWin::GetInstance().GetUniqueIdPropertyId()) {
        // We want to negate the unique id for it to be consistent across
        // different Windows accessiblity APIs. The negative unique id
        // convention originated from ::NotifyWinEvent() takes an hwnd and a
        // child id. A 0 child id means self, and a positive child id means
        // child #n. In order to fire an event for an arbitrary descendant of
        // the window, Firefox started the practice of using a negative unique
        // id. We follow the same negative unique id convention here and when we
        // fire events via ::NotifyWinEvent().
        result->vt = VT_BSTR;
        result->bstrVal =
            SysAllocString(base::NumberToWString(-GetUniqueId()).c_str());
      } else if (features::IsAccessibilityAriaVirtualContentEnabled() &&
                 property_id == UiaRegistrarWin::GetInstance()
                                    .GetVirtualContentPropertyId()) {
        if (HasStringAttribute(ax::mojom::StringAttribute::kVirtualContent)) {
          V_VT(result) = VT_BSTR;
          GetStringAttributeAsBstr(ax::mojom::StringAttribute::kVirtualContent,
                                   &V_BSTR(result));
        }
      }
      break;
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ProviderOptions(ProviderOptions* ret) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_ProviderOptions");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PROVIDER_OPTIONS);
  UIA_VALIDATE_CALL_1_ARG(ret);

  *ret = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading |
         ProviderOptions_RefuseNonClientSupport |
         ProviderOptions_HasNativeIAccessible;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_HostRawElementProvider(
    IRawElementProviderSimple** provider) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_HostRawElementProvider");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_HOST_RAW_ELEMENT_PROVIDER);
  UIA_VALIDATE_CALL_1_ARG(provider);

  *provider = nullptr;
  return S_OK;
}

//
// IRawElementProviderSimple2 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::ShowContextMenu() {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("ShowContextMenu");
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SHOWCONTEXTMENU);
  UIA_VALIDATE_CALL();

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kShowContextMenu;
  delegate_->AccessibilityPerformAction(action_data);
  return S_OK;
}

//
// IChromeAccessible implementation.
//

void SendBulkFetchResponse(
    Microsoft::WRL::ComPtr<IChromeAccessibleDelegate> delegate,
    LONG request_id,
    std::string json_result) {
  std::wstring json_result_wide = base::UTF8ToWide(json_result);
  delegate->put_bulkFetchResult(request_id,
                                SysAllocString(json_result_wide.c_str()));
}

IFACEMETHODIMP AXPlatformNodeWin::get_bulkFetch(
    BSTR input_json,
    LONG request_id,
    IChromeAccessibleDelegate* delegate) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_bulkFetch");
  COM_OBJECT_VALIDATE();
  if (!delegate)
    return E_INVALIDARG;

  // TODO(crbug.com/40692720): if parsing |input_json|, use
  // DataDecoder because the json is untrusted. For now, this is just
  // a stub that calls PostTask so that it's async, but it doesn't
  // actually parse the input.

  base::Value::Dict result;
  result.Set("role", base::Value(ui::ToString(GetRole())));

  gfx::Rect bounds = GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreenDIPs, AXClippingBehavior::kUnclipped);
  result.Set("x", base::Value(bounds.x()));
  result.Set("y", base::Value(bounds.y()));
  result.Set("width", base::Value(bounds.width()));
  result.Set("height", base::Value(bounds.height()));
  std::string json_result;
  base::JSONWriter::Write(result, &json_result);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SendBulkFetchResponse,
          Microsoft::WRL::ComPtr<IChromeAccessibleDelegate>(delegate),
          request_id, json_result));
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_hitTest(
    LONG screen_physical_pixel_x,
    LONG screen_physical_pixel_y,
    LONG request_id,
    IChromeAccessibleDelegate* delegate) {
  WIN_ACCESSIBILITY_API_TRACE_EVENT("get_hitTest");
  COM_OBJECT_VALIDATE();

  if (!delegate)
    return E_INVALIDARG;

  // TODO(crbug.com/40692720): Plumb through an actual async hit test.
  AXPlatformNodeWin* hit_child = static_cast<AXPlatformNodeWin*>(
      FromNativeViewAccessible(GetDelegate()->HitTestSync(
          screen_physical_pixel_x, screen_physical_pixel_y)));

  delegate->put_hitTestResult(request_id, static_cast<IAccessible*>(hit_child));
  return S_OK;
}

//
// IServiceProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::QueryService(REFGUID guidService,
                                               REFIID riid,
                                               void** object) {
  TRACE_EVENT("accessibility", "QueryService",
              perfetto::Flow::FromPointer(this), "guidService",
              base::WideToASCII(base::win::WStringFromGUID(guidService)),
              "riid", base::WideToASCII(base::win::WStringFromGUID(riid)));
  COM_OBJECT_VALIDATE_1_ARG(object);

  if (!GetDelegate()) {
    *object = nullptr;
    return E_FAIL;
  }

  if (riid == IID_IAccessible2) {
    for (WinAccessibilityAPIUsageObserver& observer :
         GetWinAccessibilityAPIUsageObserverList()) {
      if (!features::IsAccessibilityRestrictiveIA2AXModesEnabled()) {
        observer.OnAdvancedIAccessible2Used();
        continue;
      }

      if (GetDelegate()->IsWebContent()) {
        observer.OnAdvancedIAccessible2Used();
      } else {
        observer.OnBasicIAccessible2Used();
      }
    }
  }

  if (guidService == IID_IAccessible || guidService == IID_IAccessible2 ||
      guidService == IID_IAccessible2_2 ||
      guidService == IID_IAccessibleTable ||
      guidService == IID_IAccessibleTable2 ||
      guidService == IID_IAccessibleTableCell ||
      guidService == IID_IAccessibleText ||
      guidService == IID_IAccessibleTextSelectionContainer ||
      guidService == IID_IAccessibleValue) {
    return QueryInterface(riid, object);
  }

  if (guidService == IID_IChromeAccessible) {
    if (features::IsIChromeAccessibleEnabled()) {
      return QueryInterface(riid, object);
    }
  }

  // TODO(suproteem): Include IAccessibleEx in the list, potentially checking
  // for version.

  *object = nullptr;
  return E_FAIL;
}

//
// Methods used by the ATL COM map.
//

// static
STDMETHODIMP AXPlatformNodeWin::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID riid,
    void** object) {
  if (!object)
    return E_INVALIDARG;
  *object = nullptr;
  AXPlatformNodeWin* accessible =
      reinterpret_cast<AXPlatformNodeWin*>(this_ptr);
  DCHECK(accessible);

  if (riid == IID_IAccessibleTable || riid == IID_IAccessibleTable2) {
    if (!IsTableLike(accessible->GetRole()))
      return E_NOINTERFACE;
  } else if (riid == IID_IAccessibleTableCell) {
    if (!IsCellOrTableHeader(accessible->GetRole()))
      return E_NOINTERFACE;
  } else if (riid == IID_IAccessibleText || riid == IID_IAccessibleHypertext) {
    if (IsImageOrVideo(accessible->GetRole())) {
      return E_NOINTERFACE;
    }
  } else if (riid == IID_IAccessibleValue) {
    if (!accessible->GetData().IsRangeValueSupported()) {
      return E_NOINTERFACE;
    }
  } else if (riid == IID_IChromeAccessible) {
    if (!features::IsIChromeAccessibleEnabled()) {
      return E_NOINTERFACE;
    }
  }

  return CComObjectRootBase::InternalQueryInterface(this_ptr, entries, riid,
                                                    object);
}

HRESULT AXPlatformNodeWin::GetTextAttributeValue(
    TEXTATTRIBUTEID attribute_id,
    const std::optional<int>& start_offset,
    const std::optional<int>& end_offset,
    base::win::VariantVector* result) {
  DCHECK(!start_offset || start_offset.value() >= 0);
  DCHECK(!end_offset || end_offset.value() >= 0);
  switch (attribute_id) {
    case UIA_AnnotationObjectsAttributeId:
      GetAnnotationObjectsAttribute(result);
      break;
    case UIA_AnnotationTypesAttributeId:
      return GetAnnotationTypesAttribute(start_offset, end_offset, result);
    case UIA_BackgroundColorAttributeId:
      result->Insert<VT_I4>(
          GetIntAttributeAsCOLORREF(ax::mojom::IntAttribute::kBackgroundColor));
      break;
    case UIA_BulletStyleAttributeId:
      result->Insert<VT_I4>(ComputeUIABulletStyle());
      break;
    case UIA_CultureAttributeId: {
      std::optional<LCID> lcid = GetCultureAttributeAsLCID();
      if (!lcid)
        return E_FAIL;
      result->Insert<VT_I4>(lcid.value());
      break;
    }
    case UIA_FontNameAttributeId:
      result->Insert<VT_BSTR>(GetFontNameAttributeAsBSTR());
      break;
    case UIA_FontSizeAttributeId: {
      std::optional<float> font_size_in_points = GetFontSizeInPoints();
      if (font_size_in_points) {
        result->Insert<VT_R8>(*font_size_in_points);
      }
      break;
    }
    case UIA_FontWeightAttributeId:
      result->Insert<VT_I4>(
          GetFloatAttribute(ax::mojom::FloatAttribute::kFontWeight));
      break;
    case UIA_ForegroundColorAttributeId:
      result->Insert<VT_I4>(
          GetIntAttributeAsCOLORREF(ax::mojom::IntAttribute::kColor));
      break;
    case UIA_IsHiddenAttributeId:
      result->Insert<VT_BOOL>(IsInvisibleOrIgnored());
      break;
    case UIA_IsItalicAttributeId:
      result->Insert<VT_BOOL>(HasTextStyle(ax::mojom::TextStyle::kItalic));
      break;
    case UIA_IsReadOnlyAttributeId: {
      // If inside a text field, the text field's readonly state rules.
      const AXPlatformNodeWin* text_field = static_cast<AXPlatformNodeWin*>(
          FromNativeViewAccessible(GetDelegate()->GetTextFieldAncestor()));
      if (text_field) {
        result->Insert<VT_BOOL>(
            text_field->GetDelegate()->IsReadOnlyOrDisabled());
      } else {
        result->Insert<VT_BOOL>(GetDelegate()->IsReadOnlyOrDisabled());
      }
      break;
    }
    case UIA_IsSubscriptAttributeId:
      result->Insert<VT_BOOL>(GetData().GetTextPosition() ==
                              ax::mojom::TextPosition::kSubscript);
      break;
    case UIA_IsSuperscriptAttributeId:
      result->Insert<VT_BOOL>(GetData().GetTextPosition() ==
                              ax::mojom::TextPosition::kSuperscript);
      break;
    case UIA_OverlineStyleAttributeId:
      result->Insert<VT_I4>(GetUIATextDecorationStyle(
          ax::mojom::IntAttribute::kTextOverlineStyle));
      break;
    case UIA_StrikethroughStyleAttributeId:
      result->Insert<VT_I4>(GetUIATextDecorationStyle(
          ax::mojom::IntAttribute::kTextStrikethroughStyle));
      break;
    case UIA_StyleNameAttributeId:
      result->Insert<VT_BSTR>(GetStyleNameAttributeAsBSTR());
      break;
    case UIA_StyleIdAttributeId:
      result->Insert<VT_I4>(ComputeUIAStyleId());
      break;
    case UIA_HorizontalTextAlignmentAttributeId: {
      std::optional<HorizontalTextAlignment> horizontal_text_alignment =
          AXTextAlignToUIAHorizontalTextAlignment(GetData().GetTextAlign());
      if (horizontal_text_alignment)
        result->Insert<VT_I4>(*horizontal_text_alignment);
      break;
    }
    case UIA_UnderlineStyleAttributeId:
      result->Insert<VT_I4>(GetUIATextDecorationStyle(
          ax::mojom::IntAttribute::kTextUnderlineStyle));
      break;
    case UIA_TextFlowDirectionsAttributeId:
      result->Insert<VT_I4>(
          TextDirectionToFlowDirections(GetData().GetTextDirection()));
      break;
    default: {
      Microsoft::WRL::ComPtr<IUnknown> not_supported_value;
      HRESULT hr = ::UiaGetReservedNotSupportedValue(&not_supported_value);
      if (SUCCEEDED(hr))
        result->Insert<VT_UNKNOWN>(not_supported_value.Get());
      return hr;
    }
  }

  return S_OK;
}

void AXPlatformNodeWin::GetAnnotationObjectsAttribute(
    base::win::VariantVector* result) {
  // Most times AnnotationObject attribute is set on the container (immediate
  // parent) of the text node, but it can be on any ancestor of the text node.
  // TODO(vicfei): Need to find an efficient algorithm to walk up current node's
  // ancestors to find the attribute. https://crbug.com/1201327
  AXPlatformNodeWin* parent_platform_node = GetParentPlatformNodeWin();

  if (!parent_platform_node || !IsText())
    return;

  for (AXPlatformNodeWin* platform_node :
       CreatePlatformNodeVectorFromRelationIdVector(
           parent_platform_node->GetIntListAttribute(
               ax::mojom::IntListAttribute::kDetailsIds))) {
    Microsoft::WRL::ComPtr<IUnknown> annotation_object;
    if (SUCCEEDED(
            platform_node->QueryInterface(IID_PPV_ARGS(&annotation_object)))) {
      result->Insert<VT_UNKNOWN>(annotation_object.Get());
    }
  }
}

HRESULT AXPlatformNodeWin::GetAnnotationTypesAttribute(
    const std::optional<int>& start_offset,
    const std::optional<int>& end_offset,
    base::win::VariantVector* result) {
  base::win::VariantVector variant_vector;
  MarkerTypeRangeResult grammar_result = MarkerTypeRangeResult::kNone;
  MarkerTypeRangeResult spelling_result = MarkerTypeRangeResult::kNone;
  MarkerTypeRangeResult highlight_result = MarkerTypeRangeResult::kNone;
  MarkerTypeRangeResult highlight_spelling_result =
      MarkerTypeRangeResult::kNone;
  MarkerTypeRangeResult highlight_grammar_result = MarkerTypeRangeResult::kNone;

  if (IsText() || IsAtomicTextField()) {
    grammar_result = GetMarkerTypeFromRange(start_offset, end_offset,
                                            ax::mojom::MarkerType::kGrammar);
    spelling_result = GetMarkerTypeFromRange(start_offset, end_offset,
                                             ax::mojom::MarkerType::kSpelling);
    highlight_result = GetMarkerTypeFromRange(
        start_offset, end_offset, ax::mojom::MarkerType::kHighlight,
        ax::mojom::HighlightType::kHighlight);
    highlight_spelling_result = GetMarkerTypeFromRange(
        start_offset, end_offset, ax::mojom::MarkerType::kHighlight,
        ax::mojom::HighlightType::kSpellingError);
    highlight_grammar_result = GetMarkerTypeFromRange(
        start_offset, end_offset, ax::mojom::MarkerType::kHighlight,
        ax::mojom::HighlightType::kGrammarError);
  }

  if (grammar_result == MarkerTypeRangeResult::kMixed ||
      spelling_result == MarkerTypeRangeResult::kMixed ||
      highlight_result == MarkerTypeRangeResult::kMixed ||
      highlight_spelling_result == MarkerTypeRangeResult::kMixed ||
      highlight_grammar_result == MarkerTypeRangeResult::kMixed) {
    Microsoft::WRL::ComPtr<IUnknown> mixed_attribute_value;
    HRESULT hr = ::UiaGetReservedMixedAttributeValue(&mixed_attribute_value);
    if (SUCCEEDED(hr))
      result->Insert<VT_UNKNOWN>(mixed_attribute_value.Get());
    return hr;
  }

  if (spelling_result == MarkerTypeRangeResult::kMatch ||
      highlight_spelling_result == MarkerTypeRangeResult::kMatch)
    result->Insert<VT_I4>(AnnotationType_SpellingError);
  if (grammar_result == MarkerTypeRangeResult::kMatch ||
      highlight_grammar_result == MarkerTypeRangeResult::kMatch)
    result->Insert<VT_I4>(AnnotationType_GrammarError);
  if (highlight_result == MarkerTypeRangeResult::kMatch)
    result->Insert<VT_I4>(AnnotationType_Highlighted);

  return S_OK;
}

std::optional<LCID> AXPlatformNodeWin::GetCultureAttributeAsLCID() const {
  const std::u16string language =
      base::UTF8ToUTF16(GetDelegate()->GetLanguage());
  const LCID lcid =
      LocaleNameToLCID(base::as_wcstr(language), LOCALE_ALLOW_NEUTRAL_NAMES);
  if (!lcid)
    return std::nullopt;

  return lcid;
}

COLORREF AXPlatformNodeWin::GetIntAttributeAsCOLORREF(
    ax::mojom::IntAttribute attribute) const {
  SkColor color;
  auto maybe_value = ComputeAttribute(delegate_, attribute);
  if (maybe_value.has_value())
    color = maybe_value.value();
  else
    color = GetIntAttribute(attribute);
  return skia::SkColorToCOLORREF(color);
}

BulletStyle AXPlatformNodeWin::ComputeUIABulletStyle() const {
  // UIA expects the list style of a non-list-item to be none however the
  // default list style cascaded is disc not none. Therefore we must ensure that
  // this node is contained within a list-item to distinguish non-list-items and
  // disc styled list items.
  const AXPlatformNodeBase* current_node = this;
  while (current_node &&
         current_node->GetRole() != ax::mojom::Role::kListItem) {
    current_node = FromNativeViewAccessible(current_node->GetParent());
  }

  const ax::mojom::ListStyle list_style =
      current_node ? current_node->GetData().GetListStyle()
                   : ax::mojom::ListStyle::kNone;

  switch (list_style) {
    case ax::mojom::ListStyle::kNone:
      return BulletStyle::BulletStyle_None;
    case ax::mojom::ListStyle::kCircle:
      return BulletStyle::BulletStyle_HollowRoundBullet;
    case ax::mojom::ListStyle::kDisc:
      return BulletStyle::BulletStyle_FilledRoundBullet;
    case ax::mojom::ListStyle::kImage:
      return BulletStyle::BulletStyle_Other;
    case ax::mojom::ListStyle::kNumeric:
    case ax::mojom::ListStyle::kOther:
      return BulletStyle::BulletStyle_None;
    case ax::mojom::ListStyle::kSquare:
      return BulletStyle::BulletStyle_FilledSquareBullet;
  }
}

LONG AXPlatformNodeWin::ComputeUIAStyleId() const {
  const AXPlatformNodeBase* current_node = this;
  do {
    switch (current_node->GetRole()) {
      case ax::mojom::Role::kDisclosureTriangle:
      case ax::mojom::Role::kDisclosureTriangleGrouped:
        if (!::features::IsAccessibilityExposeSummaryAsHeadingEnabled()) {
          break;
        }
        [[fallthrough]];  // Expose heading level.
      case ax::mojom::Role::kHeading:
        return AXHierarchicalLevelToUIAStyleId(current_node->GetIntAttribute(
            ax::mojom::IntAttribute::kHierarchicalLevel));
      case ax::mojom::Role::kListItem:
        return AXListStyleToUIAStyleId(current_node->GetData().GetListStyle());
      case ax::mojom::Role::kMark:
        return StyleId_Custom;
      case ax::mojom::Role::kBlockquote:
        return StyleId_Quote;
      default:
        break;
    }
    current_node = FromNativeViewAccessible(current_node->GetParent());
  } while (current_node);

  return StyleId_Normal;
}

// static
std::optional<HorizontalTextAlignment>
AXPlatformNodeWin::AXTextAlignToUIAHorizontalTextAlignment(
    ax::mojom::TextAlign text_align) {
  switch (text_align) {
    case ax::mojom::TextAlign::kNone:
      return std::nullopt;
    case ax::mojom::TextAlign::kLeft:
      return HorizontalTextAlignment_Left;
    case ax::mojom::TextAlign::kRight:
      return HorizontalTextAlignment_Right;
    case ax::mojom::TextAlign::kCenter:
      return HorizontalTextAlignment_Centered;
    case ax::mojom::TextAlign::kJustify:
      return HorizontalTextAlignment_Justified;
  }
}

// static
LONG AXPlatformNodeWin::AXHierarchicalLevelToUIAStyleId(
    int32_t hierarchical_level) {
  switch (hierarchical_level) {
    case 0:
      return StyleId_Normal;
    case 1:
      return StyleId_Heading1;
    case 2:
      return StyleId_Heading2;
    case 3:
      return StyleId_Heading3;
    case 4:
      return StyleId_Heading4;
    case 5:
      return StyleId_Heading5;
    case 6:
      return StyleId_Heading6;
    case 7:
      return StyleId_Heading7;
    case 8:
      return StyleId_Heading8;
    case 9:
      return StyleId_Heading9;
    default:
      return StyleId_Custom;
  }
}

// static
LONG AXPlatformNodeWin::AXListStyleToUIAStyleId(
    ax::mojom::ListStyle list_style) {
  switch (list_style) {
    case ax::mojom::ListStyle::kNone:
      return StyleId_Normal;
    case ax::mojom::ListStyle::kCircle:
    case ax::mojom::ListStyle::kDisc:
    case ax::mojom::ListStyle::kImage:
    case ax::mojom::ListStyle::kSquare:
      return StyleId_BulletedList;
    case ax::mojom::ListStyle::kNumeric:
    case ax::mojom::ListStyle::kOther:
      return StyleId_NumberedList;
  }
}

// static
FlowDirections AXPlatformNodeWin::TextDirectionToFlowDirections(
    ax::mojom::WritingDirection text_direction) {
  switch (text_direction) {
    case ax::mojom::WritingDirection::kNone:
      return FlowDirections::FlowDirections_Default;
    case ax::mojom::WritingDirection::kLtr:
      return FlowDirections::FlowDirections_Default;
    case ax::mojom::WritingDirection::kRtl:
      return FlowDirections::FlowDirections_RightToLeft;
    case ax::mojom::WritingDirection::kTtb:
      return FlowDirections::FlowDirections_Vertical;
    case ax::mojom::WritingDirection::kBtt:
      return FlowDirections::FlowDirections_BottomToTop;
  }
}

// static
void AXPlatformNodeWin::AggregateRangesForMarkerType(
    AXPlatformNodeBase* node,
    ax::mojom::MarkerType marker_type,
    int offset_ranges_amount,
    std::vector<std::pair<int, int>>* ranges,
    const std::optional<ax::mojom::HighlightType>& highlight_type) {
  DCHECK(node->IsText());
  const std::vector<int32_t>& marker_types =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
  const std::vector<int32_t>& highlight_types =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kHighlightTypes);
  const std::vector<int>& marker_starts =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
  const std::vector<int>& marker_ends =
      node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);

  CHECK_EQ(marker_types.size(), marker_starts.size());
  CHECK_EQ(marker_types.size(), marker_ends.size());

  for (size_t i = 0; i < marker_types.size(); ++i) {
    if (static_cast<ax::mojom::MarkerType>(marker_types[i]) != marker_type)
      continue;

    if (marker_type == ax::mojom::MarkerType::kHighlight) {
      CHECK_EQ(highlight_types.size(), marker_types.size());
      if (highlight_type !=
          static_cast<ax::mojom::HighlightType>(highlight_types[i])) {
        continue;
      }
    }

    const int marker_start = marker_starts[i] + offset_ranges_amount;
    const int marker_end = marker_ends[i] + offset_ranges_amount;
    ranges->emplace_back(std::make_pair(marker_start, marker_end));
  }
}

AXPlatformNodeWin::MarkerTypeRangeResult
AXPlatformNodeWin::GetMarkerTypeFromRange(
    const std::optional<int>& start_offset,
    const std::optional<int>& end_offset,
    ax::mojom::MarkerType marker_type,
    const std::optional<ax::mojom::HighlightType>& highlight_type) {
  DCHECK(IsText() || IsAtomicTextField());
  std::vector<std::pair<int, int>> relevant_ranges;

  if (IsText()) {
    AggregateRangesForMarkerType(this, marker_type, /*offset_ranges_amount=*/0,
                                 &relevant_ranges, highlight_type);
  } else if (IsAtomicTextField()) {
    int offset_ranges_amount = 0;
    for (AXPlatformNodeBase* static_text = GetFirstTextOnlyDescendant();
         static_text; static_text = static_text->GetNextSibling()) {
      const int child_offset_ranges_amount = offset_ranges_amount;
      if (start_offset || end_offset) {
        // Break if the current node is after the desired |end_offset|.
        if (end_offset && child_offset_ranges_amount > end_offset.value())
          break;

        // Skip over nodes preceding the desired |start_offset|.
        offset_ranges_amount += static_text->GetHypertext().length();
        if (start_offset && offset_ranges_amount < start_offset.value())
          continue;
      }

      AggregateRangesForMarkerType(static_text, marker_type,
                                   child_offset_ranges_amount, &relevant_ranges,
                                   highlight_type);
    }
  }

  // Sort the ranges by their start offset.
  const auto sort_ranges_by_start_offset = [](const std::pair<int, int>& a,
                                              const std::pair<int, int>& b) {
    return a.first < b.first;
  };
  std::sort(relevant_ranges.begin(), relevant_ranges.end(),
            sort_ranges_by_start_offset);

  // Validate that the desired range has a contiguous MarkerType.
  std::optional<std::pair<int, int>> contiguous_range;
  for (const std::pair<int, int>& range : relevant_ranges) {
    if (end_offset && range.first > end_offset.value())
      break;
    if (start_offset && range.second < start_offset.value())
      continue;

    if (!contiguous_range) {
      contiguous_range = range;
      continue;
    }

    // If there is a gap, then the range must be mixed.
    if ((range.first - contiguous_range->second) > 1)
      return MarkerTypeRangeResult::kMixed;

    // Expand the range if possible.
    contiguous_range->second = std::max(contiguous_range->second, range.second);
  }

  // The desired range does not overlap with |marker_type|.
  if (!contiguous_range)
    return MarkerTypeRangeResult::kNone;

  // If there is a partial overlap, then the desired range must be mixed.
  // 1. The |start_offset| is not specified, treat it as offset 0.
  if (!start_offset && contiguous_range->first > 0)
    return MarkerTypeRangeResult::kMixed;
  // 2. The |end_offset| is not specified, treat it as max text offset.
  if (!end_offset &&
      static_cast<size_t>(contiguous_range->second) < GetHypertext().length())
    return MarkerTypeRangeResult::kMixed;
  // 3. The |start_offset| is specified, but is before the first matching range.
  if (start_offset && start_offset.value() < contiguous_range->first)
    return MarkerTypeRangeResult::kMixed;
  // 4. The |end_offset| is specified, but is after the last matching range.
  if (end_offset && end_offset.value() > contiguous_range->second)
    return MarkerTypeRangeResult::kMixed;

  // The desired range is a complete match for |marker_type|.
  return MarkerTypeRangeResult::kMatch;
}

// IRawElementProviderSimple support methods.

bool AXPlatformNodeWin::IsPatternProviderSupported(PATTERNID pattern_id) {
  return GetPatternProviderFactoryMethod(pattern_id);
}

//
// Private member functions.
//
int AXPlatformNodeWin::MSAARole() {
  // If this is a web area for a presentational iframe, give it a role of
  // something other than DOCUMENT so that the fact that it's a separate doc
  // is not exposed to AT.
  if (GetDelegate()->IsRootWebAreaForPresentationalIframe())
    return ROLE_SYSTEM_GROUPING;

  switch (GetRole()) {
    case ax::mojom::Role::kAlert:
      return ROLE_SYSTEM_ALERT;

    case ax::mojom::Role::kAlertDialog:
      return ROLE_SYSTEM_DIALOG;

    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kSuggestion:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kApplication:
      return ROLE_SYSTEM_APPLICATION;

    case ax::mojom::Role::kArticle:
      return ROLE_SYSTEM_DOCUMENT;

    case ax::mojom::Role::kAudio:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kBlockquote:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kButton:
      return ROLE_SYSTEM_PUSHBUTTON;

    case ax::mojom::Role::kCanvas:
      return ROLE_SYSTEM_GRAPHIC;

    case ax::mojom::Role::kCaption:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kCaret:
      return ROLE_SYSTEM_CARET;

    case ax::mojom::Role::kCell:
      return ROLE_SYSTEM_CELL;

    case ax::mojom::Role::kCheckBox:
      return ROLE_SYSTEM_CHECKBUTTON;

    case ax::mojom::Role::kClient:
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kColorWell:
      return ROLE_SYSTEM_TEXT;

    case ax::mojom::Role::kColumn:
      return ROLE_SYSTEM_COLUMN;

    case ax::mojom::Role::kColumnHeader:
      return ROLE_SYSTEM_COLUMNHEADER;

    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kComboBoxSelect:
      return ROLE_SYSTEM_COMBOBOX;

    case ax::mojom::Role::kComplementary:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      return ROLE_SYSTEM_DROPLIST;

    case ax::mojom::Role::kDefinition:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kDescriptionList:
      return ROLE_SYSTEM_LIST;

    case ax::mojom::Role::kDesktop:
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kDetails:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kDialog:
      return ROLE_SYSTEM_DIALOG;

    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kDisclosureTriangleGrouped:
      return ROLE_SYSTEM_PUSHBUTTON;

    case ax::mojom::Role::kDocCover:
      return ROLE_SYSTEM_GRAPHIC;

    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return ROLE_SYSTEM_LINK;

    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
    case ax::mojom::Role::kDocFootnote:
      return ROLE_SYSTEM_LISTITEM;

    case ax::mojom::Role::kDocPageBreak:
      return ROLE_SYSTEM_SEPARATOR;

    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIndex:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocNotice:
    case ax::mojom::Role::kDocPageFooter:
    case ax::mojom::Role::kDocPageHeader:
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
    case ax::mojom::Role::kDocSubtitle:
    case ax::mojom::Role::kDocTip:
    case ax::mojom::Role::kDocToc:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kRootWebArea:
      return ROLE_SYSTEM_DOCUMENT;

    case ax::mojom::Role::kEmbeddedObject:
      // Even though the HTML-AAM has ROLE_SYSTEM_CLIENT for <embed>, we are
      // forced to use ROLE_SYSTEM_GROUPING when the <embed> has children in the
      // accessibility tree.
      // https://www.w3.org/TR/html-aam-1.0/#html-element-role-mappings
      //
      // Screen readers Jaws and NVDA do not "see" any of the <embed>'s contents
      // if they are represented as its children in the accessibility tree. For
      // example, one of the places that would be negatively impacted is the
      // reading of PDFs.
      if (GetDelegate()->GetChildCount()) {
        return ROLE_SYSTEM_GROUPING;
      } else {
        return ROLE_SYSTEM_CLIENT;
      }

    case ax::mojom::Role::kFigcaption:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kFigure:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kFeed:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kForm:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kGenericContainer:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kGraphicsDocument:
      return ROLE_SYSTEM_DOCUMENT;

    case ax::mojom::Role::kGraphicsObject:
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kGraphicsSymbol:
      return ROLE_SYSTEM_GRAPHIC;

    case ax::mojom::Role::kGrid:
      return ROLE_SYSTEM_TABLE;

    case ax::mojom::Role::kGridCell:
      return ROLE_SYSTEM_CELL;

    case ax::mojom::Role::kGroup:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kHeading:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kIframe:
      return ROLE_SYSTEM_DOCUMENT;

    case ax::mojom::Role::kIframePresentational:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kImage:
      return ROLE_SYSTEM_GRAPHIC;

    case ax::mojom::Role::kInputTime:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kInlineTextBox:
      return ROLE_SYSTEM_STATICTEXT;

    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kLegend:
      return ROLE_SYSTEM_TEXT;

    case ax::mojom::Role::kLayoutTable:
      return ROLE_SYSTEM_TABLE;

    case ax::mojom::Role::kLayoutTableCell:
      return ROLE_SYSTEM_CELL;

    case ax::mojom::Role::kLayoutTableRow:
      return ROLE_SYSTEM_ROW;

    case ax::mojom::Role::kLink:
      return ROLE_SYSTEM_LINK;

    case ax::mojom::Role::kList:
      return ROLE_SYSTEM_LIST;

    case ax::mojom::Role::kListBox:
      return ROLE_SYSTEM_LIST;

    case ax::mojom::Role::kListBoxOption:
      return ROLE_SYSTEM_LISTITEM;

    case ax::mojom::Role::kListGrid:
      return ROLE_SYSTEM_LIST;

    case ax::mojom::Role::kListItem:
      return ROLE_SYSTEM_LISTITEM;

    case ax::mojom::Role::kListMarker:
      // If a name is exposed, it's legacy layout, and this will be a leaf.
      // Otherwise, it's LayoutNG, and the text will be exposed in children.
      // In this case use an MSAA role of group, but IA2_ROLE_REDUNDANT_OBJECT
      // in order to avoid having the object be announced in JAWS/NVDA.
      return IsNameExposed() ? ROLE_SYSTEM_STATICTEXT : ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kLog:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kMain:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kMark:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kMarquee:
      return ROLE_SYSTEM_ANIMATION;

    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMathMLMath:
      return ROLE_SYSTEM_EQUATION;

    // TODO(http://crbug.com/1260584): Refine this if/when a MSAA API exists for
    // properly exposing MathML content.
    case ax::mojom::Role::kMathMLFraction:
    case ax::mojom::Role::kMathMLIdentifier:
    case ax::mojom::Role::kMathMLMultiscripts:
    case ax::mojom::Role::kMathMLNoneScript:
    case ax::mojom::Role::kMathMLNumber:
    case ax::mojom::Role::kMathMLOperator:
    case ax::mojom::Role::kMathMLOver:
    case ax::mojom::Role::kMathMLPrescriptDelimiter:
    case ax::mojom::Role::kMathMLRoot:
    case ax::mojom::Role::kMathMLRow:
    case ax::mojom::Role::kMathMLSquareRoot:
    case ax::mojom::Role::kMathMLStringLiteral:
    case ax::mojom::Role::kMathMLSub:
    case ax::mojom::Role::kMathMLSubSup:
    case ax::mojom::Role::kMathMLSup:
    case ax::mojom::Role::kMathMLTable:
    case ax::mojom::Role::kMathMLTableCell:
    case ax::mojom::Role::kMathMLTableRow:
    case ax::mojom::Role::kMathMLText:
    case ax::mojom::Role::kMathMLUnder:
    case ax::mojom::Role::kMathMLUnderOver:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kMenu:
      return ROLE_SYSTEM_MENUPOPUP;

    case ax::mojom::Role::kMenuBar:
      return ROLE_SYSTEM_MENUBAR;

    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
      return ROLE_SYSTEM_MENUITEM;

    case ax::mojom::Role::kMenuListPopup:
      return ROLE_SYSTEM_LIST;

    case ax::mojom::Role::kMenuListOption:
      return ROLE_SYSTEM_LISTITEM;

    case ax::mojom::Role::kMeter:
      return ROLE_SYSTEM_PROGRESSBAR;

    case ax::mojom::Role::kNavigation:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kNote:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kParagraph:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kPdfActionableHighlight:
      return ROLE_SYSTEM_PUSHBUTTON;

    case ax::mojom::Role::kPluginObject:
      // See also case ax::mojom::Role::kEmbeddedObject.
      if (GetDelegate()->GetChildCount()) {
        return ROLE_SYSTEM_GROUPING;
      } else {
        return ROLE_SYSTEM_CLIENT;
      }

    case ax::mojom::Role::kPopUpButton:
      return ROLE_SYSTEM_BUTTONMENU;

    case ax::mojom::Role::kProgressIndicator:
      return ROLE_SYSTEM_PROGRESSBAR;

    case ax::mojom::Role::kRadioButton:
      return ROLE_SYSTEM_RADIOBUTTON;

    case ax::mojom::Role::kRadioGroup:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kRegion:
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kRow: {
      // Role changes depending on whether row is inside a treegrid
      // https://www.w3.org/TR/core-aam-1.1/#role-map-row
      return IsInTreeGrid() ? ROLE_SYSTEM_OUTLINEITEM : ROLE_SYSTEM_ROW;
    }

    case ax::mojom::Role::kRowGroup:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kRowHeader:
      return ROLE_SYSTEM_ROWHEADER;

    case ax::mojom::Role::kRuby:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kRubyAnnotation:
      // Generally exposed as description on <ruby> (Role::kRuby) element, not
      // as its own object in the tree.
      // However, it's possible to make a kRubyAnnotation element show up in the
      // AX tree, for example by adding tabindex="0" to the source <rp> or <rt>
      // element or making the source element the target of an aria-owns.
      // Therefore, browser side needs to gracefully handle it if it actually
      // shows up in the tree.
      return ROLE_SYSTEM_STATICTEXT;

    case ax::mojom::Role::kSection:
    case ax::mojom::Role::kSectionFooter:
    case ax::mojom::Role::kSectionHeader:
    case ax::mojom::Role::kSectionWithoutName:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kScrollBar:
      return ROLE_SYSTEM_SCROLLBAR;

    case ax::mojom::Role::kScrollView:
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kSearch:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kSlider:
      return ROLE_SYSTEM_SLIDER;

    case ax::mojom::Role::kSpinButton:
      return ROLE_SYSTEM_SPINBUTTON;

    case ax::mojom::Role::kSwitch:
      return ROLE_SYSTEM_CHECKBUTTON;

    case ax::mojom::Role::kStaticText:
      return ROLE_SYSTEM_STATICTEXT;

    case ax::mojom::Role::kStatus:
      return ROLE_SYSTEM_STATUSBAR;

    case ax::mojom::Role::kSplitter:
      return ROLE_SYSTEM_SEPARATOR;

    case ax::mojom::Role::kSubscript:
    case ax::mojom::Role::kSuperscript:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kSvgRoot:
      return ROLE_SYSTEM_GRAPHIC;

    case ax::mojom::Role::kTab:
      return ROLE_SYSTEM_PAGETAB;

    case ax::mojom::Role::kTable:
      return ROLE_SYSTEM_TABLE;

    case ax::mojom::Role::kTableHeaderContainer:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kTabList:
      return ROLE_SYSTEM_PAGETABLIST;

    case ax::mojom::Role::kTabPanel:
      return ROLE_SYSTEM_PROPERTYPAGE;

    case ax::mojom::Role::kTerm:
      return ROLE_SYSTEM_LISTITEM;

    case ax::mojom::Role::kTitleBar:
      return ROLE_SYSTEM_TITLEBAR;

    case ax::mojom::Role::kToggleButton:
      return ROLE_SYSTEM_PUSHBUTTON;

    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kSearchBox:
      return ROLE_SYSTEM_TEXT;

    case ax::mojom::Role::kTextFieldWithComboBox:
      return ROLE_SYSTEM_COMBOBOX;

    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kCode:
    case ax::mojom::Role::kEmphasis:
    case ax::mojom::Role::kStrong:
      return ROLE_SYSTEM_TEXT;

    case ax::mojom::Role::kTime:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kTimer:
      return ROLE_SYSTEM_CLOCK;

    case ax::mojom::Role::kToolbar:
      return ROLE_SYSTEM_TOOLBAR;

    case ax::mojom::Role::kTooltip:
      return ROLE_SYSTEM_TOOLTIP;

    case ax::mojom::Role::kTree:
      return ROLE_SYSTEM_OUTLINE;

    case ax::mojom::Role::kTreeGrid:
      return ROLE_SYSTEM_OUTLINE;

    case ax::mojom::Role::kTreeItem:
      return ROLE_SYSTEM_OUTLINEITEM;

    case ax::mojom::Role::kLineBreak:
      return ROLE_SYSTEM_WHITESPACE;

    case ax::mojom::Role::kVideo:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kWebView:
      return ROLE_SYSTEM_CLIENT;

    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kWindow:
      // Do not return ROLE_SYSTEM_WINDOW as that is a special MSAA system
      // role used to indicate a real native window object. It is
      // automatically created by oleacc.dll as a parent of the root of our
      // hierarchy, matching the HWND.
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kImeCandidate:
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kUnknown:
      return ROLE_SYSTEM_PANE;
    case ax::mojom::Role::kDescriptionListTermDeprecated:
    case ax::mojom::Role::kDescriptionListDetailDeprecated:
    case ax::mojom::Role::kDirectoryDeprecated:
    case ax::mojom::Role::kPreDeprecated:
    case ax::mojom::Role::kPortalDeprecated:
      NOTREACHED();
  }
}

AXPlatformNodeWin* AXPlatformNodeWin::GetParentPlatformNodeWin() const {
  return static_cast<AXPlatformNodeWin*>(
      AXPlatformNode::FromNativeViewAccessible(GetParent()));
}

int32_t AXPlatformNodeWin::ComputeIA2State() {
  int32_t ia2_state = IA2_STATE_OPAQUE;

  if (IsPlatformCheckable())
    ia2_state |= IA2_STATE_CHECKABLE;

  if (HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      GetIntAttribute(ax::mojom::IntAttribute::kInvalidState) !=
          static_cast<int32_t>(ax::mojom::InvalidState::kFalse))
    ia2_state |= IA2_STATE_INVALID_ENTRY;
  if (HasState(ax::mojom::State::kRequired))
    ia2_state |= IA2_STATE_REQUIRED;
  if (HasState(ax::mojom::State::kVertical))
    ia2_state |= IA2_STATE_VERTICAL;
  if (HasState(ax::mojom::State::kHorizontal))
    ia2_state |= IA2_STATE_HORIZONTAL;

  if (HasState(ax::mojom::State::kEditable))
    ia2_state |= IA2_STATE_EDITABLE;

  if (IsTextField()) {
    if (HasState(ax::mojom::State::kMultiline)) {
      ia2_state |= IA2_STATE_MULTI_LINE;
    } else {
      ia2_state |= IA2_STATE_SINGLE_LINE;
    }
    if (!IsInvisibleOrIgnored())
      ia2_state |= IA2_STATE_SELECTABLE_TEXT;
  }

  if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete).empty() ||
      HasState(ax::mojom::State::kAutofillAvailable)) {
    ia2_state |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;
  }

  if (GetBoolAttribute(ax::mojom::BoolAttribute::kModal))
    ia2_state |= IA2_STATE_MODAL;

  // Clear editable state on some widgets.
  switch (GetRole()) {
    case ax::mojom::Role::kTreeItem:
    case ax::mojom::Role::kListBoxOption:
      // Clear editable state if text selection changes should not be spoken.
      // Subwidgets in a contenteditable such as tree items will clear
      // IA2_STATE_EDITABLE when used with aria-activedescendant.
      // HACK: look to remove in 2022 or later, once Google Slides no longer
      // needs this to avoid NVDA double speaking in slides thumb view.
      // Normally, NVDA will speak both a text selection and activedescendant
      // changes in an editor, but clearing IA2_STATE_EDITABLE prevents that.
      // This helps with user interfaces like Google slides that have a tree or
      // listbox widget inside an editor, which they currently do in order to
      // enable paste operations. Eventually this need should go away once IE11
      // support is no longer needed and Slides instead relies on paste events.
      if (!IsFocusable() ||
          GetBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot))
        break;  // Not used with activedescendant, so preserve editable state.
      [[fallthrough]];  // Will clear editable state.
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kMenuListOption:
      ia2_state &= ~(IA2_STATE_EDITABLE);
      break;
    default:
      break;
  }

  return ia2_state;
}

// ComputeIA2Role() only returns a role if the MSAA role doesn't suffice,
// otherwise this method returns 0. See AXPlatformNodeWin::role().
int32_t AXPlatformNodeWin::ComputeIA2Role() {
  // If this is a web area for a presentational iframe, give it a role of
  // something other than DOCUMENT so that the fact that it's a separate doc
  // is not exposed to AT.
  if (GetDelegate()->IsRootWebAreaForPresentationalIframe()) {
    return ROLE_SYSTEM_GROUPING;
  }

  int32_t ia2_role = 0;

  switch (GetRole()) {
    case ax::mojom::Role::kComment:
      return IA2_ROLE_COMMENT;
    case ax::mojom::Role::kSuggestion:
      return IA2_ROLE_SUGGESTION;
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      // CORE-AAM recommends LANDMARK instead of HEADER.
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kBlockquote:
      ia2_role = IA2_ROLE_BLOCK_QUOTE;
      break;
    case ax::mojom::Role::kCanvas:
      if (GetBoolAttribute(ax::mojom::BoolAttribute::kCanvasHasFallback)) {
        ia2_role = IA2_ROLE_CANVAS;
      }
      break;
    case ax::mojom::Role::kCaption:
      ia2_role = IA2_ROLE_CAPTION;
      break;
    case ax::mojom::Role::kColorWell:
      ia2_role = IA2_ROLE_COLOR_CHOOSER;
      break;
    case ax::mojom::Role::kComplementary:
      // CORE-AAM recommends LANDMARK instead of COMPLEMENTARY_CONTENT.
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kContentDeletion:
      ia2_role = IA2_ROLE_CONTENT_DELETION;
      break;
    case ax::mojom::Role::kContentInsertion:
      ia2_role = IA2_ROLE_CONTENT_INSERTION;
      break;
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      // CORE-AAM recommends LANDMARK instead of FOOTER.
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      ia2_role = IA2_ROLE_DATE_EDITOR;
      break;
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kDisclosureTriangleGrouped:
      if (::features::IsAccessibilityExposeSummaryAsHeadingEnabled()) {
        // TODO(accessibility): Consider keeping as button if inside list item
        // or table cell.
        ia2_role = IA2_ROLE_HEADING;
      }
      break;
    case ax::mojom::Role::kDocPageFooter:
      ia2_role = IA2_ROLE_FOOTER;
      break;
    case ax::mojom::Role::kDocPageHeader:
      ia2_role = IA2_ROLE_HEADER;
      break;
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIndex:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocToc:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ax::mojom::Role::kDocSubtitle:
      ia2_role = IA2_ROLE_HEADING;
      break;
    case ax::mojom::Role::kDocTip:
    case ax::mojom::Role::kDocNotice:
      ia2_role = IA2_ROLE_NOTE;
      break;
    case ax::mojom::Role::kDocFootnote:
      ia2_role = IA2_ROLE_FOOTNOTE;
      break;
    case ax::mojom::Role::kEmbeddedObject:
      // Even though the HTML-AAM has IA2_ROLE_EMBEDDED_OBJECT for <embed>, we
      // are forced to use IA2_ROLE_SECTION when the <embed> has children in the
      // accessibility tree.
      // https://www.w3.org/TR/html-aam-1.0/#html-element-role-mappings
      //
      // Screen readers Jaws and NVDA do not "see" any of the <embed>'s contents
      // if they are represented as its children in the accessibility tree. For
      // example, one of the places that would be negatively impacted is the
      // reading of PDFs.
      if (GetDelegate()->GetChildCount()) {
        ia2_role = IA2_ROLE_SECTION;
      } else {
        ia2_role = IA2_ROLE_EMBEDDED_OBJECT;
      }
      break;
    case ax::mojom::Role::kFigcaption:
      ia2_role = IA2_ROLE_CAPTION;
      break;
    case ax::mojom::Role::kForm:
      ia2_role = IA2_ROLE_FORM;
      break;
    case ax::mojom::Role::kGenericContainer:
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ax::mojom::Role::kHeading:
      ia2_role = IA2_ROLE_HEADING;
      break;
    case ax::mojom::Role::kIframe:
      ia2_role = IA2_ROLE_INTERNAL_FRAME;
      break;
    case ax::mojom::Role::kImage:
      if (IsImageWithMap())
        ia2_role = IA2_ROLE_IMAGE_MAP;
      break;
    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kLegend:
      ia2_role = IA2_ROLE_LABEL;
      break;
    case ax::mojom::Role::kListMarker:
      if (!IsNameExposed()) {
        // This role causes JAWS and NVDA to ignore the object.
        // Otherwise, they speak "group" before each bullet or item number.
        ia2_role = IA2_ROLE_REDUNDANT_OBJECT;
      }
      break;
    case ax::mojom::Role::kMain:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kMark:
      ia2_role = IA2_ROLE_MARK;
      break;
    case ax::mojom::Role::kMenuItemCheckBox:
      ia2_role = IA2_ROLE_CHECK_MENU_ITEM;
      break;
    case ax::mojom::Role::kMenuItemRadio:
      ia2_role = IA2_ROLE_RADIO_MENU_ITEM;
      break;
    case ax::mojom::Role::kMeter:
      // TODO(accessibiity) Uncomment IA2_ROLE_LEVEL_BAR once screen readers
      // adopt it. Currently, a <meter> ends up being spoken as a progress
      // bar, which is confusing. IA2_ROLE_LEVEL_BAR is the correct mapping
      // according to CORE-AAM. ia2_role = IA2_ROLE_LEVEL_BAR;
      break;
    case ax::mojom::Role::kNavigation:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kNote:
      ia2_role = IA2_ROLE_NOTE;
      break;
    case ax::mojom::Role::kParagraph:
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ax::mojom::Role::kPluginObject:
      // See also case ax::mojom::Role::kEmbeddedObject.
      if (GetDelegate()->GetChildCount()) {
        ia2_role = IA2_ROLE_SECTION;
      } else {
        ia2_role = IA2_ROLE_EMBEDDED_OBJECT;
      }
      break;
    case ax::mojom::Role::kRegion:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kRuby:
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ax::mojom::Role::kSearch:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kSection:
    case ax::mojom::Role::kSectionWithoutName:
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ax::mojom::Role::kSwitch:
      ia2_role = IA2_ROLE_TOGGLE_BUTTON;
      break;
    case ax::mojom::Role::kTableHeaderContainer:
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ax::mojom::Role::kToggleButton:
      ia2_role = IA2_ROLE_TOGGLE_BUTTON;
      break;
    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kCode:
    case ax::mojom::Role::kEmphasis:
    case ax::mojom::Role::kStrong:
    case ax::mojom::Role::kSubscript:
    case ax::mojom::Role::kSuperscript:
    case ax::mojom::Role::kTerm:
    case ax::mojom::Role::kTime:
      ia2_role = IA2_ROLE_TEXT_FRAME;
      break;
    case ax::mojom::Role::kDescriptionListDetailDeprecated:
    case ax::mojom::Role::kPreDeprecated:
      NOTREACHED();
    default:
      break;
  }
  return ia2_role;
}

std::vector<std::wstring> AXPlatformNodeWin::ComputeIA2Attributes() {
  std::vector<std::wstring> attribute_list;
  ComputeAttributes(&attribute_list);
  return attribute_list;
}

std::wstring AXPlatformNodeWin::ComputeUIAProperties() {
  std::vector<std::wstring> properties;
  BoolAttributeToUIAAriaProperty(
      properties, ax::mojom::BoolAttribute::kLiveAtomic, "atomic");
  BoolAttributeToUIAAriaProperty(properties, ax::mojom::BoolAttribute::kBusy,
                                 "busy");

  switch (GetData().GetCheckedState()) {
    case ax::mojom::CheckedState::kNone:
      break;
    case ax::mojom::CheckedState::kFalse:
      if (GetRole() == ax::mojom::Role::kToggleButton) {
        properties.emplace_back(L"pressed=false");
      } else if (GetRole() == ax::mojom::Role::kSwitch) {
        // ARIA switches are exposed to Windows accessibility as toggle
        // buttons. For maximum compatibility with ATs, we expose both the
        // pressed and checked states.
        properties.emplace_back(L"pressed=false");
        properties.emplace_back(L"checked=false");
      } else {
        properties.emplace_back(L"checked=false");
      }
      break;
    case ax::mojom::CheckedState::kTrue:
      if (GetRole() == ax::mojom::Role::kToggleButton) {
        properties.emplace_back(L"pressed=true");
      } else if (GetRole() == ax::mojom::Role::kSwitch) {
        // ARIA switches are exposed to Windows accessibility as toggle
        // buttons. For maximum compatibility with ATs, we expose both the
        // pressed and checked states.
        properties.emplace_back(L"pressed=true");
        properties.emplace_back(L"checked=true");
      } else {
        properties.emplace_back(L"checked=true");
      }
      break;
    case ax::mojom::CheckedState::kMixed:
      if (GetRole() == ax::mojom::Role::kToggleButton) {
        properties.emplace_back(L"pressed=mixed");
      } else if (GetRole() == ax::mojom::Role::kSwitch) {
        // This is disallowed both by the ARIA standard and by Blink.
        NOTREACHED_IN_MIGRATION();
      } else {
        properties.emplace_back(L"checked=mixed");
      }
      break;
  }

  const auto restriction = static_cast<ax::mojom::Restriction>(
      GetIntAttribute(ax::mojom::IntAttribute::kRestriction));
  if (restriction == ax::mojom::Restriction::kDisabled) {
    properties.push_back(L"disabled=true");
  } else {
    // The readonly property is complex on Windows. We set "readonly=true"
    // on *some* document structure roles such as paragraph, heading or list
    // even if the node data isn't marked as read only, as long as the
    // node is not editable.
    if (GetDelegate()->IsReadOnlyOrDisabled())
      properties.push_back(L"readonly=true");
  }

  // aria-dropeffect is deprecated in WAI-ARIA 1.1.
  if (HasIntAttribute(ax::mojom::IntAttribute::kDropeffectDeprecated)) {
    NOTREACHED_IN_MIGRATION();
  }
  StateToUIAAriaProperty(properties, ax::mojom::State::kExpanded, "expanded");

  switch (static_cast<ax::mojom::HasPopup>(
      GetIntAttribute(ax::mojom::IntAttribute::kHasPopup))) {
    case ax::mojom::HasPopup::kFalse:
      break;
    case ax::mojom::HasPopup::kTrue:
      properties.push_back(L"haspopup=true");
      break;
    case ax::mojom::HasPopup::kMenu:
      properties.push_back(L"haspopup=menu");
      break;
    case ax::mojom::HasPopup::kListbox:
      properties.push_back(L"haspopup=listbox");
      break;
    case ax::mojom::HasPopup::kTree:
      properties.push_back(L"haspopup=tree");
      break;
    case ax::mojom::HasPopup::kGrid:
      properties.push_back(L"haspopup=grid");
      break;
    case ax::mojom::HasPopup::kDialog:
      properties.push_back(L"haspopup=dialog");
      break;
  }

  if (IsInvisibleOrIgnored())
    properties.push_back(L"hidden=true");

  if (HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      GetIntAttribute(ax::mojom::IntAttribute::kInvalidState) !=
          static_cast<int32_t>(ax::mojom::InvalidState::kFalse)) {
    properties.push_back(L"invalid=true");
  }

  IntAttributeToUIAAriaProperty(
      properties, ax::mojom::IntAttribute::kHierarchicalLevel, "level");
  StringAttributeToUIAAriaProperty(
      properties, ax::mojom::StringAttribute::kLiveStatus, "live");
  StateToUIAAriaProperty(properties, ax::mojom::State::kMultiline, "multiline");
  StateToUIAAriaProperty(properties, ax::mojom::State::kMultiselectable,
                         "multiselectable");
  IntAttributeToUIAAriaProperty(properties, ax::mojom::IntAttribute::kPosInSet,
                                "posinset");
  StringAttributeToUIAAriaProperty(
      properties, ax::mojom::StringAttribute::kLiveRelevant, "relevant");
  StateToUIAAriaProperty(properties, ax::mojom::State::kRequired, "required");
  BoolAttributeToUIAAriaProperty(
      properties, ax::mojom::BoolAttribute::kSelected, "selected");
  IntAttributeToUIAAriaProperty(properties, ax::mojom::IntAttribute::kSetSize,
                                "setsize");

  int32_t sort_direction;
  if (IsTableHeader(GetRole()) &&
      GetIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                      &sort_direction)) {
    switch (static_cast<ax::mojom::SortDirection>(sort_direction)) {
      case ax::mojom::SortDirection::kNone:
        break;
      case ax::mojom::SortDirection::kUnsorted:
        properties.push_back(L"sort=none");
        break;
      case ax::mojom::SortDirection::kAscending:
        properties.push_back(L"sort=ascending");
        break;
      case ax::mojom::SortDirection::kDescending:
        properties.push_back(L"sort=descending");
        break;
      case ax::mojom::SortDirection::kOther:
        properties.push_back(L"sort=other");
        break;
    }
  }

  if (GetData().IsRangeValueSupported()) {
    FloatAttributeToUIAAriaProperty(
        properties, ax::mojom::FloatAttribute::kMaxValueForRange, "valuemax");
    FloatAttributeToUIAAriaProperty(
        properties, ax::mojom::FloatAttribute::kMinValueForRange, "valuemin");
    StringAttributeToUIAAriaProperty(
        properties, ax::mojom::StringAttribute::kValue, "valuetext");

    std::wstring value_now = base::UTF16ToWide(GetValueForControl());
    SanitizeStringAttributeForUIAAriaProperty(value_now, &value_now);
    if (!value_now.empty())
      properties.push_back(L"valuenow=" + value_now);
  }

  // Expose the aria-current attribute as 'current=<value>' if <value> is not
  // 'none'.
  int32_t aria_current_attribute;
  if (GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState,
                      &aria_current_attribute)) {
    ax::mojom::AriaCurrentState aria_current_state =
        static_cast<ax::mojom::AriaCurrentState>(aria_current_attribute);
    if (aria_current_state != ax::mojom::AriaCurrentState::kNone &&
        aria_current_state != ax::mojom::AriaCurrentState::kFalse) {
      std::string value = ui::ToString(aria_current_state);
      std::wstring wide_value = base::UTF8ToWide(value);
      SanitizeStringAttributeForUIAAriaProperty(wide_value, &wide_value);
      properties.push_back(L"current=" + wide_value);
    }
  }

  std::wstring result = base::JoinString(properties, L";");
  return result;
}

AXPlatformNodeWin* AXPlatformNodeWin::ComputeUIALabeledBy() {
  // Not all control types expect a value for this property.
  if (!CanHaveUIALabeledBy())
    return nullptr;

  // This property only accepts static text elements to be returned. Find the
  // first static text used to label this node.
  for (int32_t id :
       GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds)) {
    auto* node_win =
        static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(id));
    if (!node_win)
      continue;

    // If this node is a static text, then simply return the node itself.
    if (IsValidUiaRelationTarget(node_win) &&
        node_win->GetRole() == ax::mojom::Role::kStaticText) {
      return node_win;
    }

    // Otherwise, find the first static text node in its descendants.
    for (auto iter = node_win->GetDelegate()->ChildrenBegin();
         *iter != *node_win->GetDelegate()->ChildrenEnd(); ++(*iter)) {
      AXPlatformNodeWin* child = static_cast<AXPlatformNodeWin*>(
          AXPlatformNode::FromNativeViewAccessible(
              iter->GetNativeViewAccessible()));
      if (IsValidUiaRelationTarget(child) &&
          child->GetRole() == ax::mojom::Role::kStaticText) {
        return child;
      }
    }
  }

  return nullptr;
}

bool AXPlatformNodeWin::CanHaveUIALabeledBy() {
  // Not all control types expect a value for this property. See
  // https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-supportinguiautocontroltypes
  // for a complete list of control types. Each one of them has specific
  // expectations regarding the UIA_LabeledByPropertyId.
  switch (GetUIARoleProperties().control_type) {
    case UIA_ButtonControlTypeId:
    case UIA_CheckBoxControlTypeId:
    case UIA_DataItemControlTypeId:
    case UIA_MenuControlTypeId:
    case UIA_MenuBarControlTypeId:
    case UIA_RadioButtonControlTypeId:
    case UIA_ScrollBarControlTypeId:
    case UIA_SeparatorControlTypeId:
    case UIA_StatusBarControlTypeId:
    case UIA_TabItemControlTypeId:
    case UIA_TextControlTypeId:
    case UIA_ToolBarControlTypeId:
    case UIA_ToolTipControlTypeId:
    case UIA_TreeItemControlTypeId:
      return false;
    default:
      return true;
  }
}

bool AXPlatformNodeWin::IsNameExposed() const {
  switch (GetRole()) {
    case ax::mojom::Role::kListMarker:
      return !GetDelegate()->GetChildCount();
    default:
      return true;
  }
}

bool AXPlatformNodeWin::IsUIAControl() const {
  // UIA provides multiple "views": raw, content and control. We only want to
  // populate the content and control views with items that make sense to
  // traverse over.

  if (GetDelegate()->IsWebContent()) {
    // Invisible or ignored elements should not show up in control view at all.
    if (IsInvisibleOrIgnored())
      return false;

    if (IsText()) {
      // A text leaf can be a UIAControl, but text inside of a heading, link,
      // button, etc. where the role allows the name to be generated from the
      // content is not. We want to avoid reading out a button, moving to the
      // next item, and then reading out the button's text child, causing the
      // text to be effectively repeated.
      auto* ancestor = FromNativeViewAccessible(GetDelegate()->GetParent());
      while (ancestor) {
        if (IsUIACellOrTableHeader(ancestor->GetRole()))
          return false;
        switch (ancestor->GetRole()) {
          // There are elements inside the `kColorWell` element that we want
          // exposed as UIA Control even if they are inside other elements that
          // are not exposed as UIA Controls. Like for example the text live
          // regions of the RGB channels inside the `kColorWell`. Without this
          // case, if we have a `kColorWell` inside a table cell, the RGB
          // channels text does not get announced by Narrator since we would
          // break and return false on the condition above this one when going
          // up the ancestor nodes.
          // TODO(accessibility): This is a special case mitigation for
          // `kColorWell`, there is a broader bug https://crbug.com/1414227 with
          // live region elements inside these elements that are not exposed as
          // UIA Controls that will require more work and investigation.
          case ax::mojom::Role::kColorWell:
            return true;
          case ax::mojom::Role::kListItem:
            // We only want to hide in the case that the list item is able
            // to have its name generated from its children.
            // See |ComputeListItemNameAsBstr|. This is only possible when the
            // element is a direct child of the list item, otherwise the child
            // should be exposed as a UIA Control.
            return ancestor !=
                   FromNativeViewAccessible(GetDelegate()->GetParent());
          case ax::mojom::Role::kButton:
          case ax::mojom::Role::kCheckBox:
          case ax::mojom::Role::kHeading:
          case ax::mojom::Role::kLineBreak:
          case ax::mojom::Role::kLink:
          case ax::mojom::Role::kListBoxOption:
          case ax::mojom::Role::kMenuItem:
          case ax::mojom::Role::kMenuItemCheckBox:
          case ax::mojom::Role::kMenuItemRadio:
          case ax::mojom::Role::kMenuListOption:
          case ax::mojom::Role::kPdfActionableHighlight:
          case ax::mojom::Role::kRadioButton:
          case ax::mojom::Role::kRow:
          case ax::mojom::Role::kRowGroup:
          case ax::mojom::Role::kStaticText:
          case ax::mojom::Role::kSwitch:
          case ax::mojom::Role::kTab:
          case ax::mojom::Role::kTooltip:
          case ax::mojom::Role::kTreeItem:
            return false;
          default:
            break;
        }
        ancestor = FromNativeViewAccessible(ancestor->GetParent());
      }
    }  // end of text only case.

    // https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-treeoverview#control-view
    // The control view also includes noninteractive UI items that contribute
    // to the logical structure of the UI.
    if (IsControl(GetRole()) || ComputeUIALandmarkType() ||
        IsUIATableLike(GetRole()) || IsList(GetRole())) {
      return true;
    }
    if (IsImage(GetRole())) {
      // If the author provides an explicitly empty alt text attribute then
      // the image is decorational and should not be considered as a control.
      if (GetRole() == ax::mojom::Role::kImage &&
          GetNameFrom() == ax::mojom::NameFrom::kAttributeExplicitlyEmpty) {
        return false;
      }
      return true;
    }
    switch (GetRole()) {
      case ax::mojom::Role::kArticle:
      case ax::mojom::Role::kBlockquote:
      case ax::mojom::Role::kCell:
      case ax::mojom::Role::kColumn:
      case ax::mojom::Role::kDetails:
      case ax::mojom::Role::kFigure:
      case ax::mojom::Role::kFooter:
      case ax::mojom::Role::kGridCell:
      case ax::mojom::Role::kHeader:
      case ax::mojom::Role::kListBoxOption:
      case ax::mojom::Role::kListItem:
      // Treat the root of a MathML tree as content/control so that it is seen
      // by UIA clients. The remainder of the tree remains as text for now until
      // UIA mappings for MathML are defined (https://crbug.com/1260585).
      case ax::mojom::Role::kMathMLMath:
      case ax::mojom::Role::kMeter:
      case ax::mojom::Role::kProgressIndicator:
      case ax::mojom::Role::kRow:
      case ax::mojom::Role::kSection:
      case ax::mojom::Role::kSectionFooter:
      case ax::mojom::Role::kSectionHeader:
      case ax::mojom::Role::kSplitter:
      case ax::mojom::Role::kStatus:
      case ax::mojom::Role::kTime:
        return true;
      default:
        break;
    }
    // Classify generic containers that are not clickable or focusable and have
    // no name, description, landmark type, and is not the root of editable
    // content as not controls.
    // Doing so helps Narrator find all the content of live regions.
    if (!GetBoolAttribute(ax::mojom::BoolAttribute::kHasAriaAttribute) &&
        !GetBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot) &&
        GetName().empty() &&
        GetStringAttribute(ax::mojom::StringAttribute::kDescription).empty() &&
        !HasState(ax::mojom::State::kFocusable) && !GetData().IsClickable()) {
      return false;
    }

    return true;
  }  // end of web-content only case.

  // TODO(accessibility): This condition is very wide - it returns true for most
  // elements, except the ones that are explicitly invisible/ignored and not
  // focusable, and the ones that are expected to fire live region events. We
  // might want to revisit this implementation to match the specs:
  // https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-treeoverview#control-view.
  //
  // Also, should we really have a different implementation for Views than for
  // web content?
  return !(IsInvisibleOrIgnored() && !IsFocusable()) ||
         GetRole() == ax::mojom::Role::kStatus;
}

std::optional<LONG> AXPlatformNodeWin::ComputeUIALandmarkType() const {
  switch (GetRole()) {
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kHeader:
      return UIA_CustomLandmarkTypeId;

    case ax::mojom::Role::kForm:
      // https://www.w3.org/TR/html-aam-1.0/#html-element-role-mappings
      // https://w3c.github.io/core-aam/#mapping_role_table
      // While the HTML-AAM spec states that <form> without an accessible name
      // should have no corresponding role, removing the role breaks both
      // aria-setsize and aria-posinset.
      // The only other difference for UIA is that it should not be a landmark.
      // If the author provided an accessible name, or the role was explicit,
      // then allow the form landmark.
      if (HasStringAttribute(ax::mojom::StringAttribute::kName) ||
          HasStringAttribute(ax::mojom::StringAttribute::kRole)) {
        return UIA_FormLandmarkTypeId;
      }
      return {};

    case ax::mojom::Role::kMain:
      return UIA_MainLandmarkTypeId;

    case ax::mojom::Role::kNavigation:
      return UIA_NavigationLandmarkTypeId;

    case ax::mojom::Role::kSearch:
      return UIA_SearchLandmarkTypeId;

    case ax::mojom::Role::kRegion:
      return UIA_CustomLandmarkTypeId;

    default:
      return {};
  }
}

bool AXPlatformNodeWin::IsInaccessibleForUIA() const {
  if (IsNodeInaccessibleForUIA())
    return true;

  AXPlatformNodeWin* parent = GetParentPlatformNodeWin();
  while (parent) {
    if (parent->IsNodeInaccessibleForUIA() ||
        parent->ShouldHideChildrenForUIA()) {
      return true;
    }
    parent = parent->GetParentPlatformNodeWin();
  }
  return false;
}

bool AXPlatformNodeWin::ShouldHideChildrenForUIA() const {
  if (IsAtomicTextField())
    return true;

  auto role = GetRole();
  switch (role) {
    // Even though a node with  role kButton has presentational children, it
    // should only hide its children from UIA when it has a single text node
    // (to avoid having its name announced twice). This is because buttons can
    // have complex structures and they shouldn't hide their subtree.
    case ax::mojom::Role::kButton:
    // TODO(bebeaudr): We might be able to remove ax::mojom::Role::kLink once
    // http://crbug.com/1054514 is fixed. Links should not have to hide their
    // children.
    case ax::mojom::Role::kLink:
      // Links with a single text-only child should hide their subtree.
      if (GetChildCount() == 1) {
        AXPlatformNodeBase* only_child = GetFirstChild();
        return only_child && only_child->IsText();
      }
      return false;
    // TODO(virens): |kPdfActionableHighlight| needs to follow a fix similar to
    // links. At present Pdf highlights have text nodes as children. But, we may
    // enable pdf highlights to have complex children like links based on user
    // feedback.
    case ax::mojom::Role::kPdfActionableHighlight:
      return true;
    default:
      // UIA expects nodes that have "Children Presentational: True" to hide
      // their children.
      return HasPresentationalChildren(role);
  }
}

ULONG AXPlatformNodeWin::InternalAddRef() {
  // Instances of AXPlatformNodeWin hold a reference to themselves (acquired in
  // `Create`; released in `Dispose`). When the refcount rises from 1 to 2
  // before the node has been disposed, infer that the instance is being used
  // for some COM-ish purpose; for example, being handed to an accessibility
  // tool via a WM_GETOBJECT message handler.
  const auto ref_count = SequenceAffineComObjectRoot::InternalAddRef();
  if (delegate_ && ref_count == 2) {
    OnReferenced();
  }
  return ref_count;
}

ULONG AXPlatformNodeWin::InternalRelease() {
  // As above, infer that the instance is no longer being used for some COM-ish
  // purpose when the refcount drops back down to 1 (if it has yet to be
  // disposed) or 0 (if it has been).
  const auto ref_count = SequenceAffineComObjectRoot::InternalRelease();
  if (ref_count == (delegate_ ? 1 : 0)) {
    OnDereferenced();
  }
  return ref_count;
}

void AXPlatformNodeWin::OnReferenced() {
  TRACE_EVENT("accessibility", "OnReferenced",
              perfetto::Flow::FromPointer(this), "UniqueId",
              base::NumberToString(GetUniqueId()));
}

void AXPlatformNodeWin::OnDereferenced() {
  TRACE_EVENT("accessibility", "OnDereferenced",
              perfetto::TerminatingFlow::FromPointer(this));
}

bool AXPlatformNodeWin::IsPlatformCheckable() const {
  if (GetRole() == ax::mojom::Role::kToggleButton)
    return false;

  return AXPlatformNodeBase::IsPlatformCheckable();
}

int AXPlatformNodeWin::MSAAState() const {
  int msaa_state = 0;

  // Map the ax::mojom::State to MSAA state. Note that some of the states are
  // not currently handled.

  // TODO(accessibility): https://crbug.com/1292018
  // Exposing the busy state on the root web area means the NVDA user will end
  // up without a virtualBuffer until the page fully loads. So if we have
  // content, don't expose the busy state.
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kBusy)) {
    if (!IsPlatformDocument() || !GetChildCount())
      msaa_state |= STATE_SYSTEM_BUSY;
  }

  if (HasState(ax::mojom::State::kCollapsed))
    msaa_state |= STATE_SYSTEM_COLLAPSED;

  if (HasState(ax::mojom::State::kDefault))
    msaa_state |= STATE_SYSTEM_DEFAULT;

  // TODO(dougt) unhandled ux::ax::mojom::State::kEditable

  if (HasState(ax::mojom::State::kExpanded))
    msaa_state |= STATE_SYSTEM_EXPANDED;

  if (IsFocusable())
    msaa_state |= STATE_SYSTEM_FOCUSABLE;

  // Built-in autofill and autocomplete wil also set has popup.
  if (HasIntAttribute(ax::mojom::IntAttribute::kHasPopup))
    msaa_state |= STATE_SYSTEM_HASPOPUP;

  // TODO(dougt) unhandled ux::ax::mojom::State::kHorizontal

  if (HasState(ax::mojom::State::kHovered)) {
    // Expose whether or not the mouse is over an element, but suppress
    // this for tests because it can make the test results flaky depending
    // on the position of the mouse.
    if (GetDelegate()->ShouldIgnoreHoveredStateForTesting())
      msaa_state |= STATE_SYSTEM_HOTTRACKED;
  }

  // If the node is ignored, we want these elements to be invisible so that
  // they are hidden from the screen reader.
  if (IsInvisibleOrIgnored())
    msaa_state |= STATE_SYSTEM_INVISIBLE;

  if (HasState(ax::mojom::State::kLinked))
    msaa_state |= STATE_SYSTEM_LINKED;

  // TODO(dougt) unhandled ux::ax::mojom::State::kMultiline

  if (HasState(ax::mojom::State::kMultiselectable)) {
    msaa_state |= STATE_SYSTEM_EXTSELECTABLE;
    msaa_state |= STATE_SYSTEM_MULTISELECTABLE;
  }

  if (GetDelegate()->IsOffscreen())
    msaa_state |= STATE_SYSTEM_OFFSCREEN;

  if (HasState(ax::mojom::State::kProtected))
    msaa_state |= STATE_SYSTEM_PROTECTED;

  // TODO(dougt) unhandled ux::ax::mojom::State::kRequired
  // TODO(dougt) unhandled ux::ax::mojom::State::kRichlyEditable

  if (GetData().IsSelectable())
    msaa_state |= STATE_SYSTEM_SELECTABLE;

  if (GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    msaa_state |= STATE_SYSTEM_SELECTED;

  // TODO(dougt) unhandled VERTICAL

  if (HasState(ax::mojom::State::kVisited))
    msaa_state |= STATE_SYSTEM_TRAVERSED;

  //
  // Checked state
  //

  switch (GetData().GetCheckedState()) {
    case ax::mojom::CheckedState::kNone:
    case ax::mojom::CheckedState::kFalse:
      break;
    case ax::mojom::CheckedState::kTrue:
      if (GetRole() == ax::mojom::Role::kToggleButton) {
        msaa_state |= STATE_SYSTEM_PRESSED;
      } else if (GetRole() == ax::mojom::Role::kSwitch) {
        // ARIA switches are exposed to Windows accessibility as toggle
        // buttons. For maximum compatibility with ATs, we expose both the
        // pressed and checked states.
        msaa_state |= STATE_SYSTEM_PRESSED | STATE_SYSTEM_CHECKED;
      } else {
        msaa_state |= STATE_SYSTEM_CHECKED;
      }
      break;
    case ax::mojom::CheckedState::kMixed:
      msaa_state |= STATE_SYSTEM_MIXED;
      break;
  }

  const auto restriction = static_cast<ax::mojom::Restriction>(
      GetIntAttribute(ax::mojom::IntAttribute::kRestriction));
  switch (restriction) {
    case ax::mojom::Restriction::kDisabled:
      msaa_state |= STATE_SYSTEM_UNAVAILABLE;
      break;
    case ax::mojom::Restriction::kReadOnly:
      msaa_state |= STATE_SYSTEM_READONLY;
      break;
    default:
      // READONLY state is complex on Windows.  We set STATE_SYSTEM_READONLY
      // on *some* document structure roles such as paragraph, heading or list
      // even if the node data isn't marked as read only, as long as the
      // node is not editable.
      if (!HasState(ax::mojom::State::kRichlyEditable) &&
          ShouldHaveReadonlyStateByDefault(GetRole())) {
        msaa_state |= STATE_SYSTEM_READONLY;
      }
      break;
  }

  // Windowless plugins should have STATE_SYSTEM_UNAVAILABLE.
  //
  // (All of our plugins are windowless.)
  if (GetRole() == ax::mojom::Role::kPluginObject ||
      GetRole() == ax::mojom::Role::kEmbeddedObject) {
    msaa_state |= STATE_SYSTEM_UNAVAILABLE;
  }

  //
  // Handle STATE_SYSTEM_FOCUSED
  //
  gfx::NativeViewAccessible focus = GetDelegate()->GetFocus();
  if (focus == const_cast<AXPlatformNodeWin*>(this)->GetNativeViewAccessible())
    msaa_state |= STATE_SYSTEM_FOCUSED;

  // In focused single selection UI menus and listboxes, mirror item selection
  // to focus. This helps NVDA read the selected option as it changes.
  if ((GetRole() == ax::mojom::Role::kListBoxOption || IsMenuItem(GetRole())) &&
      GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    AXPlatformNodeBase* container = FromNativeViewAccessible(GetParent());
    if (container && container->GetParent() == focus) {
      if ((container->GetRole() == ax::mojom::Role::kListBox ||
           container->GetRole() == ax::mojom::Role::kMenu) &&
          !container->HasState(ax::mojom::State::kMultiselectable)) {
        msaa_state |= STATE_SYSTEM_FOCUSED;
      }
    }
  }

  // On Windows, the "focus" bit should be set on certain containers, like
  // menu bars, when visible.
  //
  // Note: this should probably check if focus is actually inside
  // the menu bar, but we don't currently track focus inside menu pop-ups,
  // and Chrome only has one menu visible at a time so this works for now.
  if (GetRole() == ax::mojom::Role::kMenuBar && !IsInvisibleOrIgnored())
    msaa_state |= STATE_SYSTEM_FOCUSED;

  // Handle STATE_SYSTEM_LINKED
  if (GetRole() == ax::mojom::Role::kLink)
    msaa_state |= STATE_SYSTEM_LINKED;

  // Special case for indeterminate progressbar.
  if (GetRole() == ax::mojom::Role::kProgressIndicator &&
      !HasStringAttribute(ax::mojom::StringAttribute::kValue) &&
      !HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange)) {
    msaa_state |= STATE_SYSTEM_MIXED;
  }

  return msaa_state;
}

// static
std::optional<DWORD> AXPlatformNodeWin::MojoEventToMSAAEvent(
    ax::mojom::Event event) {
  switch (event) {
    case ax::mojom::Event::kAlert:
      return EVENT_SYSTEM_ALERT;
    case ax::mojom::Event::kActiveDescendantChanged:
      return IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
    case ax::mojom::Event::kCheckedStateChanged:
    case ax::mojom::Event::kExpandedChanged:
    case ax::mojom::Event::kStateChanged:
      return EVENT_OBJECT_STATECHANGE;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
    case ax::mojom::Event::kFocusAfterMenuClose:
      return EVENT_OBJECT_FOCUS;
    case ax::mojom::Event::kLiveRegionChanged:
      return EVENT_OBJECT_LIVEREGIONCHANGED;
    case ax::mojom::Event::kMenuStart:
      return EVENT_SYSTEM_MENUSTART;
    case ax::mojom::Event::kMenuEnd:
      return EVENT_SYSTEM_MENUEND;
    case ax::mojom::Event::kMenuPopupStart:
      return EVENT_SYSTEM_MENUPOPUPSTART;
    case ax::mojom::Event::kMenuPopupEnd:
      return EVENT_SYSTEM_MENUPOPUPEND;
    case ax::mojom::Event::kSelection:
      return EVENT_OBJECT_SELECTION;
    case ax::mojom::Event::kSelectionAdd:
      return EVENT_OBJECT_SELECTIONADD;
    case ax::mojom::Event::kSelectionRemove:
      return EVENT_OBJECT_SELECTIONREMOVE;
    case ax::mojom::Event::kTextChanged:
      return EVENT_OBJECT_NAMECHANGE;
    case ax::mojom::Event::kTextSelectionChanged:
      return IA2_EVENT_TEXT_CARET_MOVED;
    case ax::mojom::Event::kTooltipClosed:
      return EVENT_OBJECT_HIDE;
    case ax::mojom::Event::kTooltipOpened:
      return EVENT_OBJECT_SHOW;
    case ax::mojom::Event::kValueChanged:
      return EVENT_OBJECT_VALUECHANGE;
    default:
      return std::nullopt;
  }
}

// static
std::optional<EVENTID> AXPlatformNodeWin::MojoEventToUIAEvent(
    ax::mojom::Event event) {
  if (!AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    return std::nullopt;
  }

  switch (event) {
    case ax::mojom::Event::kAlert:
      return UIA_SystemAlertEventId;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
    case ax::mojom::Event::kFocusAfterMenuClose:
      return UIA_AutomationFocusChangedEventId;
    case ax::mojom::Event::kLiveRegionChanged:
      return UIA_LiveRegionChangedEventId;
    case ax::mojom::Event::kMenuStart:
      return UIA_MenuModeStartEventId;
    case ax::mojom::Event::kMenuEnd:
      return UIA_MenuModeEndEventId;
    case ax::mojom::Event::kMenuPopupStart:
      return UIA_MenuOpenedEventId;
    case ax::mojom::Event::kMenuPopupEnd:
      return UIA_MenuClosedEventId;
    case ax::mojom::Event::kSelection:
      return UIA_SelectionItem_ElementSelectedEventId;
    case ax::mojom::Event::kSelectionAdd:
      return UIA_SelectionItem_ElementAddedToSelectionEventId;
    case ax::mojom::Event::kSelectionRemove:
      return UIA_SelectionItem_ElementRemovedFromSelectionEventId;
    case ax::mojom::Event::kTextSelectionChanged:
      return UIA_Text_TextSelectionChangedEventId;
    case ax::mojom::Event::kTooltipClosed:
      return UIA_ToolTipClosedEventId;
    case ax::mojom::Event::kTooltipOpened:
      return UIA_ToolTipOpenedEventId;
    default:
      return std::nullopt;
  }
}

// static
std::optional<PROPERTYID> AXPlatformNodeWin::MojoEventToUIAProperty(
    ax::mojom::Event event) {
  if (!AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    return std::nullopt;
  }

  switch (event) {
    case ax::mojom::Event::kControlsChanged:
      return UIA_ControllerForPropertyId;
    case ax::mojom::Event::kCheckedStateChanged:
      return UIA_ToggleToggleStatePropertyId;
    case ax::mojom::Event::kExpandedChanged:
    case ax::mojom::Event::kRowCollapsed:
    case ax::mojom::Event::kRowExpanded:
      return UIA_ExpandCollapseExpandCollapseStatePropertyId;
    case ax::mojom::Event::kSelection:
    case ax::mojom::Event::kSelectionAdd:
    case ax::mojom::Event::kSelectionRemove:
      return UIA_SelectionItemIsSelectedPropertyId;
    case ax::mojom::Event::kTextChanged:
      return UIA_NamePropertyId;
    default:
      return std::nullopt;
  }
}

// static
BSTR AXPlatformNodeWin::GetValueAttributeAsBstr(AXPlatformNodeWin* target) {
  if (target->IsPlatformDocument()) {
    std::wstring url =
        base::UTF8ToWide(target->GetDelegate()->GetTreeData().url);
    BSTR value = SysAllocString(url.c_str());
    DCHECK(value);
    return value;
  }

  if (IsLink(target->GetRole())) {
    std::wstring url = base::UTF8ToWide(
        target->GetStringAttribute(ax::mojom::StringAttribute::kUrl));
    BSTR value = SysAllocString(url.c_str());
    DCHECK(value);
    return value;
  }

  BSTR value = SysAllocString(base::as_wcstr(target->GetValueForControl()));
  DCHECK(value);
  return value;
}

HRESULT AXPlatformNodeWin::GetStringAttributeAsBstr(
    ax::mojom::StringAttribute attribute,
    BSTR* value_bstr) const {
  std::u16string str;

  if (!GetString16Attribute(attribute, &str))
    return S_FALSE;

  *value_bstr = SysAllocString(base::as_wcstr(str));
  DCHECK(*value_bstr);

  return S_OK;
}

HRESULT AXPlatformNodeWin::GetNameAsBstr(BSTR* value_bstr) const {
  std::wstring str = base::UTF8ToWide(GetName());

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);
  return S_OK;
}

HRESULT AXPlatformNodeWin::ComputeListItemNameAsBstr(BSTR* value_bstr) const {
  DCHECK_EQ(GetRole(), ax::mojom::Role::kListItem);
  DCHECK(!HasStringAttribute(ax::mojom::StringAttribute::kName));
  std::wstring str;
  // If the name is specified by the author.
  if (GetNameFrom() == ax::mojom::NameFrom::kAttribute) {
    str = base::UTF8ToWide(GetName());
  } else {
    str = GetDelegate()->ComputeListItemNameFromContent();
  }

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);
  return S_OK;
}

void AXPlatformNodeWin::AddAlertTarget() {
  g_alert_targets.Get().insert(this);
}

void AXPlatformNodeWin::RemoveAlertTarget() {
  if (g_alert_targets.Get().find(this) != g_alert_targets.Get().end())
    g_alert_targets.Get().erase(this);
}

void AXPlatformNodeWin::HandleSpecialTextOffset(LONG* offset) {
  if (*offset == IA2_TEXT_OFFSET_LENGTH) {
    *offset = static_cast<LONG>(GetHypertext().length());
  } else if (*offset == IA2_TEXT_OFFSET_CARET) {
    int selection_start, selection_end;
    GetSelectionOffsets(&selection_start, &selection_end);
    // TODO(nektar): Deprecate selection_start and selection_end in favor of
    // anchor_offset/focus_offset. See https://crbug.com/645596.
    if (selection_end < 0)
      *offset = 0;
    else
      *offset = static_cast<LONG>(selection_end);
  }
}

LONG AXPlatformNodeWin::FindBoundary(IA2TextBoundaryType ia2_boundary,
                                     LONG start_offset,
                                     ax::mojom::MoveDirection direction) {
  HandleSpecialTextOffset(&start_offset);

  // If the |start_offset| is equal to the location of the caret, then use the
  // focus affinity, otherwise default to downstream affinity.
  ax::mojom::TextAffinity affinity = ax::mojom::TextAffinity::kDownstream;
  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  if (selection_end >= 0 && start_offset == selection_end)
    affinity = GetDelegate()->GetTreeData().sel_focus_affinity;

  ax::mojom::TextBoundary boundary = FromIA2TextBoundary(ia2_boundary);
  return static_cast<LONG>(
      FindTextBoundary(boundary, start_offset, direction, affinity));
}

AXPlatformNodeWin* AXPlatformNodeWin::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (V_VT(&var_id) != VT_I4)
    return nullptr;

  LONG child_id = V_I4(&var_id);
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 &&
      static_cast<size_t>(child_id) <= GetDelegate()->GetChildCount()) {
    // Positive child ids are a 1-based child index, used by clients
    // that want to enumerate all immediate children.
    AXPlatformNodeBase* base = FromNativeViewAccessible(
        GetDelegate()->ChildAtIndex(static_cast<size_t>(child_id - 1)));
    return static_cast<AXPlatformNodeWin*>(base);
  }

  if (child_id >= 0)
    return nullptr;

  // Negative child ids can be used to map to any descendant.
  AXPlatformNode* node = GetFromUniqueId(-child_id);
  if (!node)
    return nullptr;

  AXPlatformNodeBase* base =
      FromNativeViewAccessible(node->GetNativeViewAccessible());
  if (base && !IsDescendant(base))
    base = nullptr;

  return static_cast<AXPlatformNodeWin*>(base);
}

bool AXPlatformNodeWin::IsInTreeGrid() {
  AXPlatformNodeBase* container = FromNativeViewAccessible(GetParent());

  // If parent was a rowgroup, we need to look at the grandparent
  if (container && container->GetRole() == ax::mojom::Role::kRowGroup)
    container = FromNativeViewAccessible(container->GetParent());

  if (!container)
    return false;

  return container->GetRole() == ax::mojom::Role::kTreeGrid;
}

HRESULT AXPlatformNodeWin::AllocateComArrayFromVector(
    std::vector<LONG>& results,
    LONG max,
    LONG** selected,
    LONG* n_selected) {
  DCHECK_GT(max, 0);
  DCHECK(selected);
  DCHECK(n_selected);

  auto count = std::min((LONG)results.size(), max);
  *n_selected = count;
  *selected = static_cast<LONG*>(CoTaskMemAlloc(sizeof(LONG) * count));

  for (LONG i = 0; i < count; i++)
    (*selected)[i] = results[i];
  return S_OK;
}

bool AXPlatformNodeWin::IsHyperlink() {
  int32_t hyperlink_index = -1;
  AXPlatformNodeWin* parent = GetParentPlatformNodeWin();
  if (parent) {
    hyperlink_index = parent->GetHyperlinkIndexFromChild(this);
  }

  if (hyperlink_index >= 0)
    return true;
  return false;
}

void AXPlatformNodeWin::ResetComputedHypertext() {
  hypertext_ = AXLegacyHypertext();
}

double AXPlatformNodeWin::GetHorizontalScrollPercent() {
  if (!IsHorizontallyScrollable())
    return UIA_ScrollPatternNoScroll;

  float x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  float x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  float x = GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  return 100.0 * (x - x_min) / (x_max - x_min);
}

double AXPlatformNodeWin::GetVerticalScrollPercent() {
  if (!IsVerticallyScrollable())
    return UIA_ScrollPatternNoScroll;

  float y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  float y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
  float y = GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
  return 100.0 * (y - y_min) / (y_max - y_min);
}

BSTR AXPlatformNodeWin::GetFontNameAttributeAsBSTR() const {
  const std::wstring string = base::UTF8ToWide(
      GetInheritedStringAttribute(ax::mojom::StringAttribute::kFontFamily));

  return SysAllocString(string.c_str());
}

BSTR AXPlatformNodeWin::GetStyleNameAttributeAsBSTR() const {
  std::u16string style_name =
      GetDelegate()->GetStyleNameAttributeAsLocalizedString();

  return SysAllocString(base::as_wcstr(style_name));
}

TextDecorationLineStyle AXPlatformNodeWin::GetUIATextDecorationStyle(
    const ax::mojom::IntAttribute int_attribute) const {
  const ax::mojom::TextDecorationStyle text_decoration_style =
      static_cast<ax::mojom::TextDecorationStyle>(
          GetIntAttribute(int_attribute));

  switch (text_decoration_style) {
    case ax::mojom::TextDecorationStyle::kNone:
      return TextDecorationLineStyle::TextDecorationLineStyle_None;
    case ax::mojom::TextDecorationStyle::kDotted:
      return TextDecorationLineStyle::TextDecorationLineStyle_Dot;
    case ax::mojom::TextDecorationStyle::kDashed:
      return TextDecorationLineStyle::TextDecorationLineStyle_Dash;
    case ax::mojom::TextDecorationStyle::kSolid:
      return TextDecorationLineStyle::TextDecorationLineStyle_Single;
    case ax::mojom::TextDecorationStyle::kDouble:
      return TextDecorationLineStyle::TextDecorationLineStyle_Double;
    case ax::mojom::TextDecorationStyle::kWavy:
      return TextDecorationLineStyle::TextDecorationLineStyle_Wavy;
  }
}

// IRawElementProviderSimple support methods.

AXPlatformNodeWin::PatternProviderFactoryMethod
AXPlatformNodeWin::GetPatternProviderFactoryMethod(PATTERNID pattern_id) {
  switch (pattern_id) {
    case UIA_AnnotationPatternId:
      if (IsStructuredAnnotation()) {
        return &PatternProvider<IAnnotationProvider>;
      }
      break;
    case UIA_ExpandCollapsePatternId:
      if (GetData().SupportsExpandCollapse()) {
        return &PatternProvider<IExpandCollapseProvider>;
      }
      break;

    case UIA_GridPatternId:
      if (IsUIATableLike(GetRole())) {
        return &PatternProvider<IGridProvider>;
      }
      break;

    case UIA_GridItemPatternId:
      // Griditem pattern should not be exposed unless the
      // the gridpattern role is exposed on an ancestor.
      if (IsUIACellOrTableHeader(GetRole()) && GetUIATableAncestor()) {
        return &PatternProvider<IGridItemProvider>;
      }
      break;

    case UIA_InvokePatternId:
      if (IsInvokeSupported()) {
        return &PatternProvider<IInvokeProvider>;
      }
      break;

    case UIA_RangeValuePatternId:
      if (GetData().IsRangeValueSupported()) {
        return &PatternProvider<IRangeValueProvider>;
      }
      break;

    case UIA_ScrollPatternId:
      if (IsScrollable()) {
        return &PatternProvider<IScrollProvider>;
      }
      break;

    case UIA_ScrollItemPatternId:
      return &PatternProvider<IScrollItemProvider>;

    case UIA_SelectionItemPatternId:
      if (IsSelectionItemSupported()) {
        return &PatternProvider<ISelectionItemProvider>;
      }
      break;

    case UIA_SelectionPatternId:
      if (IsContainerWithSelectableChildren(GetRole())) {
        return &PatternProvider<ISelectionProvider>;
      }
      break;

    case UIA_TablePatternId:
      // Despite the following documentation (as of April 2022)
      //     https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcore/nn-uiautomationcore-itableprovider
      // which mentions that ITableProvider must expose column and/or row
      // headers, we should expose the Table pattern on all table-like roles.
      // This will allow clients to detect such constructs as tables and expose
      // row/column counts and navigation along with Table semantics.
      //
      // On UIA, we don't want to expose the ITableProvider for layout tables
      // because it can cause extraneous, confusing announcements for users. We
      // initially exposed it, but decided to re-evaluate our decision after
      // hearing from users.
      if (IsUIATableLike(GetRole()))
        return &PatternProvider<ITableProvider>;
      break;

    case UIA_TableItemPatternId:
      // Despite the following documentation (as of April 2022)
      //     https://docs.microsoft.com/en-us/windows/win32/api/uiautomationcore/nn-uiautomationcore-itableprovider
      // which mentions that ITableProvider must expose column and/or row
      // headers, we should expose the Table pattern on all table-like roles.
      // This will allow clients to detect such constructs as tables and expose
      // row/column counts and navigation along with Table semantics.
      //
      // On UIA, we don't want to expose the ITableProvider for layout tables
      // because it can cause extraneous, confusing announcements for users. We
      // initially exposed it, but decided to re-evaluate our decision after
      // hearing from users.
      if (IsUIACellOrTableHeader(GetRole())) {
        return &PatternProvider<ITableProvider>;
      }
      break;

    case UIA_TextChildPatternId:
      if (AXPlatformNodeTextChildProviderWin::GetTextContainer(this)) {
        return &AXPlatformNodeTextChildProviderWin::CreateIUnknown;
      }
      break;

    case UIA_TextEditPatternId:
    case UIA_TextPatternId:
      if (IsPlatformDocument() || IsTextField() || IsText())
        return &AXPlatformNodeTextProviderWin::CreateIUnknown;
      break;

    case UIA_TogglePatternId:
      if (IsToggleSupported()) {
        return &PatternProvider<IToggleProvider>;
      }
      break;

    case UIA_ValuePatternId:
      if (IsValuePatternSupported(GetDelegate())) {
        return &PatternProvider<IValueProvider>;
      }
      break;

    case UIA_WindowPatternId:
      if (HasBoolAttribute(ax::mojom::BoolAttribute::kModal)) {
        return &PatternProvider<IWindowProvider>;
      }
      break;

    // Not currently implemented.
    case UIA_CustomNavigationPatternId:
    case UIA_DockPatternId:
    case UIA_DragPatternId:
    case UIA_DropTargetPatternId:
    case UIA_ItemContainerPatternId:
    case UIA_MultipleViewPatternId:
    case UIA_ObjectModelPatternId:
    case UIA_SpreadsheetPatternId:
    case UIA_SpreadsheetItemPatternId:
    case UIA_StylesPatternId:
    case UIA_SynchronizedInputPatternId:
    case UIA_TextPattern2Id:
    case UIA_TransformPatternId:
    case UIA_TransformPattern2Id:
    case UIA_VirtualizedItemPatternId:
      break;

    // Provided by UIA Core; we should not implement.
    case UIA_LegacyIAccessiblePatternId:
      break;
  }
  return nullptr;
}

AXPlatformNodeWin* AXPlatformNodeWin::GetLowestAccessibleElementForUIA() {
  AXPlatformNodeWin* node = this;
  while (node) {
    if (!node->IsInaccessibleForUIA())
      return node;
    node = node->GetParentPlatformNodeWin();
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

AXPlatformNodeWin* AXPlatformNodeWin::GetFirstTextOnlyDescendant() {
  for (auto* child = static_cast<AXPlatformNodeWin*>(GetFirstChild()); child;
       child = static_cast<AXPlatformNodeWin*>(child->GetNextSibling())) {
    if (child->IsText())
      return child;
    if (AXPlatformNodeWin* descendant = child->GetFirstTextOnlyDescendant())
      return descendant;
  }
  return nullptr;
}

void AXPlatformNodeWin::SanitizeTextAttributeValue(const std::string& input,
                                                   std::string* output) const {
  SanitizeStringAttributeForIA2(input, output);
}

void AXPlatformNodeWin::NotifyObserverForMSAAUsage() const {
  GetWinAccessibilityAPIUsageObserverList().Notify(
      &WinAccessibilityAPIUsageObserver::OnMSAAUsed);
}

void AXPlatformNodeWin::NotifyAddAXModeFlagsForIA2(
    const uint32_t ax_modes) const {
  if (features::IsAccessibilityRestrictiveIA2AXModesEnabled()) {
    // Non-web content is always enabled, if a client isn't looking for web
    // content, don't enable.
    if (!GetDelegate() || !GetDelegate()->IsWebContent()) {
      return;
    }
  }

  AXPlatformNode::NotifyAddAXModeFlags(ax_modes);
}

void AXPlatformNodeWin::NotifyAPIObserverForPatternRequest(
    PATTERNID pattern_id) const {
  if (!GetDelegate() || !GetDelegate()->IsWebContent()) {
    return;
  }

  bool probable_advanced_client_detected = false;
  bool text_pattern_support_needed = false;
  switch (pattern_id) {
    case UIA_TextPatternId:
    case UIA_TextChildPatternId:
      // These properties require information gated behind the kInlineTextBoxes
      // AXMode. See kInlineTextBoxes for details.
      text_pattern_support_needed = true;
      break;
    // These properties require more advanced accessibility features to be
    // enabled See kScreenReader for details.
    case UIA_RangeValuePatternId:
    case UIA_TableItemPatternId:
      probable_advanced_client_detected = true;
      break;
  }

  for (WinAccessibilityAPIUsageObserver& observer :
       GetWinAccessibilityAPIUsageObserverList()) {
    if (probable_advanced_client_detected)
      observer.OnAdvancedUIAutomationUsed();
    if (text_pattern_support_needed)
      observer.OnTextPatternRequested();
  }
}

void AXPlatformNodeWin::NotifyAPIObserverForPropertyRequest(
    PROPERTYID property_id) const {
  // Non-web content is always enabled, if a client isn't looking for web
  // content, don't enable.
  if (!GetDelegate() || !GetDelegate()->IsWebContent())
    return;

  bool probable_advanced_client_detected = false;
  bool probable_screen_reader_detected = false;
  switch (property_id) {
    // These properties are used by non-screenreader UIA clients. They should
    // not cause additional enablement.
    case UIA_HasKeyboardFocusPropertyId:
    case UIA_FrameworkIdPropertyId:
    case UIA_IsEnabledPropertyId:
      break;
    //  These properties are not currently implemented and should not cause
    //  enablement.
    case UIA_AnnotationTypesPropertyId:
    case UIA_CenterPointPropertyId:
    case UIA_FillColorPropertyId:
    case UIA_FillTypePropertyId:
    case UIA_HeadingLevelPropertyId:
    case UIA_ItemTypePropertyId:
    case UIA_OutlineColorPropertyId:
    case UIA_OutlineThicknessPropertyId:
    case UIA_RotationPropertyId:
    case UIA_SizePropertyId:
    case UIA_VisualEffectsPropertyId:
      break;
    // These properties are provided by UIA Core; we should not implement, and
    // they should not cause enablement.
    case UIA_BoundingRectanglePropertyId:
    case UIA_NativeWindowHandlePropertyId:
    case UIA_ProcessIdPropertyId:
    case UIA_ProviderDescriptionPropertyId:
    case UIA_RuntimeIdPropertyId:
      break;
    // These properties require the screenreader mode to get correct results
    case UIA_LabeledByPropertyId:
    case UIA_LiveSettingPropertyId:
    case UIA_LevelPropertyId:
    case UIA_DescribedByPropertyId:
    case UIA_AutomationIdPropertyId:
    // These properties are indicative of a screenreader.
    case UIA_AriaRolePropertyId:
    case UIA_ControlTypePropertyId:
    case UIA_LocalizedControlTypePropertyId:
    case UIA_NamePropertyId:
    case UIA_AcceleratorKeyPropertyId:
    case UIA_AccessKeyPropertyId:
    case UIA_IsKeyboardFocusablePropertyId:
    case UIA_ClassNamePropertyId:
    case UIA_HelpTextPropertyId:
    case UIA_ClickablePointPropertyId:
    case UIA_CulturePropertyId:
    case UIA_IsControlElementPropertyId:
    case UIA_IsContentElementPropertyId:
    case UIA_IsPasswordPropertyId:
    case UIA_IsOffscreenPropertyId:
    case UIA_OrientationPropertyId:
    case UIA_IsRequiredForFormPropertyId:
    case UIA_ItemStatusPropertyId:
    case UIA_ExpandCollapseExpandCollapseStatePropertyId:
    case UIA_SelectionItemIsSelectedPropertyId:
    case UIA_ToggleToggleStatePropertyId:
    case UIA_AriaPropertiesPropertyId:
    case UIA_IsDataValidForFormPropertyId:
    case UIA_ControllerForPropertyId:
    case UIA_FlowsToPropertyId:
    case UIA_OptimizeForVisualContentPropertyId:
    case UIA_FlowsFromPropertyId:
    case UIA_IsPeripheralPropertyId:
    case UIA_PositionInSetPropertyId:
    case UIA_SizeOfSetPropertyId:
    case UIA_AnnotationObjectsPropertyId:
    case UIA_LandmarkTypePropertyId:
    case UIA_LocalizedLandmarkTypePropertyId:
    case UIA_FullDescriptionPropertyId:
    case UIA_IsDialogPropertyId:
      probable_screen_reader_detected = true;
      probable_advanced_client_detected = true;
      break;
  }

  for (WinAccessibilityAPIUsageObserver& observer :
       GetWinAccessibilityAPIUsageObserverList()) {
    if (probable_advanced_client_detected)
      observer.OnAdvancedUIAutomationUsed();
    if (probable_screen_reader_detected)
      observer.OnProbableUIAutomationScreenReaderDetected();
  }
}

AXPlatformNodeWin* AXPlatformNodeWin::GetUIATableAncestor() const {
  AXPlatformNodeWin* parent = const_cast<AXPlatformNodeWin*>(this);
  while (parent) {
    if (IsUIATableLike(parent->GetRole())) {
      return parent;
    }
    parent = parent->GetParentPlatformNodeWin();
  }

  return nullptr;
}

// static
void AXPlatformNodeWin::SanitizeStringAttributeForIA2(const std::string& input,
                                                      std::string* output) {
  DCHECK(output);
  // According to the IA2 Spec, these characters need to be escaped with a
  // backslash: backslash, colon, comma, equals and semicolon.
  // Note that backslash must be replaced first.
  base::ReplaceChars(input, "\\", "\\\\", output);
  base::ReplaceChars(*output, ":", "\\:", output);
  base::ReplaceChars(*output, ",", "\\,", output);
  base::ReplaceChars(*output, "=", "\\=", output);
  base::ReplaceChars(*output, ";", "\\;", output);
}

bool AXPlatformNodeWin::IsSelectionItemSupported() const {
  switch (GetRole()) {
    // An ARIA 1.1+ role of "row" inside an ARIA 1.1 role of "table", should not
    // be selectable. ARIA "table" is not interactable, ARIA "grid" is.
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRow:
    case ax::mojom::Role::kRowHeader: {
      // An ARIA grid subwidget is only selectable if explicitly marked as
      // selected (or not) with the aria-selected property.
      if (!HasBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
        return false;
      }

      AXPlatformNodeBase* table = GetTable();
      if (!table) {
        return false;
      }

      return table->GetRole() == ax::mojom::Role::kGrid ||
             table->GetRole() == ax::mojom::Role::kListGrid ||
             table->GetRole() == ax::mojom::Role::kTreeGrid;
    }
    // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
    // SelectionItem.IsSelected is exposed when aria-checked is True or False,
    // for 'radio' and 'menuitemradio' roles.
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kMenuItemRadio: {
      if (GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue ||
          GetData().GetCheckedState() == ax::mojom::CheckedState::kFalse) {
        return true;
      }
      return false;
    }
    // https://www.w3.org/TR/wai-aria-1.1/#aria-selected
    // SelectionItem.IsSelected is exposed when aria-select is True or False.
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kTreeItem:
      return HasBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    case ax::mojom::Role::kGridCell:
    case ax::mojom::Role::kTab:
      // According to the UIA documentation, this role should always support the
      // SelectionItem control pattern:
      // https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supporttabitemcontroltype#required-control-patterns.
      return true;
    default:
      return false;
  }
}

bool AXPlatformNodeWin::IsToggleSupported() const {
  ax::mojom::Role role = GetRole();

  // As per the spec [1], RadioButton control does not implement
  // IToggleProvider, as it is not capable of cycling through its valid states.
  //
  // [1]:https://docs.microsoft.com/en-us/dotnet/framework/ui-automation/implementing-the-ui-automation-toggle-control-pattern
  if (IsRadio(role)) {
    return false;
  }

  // The documentation[2] and test case[3] seem to indicate that the Toggle
  // control pattern should not be exposed when the ExpandCollapse control
  // pattern is already exposed for a button. Please note that this last
  // requirement only applies to buttons, not all elements; it's possible for an
  // other role (e.g., treeitem) to support both the Toggle and the
  // ExpandCollapse control patterns.
  //
  // [2]:https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supportbuttoncontroltype#required-control-patterns
  // [3]:https://github.com/microsoft/axe-windows/blob/main/src/Rules/Library/ButtonInvokeAndExpandeCollapsePatterns.cs
  if (GetData().SupportsExpandCollapse() && IsButton(role)) {
    return false;
  }

  // According to the CoreAAM spec [4], TogglePattern should be exposed for
  // all aria-checkable roles.
  //
  // [4]:https://w3c.github.io/core-aam/#mapping_state-property_table
  return IsPlatformCheckable() || SupportsToggle(role);
}

bool AXPlatformNodeWin::IsInvokeSupported() const {
  ax::mojom::Role role = GetRole();

  // As per the documentation [1], tab item should never support
  // IInvokeProvider.
  //
  // [1]:https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supporttabitemcontroltype#required-control-patterns
  if (role == ax::mojom::Role::kTab) {
    return false;
  }

  // According to this Accessibility Insights test [2], the UIA Invoke and
  // Toggle patterns should never be used together for buttons.
  //
  // Note: It's not specified which pattern should be exposed when both could
  // technically be supported. However, priority is given to Toggle Pattern
  // here, as its the more specialized one.
  //
  // [2]:https://github.com/microsoft/axe-windows/blob/main/src/Rules/Library/ButtonInvokeAndTogglePatterns.cs
  if (IsToggleSupported() && IsButton(role)) {
    return false;
  }

  // According to the Accessibility Insights rules [3] and UIA documentation
  // [4], the Invoke control pattern should not be supported on the following
  // control types because another control pattern will always be available to
  // support the same invocable behavior:
  //   - UIA_AppBarControlTypeId
  //   - UIA_TabItemControlTypeId
  //
  // [3]:https://github.com/microsoft/axe-windows/blob/main/src/Rules/Library/ControlShouldNotSupportInvokePattern.cs
  // [4]:https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/implementing-the-ui-automation-invoke-control-pattern
  //
  // TODO(accessibility): Add the condition for the UIA_AppBarControlTypeId if
  // we ever start exposing this control type in Chromium.
  return GetData().IsInvocable();
}

}  // namespace ui
