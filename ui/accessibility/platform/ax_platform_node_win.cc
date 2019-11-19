// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/enum_variant.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_utils_win.h"
#include "ui/accessibility/platform/ax_platform_node_textchildprovider_win.h"
#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"
#include "ui/accessibility/platform/ax_platform_relation_win.h"
#include "ui/base/win/atl_module.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

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

base::LazyInstance<base::ObserverList<IAccessible2UsageObserver>::Unchecked>::
    Leaky g_iaccessible2_usage_observer_list = LAZY_INSTANCE_INITIALIZER;

// Sets the multiplier by which large changes to a RangeValueProvider are
// greater than small changes.
constexpr int kLargeChangeScaleFactor = 10;

// The amount to scroll when UI Automation asks to scroll by a small increment.
// Value is in device independent pixels and is the same used by Blink when
// cursor keys are used to scroll a webpage.
constexpr float kSmallScrollIncrement = 40.0f;

void AppendTextToString(base::string16 extra_text, base::string16* string) {
  if (extra_text.empty())
    return;

  if (string->empty()) {
    *string = extra_text;
    return;
  }

  *string += base::string16(L". ") + extra_text;
}

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
  attributes->push_back(base::UTF8ToUTF16(name) + L":" +
                        base::UTF8ToUTF16(str_value));
}

// There is no easy way to decouple |kScreenReader| and |kHTML| accessibility
// modes when Windows screen readers are used. For example, certain roles use
// the HTML tag name. Input fields require their type attribute to be exposed.
const uint32_t kScreenReaderAndHTMLAccessibilityModes =
    AXMode::kScreenReader | AXMode::kHTML;

//
// IAccessible2UsageObserver
//

IAccessible2UsageObserver::IAccessible2UsageObserver() {}

IAccessible2UsageObserver::~IAccessible2UsageObserver() {}

// static
base::ObserverList<IAccessible2UsageObserver>::Unchecked&
GetIAccessible2UsageObserverList() {
  return g_iaccessible2_usage_observer_list.Get();
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
  accessible->QueryInterface(ax_platform_node.GetAddressOf());
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
    const base::string16& input,
    base::string16* output) {
  DCHECK(output);
  // According to the UIA Spec, these characters need to be escaped with a
  // backslash in an AriaProperties string: backslash, equals and semicolon.
  // Note that backslash must be replaced first.
  base::ReplaceChars(input, L"\\", L"\\\\", output);
  base::ReplaceChars(*output, L"=", L"\\=", output);
  base::ReplaceChars(*output, L";", L"\\;", output);
}

void AXPlatformNodeWin::StringAttributeToUIAAriaProperty(
    std::vector<base::string16>& properties,
    ax::mojom::StringAttribute attribute,
    const char* uia_aria_property) {
  base::string16 value;
  if (GetString16Attribute(attribute, &value)) {
    SanitizeStringAttributeForUIAAriaProperty(value, &value);
    properties.push_back(base::ASCIIToUTF16(uia_aria_property) + L"=" + value);
  }
}

void AXPlatformNodeWin::BoolAttributeToUIAAriaProperty(
    std::vector<base::string16>& properties,
    ax::mojom::BoolAttribute attribute,
    const char* uia_aria_property) {
  bool value;
  if (GetBoolAttribute(attribute, &value)) {
    properties.push_back((base::ASCIIToUTF16(uia_aria_property) + L"=") +
                         (value ? L"true" : L"false"));
  }
}

void AXPlatformNodeWin::IntAttributeToUIAAriaProperty(
    std::vector<base::string16>& properties,
    ax::mojom::IntAttribute attribute,
    const char* uia_aria_property) {
  int value;
  if (GetIntAttribute(attribute, &value)) {
    properties.push_back(base::ASCIIToUTF16(uia_aria_property) + L"=" +
                         base::NumberToString16(value));
  }
}

void AXPlatformNodeWin::FloatAttributeToUIAAriaProperty(
    std::vector<base::string16>& properties,
    ax::mojom::FloatAttribute attribute,
    const char* uia_aria_property) {
  float value;
  if (GetFloatAttribute(attribute, &value)) {
    properties.push_back(base::ASCIIToUTF16(uia_aria_property) + L"=" +
                         base::NumberToString16(value));
  }
}

void AXPlatformNodeWin::StateToUIAAriaProperty(
    std::vector<base::string16>& properties,
    ax::mojom::State state,
    const char* uia_aria_property) {
  const AXNodeData& data = GetData();
  bool value = data.HasState(state);
  properties.push_back((base::ASCIIToUTF16(uia_aria_property) + L"=") +
                       (value ? L"true" : L"false"));
}

void AXPlatformNodeWin::HtmlAttributeToUIAAriaProperty(
    std::vector<base::string16>& properties,
    const char* html_attribute_name,
    const char* uia_aria_property) {
  base::string16 html_attribute_value;
  if (GetData().GetHtmlAttribute(html_attribute_name, &html_attribute_value)) {
    SanitizeStringAttributeForUIAAriaProperty(html_attribute_value,
                                              &html_attribute_value);
    properties.push_back(base::ASCIIToUTF16(uia_aria_property) + L"=" +
                         html_attribute_value);
  }
}

std::vector<AXPlatformNodeWin*>
AXPlatformNodeWin::CreatePlatformNodeVectorFromRelationIdVector(
    std::vector<int32_t>& relation_id_list) {
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
    std::vector<AXPlatformNodeWin*>& platform_node_list) {
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

  if (GetActivePopupAxUniqueId() != base::nullopt) {
    AXPlatformNodeWin* view_popup_node_win = static_cast<AXPlatformNodeWin*>(
        GetFromUniqueId(GetActivePopupAxUniqueId().value()));

    if (IsValidUiaRelationTarget(view_popup_node_win))
      platform_node_list.push_back(view_popup_node_win);
  }

  return CreateUIAElementsSafeArray(platform_node_list);
}

SAFEARRAY* AXPlatformNodeWin::CreateUIAElementsArrayForRelation(
    const ax::mojom::IntListAttribute& attribute) {
  std::vector<int32_t> relation_id_list = GetIntListAttribute(attribute);

  std::vector<AXPlatformNodeWin*> platform_node_list =
      CreatePlatformNodeVectorFromRelationIdVector(relation_id_list);

  return CreateUIAElementsSafeArray(platform_node_list);
}

SAFEARRAY* AXPlatformNodeWin::CreateUIAElementsArrayForReverseRelation(
    const ax::mojom::IntListAttribute& attribute) {
  std::set<AXPlatformNode*> reverse_relations =
      GetDelegate()->GetReverseRelations(attribute);

  std::vector<int32_t> id_list;
  std::transform(
      reverse_relations.cbegin(), reverse_relations.cend(),
      std::back_inserter(id_list), [](AXPlatformNode* platform_node) {
        return static_cast<AXPlatformNodeWin*>(platform_node)->GetData().id;
      });

  std::vector<AXPlatformNodeWin*> platform_node_list =
      CreatePlatformNodeVectorFromRelationIdVector(id_list);

  return CreateUIAElementsSafeArray(platform_node_list);
}

SAFEARRAY* AXPlatformNodeWin::CreateClickablePointArray() {
  SAFEARRAY* clickable_point_array = SafeArrayCreateVector(VT_R8, 0, 2);
  gfx::Point center = GetDelegate()
                          ->GetBoundsRect(AXCoordinateSystem::kScreen,
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
      AXCoordinateSystem::kScreen, AXClippingBehavior::kClipped);
  const int large_horizontal_change = bounds.width();
  const int large_vertical_change = bounds.height();

  const HWND hwnd = GetDelegate()->GetTargetForNativeAccessibilityEvent();
  DCHECK(hwnd);
  const float scale_factor =
      display::win::ScreenWin::GetScaleFactorForHWND(hwnd);
  const int small_change =
      gfx::ToRoundedInt(kSmallScrollIncrement * scale_factor);

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
  // Menu items fire selection events but Windows screen readers work reliably
  // with focus events. Remap here.
  if (event_type == ax::mojom::Event::kSelection) {
    // A menu item could have something other than a role of
    // |ROLE_SYSTEM_MENUITEM|. Zoom modification controls for example have a
    // role of button.
    auto* parent =
        static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(GetParent()));
    int role = MSAARole();
    if (role == ROLE_SYSTEM_MENUITEM) {
      event_type = ax::mojom::Event::kFocus;
    } else if (role == ROLE_SYSTEM_LISTITEM) {
      if (AXPlatformNodeBase* container = GetSelectionContainer()) {
        const ui::AXNodeData& data = container->GetData();
        if (data.role == ax::mojom::Role::kListBox &&
            !data.HasState(ax::mojom::State::kMultiselectable) &&
            GetDelegate()->GetFocus() == GetNativeViewAccessible()) {
          event_type = ax::mojom::Event::kFocus;
        }
      }
    } else if (parent) {
      int parent_role = parent->MSAARole();
      if (parent_role == ROLE_SYSTEM_MENUPOPUP ||
          parent_role == ROLE_SYSTEM_LIST) {
        event_type = ax::mojom::Event::kFocus;
      }
    }
  }

  if (base::Optional<DWORD> native_event = MojoEventToMSAAEvent(event_type)) {
    HWND hwnd = GetDelegate()->GetTargetForNativeAccessibilityEvent();
    if (!hwnd)
      return;

    ::NotifyWinEvent((*native_event), hwnd, OBJID_CLIENT, -GetUniqueId());
  }

  if (base::Optional<PROPERTYID> uia_property =
          MojoEventToUIAProperty(event_type)) {
    // For this event, we're not concerned with the old value.
    base::win::ScopedVariant old_value;
    ::VariantInit(old_value.Receive());
    base::win::ScopedVariant new_value;
    ::VariantInit(new_value.Receive());
    GetPropertyValue((*uia_property), new_value.Receive());
    ::UiaRaiseAutomationPropertyChangedEvent(this, (*uia_property), old_value,
                                             new_value);
  }

  if (base::Optional<EVENTID> uia_event = MojoEventToUIAEvent(event_type))
    ::UiaRaiseAutomationEvent(this, (*uia_event));

  // Keep track of objects that are a target of an alert event.
  if (event_type == ax::mojom::Event::kAlert)
    AddAlertTarget();
}

int AXPlatformNodeWin::GetIndexInParent() {
  Microsoft::WRL::ComPtr<IDispatch> parent_dispatch;
  Microsoft::WRL::ComPtr<IAccessible> parent_accessible;
  if (S_OK != get_accParent(parent_dispatch.GetAddressOf()))
    return -1;
  if (S_OK != parent_dispatch.CopyTo(parent_accessible.GetAddressOf()))
    return -1;

  LONG child_count = 0;
  if (S_OK != parent_accessible->get_accChildCount(&child_count))
    return -1;

  // Ask the delegate for the index in parent, and return it if it's plausible.
  //
  // Delegates are allowed to not implement this (AXPlatformNodeDelegateBase
  // returns -1). Also, delegates may not know the correct answer if this
  // node is the root of a tree that's embedded in another tree, in which
  // case the delegate should return -1 and we'll compute it.
  int index_in_parent = GetDelegate()->GetIndexInParent();
  if (index_in_parent >= 0 && index_in_parent < child_count)
    return index_in_parent;

  // Otherwise, search the parent's children.
  for (LONG index = 1; index <= child_count; ++index) {
    base::win::ScopedVariant childid_index(index);
    Microsoft::WRL::ComPtr<IDispatch> child_dispatch;
    Microsoft::WRL::ComPtr<IAccessible> child_accessible;
    if (S_OK == parent_accessible->get_accChild(
                    childid_index, child_dispatch.GetAddressOf()) &&
        S_OK == child_dispatch.CopyTo(child_accessible.GetAddressOf())) {
      if (child_accessible.Get() == this)
        return index - 1;
    }
  }
  return -1;
}

base::string16 AXPlatformNodeWin::GetHypertext() const {
  // Hypertext of platform leaves, which internally are composite objects, are
  // represented with the inner text of the internal composite object.
  if (IsChildOfLeaf())
    return GetInnerText();

  return hypertext_.hypertext;
}

bool AXPlatformNodeWin::HasActiveComposition() const {
  return active_composition_range_.end() > active_composition_range_.start();
}

gfx::Range AXPlatformNodeWin::GetActiveCompositionOffsets() const {
  return active_composition_range_;
}

void AXPlatformNodeWin::OnActiveComposition(
    const gfx::Range& range,
    const base::string16& active_composition_text,
    bool is_composition_committed) {
  // Cache the composition range that will be used when
  // GetActiveComposition and GetConversionTarget is called in
  // AXPlatformNodeTextProviderWin
  active_composition_range_ = range;
  // Fire the UiaTextEditTextChangedEvent
  FireUiaTextEditTextChangedEvent(range, active_composition_text,
                                  is_composition_committed);
}

void AXPlatformNodeWin::FireUiaTextEditTextChangedEvent(
    const gfx::Range& range,
    const base::string16& active_composition_text,
    bool is_composition_committed) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled()) {
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

  long index = 0;
  HRESULT hr =
      SafeArrayPutElement(changed_data.Get(), &index, composition_text);

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

//
// IAccessible implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::accHitTest(LONG x_left,
                                             LONG y_top,
                                             VARIANT* child) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_HIT_TEST);
  COM_OBJECT_VALIDATE_1_ARG(child);

  gfx::Point point(x_left, y_top);
  if (!GetDelegate()
           ->GetBoundsRect(AXCoordinateSystem::kScreen,
                           AXClippingBehavior::kClipped)
           .Contains(point)) {
    // Return S_FALSE and VT_EMPTY when outside the object's boundaries.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  gfx::NativeViewAccessible hit_child =
      GetDelegate()->HitTestSync(x_left, y_top);
  if (!hit_child) {
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  if (hit_child == this) {
    // This object is the best match, so return CHILDID_SELF. It's tempting to
    // simplify the logic and use VT_DISPATCH everywhere, but the Windows
    // call AccessibleObjectFromPoint will keep calling accHitTest until some
    // object returns CHILDID_SELF.
    child->vt = VT_I4;
    child->lVal = CHILDID_SELF;
    return S_OK;
  }

  // Call accHitTest recursively on the result, which may be a recursive call
  // to this function or it may be overridden, for example in the case of a
  // WebView.
  HRESULT result = hit_child->accHitTest(x_left, y_top, child);

  // If the recursive call returned CHILDID_SELF, we have to convert that
  // into a VT_DISPATCH for the return value to this call.
  if (S_OK == result && child->vt == VT_I4 && child->lVal == CHILDID_SELF) {
    child->vt = VT_DISPATCH;
    child->pdispVal = hit_child;
    // Always increment ref when returning a reference to a COM object.
    child->pdispVal->AddRef();
  }
  return result;
}

IFACEMETHODIMP AXPlatformNodeWin::accDoDefaultAction(VARIANT var_id) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_DO_DEFAULT_ACTION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_id, target);
  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;

  if (target->GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::accLocation(LONG* x_left,
                                              LONG* y_top,
                                              LONG* width,
                                              LONG* height,
                                              VARIANT var_id) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ACC_LOCATION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_4_ARGS_AND_GET_TARGET(var_id, x_left, y_top, width,
                                                   height, target);

  gfx::Rect bounds = target->GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreen, AXClippingBehavior::kUnclipped);
  *x_left = bounds.x();
  *y_top = bounds.y();
  *width = bounds.width();
  *height = bounds.height();

  if (bounds.IsEmpty())
    return S_FALSE;

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::accNavigate(LONG nav_dir,
                                              VARIANT start,
                                              VARIANT* end) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_CHILD);

  *disp_child = nullptr;
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_child, target);

  *disp_child = target;
  (*disp_child)->AddRef();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accChildCount(LONG* child_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_CHILD_COUNT);
  COM_OBJECT_VALIDATE_1_ARG(child_count);
  *child_count = GetDelegate()->GetChildCount();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accDefaultAction(VARIANT var_id,
                                                       BSTR* def_action) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_DEFAULT_ACTION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, def_action, target);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  int action;
  if (!target->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                               &action)) {
    *def_action = nullptr;
    return S_FALSE;
  }

  base::string16 action_verb = ActionVerbToLocalizedString(
      static_cast<ax::mojom::DefaultActionVerb>(action));
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_DESCRIPTION);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, desc, target);

  return target->GetStringAttributeAsBstr(
      ax::mojom::StringAttribute::kDescription, desc);
}

IFACEMETHODIMP AXPlatformNodeWin::get_accFocus(VARIANT* focus_child) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_KEYBOARD_SHORTCUT);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, acc_key, target);

  return target->GetStringAttributeAsBstr(
      ax::mojom::StringAttribute::kKeyShortcuts, acc_key);
}

IFACEMETHODIMP AXPlatformNodeWin::get_accName(VARIANT var_id, BSTR* name_bstr) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_NAME);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, name_bstr, target);

  for (IAccessible2UsageObserver& observer :
       GetIAccessible2UsageObserverList()) {
    observer.OnAccNameCalled();
  }

  bool has_name = target->HasStringAttribute(ax::mojom::StringAttribute::kName);
  base::string16 name =
      target->GetString16Attribute(ax::mojom::StringAttribute::kName);
  auto status = GetData().GetImageAnnotationStatus();
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
      AppendTextToString(
          GetDelegate()->GetLocalizedStringForImageAnnotationStatus(status),
          &name);
      break;

    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      AppendTextToString(
          GetString16Attribute(ax::mojom::StringAttribute::kImageAnnotation),
          &name);
      break;
  }

  if (name.empty() && !has_name)
    return S_FALSE;

  *name_bstr = SysAllocString(name.c_str());
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accParent(IDispatch** disp_parent) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_ROLE);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, role, target);

  // For historical reasons, we return a string (typically
  // containing the HTML tag name) as the MSAA role, rather
  // than a int.
  std::string role_string =
      base::ToUpperASCII(target->StringOverrideForMSAARole());
  if (!role_string.empty()) {
    role->vt = VT_BSTR;
    std::wstring wsTmp(role_string.begin(), role_string.end());
    role->bstrVal = SysAllocString(wsTmp.c_str());
    return S_OK;
  }

  role->vt = VT_I4;
  role->lVal = target->MSAARole();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accState(VARIANT var_id, VARIANT* state) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_STATE);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, state, target);
  state->vt = VT_I4;
  state->lVal = target->MSAAState();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accHelp(VARIANT var_id, BSTR* help) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_HELP);
  COM_OBJECT_VALIDATE_1_ARG(help);
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accValue(VARIANT var_id, BSTR* value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_VALUE);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, value, target);
  *value = GetValueAttributeAsBstr(target);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::put_accValue(VARIANT var_id, BSTR new_value) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_SELECTION);
  COM_OBJECT_VALIDATE_1_ARG(selected);
  std::vector<Microsoft::WRL::ComPtr<IDispatch>> selected_nodes;
  for (int i = 0; i < GetDelegate()->GetChildCount(); ++i) {
    auto* node = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(GetDelegate()->ChildAtIndex(i)));
    if (node &&
        node->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
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
  // TODO(dougt): We may want to collect an API histogram here.
  // Deprecated.
  return E_NOTIMPL;
}

//
// IAccessible2 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::role(LONG* role) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_STATES);
  COM_OBJECT_VALIDATE_1_ARG(states);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  *states = ComputeIA2State();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_uniqueID(LONG* id) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_UNIQUE_ID);
  COM_OBJECT_VALIDATE_1_ARG(id);
  *id = -GetUniqueId();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_windowHandle(HWND* window_handle) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_WINDOW_HANDLE);
  COM_OBJECT_VALIDATE_1_ARG(window_handle);
  *window_handle = GetDelegate()->GetTargetForNativeAccessibilityEvent();
  return *window_handle ? S_OK : S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_relationTargetsOfType(BSTR type_bstr,
                                                            LONG max_targets,
                                                            IUnknown*** targets,
                                                            LONG* n_targets) {
  COM_OBJECT_VALIDATE_2_ARGS(targets, n_targets);
  if (!type_bstr)
    return E_INVALIDARG;

  *n_targets = 0;
  *targets = nullptr;

  // Special case for relations of type "alerts".
  base::string16 type(type_bstr);
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

  base::string16 relation_type;
  std::set<AXPlatformNode*> enumerated_targets;
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
      if (++index > count)
        break;
    }
  }
  *n_targets = index;
  return index > 0 ? S_OK : S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_attributes(BSTR* attributes) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IA2_GET_ATTRIBUTES);
  COM_OBJECT_VALIDATE_1_ARG(attributes);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  *attributes = nullptr;

  base::string16 attributes_str;
  std::vector<base::string16> computed_attributes = ComputeIA2Attributes();
  for (const base::string16& attribute : computed_attributes)
    attributes_str += attribute + L';';

  if (attributes_str.empty())
    return S_FALSE;

  *attributes = SysAllocString(attributes_str.c_str());
  DCHECK(*attributes);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_indexInParent(LONG* index_in_parent) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_INDEX_IN_PARENT);
  COM_OBJECT_VALIDATE_1_ARG(index_in_parent);
  *index_in_parent = GetIndexInParent();
  if (*index_in_parent < 0)
    return E_FAIL;

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nRelations(LONG* n_relations) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_RELATIONS);
  COM_OBJECT_VALIDATE_1_ARG(n_relations);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  int count = AXPlatformRelationWin::EnumerateRelationships(
      this, -1, base::string16(), nullptr, nullptr);
  *n_relations = count;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_relation(LONG relation_index,
                                               IAccessibleRelation** relation) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_RELATION);
  COM_OBJECT_VALIDATE_1_ARG(relation);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::string16 relation_type;
  std::set<AXPlatformNode*> targets;
  int found = AXPlatformRelationWin::EnumerateRelationships(
      this, relation_index, base::string16(), &relation_type, &targets);
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_RELATIONS);
  COM_OBJECT_VALIDATE_2_ARGS(relations, n_relations);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_GROUP_POSITION);
  COM_OBJECT_VALIDATE_3_ARGS(group_level, similar_items_in_group,
                             position_in_group);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  *group_level = GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
  *similar_items_in_group = GetSetSize().value_or(0);
  *position_in_group = GetPosInSet().value_or(0);

  if (!*group_level && !*similar_items_in_group && !*position_in_group)
    return S_FALSE;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_localizedExtendedRole(
    BSTR* localized_extended_role) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALIZED_EXTENDED_ROLE);
  COM_OBJECT_VALIDATE_1_ARG(localized_extended_role);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::string16 role_description =
      GetRoleDescriptionFromImageAnnotationStatusOrFromAttribute();
  if (base::ContainsOnlyChars(role_description, base::kWhitespaceUTF16))
    return S_FALSE;

  *localized_extended_role = SysAllocString(role_description.c_str());
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_attribute(BSTR name, VARIANT* attribute) {
  COM_OBJECT_VALIDATE_1_ARG(attribute);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::string16 desired_attribute(name);

  // Each computed attribute from ComputeIA2Attributes is a string of
  // the form "key:value". Search for strings that start with the
  // attribute name plus a colon.
  base::string16 prefix = desired_attribute + L":";

  // Let's accept any case.
  const auto compare_case = base::CompareCase::INSENSITIVE_ASCII;

  const std::vector<base::string16> computed_attributes =
      ComputeIA2Attributes();
  for (const base::string16& computed_attribute : computed_attributes) {
    if (base::StartsWith(computed_attribute, prefix, compare_case)) {
      base::string16 value = computed_attribute.substr(prefix.size());
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
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::scrollTo(enum IA2ScrollType ia2_scroll_type) {
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
  COM_OBJECT_VALIDATE();
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_TO_POINT);

  // Convert to screen-relative coordinates if necessary.
  gfx::Point scroll_to(x, y);
  if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    if (GetParent()) {
      AXPlatformNodeBase* base = FromNativeViewAccessible(GetParent());
      scroll_to += base->GetDelegate()
                       ->GetBoundsRect(AXCoordinateSystem::kScreen,
                                       AXClippingBehavior::kUnclipped)
                       .OffsetFromOrigin();
    }
  } else if (coordinate_type != IA2_COORDTYPE_SCREEN_RELATIVE) {
    return E_INVALIDARG;
  }

  ui::AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = scroll_to;
  GetDelegate()->AccessibilityPerformAction(action_data);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nExtendedStates(LONG* n_extended_states) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_EXTENDED_STATES);
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_extendedStates(LONG max_extended_states,
                                                     BSTR** extended_states,
                                                     LONG* n_extended_states) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_EXTENDED_STATES);
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_localizedExtendedStates(
    LONG max_localized_extended_states,
    BSTR** localized_extended_states,
    LONG* n_localized_extended_states) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALIZED_EXTENDED_STATES);

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_locale(IA2Locale* locale) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALE);
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_accessibleWithCaret(IUnknown** accessible,
                                                          LONG* caret_offset) {
  return E_NOTIMPL;
}

//
// IAccessible2_3 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::get_selectionRanges(IA2Range** ranges,
                                                      LONG* nRanges) {
  COM_OBJECT_VALIDATE_2_ARGS(ranges, nRanges);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  AXTree::Selection unignored_selection =
      GetDelegate()->GetUnignoredSelection();
  int32_t anchor_id = unignored_selection.anchor_object_id;
  auto* anchor_node =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(anchor_id));
  if (!anchor_node)
    return E_FAIL;
  int anchor_offset = int{unignored_selection.anchor_offset};

  int32_t focus_id = unignored_selection.focus_object_id;
  auto* focus_node =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(focus_id));
  if (!focus_node)
    return E_FAIL;
  int focus_offset = int{unignored_selection.focus_offset};

  if (!IsDescendant(anchor_node) || !IsDescendant(focus_node))
    return S_FALSE;  // No selection within this subtree.

  *ranges = reinterpret_cast<IA2Range*>(CoTaskMemAlloc(sizeof(IA2Range)));
  anchor_node->AddRef();
  ranges[0]->anchor = static_cast<IAccessible*>(anchor_node);
  ranges[0]->anchorOffset = anchor_offset;
  focus_node->AddRef();
  ranges[0]->active = static_cast<IAccessible*>(focus_node);
  ranges[0]->activeOffset = focus_offset;
  *nRanges = 1;
  return S_OK;
}

//
// IAccessible2_4 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::setSelectionRanges(LONG nRanges,
                                                     IA2Range* ranges) {
  COM_OBJECT_VALIDATE();
  // Blink supports only one selection range for now.
  if (nRanges != 1)
    return E_INVALIDARG;
  if (!ranges)
    return E_INVALIDARG;
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

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

  if (ranges->anchorOffset < 0 || ranges->activeOffset < 0)
    return E_INVALIDARG;

  if (anchor_node->IsTextOnlyObject() || anchor_node->IsPlainTextField()) {
    if (size_t{ranges->anchorOffset} > anchor_node->GetHypertext().length()) {
      return E_INVALIDARG;
    }
  } else {
    if (ranges->anchorOffset > anchor_node->GetChildCount())
      return E_INVALIDARG;
  }

  if (focus_node->IsTextOnlyObject() || focus_node->IsPlainTextField()) {
    if (size_t{ranges->activeOffset} > focus_node->GetHypertext().length())
      return E_INVALIDARG;
  } else {
    if (ranges->activeOffset > focus_node->GetChildCount())
      return E_INVALIDARG;
  }

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = anchor_node->GetData().id;
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_OBJECT_FOR_CHILD);
  // No support for child IDs in this implementation.
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = nullptr;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetIAccessiblePair(IAccessible** accessible,
                                                     LONG* child_id) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_IACCESSIBLE_PAIR);
  COM_OBJECT_VALIDATE_2_ARGS(accessible, child_id);
  *accessible = static_cast<IAccessible*>(this);
  (*accessible)->AddRef();
  *child_id = CHILDID_SELF;
  return S_OK;
}

//
// IExpandCollapseProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Collapse() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_EXPANDCOLLAPSE_COLLAPSE);
  UIA_VALIDATE_CALL();
  if (GetData().GetRestriction() == ax::mojom::Restriction::kDisabled)
    return UIA_E_ELEMENTNOTAVAILABLE;

  if (GetData().HasState(ax::mojom::State::kCollapsed))
    return UIA_E_INVALIDOPERATION;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::Expand() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_EXPANDCOLLAPSE_EXPAND);
  UIA_VALIDATE_CALL();
  if (GetData().GetRestriction() == ax::mojom::Restriction::kDisabled)
    return UIA_E_ELEMENTNOTAVAILABLE;

  if (GetData().HasState(ax::mojom::State::kExpanded))
    return UIA_E_INVALIDOPERATION;

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ExpandCollapseState(
    ExpandCollapseState* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(
      UMA_API_EXPANDCOLLAPSE_GET_EXPANDCOLLAPSESTATE);
  UIA_VALIDATE_CALL_1_ARG(result);

  const AXNodeData& data = GetData();
  const bool is_menu_button = data.GetHasPopup() == ax::mojom::HasPopup::kMenu;
  const bool is_expanded_menu_button =
      is_menu_button &&
      data.GetCheckedState() == ax::mojom::CheckedState::kTrue;
  const bool is_collapsed_menu_button =
      is_menu_button &&
      data.GetCheckedState() != ax::mojom::CheckedState::kTrue;

  if (data.HasState(ax::mojom::State::kExpanded) || is_expanded_menu_button) {
    *result = ExpandCollapseState_Expanded;
  } else if (data.HasState(ax::mojom::State::kCollapsed) ||
             is_collapsed_menu_button) {
    *result = ExpandCollapseState_Collapsed;
  } else {
    *result = ExpandCollapseState_LeafNode;
  }
  return S_OK;
}

//
// IGridItemProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::get_Column(int* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_COLUMN);
  UIA_VALIDATE_CALL_1_ARG(result);
  base::Optional<int> column = GetTableColumn();
  if (!column)
    return E_FAIL;
  *result = *column;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ColumnSpan(int* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_COLUMNSPAN);
  UIA_VALIDATE_CALL_1_ARG(result);
  base::Optional<int> column_span = GetTableColumnSpan();
  if (!column_span)
    return E_FAIL;
  *result = *column_span;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ContainingGrid(
    IRawElementProviderSimple** result) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_ROW);
  UIA_VALIDATE_CALL_1_ARG(result);
  base::Optional<int> row = GetTableRow();
  if (!row)
    return E_FAIL;
  *result = *row;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowSpan(int* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRIDITEM_GET_ROWSPAN);
  UIA_VALIDATE_CALL_1_ARG(result);
  base::Optional<int> row_span = GetTableRowSpan();
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRID_GETITEM);
  UIA_VALIDATE_CALL_1_ARG(result);

  AXPlatformNodeBase* cell = GetTableCell(row, column);
  if (!cell)
    return E_INVALIDARG;

  auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
  node_win->AddRef();
  *result = static_cast<IRawElementProviderSimple*>(node_win);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowCount(int* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRID_GET_ROWCOUNT);
  UIA_VALIDATE_CALL_1_ARG(result);

  base::Optional<int> row_count = GetTableAriaRowCount();
  if (!row_count)
    return E_UNEXPECTED;
  *result = *row_count;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ColumnCount(int* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GRID_GET_COLUMNCOUNT);
  UIA_VALIDATE_CALL_1_ARG(result);

  base::Optional<int> column_count = GetTableAriaColumnCount();
  if (!column_count)
    return E_UNEXPECTED;
  *result = *column_count;
  return S_OK;
}

//
// IInvokeProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Invoke() {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLLITEM_SCROLLINTOVIEW);
  UIA_VALIDATE_CALL();
  gfx::Rect r = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
  r -= r.OffsetFromOrigin();

  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.target_rect = r;
  action_data.horizontal_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.vertical_scroll_alignment =
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

//
// IScrollProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Scroll(ScrollAmount horizontal_amount,
                                         ScrollAmount vertical_amount) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_SETSCROLLPERCENT);
  UIA_VALIDATE_CALL();
  if (!IsScrollable())
    return E_FAIL;

  const double x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  const double x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  const double y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  const double y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
  const int x =
      gfx::ToRoundedInt(horizontal_percent / 100.0 * (x_max - x_min) + x_min);
  const int y =
      gfx::ToRoundedInt(vertical_percent / 100.0 * (y_max - y_min) + y_min);
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_HORIZONTALLYSCROLLABLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = IsHorizontallyScrollable();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_HorizontalScrollPercent(double* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_HORIZONTALSCROLLPERCENT);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetHorizontalScrollPercent();
  return S_OK;
}

// Horizontal size of the viewable region as a percentage of the total content
// area.
IFACEMETHODIMP AXPlatformNodeWin::get_HorizontalViewSize(double* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_HORIZONTALVIEWSIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  if (!IsHorizontallyScrollable()) {
    *result = 100.;
    return S_OK;
  }

  gfx::RectF clipped_bounds(GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreen, AXClippingBehavior::kClipped));
  float x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  float x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  float total_width = clipped_bounds.width() + x_max - x_min;
  DCHECK_LE(clipped_bounds.width(), total_width);
  *result = 100.0 * clipped_bounds.width() / total_width;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_VerticallyScrollable(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_VERTICALLYSCROLLABLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = IsVerticallyScrollable();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_VerticalScrollPercent(double* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_VERTICALSCROLLPERCENT);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetVerticalScrollPercent();
  return S_OK;
}

// Vertical size of the viewable region as a percentage of the total content
// area.
IFACEMETHODIMP AXPlatformNodeWin::get_VerticalViewSize(double* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_GET_VERTICALVIEWSIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  if (!IsVerticallyScrollable()) {
    *result = 100.0;
    return S_OK;
  }

  gfx::RectF clipped_bounds(GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreen, AXClippingBehavior::kClipped));
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

HRESULT AXPlatformNodeWin::ISelectionItemProviderSetSelected(bool selected) {
  UIA_VALIDATE_CALL();

  int restriction;
  if (GetIntAttribute(ax::mojom::IntAttribute::kRestriction, &restriction)) {
    if (restriction == static_cast<int>(ax::mojom::Restriction::kDisabled))
      return UIA_E_ELEMENTNOTENABLED;
  }

  bool is_selected;
  if (!GetBoolAttribute(ax::mojom::BoolAttribute::kSelected, &is_selected))
    return UIA_E_INVALIDOPERATION;
  if (is_selected == selected)
    return S_OK;

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return UIA_E_INVALIDOPERATION;
}

IFACEMETHODIMP AXPlatformNodeWin::AddToSelection() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_ADDTOSELECTION);
  return ISelectionItemProviderSetSelected(true);
}

IFACEMETHODIMP AXPlatformNodeWin::RemoveFromSelection() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_REMOVEFROMSELECTION);
  return ISelectionItemProviderSetSelected(false);
}

IFACEMETHODIMP AXPlatformNodeWin::Select() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_SELECT);
  return ISelectionItemProviderSetSelected(true);
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsSelected(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTIONITEM_GET_ISSELECTED);
  UIA_VALIDATE_CALL_1_ARG(result);
  // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
  // SelectionItem.IsSelected is set according to the True or False value of
  // aria-checked for 'radio' and 'menuitemradio' roles.
  if (GetData().role == ax::mojom::Role::kRadioButton ||
      GetData().role == ax::mojom::Role::kMenuItemRadio) {
    *result = (GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue);
    return S_OK;
  }

  // https://www.w3.org/TR/wai-aria-1.1/#aria-selected
  // SelectionItem.IsSelected is set according to the True or False value of
  // aria-selected.
  bool is_selected;
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kSelected, &is_selected))
    *result = is_selected;

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_SelectionContainer(
    IRawElementProviderSimple** result) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTION_GETSELECTION);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::vector<AXPlatformNodeWin*> selected_children;
  LONG child_count = GetDelegate()->GetChildCount();
  for (LONG i = 0; i < child_count; ++i) {
    auto* child = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(GetDelegate()->ChildAtIndex(i)));
    DCHECK(child);
    if (child->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
      selected_children.push_back(child);
  }

  LONG selected_children_count = selected_children.size();
  *result = SafeArrayCreateVector(VT_UNKNOWN, 0, selected_children_count);
  if (!*result)
    return E_OUTOFMEMORY;

  for (LONG i = 0; i < selected_children_count; ++i) {
    HRESULT hr = SafeArrayPutElement(
        *result, &i,
        static_cast<IRawElementProviderSimple*>(selected_children[i]));
    if (FAILED(hr)) {
      SafeArrayDestroy(*result);
      *result = nullptr;
      return hr;
    }
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_CanSelectMultiple(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTION_GET_CANSELECTMULTIPLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetData().HasState(ax::mojom::State::kMultiselectable);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsSelectionRequired(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SELECTION_GET_ISSELECTIONREQUIRED);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetData().HasState(ax::mojom::State::kRequired);
  return S_OK;
}

//
// ITableItemProvider methods.
//

IFACEMETHODIMP AXPlatformNodeWin::GetColumnHeaderItems(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLEITEM_GETCOLUMNHEADERITEMS);
  UIA_VALIDATE_CALL_1_ARG(result);

  base::Optional<int> column = GetTableColumn();
  if (!column)
    return E_FAIL;

  std::vector<int32_t> column_header_ids =
      GetDelegate()->GetColHeaderNodeIds(*column);

  std::vector<AXPlatformNodeWin*> platform_node_list =
      CreatePlatformNodeVectorFromRelationIdVector(column_header_ids);

  *result = CreateUIAElementsSafeArray(platform_node_list);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetRowHeaderItems(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLEITEM_GETROWHEADERITEMS);
  UIA_VALIDATE_CALL_1_ARG(result);

  base::Optional<int> row = GetTableRow();
  if (!row)
    return E_FAIL;

  std::vector<int32_t> row_header_ids =
      GetDelegate()->GetRowHeaderNodeIds(*row);

  std::vector<AXPlatformNodeWin*> platform_node_list =
      CreatePlatformNodeVectorFromRelationIdVector(row_header_ids);

  *result = CreateUIAElementsSafeArray(platform_node_list);
  return S_OK;
}

//
// ITableProvider methods.
//

IFACEMETHODIMP AXPlatformNodeWin::GetColumnHeaders(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLE_GETCOLUMNHEADERS);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::vector<int32_t> column_header_ids = GetDelegate()->GetColHeaderNodeIds();

  std::vector<AXPlatformNodeWin*> platform_node_list =
      CreatePlatformNodeVectorFromRelationIdVector(column_header_ids);

  *result = CreateUIAElementsSafeArray(platform_node_list);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetRowHeaders(SAFEARRAY** result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TABLE_GETROWHEADERS);
  UIA_VALIDATE_CALL_1_ARG(result);

  std::vector<int32_t> row_header_ids = GetDelegate()->GetRowHeaderNodeIds();

  std::vector<AXPlatformNodeWin*> platform_node_list =
      CreatePlatformNodeVectorFromRelationIdVector(row_header_ids);

  *result = CreateUIAElementsSafeArray(platform_node_list);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowOrColumnMajor(
    RowOrColumnMajor* result) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TOGGLE_TOGGLE);
  UIA_VALIDATE_CALL();
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;

  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ToggleState(ToggleState* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_TOGGLE_GET_TOGGLESTATE);
  UIA_VALIDATE_CALL_1_ARG(result);
  const auto checked_state = GetData().GetCheckedState();
  if (checked_state == ax::mojom::CheckedState::kTrue) {
    *result = ToggleState_On;
  } else if (checked_state == ax::mojom::CheckedState::kMixed) {
    *result = ToggleState_Indeterminate;
  } else {
    *result = ToggleState_Off;
  }
  return S_OK;
}

//
// IValueProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::SetValue(LPCWSTR value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_VALUE_SETVALUE);
  UIA_VALIDATE_CALL();
  if (!value)
    return E_INVALIDARG;

  if (GetData().IsReadOnlyOrDisabled())
    return UIA_E_ELEMENTNOTENABLED;

  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = base::WideToUTF8(value);
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsReadOnly(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_VALUE_GET_ISREADONLY);
  UIA_VALIDATE_CALL_1_ARG(result);
  *result = GetData().IsReadOnlyOrDisabled();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Value(BSTR* result) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_SETVISUALSTATE);
  UIA_VALIDATE_CALL();
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::Close() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_CLOSE);
  UIA_VALIDATE_CALL();
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::WaitForInputIdle(int milliseconds,
                                                   BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_WAITFORINPUTIDLE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_CanMaximize(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_CANMAXIMIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_CanMinimize(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_CANMINIMIZE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsModal(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_ISMODAL);
  UIA_VALIDATE_CALL_1_ARG(result);

  *result = GetBoolAttribute(ax::mojom::BoolAttribute::kModal);

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_WindowVisualState(
    WindowVisualState* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_WINDOWVISUALSTATE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_WindowInteractionState(
    WindowInteractionState* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_WINDOWINTERACTIONSTATE);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsTopmost(BOOL* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_WINDOW_GET_ISTOPMOST);
  UIA_VALIDATE_CALL_1_ARG(result);
  return UIA_E_NOTSUPPORTED;
}

//
// IRangeValueProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::SetValue(double value) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_GET_LARGECHANGE);
  UIA_VALIDATE_CALL_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange,
                        &attribute)) {
    *result = attribute * kLargeChangeScaleFactor;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Maximum(double* result) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_RANGEVALUE_GET_SMALLCHANGE);
  UIA_VALIDATE_CALL_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Value(double* result) {
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_CONVERT_RETURNED_ELEMENT);
  return E_NOTIMPL;
}

//
// IAccessibleTable methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_accessibleAt(LONG row,
                                                   LONG column,
                                                   IUnknown** accessible) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACCESSIBLE_AT);
  COM_OBJECT_VALIDATE_1_ARG(accessible);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
  return node_win->QueryInterface(IID_PPV_ARGS(accessible));
}

IFACEMETHODIMP AXPlatformNodeWin::get_caption(IUnknown** accessible) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CAPTION);
  COM_OBJECT_VALIDATE_1_ARG(accessible);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  AXPlatformNodeBase* caption = GetTableCaption();
  if (!caption)
    return S_FALSE;

  auto* node_win = static_cast<AXPlatformNodeWin*>(caption);
  return node_win->QueryInterface(IID_PPV_ARGS(accessible));
}

IFACEMETHODIMP AXPlatformNodeWin::get_childIndex(LONG row,
                                                 LONG column,
                                                 LONG* cell_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CHILD_INDEX);
  COM_OBJECT_VALIDATE_1_ARG(cell_index);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  base::Optional<int> index = cell->GetTableCellIndex();
  if (!index)
    return E_FAIL;

  *cell_index = LONG{*index};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnDescription(LONG column,
                                                        BSTR* description) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_DESCRIPTION);
  COM_OBJECT_VALIDATE_1_ARG(description);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
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

    base::string16 cell_name =
        cell->GetString16Attribute(ax::mojom::StringAttribute::kName);
    if (!cell_name.empty()) {
      *description = SysAllocString(cell_name.c_str());
      return S_OK;
    }

    cell_name =
        cell->GetString16Attribute(ax::mojom::StringAttribute::kDescription);
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_EXTENT_AT);
  COM_OBJECT_VALIDATE_1_ARG(n_columns_spanned);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  base::Optional<int> column_span = cell->GetTableColumnSpan();
  if (!column_span)
    return E_FAIL;
  *n_columns_spanned = LONG{*column_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnHeader(
    IAccessibleTable** accessible_table,
    LONG* starting_row_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_HEADER);
  COM_OBJECT_VALIDATE_2_ARGS(accessible_table, starting_row_index);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnIndex(LONG cell_index,
                                                  LONG* column_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_INDEX);
  COM_OBJECT_VALIDATE_1_ARG(column_index);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  AXPlatformNodeBase* cell = GetTableCell(cell_index);
  if (!cell)
    return E_INVALIDARG;

  base::Optional<int> cell_column = cell->GetTableColumn();
  if (!cell_column)
    return E_FAIL;
  *column_index = LONG{*cell_column};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nColumns(LONG* column_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_COLUMNS);
  COM_OBJECT_VALIDATE_1_ARG(column_count);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  base::Optional<int> columns = GetTableColumnCount();
  if (!columns)
    return E_FAIL;
  *column_count = LONG{*columns};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nRows(LONG* row_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_ROWS);
  COM_OBJECT_VALIDATE_1_ARG(row_count);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  base::Optional<int> rows = GetTableRowCount();
  if (!rows)
    return E_FAIL;
  *row_count = LONG{*rows};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedChildren(LONG* cell_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_CHILDREN);
  COM_OBJECT_VALIDATE_1_ARG(cell_count);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  LONG result = 0;
  for (int r = 0; r < *rows; ++r) {
    for (int c = 0; c < *columns; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell &&
          cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
        ++result;
    }
  }
  *cell_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedColumns(LONG* column_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_COLUMNS);
  COM_OBJECT_VALIDATE_1_ARG(column_count);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  // If every cell in a column is selected, then that column is selected.
  LONG result = 0;
  for (int c = 0; c < *columns; ++c) {
    bool selected = true;
    for (int r = 0; r < *rows && selected == true; ++r) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell || !(cell->GetData().GetBoolAttribute(
                       ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      ++result;
  }

  *column_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedRows(LONG* row_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_ROWS);
  COM_OBJECT_VALIDATE_1_ARG(row_count);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  // If every cell in a row is selected, then that row is selected.
  LONG result = 0;
  for (int r = 0; r < *rows; ++r) {
    bool selected = true;
    for (int c = 0; c < *columns && selected == true; ++c) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell || !(cell->GetData().GetBoolAttribute(
                       ax::mojom::BoolAttribute::kSelected)))
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_DESCRIPTION);
  COM_OBJECT_VALIDATE_1_ARG(description);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> rows = GetTableRowCount();
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

    base::string16 cell_name =
        cell->GetString16Attribute(ax::mojom::StringAttribute::kName);
    if (!cell_name.empty()) {
      *description = SysAllocString(cell_name.c_str());
      return S_OK;
    }

    cell_name =
        cell->GetString16Attribute(ax::mojom::StringAttribute::kDescription);
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_EXTENT_AT);
  COM_OBJECT_VALIDATE_1_ARG(n_rows_spanned);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  const AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  base::Optional<int> cell_row_span = cell->GetTableRowSpan();
  if (!cell_row_span)
    return E_FAIL;
  *n_rows_spanned = LONG{*cell_row_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowHeader(
    IAccessibleTable** accessible_table,
    LONG* starting_column_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_HEADER);
  COM_OBJECT_VALIDATE_2_ARGS(accessible_table, starting_column_index);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowIndex(LONG cell_index,
                                               LONG* row_index) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(row_index);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  const AXPlatformNodeBase* cell = GetTableCell(cell_index);
  if (!cell)
    return E_INVALIDARG;

  base::Optional<int> cell_row = cell->GetTableRow();
  if (!cell_row)
    return E_FAIL;
  *row_index = LONG{*cell_row};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedChildren(LONG max_children,
                                                       LONG** children,
                                                       LONG* n_children) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_2_ARGS(children, n_children);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (max_children <= 0)
    return E_INVALIDARG;

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  std::vector<LONG> results;
  for (int r = 0; r < *rows; ++r) {
    for (int c = 0; c < *columns; ++c) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell && cell->GetData().GetBoolAttribute(
                      ax::mojom::BoolAttribute::kSelected)) {
        base::Optional<int> cell_index = cell->GetTableCellIndex();
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
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_2_ARGS(columns, n_columns);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (max_columns <= 0)
    return E_INVALIDARG;

  base::Optional<int> column_count = GetTableColumnCount();
  base::Optional<int> row_count = GetTableRowCount();
  if (!column_count || !row_count)
    return E_FAIL;

  std::vector<LONG> results;
  for (int c = 0; c < *column_count; ++c) {
    bool selected = true;
    for (int r = 0; r < *row_count && selected == true; ++r) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell || !(cell->GetData().GetBoolAttribute(
                       ax::mojom::BoolAttribute::kSelected)))
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
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_2_ARGS(rows, n_rows);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (max_rows <= 0)
    return E_INVALIDARG;

  base::Optional<int> column_count = GetTableColumnCount();
  base::Optional<int> row_count = GetTableRowCount();
  if (!column_count || !row_count)
    return E_FAIL;

  std::vector<LONG> results;
  for (int r = 0; r < *row_count; ++r) {
    bool selected = true;
    for (int c = 0; c < *column_count && selected == true; ++c) {
      const AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell || !(cell->GetData().GetBoolAttribute(
                       ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      results.push_back(r);
  }

  return AllocateComArrayFromVector(results, max_rows, rows, n_rows);
}

IFACEMETHODIMP AXPlatformNodeWin::get_summary(IUnknown** accessible) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(accessible);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  // TODO(dmazzoni): implement.
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isColumnSelected(LONG column,
                                                       boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  if (column < 0 || column >= *columns)
    return E_INVALIDARG;

  for (int r = 0; r < *rows; ++r) {
    const AXPlatformNodeBase* cell = GetTableCell(r, column);
    if (!cell || !(cell->GetData().GetBoolAttribute(
                     ax::mojom::BoolAttribute::kSelected)))
      return S_OK;
  }

  *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isRowSelected(LONG row,
                                                    boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  if (row < 0 || row >= *rows)
    return E_INVALIDARG;

  for (int c = 0; c < *columns; ++c) {
    const AXPlatformNodeBase* cell = GetTableCell(row, c);
    if (!cell || !(cell->GetData().GetBoolAttribute(
                     ax::mojom::BoolAttribute::kSelected)))
      return S_OK;
  }

  *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isSelected(LONG row,
                                                 LONG column,
                                                 boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  const AXPlatformNodeBase* cell = GetTableCell(int{row}, int{column});
  if (!cell)
    return E_INVALIDARG;

  if (cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
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
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_5_ARGS(row, column, row_extents, column_extents,
                             is_selected);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  const AXPlatformNodeBase* cell = GetTableCell(index);
  if (!cell)
    return E_INVALIDARG;

  base::Optional<int> row_index = cell->GetTableRow();
  base::Optional<int> column_index = cell->GetTableColumn();
  base::Optional<int> row_span = cell->GetTableRowSpan();
  base::Optional<int> column_span = cell->GetTableColumnSpan();
  if (!row_index || !column_index || !row_span || !column_span)
    return E_FAIL;

  *row = LONG{*row_index};
  *column = LONG{*column_index};
  *row_extents = LONG{*row_span};
  *column_extents = LONG{*column_span};
  if (cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::selectRow(LONG row) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> rows = GetTableRowCount();
  if (!rows)
    return E_FAIL;

  if (row < 0 || row >= *rows)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::selectColumn(LONG column) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  if (!columns)
    return E_FAIL;

  if (column < 0 || column >= *columns)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::unselectRow(LONG row) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> rows = GetTableRowCount();
  if (!rows)
    return E_FAIL;

  if (row < 0 || row >= *rows)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::unselectColumn(LONG column) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  if (!columns)
    return E_FAIL;

  if (column < 0 || column >= *columns)
    return E_INVALIDARG;

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_modelChange(
    IA2TableModelChange* model_change) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(model_change);
  return E_NOTIMPL;
}

//
// IAccessibleTable2 methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_cellAt(LONG row,
                                             LONG column,
                                             IUnknown** cell) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(cell);
  AXPlatformNode::NotifyAddAXModeFlags(AXMode::kScreenReader);

  AXPlatformNodeBase* table_cell = GetTableCell(int{row}, int{column});
  if (!table_cell)
    return E_INVALIDARG;

  auto* node_win = static_cast<AXPlatformNodeWin*>(table_cell);
  return node_win->QueryInterface(IID_PPV_ARGS(cell));
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedCells(LONG* cell_count) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  // Note that this method does not need to set any ax mode since it
  // calls into get_nSelectedChildren() which does.
  return get_nSelectedChildren(cell_count);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedCells(IUnknown*** cells,
                                                    LONG* n_selected_cells) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_2_ARGS(cells, n_selected_cells);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> columns = GetTableColumnCount();
  base::Optional<int> rows = GetTableRowCount();
  if (!columns || !rows)
    return E_FAIL;

  std::vector<AXPlatformNodeBase*> selected;
  for (int r = 0; r < *rows; ++r) {
    for (int c = 0; c < *columns; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell &&
          cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
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
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  return get_selectedColumns(INT_MAX, columns, n_columns);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedRows(LONG** rows, LONG* n_rows) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  return get_selectedRows(INT_MAX, rows, n_rows);
}

//
// IAccessibleTableCell methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_columnExtent(LONG* n_columns_spanned) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(n_columns_spanned);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> column_span = GetTableColumnSpan();
  if (!column_span)
    return E_FAIL;
  *n_columns_spanned = LONG{*column_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnHeaderCells(
    IUnknown*** cell_accessibles,
    LONG* n_column_header_cells) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_2_ARGS(cell_accessibles, n_column_header_cells);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> column = GetTableColumn();
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

  *n_column_header_cells = LONG{column_header_ids.size()};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnIndex(LONG* column_index) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(column_index);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> column = GetTableColumn();
  if (!column)
    return E_FAIL;
  *column_index = LONG{*column};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowExtent(LONG* n_rows_spanned) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(n_rows_spanned);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> row_span = GetTableRowSpan();
  if (!row_span)
    return E_FAIL;
  *n_rows_spanned = LONG{*row_span};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowHeaderCells(
    IUnknown*** cell_accessibles,
    LONG* n_row_header_cells) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_2_ARGS(cell_accessibles, n_row_header_cells);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> row = GetTableRow();
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

  *n_row_header_cells = LONG{row_header_ids.size()};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowIndex(LONG* row_index) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(row_index);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> row = GetTableRow();
  if (!row)
    return E_FAIL;
  *row_index = LONG{*row};
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isSelected(boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(is_selected);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowColumnExtents(LONG* row_index,
                                                       LONG* column_index,
                                                       LONG* row_extents,
                                                       LONG* column_extents,
                                                       boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_5_ARGS(row_index, column_index, row_extents,
                             column_extents, is_selected);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::Optional<int> row = GetTableRow();
  base::Optional<int> column = GetTableColumn();
  base::Optional<int> row_span = GetTableRowSpan();
  base::Optional<int> column_span = GetTableColumnSpan();
  if (!row || !column || !row_span || !column_span)
    return E_FAIL;

  *row_index = LONG{*row};
  *column_index = LONG{*column};
  *row_extents = LONG{*row_span};
  *column_extents = LONG{*column_span};
  if (GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    *is_selected = true;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_table(IUnknown** table) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  COM_OBJECT_VALIDATE_1_ARG(table);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_CHARACTERS);
  COM_OBJECT_VALIDATE_1_ARG(n_characters);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                                       ui::AXMode::kInlineTextBoxes);

  base::string16 text = TextForIAccessibleText();
  *n_characters = static_cast<LONG>(text.size());

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_caretOffset(LONG* offset) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CARET_OFFSET);
  COM_OBJECT_VALIDATE_1_ARG(offset);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  *offset = 0;

  if (!HasCaret())
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTIONS);
  COM_OBJECT_VALIDATE_1_ARG(n_selections);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_SELECTION);
  COM_OBJECT_VALIDATE_2_ARGS(start_offset, end_offset);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT);
  COM_OBJECT_VALIDATE_1_ARG(text);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset)
    std::swap(start_offset, end_offset);

  const base::string16 str = TextForIAccessibleText();
  LONG str_len = static_cast<LONG>(str.length());
  if (start_offset < 0 || start_offset > str_len)
    return E_INVALIDARG;
  if (end_offset < 0 || end_offset > str_len)
    return E_INVALIDARG;

  base::string16 substr = str.substr(start_offset, end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(substr.c_str());
  DCHECK(*text);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_textAtOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  COM_OBJECT_VALIDATE_3_ARGS(start_offset, end_offset, text);
  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screen reader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = nullptr;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(text_str, boundary_type, offset,
                               AXTextBoundaryDirection::kBackwards);
  *end_offset = FindBoundary(text_str, boundary_type, offset,
                             AXTextBoundaryDirection::kForwards);
  return get_text(*start_offset, *end_offset, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_textBeforeOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = nullptr;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(text_str, boundary_type, offset,
                               AXTextBoundaryDirection::kBackwards);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_textAfterOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = nullptr;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = offset;
  *end_offset = FindBoundary(text_str, boundary_type, offset,
                             AXTextBoundaryDirection::kForwards);
  return get_text(*start_offset, *end_offset, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_offsetAtPoint(
    LONG x,
    LONG y,
    enum IA2CoordinateType coord_type,
    LONG* offset) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_OFFSET_AT_POINT);
  COM_OBJECT_VALIDATE_1_ARG(offset);

  *offset = -1;

  if (coord_type == IA2CoordinateType::IA2_COORDTYPE_PARENT_RELATIVE) {
    // We don't support when the IA2 coordinate type is parent relative, but
    // we have to return something rather than E_NOTIMPL or screen readers
    // will complain.
    NOTIMPLEMENTED_LOG_ONCE() << "See http://crbug.com/1010726";
    return S_FALSE;
  }

  // We currently only handle IA2 screen relative coord type.
  DCHECK_EQ(coord_type, IA2_COORDTYPE_SCREEN_RELATIVE);

  const AXPlatformNodeWin* hit_child = static_cast<AXPlatformNodeWin*>(
      FromNativeViewAccessible(GetDelegate()->HitTestSync(x, y)));

  if (!hit_child || !hit_child->IsTextOnlyObject()) {
    return S_FALSE;
  }

  for (int i = 0, text_length = hit_child->GetInnerText().length();
       i < text_length; ++i) {
    gfx::Rect char_bounds =
        hit_child->GetDelegate()->GetInnerTextRangeBoundsRect(
            i, i + 1, AXCoordinateSystem::kScreen,
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ADD_SELECTION);
  COM_OBJECT_VALIDATE();
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  // We only support one selection.
  return setSelection(0, start_offset, end_offset);
}

IFACEMETHODIMP AXPlatformNodeWin::removeSelection(LONG selection_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_REMOVE_SELECTION);
  COM_OBJECT_VALIDATE();
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (selection_index != 0)
    return E_INVALIDARG;
  // Simply collapse the selection to the position of the caret if a caret is
  // visible, otherwise set the selection to 0.
  return setCaretOffset(GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));
}

IFACEMETHODIMP AXPlatformNodeWin::setCaretOffset(LONG offset) {
  return setSelection(0, offset, offset);
}

IFACEMETHODIMP AXPlatformNodeWin::setSelection(LONG selection_index,
                                               LONG start_offset,
                                               LONG end_offset) {
  if (selection_index != 0)
    return E_INVALIDARG;

  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);
  if (start_offset < 0 ||
      start_offset > static_cast<LONG>(TextForIAccessibleText().length())) {
    return E_INVALIDARG;
  }
  if (end_offset < 0 ||
      end_offset > static_cast<LONG>(TextForIAccessibleText().length())) {
    return E_INVALIDARG;
  }

  if (SetHypertextSelection(int{start_offset}, int{end_offset})) {
    return S_OK;
  }
  return E_FAIL;
}

//
// IAccessibleText methods not implemented.
//

IFACEMETHODIMP AXPlatformNodeWin::get_newText(IA2TextSegment* new_text) {
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::get_oldText(IA2TextSegment* old_text) {
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::get_characterExtents(
    LONG offset,
    enum IA2CoordinateType coord_type,
    LONG* x,
    LONG* y,
    LONG* width,
    LONG* height) {
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::scrollSubstringTo(
    LONG start_index,
    LONG end_index,
    enum IA2ScrollType scroll_type) {
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::scrollSubstringToPoint(
    LONG start_index,
    LONG end_index,
    enum IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::get_attributes(LONG offset,
                                                 LONG* start_offset,
                                                 LONG* end_offset,
                                                 BSTR* text_attributes) {
  return E_NOTIMPL;
}

//
// IRawElementProviderFragment implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Navigate(
    NavigateDirection direction,
    IRawElementProviderFragment** element_provider) {
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
      if (UNLIKELY(fragment_root)) {
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
      NOTREACHED();
      break;
  }

  if (neighbor) {
    if (direction != NavigateDirection_Parent) {
      // 2 / 3b. If target node Ux is the child of a fragment root and the
      // source node isn't Rx, return Rx.
      AXFragmentRootWin* fragment_root =
          AXFragmentRootWin::GetFragmentRootParentOf(neighbor);
      if (UNLIKELY(fragment_root && fragment_root != GetDelegate()))
        neighbor = fragment_root->GetNativeViewAccessible();
    }
    neighbor->QueryInterface(IID_PPV_ARGS(element_provider));
  }

  return S_OK;
}

void AXPlatformNodeWin::GetRuntimeIdArray(
    AXPlatformNodeWin::RuntimeIdArray& runtime_id) {
  runtime_id[0] = UiaAppendRuntimeId;
  runtime_id[1] = GetUniqueId();
}

IFACEMETHODIMP AXPlatformNodeWin::GetRuntimeId(SAFEARRAY** runtime_id) {
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
    UiaRect* bounding_rectangle) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_BOUNDINGRECTANGLE);
  UIA_VALIDATE_CALL_1_ARG(bounding_rectangle);

  gfx::Rect bounds = delegate_->GetBoundsRect(AXCoordinateSystem::kScreen,
                                              AXClippingBehavior::kUnclipped);
  bounding_rectangle->left = bounds.x();
  bounding_rectangle->top = bounds.y();
  bounding_rectangle->width = bounds.width();
  bounding_rectangle->height = bounds.height();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetEmbeddedFragmentRoots(
    SAFEARRAY** embedded_fragment_roots) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GETEMBEDDEDFRAGMENTROOTS);
  UIA_VALIDATE_CALL_1_ARG(embedded_fragment_roots);

  *embedded_fragment_roots = nullptr;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::SetFocus() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SETFOCUS);
  UIA_VALIDATE_CALL();

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  delegate_->AccessibilityPerformAction(action_data);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_FragmentRoot(
    IRawElementProviderFragmentRoot** fragment_root) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_FRAGMENTROOT);
  UIA_VALIDATE_CALL_1_ARG(fragment_root);

  gfx::AcceleratedWidget widget =
      delegate_->GetTargetForNativeAccessibilityEvent();
  if (widget) {
    ui::AXFragmentRootWin* root =
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PATTERN_PROVIDER);
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
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PROPERTY_VALUE);

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

  UIA_VALIDATE_CALL_1_ARG(result);

  result->vt = VT_EMPTY;

  int int_attribute;
  const AXNodeData& data = GetData();

  switch (property_id) {
    case UIA_AriaPropertiesPropertyId:
      result->vt = VT_BSTR;
      result->bstrVal = SysAllocString(ComputeUIAProperties().c_str());
      break;

    case UIA_AriaRolePropertyId:
      result->vt = VT_BSTR;
      result->bstrVal = SysAllocString(UIAAriaRole().c_str());
      break;

    case UIA_AutomationIdPropertyId:
      V_VT(result) = VT_BSTR;
      V_BSTR(result) =
          SysAllocString(GetDelegate()->GetAuthorUniqueId().c_str());
      break;

    case UIA_ClassNamePropertyId:
      result->vt = VT_BSTR;
      GetStringAttributeAsBstr(ax::mojom::StringAttribute::kClassName,
                               &result->bstrVal);
      break;

    case UIA_ClickablePointPropertyId:
      result->vt = VT_ARRAY | VT_R8;
      result->parray = CreateClickablePointArray();
      break;

    case UIA_ControllerForPropertyId:
      result->vt = VT_ARRAY | VT_UNKNOWN;
      result->parray = CreateUIAControllerForArray();
      break;

    case UIA_ControlTypePropertyId:
      result->vt = VT_I4;
      result->lVal = ComputeUIAControlType();
      break;

    case UIA_CulturePropertyId:
      result->vt = VT_BSTR;
      GetStringAttributeAsBstr(ax::mojom::StringAttribute::kLanguage,
                               &result->bstrVal);
      break;

    case UIA_DescribedByPropertyId:
      result->vt = VT_ARRAY | VT_UNKNOWN;
      result->parray = CreateUIAElementsArrayForRelation(
          ax::mojom::IntListAttribute::kDescribedbyIds);
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
      } else if (data.GetNameFrom() == ax::mojom::NameFrom::kPlaceholder ||
                 data.GetNameFrom() == ax::mojom::NameFrom::kTitle) {
        V_VT(result) = VT_BSTR;
        GetStringAttributeAsBstr(ax::mojom::StringAttribute::kName,
                                 &V_BSTR(result));
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
      if (data.GetIntAttribute(ax::mojom::IntAttribute::kInvalidState,
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
      result->boolVal = IsDialog(data.role) ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_IsKeyboardFocusablePropertyId:
      result->vt = VT_BOOL;
      result->boolVal =
          ShouldNodeHaveFocusableState(data) ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_IsOffscreenPropertyId:
      result->vt = VT_BOOL;
      result->boolVal =
          GetDelegate()->IsOffscreen() ? VARIANT_TRUE : VARIANT_FALSE;
      break;

    case UIA_IsRequiredForFormPropertyId:
      result->vt = VT_BOOL;
      if (data.HasState(ax::mojom::State::kRequired)) {
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
      if (IsTableHeader(data.role) &&
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
      for (int32_t id : data.GetIntListAttribute(
               ax::mojom::IntListAttribute::kLabelledbyIds)) {
        auto* node_win = GetDelegate()->GetFromNodeID(id);
        if (IsValidUiaRelationTarget(node_win)) {
          result->vt = VT_UNKNOWN;
          result->punkVal = node_win->GetNativeViewAccessible();
          result->punkVal->AddRef();
          break;
        }
      }
      break;

    case UIA_LocalizedControlTypePropertyId: {
      base::string16 localized_control_type = GetRoleDescription();
      if (!localized_control_type.empty()) {
        result->vt = VT_BSTR;
        result->bstrVal = SysAllocString(localized_control_type.c_str());
      }
      // If a role description has not been provided, leave as VT_EMPTY.
      // UIA core handles Localized Control type for some built-in types and
      // also has a mapping for ARIA roles. To get these defaults, we need to
      // have returned VT_EMPTY.
    } break;

    case UIA_NamePropertyId:
      result->vt = VT_BSTR;
      GetStringAttributeAsBstr(ax::mojom::StringAttribute::kName,
                               &result->bstrVal);
      break;

    case UIA_OrientationPropertyId:
      if (SupportsOrientation(data.role)) {
        if (data.HasState(ax::mojom::State::kHorizontal) &&
            data.HasState(ax::mojom::State::kVertical)) {
          NOTREACHED() << "An accessibility object cannot have a horizontal "
                          "and a vertical orientation at the same time.";
        }
        if (data.HasState(ax::mojom::State::kHorizontal)) {
          result->vt = VT_I4;
          result->intVal = OrientationType_Horizontal;
        }
        if (data.HasState(ax::mojom::State::kVertical)) {
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
      switch (data.GetRestriction()) {
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
      result->boolVal = data.HasState(ax::mojom::State::kProtected)
                            ? VARIANT_TRUE
                            : VARIANT_FALSE;
      break;

    case UIA_AcceleratorKeyPropertyId:
      if (data.HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts)) {
        result->vt = VT_BSTR;
        GetStringAttributeAsBstr(ax::mojom::StringAttribute::kKeyShortcuts,
                                 &result->bstrVal);
      }
      break;

    case UIA_AccessKeyPropertyId:
      if (data.HasStringAttribute(ax::mojom::StringAttribute::kAccessKey)) {
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
      if (data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                               &int_attribute)) {
        result->vt = VT_I4;
        result->intVal = int_attribute;
      }
      break;

    case UIA_LiveSettingPropertyId: {
      result->vt = VT_I4;
      result->intVal = LiveSetting::Off;

      std::string string_attribute;
      if (data.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
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
      base::Optional<int> pos_in_set = GetPosInSet();
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

    case UIA_SizeOfSetPropertyId: {
      base::Optional<int> set_size = GetSetSize();
      if (set_size) {
        result->vt = VT_I4;
        result->intVal = *set_size;
      }
    } break;

    case UIA_LandmarkTypePropertyId: {
      base::Optional<LONG> landmark_type = ComputeUIALandmarkType();
      if (landmark_type) {
        result->vt = VT_I4;
        result->intVal = landmark_type.value();
      }
      break;
    }

    case UIA_LocalizedLandmarkTypePropertyId: {
      base::string16 localized_landmark_type =
          GetDelegate()->GetLocalizedStringForLandmarkType();
      if (!localized_landmark_type.empty()) {
        result->vt = VT_BSTR;
        result->bstrVal = SysAllocString(localized_landmark_type.c_str());
      }
      break;
    }

    // Not currently implemented.
    case UIA_AnnotationObjectsPropertyId:
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
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ProviderOptions(ProviderOptions* ret) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PROVIDER_OPTIONS);
  UIA_VALIDATE_CALL_1_ARG(ret);

  *ret = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading |
         ProviderOptions_RefuseNonClientSupport;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_HostRawElementProvider(
    IRawElementProviderSimple** provider) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_HOST_RAW_ELEMENT_PROVIDER);
  UIA_VALIDATE_CALL_1_ARG(provider);

  *provider = nullptr;
  return S_OK;
}

//
// IRawElementProviderSimple2 implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::ShowContextMenu() {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SHOWCONTEXTMENU);
  UIA_VALIDATE_CALL();

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kShowContextMenu;
  delegate_->AccessibilityPerformAction(action_data);
  return S_OK;
}

//
// IServiceProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::QueryService(REFGUID guidService,
                                               REFIID riid,
                                               void** object) {
  COM_OBJECT_VALIDATE_1_ARG(object);

  if (riid == IID_IAccessible2) {
    for (IAccessible2UsageObserver& observer :
         GetIAccessible2UsageObserverList()) {
      observer.OnIAccessible2Used();
    }
  }

  if (guidService == IID_IAccessible || guidService == IID_IAccessible2 ||
      guidService == IID_IAccessible2_2 ||
      guidService == IID_IAccessibleTable ||
      guidService == IID_IAccessibleTable2 ||
      guidService == IID_IAccessibleTableCell ||
      guidService == IID_IAccessibleText) {
    return QueryInterface(riid, object);
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
    if (!IsTableLike(accessible->GetData().role))
      return E_NOINTERFACE;
  } else if (riid == IID_IAccessibleTableCell) {
    if (!IsCellOrTableHeader(accessible->GetData().role))
      return E_NOINTERFACE;
  } else if (riid == IID_IAccessibleText || riid == IID_IAccessibleHypertext) {
    if (ui::IsImageOrVideo(accessible->GetData().role)) {
      return E_NOINTERFACE;
    }
  }

  return CComObjectRootBase::InternalQueryInterface(this_ptr, entries, riid,
                                                    object);
}

HRESULT AXPlatformNodeWin::GetTextAttributeValue(TEXTATTRIBUTEID attribute_id,
                                                 VARIANT* result) {
  // Text attributes of kInlineTextBox nodes are stored on the parent node
  // (which is typically a kStaticText or kLineBreak node).
  if (GetData().role == ax::mojom::Role::kInlineTextBox) {
    AXPlatformNodeWin* parent_platform_node =
        static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(GetParent()));
    if (!parent_platform_node)
      return UIA_E_ELEMENTNOTAVAILABLE;

    return parent_platform_node->GetTextAttributeValue(attribute_id, result);
  }

  switch (attribute_id) {
    case UIA_BackgroundColorAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) =
          GetIntAttributeAsCOLORREF(ax::mojom::IntAttribute::kBackgroundColor);
      break;
    case UIA_BulletStyleAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) = ComputeUIABulletStyle();
      break;
    case UIA_CultureAttributeId:
      return GetCultureAttributeAsVariant(result);
    case UIA_FontNameAttributeId:
      V_VT(result) = VT_BSTR;
      V_BSTR(result) = GetFontNameAttributeAsBSTR();
      break;
    case UIA_FontSizeAttributeId:
      V_VT(result) = VT_R8;
      V_R8(result) = GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize);
      break;
    case UIA_FontWeightAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) = GetFloatAttribute(ax::mojom::FloatAttribute::kFontWeight);
      break;
    case UIA_ForegroundColorAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) = GetIntAttributeAsCOLORREF(ax::mojom::IntAttribute::kColor);
      break;
    case UIA_IsHiddenAttributeId:
      V_VT(result) = VT_BOOL;
      V_BOOL(result) = IsInvisibleOrIgnored() ? VARIANT_TRUE : VARIANT_FALSE;
      break;
    case UIA_IsItalicAttributeId:
      V_VT(result) = VT_BOOL;
      V_BOOL(result) = GetData().HasTextStyle(ax::mojom::TextStyle::kItalic)
                           ? VARIANT_TRUE
                           : VARIANT_FALSE;
      break;
    case UIA_IsReadOnlyAttributeId:
      // Placeholder text should return the enclosing element's read-only value.
      if (IsPlaceholderText()) {
        AXPlatformNodeWin* parent_platform_node =
            static_cast<AXPlatformNodeWin*>(
                FromNativeViewAccessible(GetParent()));
        return parent_platform_node->GetTextAttributeValue(attribute_id,
                                                           result);
      }
      V_VT(result) = VT_BOOL;
      V_BOOL(result) =
          (GetData().GetRestriction() == ax::mojom::Restriction::kReadOnly ||
           !GetData().HasState(ax::mojom::State::kEditable))
              ? VARIANT_TRUE
              : VARIANT_FALSE;
      break;
    case UIA_IsSubscriptAttributeId:
      V_VT(result) = VT_BOOL;
      V_BOOL(result) =
          (GetData().GetTextPosition() == ax::mojom::TextPosition::kSubscript)
              ? VARIANT_TRUE
              : VARIANT_FALSE;
      break;
    case UIA_IsSuperscriptAttributeId:
      V_VT(result) = VT_BOOL;
      V_BOOL(result) =
          (GetData().GetTextPosition() == ax::mojom::TextPosition::kSuperscript)
              ? VARIANT_TRUE
              : VARIANT_FALSE;
      break;
    case UIA_OverlineStyleAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) = GetUIATextDecorationStyle(
          ax::mojom::IntAttribute::kTextOverlineStyle);
      break;
    case UIA_StrikethroughStyleAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) = GetUIATextDecorationStyle(
          ax::mojom::IntAttribute::kTextStrikethroughStyle);
      break;
    case UIA_StyleNameAttributeId:
      V_VT(result) = VT_BSTR;
      V_BSTR(result) = GetStyleNameAttributeAsBSTR();
      break;
    case UIA_StyleIdAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) = ComputeUIAStyleId();
      break;
    case UIA_UnderlineStyleAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) = GetUIATextDecorationStyle(
          ax::mojom::IntAttribute::kTextUnderlineStyle);
      break;
    case UIA_TextFlowDirectionsAttributeId:
      V_VT(result) = VT_I4;
      V_I4(result) =
          TextDirectionToFlowDirections(GetData().GetTextDirection());
      break;
    default:
      V_VT(result) = VT_UNKNOWN;
      return ::UiaGetReservedNotSupportedValue(&V_UNKNOWN(result));
  }

  return S_OK;
}

HRESULT AXPlatformNodeWin::GetCultureAttributeAsVariant(VARIANT* result) const {
  const base::string16 language =
      GetInheritedString16Attribute(ax::mojom::StringAttribute::kLanguage);
  const LCID lcid =
      LocaleNameToLCID(language.c_str(), LOCALE_ALLOW_NEUTRAL_NAMES);
  if (!lcid)
    return E_FAIL;

  V_VT(result) = VT_I4;
  V_I4(result) = lcid;
  return S_OK;
}

COLORREF AXPlatformNodeWin::GetIntAttributeAsCOLORREF(
    ax::mojom::IntAttribute attribute) const {
  const SkColor color = GetIntAttribute(attribute);
  return skia::SkColorToCOLORREF(color);
}

BulletStyle AXPlatformNodeWin::ComputeUIABulletStyle() const {
  // UIA expects the list style of a non-list-item to be none however the
  // default list style cascaded is disc not none. Therefore we must ensure that
  // this node is contained within a list-item to distinguish non-list-items and
  // disc styled list items.
  const AXPlatformNodeBase* current_node = this;
  while (current_node &&
         current_node->GetData().role != ax::mojom::Role::kListItem) {
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
    switch (current_node->GetData().role) {
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
    ax::mojom::TextDirection text_direction) {
  switch (text_direction) {
    case ax::mojom::TextDirection::kNone:
      return FlowDirections::FlowDirections_Default;
    case ax::mojom::TextDirection::kLtr:
      return FlowDirections::FlowDirections_Default;
    case ax::mojom::TextDirection::kRtl:
      return FlowDirections::FlowDirections_RightToLeft;
    case ax::mojom::TextDirection::kTtb:
      return FlowDirections::FlowDirections_Vertical;
    case ax::mojom::TextDirection::kBtt:
      return FlowDirections::FlowDirections_BottomToTop;
  }
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
  if (IsWebAreaForPresentationalIframe())
    return ROLE_SYSTEM_GROUPING;

  switch (GetData().role) {
    case ax::mojom::Role::kAlert:
    case ax::mojom::Role::kAlertDialog:
      return ROLE_SYSTEM_ALERT;

    case ax::mojom::Role::kAnchor:
      return ROLE_SYSTEM_LINK;

    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kCommentSection:
    case ax::mojom::Role::kRevision:
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
      return ROLE_SYSTEM_TEXT;

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

    case ax::mojom::Role::kDescriptionListDetail:
      return ROLE_SYSTEM_TEXT;

    case ax::mojom::Role::kDescriptionList:
      return ROLE_SYSTEM_LIST;

    case ax::mojom::Role::kDescriptionListTerm:
      return ROLE_SYSTEM_LISTITEM;

    case ax::mojom::Role::kDesktop:
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kDetails:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kDialog:
      return ROLE_SYSTEM_DIALOG;

    case ax::mojom::Role::kDisclosureTriangle:
      return ROLE_SYSTEM_PUSHBUTTON;

    case ax::mojom::Role::kDirectory:
      return ROLE_SYSTEM_LIST;

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
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kWebArea:
      return ROLE_SYSTEM_DOCUMENT;

    case ax::mojom::Role::kEmbeddedObject:
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

    case ax::mojom::Role::kFooterAsNonLandmark:
    case ax::mojom::Role::kHeaderAsNonLandmark:
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

    case ax::mojom::Role::kImageMap:
      return ROLE_SYSTEM_CLIENT;

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

    case ax::mojom::Role::kLayoutTableColumn:
      return ROLE_SYSTEM_COLUMN;

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

    case ax::mojom::Role::kLog:
      return ROLE_SYSTEM_CLIENT;

    case ax::mojom::Role::kMain:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kMark:
      return ROLE_SYSTEM_TEXT;

    case ax::mojom::Role::kMarquee:
      return ROLE_SYSTEM_ANIMATION;

    case ax::mojom::Role::kMath:
      return ROLE_SYSTEM_EQUATION;

    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuButton:
      return ROLE_SYSTEM_MENUPOPUP;

    case ax::mojom::Role::kMenuBar:
      return ROLE_SYSTEM_MENUBAR;

    case ax::mojom::Role::kMenuItem:
      return ROLE_SYSTEM_MENUITEM;

    case ax::mojom::Role::kMenuItemCheckBox:
      return ROLE_SYSTEM_MENUITEM;

    case ax::mojom::Role::kMenuItemRadio:
      return ROLE_SYSTEM_MENUITEM;

    case ax::mojom::Role::kMenuListPopup:
      if (IsAncestorComboBox())
        return ROLE_SYSTEM_LIST;
      return ROLE_SYSTEM_MENUPOPUP;

    case ax::mojom::Role::kMenuListOption:
      if (IsAncestorComboBox())
        return ROLE_SYSTEM_LISTITEM;
      return ROLE_SYSTEM_MENUITEM;

    case ax::mojom::Role::kMeter:
      return ROLE_SYSTEM_PROGRESSBAR;

    case ax::mojom::Role::kNavigation:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kNote:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kParagraph:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kPopUpButton: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "select")
        return ROLE_SYSTEM_COMBOBOX;
      return ROLE_SYSTEM_BUTTONMENU;
    }

    case ax::mojom::Role::kPre:
      return ROLE_SYSTEM_TEXT;

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

    case ax::mojom::Role::kRowHeader:
      return ROLE_SYSTEM_ROWHEADER;

    case ax::mojom::Role::kRuby:
      return ROLE_SYSTEM_TEXT;

    case ax::mojom::Role::kSection: {
      if (GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return ROLE_SYSTEM_GROUPING;
      }
      // Use ARIA mapping.
      return ROLE_SYSTEM_PANE;
    }

    case ax::mojom::Role::kScrollBar:
      return ROLE_SYSTEM_SCROLLBAR;

    case ax::mojom::Role::kScrollView:
      return ROLE_SYSTEM_PANE;

    case ax::mojom::Role::kSearch:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kSlider:
      return ROLE_SYSTEM_SLIDER;

    case ax::mojom::Role::kSliderThumb:
      return ROLE_SYSTEM_SLIDER;

    case ax::mojom::Role::kSpinButton:
      return ROLE_SYSTEM_SPINBUTTON;

    case ax::mojom::Role::kSwitch:
      return ROLE_SYSTEM_CHECKBUTTON;

    case ax::mojom::Role::kRubyAnnotation:
    case ax::mojom::Role::kListMarker:
    case ax::mojom::Role::kStaticText:
      return ROLE_SYSTEM_STATICTEXT;

    case ax::mojom::Role::kStatus:
      return ROLE_SYSTEM_STATUSBAR;

    case ax::mojom::Role::kSplitter:
      return ROLE_SYSTEM_SEPARATOR;

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
    case ax::mojom::Role::kTime:
      return ROLE_SYSTEM_TEXT;

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

    case ax::mojom::Role::kIgnored:
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kPresentational:
    case ax::mojom::Role::kUnknown:
      return ROLE_SYSTEM_PANE;
  }

  NOTREACHED();
  return ROLE_SYSTEM_CLIENT;
}

std::string AXPlatformNodeWin::StringOverrideForMSAARole() {
  std::string html_tag =
      GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);

  switch (GetData().role) {
    case ax::mojom::Role::kBlockquote:
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kImageMap:
      return html_tag;

    case ax::mojom::Role::kCanvas:
      if (GetData().GetBoolAttribute(
              ax::mojom::BoolAttribute::kCanvasHasFallback)) {
        return html_tag;
      }
      break;

    case ax::mojom::Role::kForm:
      // This could be a div with the role of form
      // so we return just the string "form".
      return "form";

    case ax::mojom::Role::kHeading:
      if (!html_tag.empty())
        return html_tag;
      break;

    case ax::mojom::Role::kParagraph:
      return html_tag;

    case ax::mojom::Role::kLog:
      return "log";

    case ax::mojom::Role::kGenericContainer:
      // Use html tag if available. In the case where there is no tag, e.g.
      // for anonymous content inserted by blink, treat it as a "div". This
      // can occur if the markup had a block and inline element as siblings --
      // blink will wrap the inline with a block in this case.
      if (html_tag.empty())
        return "div";
      return html_tag;

    case ax::mojom::Role::kSwitch:
      return "switch";

    default:
      return "";
  }

  return "";
}

bool AXPlatformNodeWin::IsWebAreaForPresentationalIframe() {
  if (GetData().role != ax::mojom::Role::kWebArea &&
      GetData().role != ax::mojom::Role::kRootWebArea) {
    return false;
  }

  AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent());
  if (!parent)
    return false;

  return parent->GetData().role == ax::mojom::Role::kIframePresentational;
}

int32_t AXPlatformNodeWin::ComputeIA2State() {
  const AXNodeData& data = GetData();
  int32_t ia2_state = IA2_STATE_OPAQUE;

  if (HasIntAttribute(ax::mojom::IntAttribute::kCheckedState))
    ia2_state |= IA2_STATE_CHECKABLE;

  if (HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      GetIntAttribute(ax::mojom::IntAttribute::kInvalidState) !=
          static_cast<int32_t>(ax::mojom::InvalidState::kFalse))
    ia2_state |= IA2_STATE_INVALID_ENTRY;
  if (data.HasState(ax::mojom::State::kRequired))
    ia2_state |= IA2_STATE_REQUIRED;
  if (data.HasState(ax::mojom::State::kVertical))
    ia2_state |= IA2_STATE_VERTICAL;
  if (data.HasState(ax::mojom::State::kHorizontal))
    ia2_state |= IA2_STATE_HORIZONTAL;

  if (data.HasState(ax::mojom::State::kEditable))
    ia2_state |= IA2_STATE_EDITABLE;

  if (IsPlainTextField() || IsRichTextField()) {
    if (data.HasState(ax::mojom::State::kMultiline)) {
      ia2_state |= IA2_STATE_MULTI_LINE;
    } else {
      ia2_state |= IA2_STATE_SINGLE_LINE;
    }
    if (!IsInvisibleOrIgnored())
      ia2_state |= IA2_STATE_SELECTABLE_TEXT;
  }

  if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete).empty() ||
      data.HasState(ax::mojom::State::kAutofillAvailable)) {
    ia2_state |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;
  }

  if (GetBoolAttribute(ax::mojom::BoolAttribute::kModal))
    ia2_state |= IA2_STATE_MODAL;

  switch (data.role) {
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
  if (IsWebAreaForPresentationalIframe()) {
    return ROLE_SYSTEM_GROUPING;
  }

  int32_t ia2_role = 0;

  switch (GetData().role) {
    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kCommentSection:
    case ax::mojom::Role::kRevision:
    case ax::mojom::Role::kSuggestion:
      return IA2_ROLE_SECTION;
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      // CORE-AAM recommends LANDMARK instead of HEADER.
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kBlockquote:
      ia2_role = IA2_ROLE_SECTION;
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
    case ax::mojom::Role::kDefinition:
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ax::mojom::Role::kDescriptionListDetail:
      ia2_role = IA2_ROLE_PARAGRAPH;
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
      if (!GetDelegate()->GetChildCount()) {
        ia2_role = IA2_ROLE_EMBEDDED_OBJECT;
      }
      break;
    case ax::mojom::Role::kFigcaption:
      ia2_role = IA2_ROLE_CAPTION;
      break;
    case ax::mojom::Role::kFooterAsNonLandmark:
    case ax::mojom::Role::kHeaderAsNonLandmark:
      ia2_role = IA2_ROLE_SECTION;
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
    case ax::mojom::Role::kImageMap:
      ia2_role = IA2_ROLE_IMAGE_MAP;
      break;
    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kLegend:
      ia2_role = IA2_ROLE_LABEL;
      break;
    case ax::mojom::Role::kMain:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kMark:
      ia2_role = IA2_ROLE_TEXT_FRAME;
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
    case ax::mojom::Role::kPre:
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ax::mojom::Role::kRegion:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kRuby:
      ia2_role = IA2_ROLE_TEXT_FRAME;
      break;
    case ax::mojom::Role::kSearch:
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kSection: {
      if (GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        ia2_role = IA2_ROLE_SECTION;
      } else {
        // Use ARIA mapping.
        ia2_role = IA2_ROLE_LANDMARK;
      }
      break;
    }
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
    case ax::mojom::Role::kTime:
      ia2_role = IA2_ROLE_TEXT_FRAME;
      break;
    default:
      break;
  }
  return ia2_role;
}

std::vector<base::string16> AXPlatformNodeWin::ComputeIA2Attributes() {
  std::vector<base::string16> attribute_list;
  ComputeAttributes(&attribute_list);
  return attribute_list;
}

base::string16 AXPlatformNodeWin::UIAAriaRole() {
  // If this is a web area for a presentational iframe, give it a role of
  // something other than document so that the fact that it's a separate doc
  // is not exposed to AT.
  if (IsWebAreaForPresentationalIframe())
    return L"group";

  switch (GetData().role) {
    case ax::mojom::Role::kAlert:
      return L"alert";

    case ax::mojom::Role::kAlertDialog:
      // Our MSAA implementation suggests the use of
      // "alert", not "alertdialog" because some
      // Windows screen readers are not compatible with
      // |ax::mojom::Role::kAlertDialog| yet.
      return L"alert";

    case ax::mojom::Role::kAnchor:
      return L"link";

    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kCommentSection:
    case ax::mojom::Role::kRevision:
    case ax::mojom::Role::kSuggestion:
      return L"group";

    case ax::mojom::Role::kApplication:
      return L"application";

    case ax::mojom::Role::kArticle:
      return L"article";

    case ax::mojom::Role::kAudio:
      return L"group";

    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return L"banner";

    case ax::mojom::Role::kBlockquote:
      return L"group";

    case ax::mojom::Role::kButton:
      return L"button";

    case ax::mojom::Role::kCanvas:
      return L"img";

    case ax::mojom::Role::kCaption:
      return L"description";

    case ax::mojom::Role::kCaret:
      return L"region";

    case ax::mojom::Role::kCell:
      return L"gridcell";

    case ax::mojom::Role::kCode:
      return L"code";

    case ax::mojom::Role::kCheckBox:
      return L"checkbox";

    case ax::mojom::Role::kClient:
      return L"region";

    case ax::mojom::Role::kColorWell:
      return L"textbox";

    case ax::mojom::Role::kColumn:
      return L"region";

    case ax::mojom::Role::kColumnHeader:
      return L"columnheader";

    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
      return L"combobox";

    case ax::mojom::Role::kComplementary:
      return L"complementary";

    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
      return L"group";

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return L"contentinfo";

    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      return L"textbox";

    case ax::mojom::Role::kDefinition:
      return L"definition";

    case ax::mojom::Role::kDescriptionListDetail:
      return L"description";

    case ax::mojom::Role::kDescriptionList:
      return L"list";

    case ax::mojom::Role::kDescriptionListTerm:
      return L"listitem";

    case ax::mojom::Role::kDesktop:
      return L"document";

    case ax::mojom::Role::kDetails:
      return L"group";

    case ax::mojom::Role::kDialog:
      return L"dialog";

    case ax::mojom::Role::kDisclosureTriangle:
      return L"button";

    case ax::mojom::Role::kDirectory:
      return L"directory";

    case ax::mojom::Role::kDocCover:
      return L"img";

    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return L"link";

    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
    case ax::mojom::Role::kDocFootnote:
      return L"listitem";

    case ax::mojom::Role::kDocPageBreak:
      return L"separator";

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
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
    case ax::mojom::Role::kDocSubtitle:
    case ax::mojom::Role::kDocTip:
    case ax::mojom::Role::kDocToc:
      return L"group";

    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kWebArea:
      return L"document";

    case ax::mojom::Role::kEmbeddedObject:
      if (GetDelegate()->GetChildCount()) {
        return L"group";
      } else {
        return L"document";
      }

    case ax::mojom::Role::kEmphasis:
      return L"emphasis";

    case ax::mojom::Role::kFeed:
      return L"group";

    case ax::mojom::Role::kFigcaption:
      return L"description";

    case ax::mojom::Role::kFigure:
      return L"group";

    case ax::mojom::Role::kFooterAsNonLandmark:
    case ax::mojom::Role::kHeaderAsNonLandmark:
      return L"group";

    case ax::mojom::Role::kForm:
      return L"form";

    case ax::mojom::Role::kGenericContainer:
      return L"group";

    case ax::mojom::Role::kGraphicsDocument:
      return L"document";

    case ax::mojom::Role::kGraphicsObject:
      return L"region";

    case ax::mojom::Role::kGraphicsSymbol:
      return L"img";

    case ax::mojom::Role::kGrid:
      return L"grid";

    case ax::mojom::Role::kGroup:
      return L"group";

    case ax::mojom::Role::kHeading:
      return L"heading";

    case ax::mojom::Role::kIframe:
      return L"document";

    case ax::mojom::Role::kIframePresentational:
      return L"group";

    case ax::mojom::Role::kImage:
      return L"img";

    case ax::mojom::Role::kImageMap:
      return L"document";

    case ax::mojom::Role::kInputTime:
      return L"group";

    case ax::mojom::Role::kInlineTextBox:
      return L"textbox";

    case ax::mojom::Role::kKeyboard:
      return L"group";

    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kLegend:
      return L"description";

    case ax::mojom::Role::kLayoutTable:
      return L"grid";

    case ax::mojom::Role::kLayoutTableCell:
      return L"gridcell";

    case ax::mojom::Role::kLayoutTableColumn:
      return L"region";

    case ax::mojom::Role::kLayoutTableRow:
      return L"row";

    case ax::mojom::Role::kLink:
      return L"link";

    case ax::mojom::Role::kList:
      return L"list";

    case ax::mojom::Role::kListBox:
      return L"listbox";

    case ax::mojom::Role::kListBoxOption:
      return L"option";

    case ax::mojom::Role::kListGrid:
      return L"listview";

    case ax::mojom::Role::kListItem:
      return L"listitem";

    case ax::mojom::Role::kLog:
      return L"log";

    case ax::mojom::Role::kMain:
      return L"main";

    case ax::mojom::Role::kMark:
      return L"description";

    case ax::mojom::Role::kMarquee:
      return L"marquee";

    case ax::mojom::Role::kMath:
      return L"group";

    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuButton:
      return L"menu";

    case ax::mojom::Role::kMenuBar:
      return L"menubar";

    case ax::mojom::Role::kMenuItem:
      return L"menuitem";

    case ax::mojom::Role::kMenuItemCheckBox:
      return L"menuitemcheckbox";

    case ax::mojom::Role::kMenuItemRadio:
      return L"menuitemradio";

    case ax::mojom::Role::kMenuListPopup:
      if (IsAncestorComboBox())
        return L"list";
      return L"menu";

    case ax::mojom::Role::kMenuListOption:
      if (IsAncestorComboBox())
        return L"listitem";
      return L"menuitem";

    case ax::mojom::Role::kMeter:
      return L"progressbar";

    case ax::mojom::Role::kNavigation:
      return L"navigation";

    case ax::mojom::Role::kNote:
      return L"note";

    case ax::mojom::Role::kParagraph:
      return L"group";

    case ax::mojom::Role::kPopUpButton: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "select")
        return L"combobox";
      return L"menu";
    }

    case ax::mojom::Role::kPre:
      return L"region";

    case ax::mojom::Role::kProgressIndicator:
      return L"progressbar";

    case ax::mojom::Role::kRadioButton:
      return L"radio";

    case ax::mojom::Role::kRadioGroup:
      return L"radiogroup";

    case ax::mojom::Role::kRegion:
      return L"region";

    case ax::mojom::Role::kRow: {
      // Role changes depending on whether row is inside a treegrid
      // https://www.w3.org/TR/core-aam-1.1/#role-map-row
      return IsInTreeGrid() ? L"treeitem" : L"row";
    }

    case ax::mojom::Role::kRowHeader:
      return L"rowheader";

    case ax::mojom::Role::kRuby:
      return L"region";

    case ax::mojom::Role::kSection: {
      if (GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return L"group";
      }
      // Use ARIA mapping.
      return L"region";
    }

    case ax::mojom::Role::kScrollBar:
      return L"scrollbar";

    case ax::mojom::Role::kScrollView:
      return L"region";

    case ax::mojom::Role::kSearch:
      return L"search";

    case ax::mojom::Role::kSlider:
      return L"slider";

    case ax::mojom::Role::kSliderThumb:
      return L"slider";

    case ax::mojom::Role::kSpinButton:
      return L"spinbutton";

    case ax::mojom::Role::kStrong:
      return L"strong";

    case ax::mojom::Role::kSwitch:
      return L"switch";

    case ax::mojom::Role::kRubyAnnotation:
    case ax::mojom::Role::kListMarker:
    case ax::mojom::Role::kStaticText:
      return L"description";

    case ax::mojom::Role::kStatus:
      return L"status";

    case ax::mojom::Role::kSplitter:
      return L"separator";

    case ax::mojom::Role::kSvgRoot:
      return L"img";

    case ax::mojom::Role::kTab:
      return L"tab";

    case ax::mojom::Role::kTable:
      return L"grid";

    case ax::mojom::Role::kTableHeaderContainer:
      return L"group";

    case ax::mojom::Role::kTabList:
      return L"tablist";

    case ax::mojom::Role::kTabPanel:
      return L"tabpanel";

    case ax::mojom::Role::kTerm:
      return L"listitem";

    case ax::mojom::Role::kTitleBar:
      return L"document";

    case ax::mojom::Role::kToggleButton:
      return L"button";

    case ax::mojom::Role::kTextField:
      return L"textbox";

    case ax::mojom::Role::kSearchBox:
      return L"searchbox";

    case ax::mojom::Role::kTextFieldWithComboBox:
      return L"combobox";

    case ax::mojom::Role::kAbbr:
      return L"description";

    case ax::mojom::Role::kTime:
      return L"time";

    case ax::mojom::Role::kTimer:
      return L"timer";

    case ax::mojom::Role::kToolbar:
      return L"toolbar";

    case ax::mojom::Role::kTooltip:
      return L"tooltip";

    case ax::mojom::Role::kTree:
      return L"tree";

    case ax::mojom::Role::kTreeGrid:
      return L"treegrid";

    case ax::mojom::Role::kTreeItem:
      return L"treeitem";

    case ax::mojom::Role::kLineBreak:
      return L"separator";

    case ax::mojom::Role::kVideo:
      return L"group";

    case ax::mojom::Role::kWebView:
      return L"document";

    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kWindow:
    case ax::mojom::Role::kIgnored:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kPresentational:
    case ax::mojom::Role::kUnknown:
      return L"region";
  }

  NOTREACHED();
  return L"document";
}

base::string16 AXPlatformNodeWin::ComputeUIAProperties() {
  std::vector<base::string16> properties;
  const AXNodeData& data = GetData();

  BoolAttributeToUIAAriaProperty(
      properties, ax::mojom::BoolAttribute::kLiveAtomic, "atomic");
  BoolAttributeToUIAAriaProperty(properties, ax::mojom::BoolAttribute::kBusy,
                                 "busy");

  switch (data.GetCheckedState()) {
    case ax::mojom::CheckedState::kNone:
      break;
    case ax::mojom::CheckedState::kFalse:
      if (data.role == ax::mojom::Role::kToggleButton) {
        properties.emplace_back(L"pressed=false");
      } else if (data.role == ax::mojom::Role::kSwitch) {
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
      if (data.role == ax::mojom::Role::kToggleButton) {
        properties.emplace_back(L"pressed=true");
      } else if (data.role == ax::mojom::Role::kSwitch) {
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
      if (data.role == ax::mojom::Role::kToggleButton) {
        properties.emplace_back(L"pressed=mixed");
      } else if (data.role == ax::mojom::Role::kSwitch) {
        // This is disallowed both by the ARIA standard and by Blink.
        NOTREACHED();
      } else {
        properties.emplace_back(L"checked=mixed");
      }
      break;
  }

  const auto restriction = static_cast<ax::mojom::Restriction>(
      GetIntAttribute(ax::mojom::IntAttribute::kRestriction));
  switch (restriction) {
    case ax::mojom::Restriction::kDisabled:
      properties.push_back(L"disabled=true");
      break;
    case ax::mojom::Restriction::kReadOnly:
      properties.push_back(L"readonly=true");
      break;
    default:
      // The readonly property is complex on windows. We set "readonly=true"
      // on *some* document structure roles such as paragraph, heading or list
      // even if the node data isn't marked as read only, as long as the
      // node is not editable.
      if (!data.HasState(ax::mojom::State::kRichlyEditable) &&
          ShouldHaveReadonlyStateByDefault(data.role))
        properties.push_back(L"readonly=true");
      break;
  }

  // aria-dropeffect is deprecated in WAI-ARIA 1.1.
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kDropeffect)) {
    properties.push_back(L"dropeffect=" +
                         base::UTF8ToUTF16(data.DropeffectBitfieldToString()));
  }
  StateToUIAAriaProperty(properties, ax::mojom::State::kExpanded, "expanded");
  BoolAttributeToUIAAriaProperty(properties, ax::mojom::BoolAttribute::kGrabbed,
                                 "grabbed");

  switch (static_cast<ax::mojom::HasPopup>(
      data.GetIntAttribute(ax::mojom::IntAttribute::kHasPopup))) {
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
  if (IsTableHeader(data.role) &&
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

  if (IsRangeValueSupported(data)) {
    FloatAttributeToUIAAriaProperty(
        properties, ax::mojom::FloatAttribute::kMaxValueForRange, "valuemax");
    FloatAttributeToUIAAriaProperty(
        properties, ax::mojom::FloatAttribute::kMinValueForRange, "valuemin");
    StringAttributeToUIAAriaProperty(
        properties, ax::mojom::StringAttribute::kValue, "valuetext");

    base::string16 value_now = GetRangeValueText();
    SanitizeStringAttributeForUIAAriaProperty(value_now, &value_now);
    if (!value_now.empty())
      properties.push_back(L"valuenow=" + value_now);
  }

  base::string16 result = base::JoinString(properties, L";");
  return result;
}

LONG AXPlatformNodeWin::ComputeUIAControlType() {  // NOLINT(runtime/int)
  // If this is a web area for a presentational iframe, give it a role of
  // something other than document so that the fact that it's a separate doc
  // is not exposed to AT.
  if (IsWebAreaForPresentationalIframe())
    return UIA_GroupControlTypeId;

  switch (GetData().role) {
    case ax::mojom::Role::kAlert:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kAlertDialog:
      // Our MSAA implementation suggests the use of
      // |UIA_TextControlTypeId|, not |UIA_PaneControlTypeId| because some
      // Windows screen readers are not compatible with
      // |ax::mojom::Role::kAlertDialog| yet.
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kAnchor:
      return UIA_HyperlinkControlTypeId;

    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kCommentSection:
    case ax::mojom::Role::kRevision:
    case ax::mojom::Role::kSuggestion:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kApplication:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kArticle:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kAudio:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kBlockquote:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kButton:
      return UIA_ButtonControlTypeId;

    case ax::mojom::Role::kCanvas:
      return UIA_ImageControlTypeId;

    case ax::mojom::Role::kCaption:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kCaret:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kCell:
      return UIA_DataItemControlTypeId;

    case ax::mojom::Role::kCheckBox:
      return UIA_CheckBoxControlTypeId;

    case ax::mojom::Role::kClient:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kCode:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kColorWell:
      return UIA_ButtonControlTypeId;

    case ax::mojom::Role::kColumn:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kColumnHeader:
      return UIA_DataItemControlTypeId;

    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
      return UIA_ComboBoxControlTypeId;

    case ax::mojom::Role::kComplementary:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      return UIA_EditControlTypeId;

    case ax::mojom::Role::kDefinition:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kDescriptionListDetail:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kDescriptionList:
      return UIA_ListControlTypeId;

    case ax::mojom::Role::kDescriptionListTerm:
      return UIA_ListItemControlTypeId;

    case ax::mojom::Role::kDesktop:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kDetails:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kDialog:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kDisclosureTriangle:
      return UIA_ButtonControlTypeId;

    case ax::mojom::Role::kDirectory:
      return UIA_ListControlTypeId;

    case ax::mojom::Role::kDocCover:
      return UIA_ImageControlTypeId;

    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return UIA_HyperlinkControlTypeId;

    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
    case ax::mojom::Role::kDocFootnote:
      return UIA_ListItemControlTypeId;

    case ax::mojom::Role::kDocPageBreak:
      return UIA_SeparatorControlTypeId;

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
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
    case ax::mojom::Role::kDocSubtitle:
    case ax::mojom::Role::kDocTip:
    case ax::mojom::Role::kDocToc:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kWebArea:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kEmbeddedObject:
      if (GetDelegate()->GetChildCount()) {
        return UIA_GroupControlTypeId;
      } else {
        return UIA_DocumentControlTypeId;
      }

    case ax::mojom::Role::kEmphasis:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kFeed:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kFigcaption:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kFigure:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kFooterAsNonLandmark:
    case ax::mojom::Role::kHeaderAsNonLandmark:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kForm:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kGenericContainer:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kGraphicsDocument:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kGraphicsObject:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kGraphicsSymbol:
      return UIA_ImageControlTypeId;

    case ax::mojom::Role::kGrid:
      return UIA_DataGridControlTypeId;

    case ax::mojom::Role::kGroup:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kHeading:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kIframe:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kIframePresentational:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kImage:
      return UIA_ImageControlTypeId;

    case ax::mojom::Role::kImageMap:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kInputTime:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kInlineTextBox:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kLegend:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kLayoutTable:
      return UIA_TableControlTypeId;

    case ax::mojom::Role::kLayoutTableCell:
      return UIA_DataItemControlTypeId;

    case ax::mojom::Role::kLayoutTableColumn:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kLayoutTableRow:
      return UIA_DataItemControlTypeId;

    case ax::mojom::Role::kLink:
      return UIA_HyperlinkControlTypeId;

    case ax::mojom::Role::kList:
      return UIA_ListControlTypeId;

    case ax::mojom::Role::kListBox:
      return UIA_ListControlTypeId;

    case ax::mojom::Role::kListBoxOption:
      return UIA_ListItemControlTypeId;

    case ax::mojom::Role::kListGrid:
      return UIA_DataGridControlTypeId;

    case ax::mojom::Role::kListItem:
      return UIA_ListItemControlTypeId;

    case ax::mojom::Role::kLog:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kMain:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kMark:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kMarquee:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kMath:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuButton:
      return UIA_MenuControlTypeId;

    case ax::mojom::Role::kMenuBar:
      return UIA_MenuBarControlTypeId;

    case ax::mojom::Role::kMenuItem:
      return UIA_MenuItemControlTypeId;

    case ax::mojom::Role::kMenuItemCheckBox:
      return UIA_CheckBoxControlTypeId;

    case ax::mojom::Role::kMenuItemRadio:
      return UIA_RadioButtonControlTypeId;

    case ax::mojom::Role::kMenuListPopup:
      if (IsAncestorComboBox())
        return UIA_ListControlTypeId;
      return UIA_MenuControlTypeId;

    case ax::mojom::Role::kMenuListOption:
      if (IsAncestorComboBox())
        return UIA_ListItemControlTypeId;
      return UIA_MenuItemControlTypeId;

    case ax::mojom::Role::kMeter:
      return UIA_ProgressBarControlTypeId;

    case ax::mojom::Role::kNavigation:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kNote:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kParagraph:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kPopUpButton: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "select")
        return UIA_ComboBoxControlTypeId;
      return UIA_MenuControlTypeId;
    }

    case ax::mojom::Role::kPre:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kProgressIndicator:
      return UIA_ProgressBarControlTypeId;

    case ax::mojom::Role::kRadioButton:
      return UIA_RadioButtonControlTypeId;

    case ax::mojom::Role::kRadioGroup:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kRegion:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kRow: {
      // Role changes depending on whether row is inside a treegrid
      // https://www.w3.org/TR/core-aam-1.1/#role-map-row
      return IsInTreeGrid() ? UIA_TreeItemControlTypeId
                            : UIA_DataItemControlTypeId;
    }

    case ax::mojom::Role::kRowHeader:
      return UIA_DataItemControlTypeId;

    case ax::mojom::Role::kRuby:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kSection:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kScrollBar:
      return UIA_ScrollBarControlTypeId;

    case ax::mojom::Role::kScrollView:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kSearch:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kSlider:
      return UIA_SliderControlTypeId;

    case ax::mojom::Role::kSliderThumb:
      return UIA_SliderControlTypeId;

    case ax::mojom::Role::kSpinButton:
      return UIA_SpinnerControlTypeId;

    case ax::mojom::Role::kSwitch:
      return UIA_ButtonControlTypeId;

    case ax::mojom::Role::kRubyAnnotation:
    case ax::mojom::Role::kListMarker:
    case ax::mojom::Role::kStaticText:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kStatus:
      return UIA_StatusBarControlTypeId;

    case ax::mojom::Role::kStrong:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kSplitter:
      return UIA_SeparatorControlTypeId;

    case ax::mojom::Role::kSvgRoot:
      return UIA_ImageControlTypeId;

    case ax::mojom::Role::kTab:
      return UIA_TabItemControlTypeId;

    case ax::mojom::Role::kTable:
      return UIA_TableControlTypeId;

    case ax::mojom::Role::kTableHeaderContainer:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kTabList:
      return UIA_TabControlTypeId;

    case ax::mojom::Role::kTabPanel:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kTerm:
      return UIA_ListItemControlTypeId;

    case ax::mojom::Role::kTitleBar:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kToggleButton:
      return UIA_ButtonControlTypeId;

    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kSearchBox:
      return UIA_EditControlTypeId;

    case ax::mojom::Role::kTextFieldWithComboBox:
      return UIA_ComboBoxControlTypeId;

    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kTime:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kTimer:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kToolbar:
      return UIA_ToolBarControlTypeId;

    case ax::mojom::Role::kTooltip:
      return UIA_ToolTipControlTypeId;

    case ax::mojom::Role::kTree:
      return UIA_TreeControlTypeId;

    case ax::mojom::Role::kTreeGrid:
      return UIA_DataGridControlTypeId;

    case ax::mojom::Role::kTreeItem:
      return UIA_TreeItemControlTypeId;

    case ax::mojom::Role::kLineBreak:
      return UIA_SeparatorControlTypeId;

    case ax::mojom::Role::kVideo:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kWebView:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kWindow:
    case ax::mojom::Role::kIgnored:
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kPresentational:
    case ax::mojom::Role::kUnknown:
      return UIA_PaneControlTypeId;
  }

  NOTREACHED();
  return UIA_DocumentControlTypeId;
}

bool AXPlatformNodeWin::IsUIAControl() const {
  // UIA provides multiple "views": raw, content and control. We only want to
  // populate the content and control views with items that make sense to
  // traverse over.
  if (GetDelegate()->IsWebContent()) {
    if (IsTextOnlyObject()) {
      // A text leaf can be a UIAControl, but text inside of a heading, link,
      // button, etc. where the role allows the name to be generated from the
      // content is not. We want to avoid reading out a button, moving to the
      // next item, and then reading out the button's text child, causing the
      // text to be effectively repeated.
      auto* parent = FromNativeViewAccessible(GetDelegate()->GetParent());
      while (parent) {
        const ui::AXNodeData& data = parent->GetData();
        switch (data.role) {
          case ax::mojom::Role::kButton:
          case ax::mojom::Role::kCell:
          case ax::mojom::Role::kCheckBox:
          case ax::mojom::Role::kColumnHeader:
          case ax::mojom::Role::kGroup:
          case ax::mojom::Role::kHeading:
          case ax::mojom::Role::kLineBreak:
          case ax::mojom::Role::kLink:
          case ax::mojom::Role::kListBoxOption:
          case ax::mojom::Role::kListItem:
          case ax::mojom::Role::kMenuItem:
          case ax::mojom::Role::kMenuItemCheckBox:
          case ax::mojom::Role::kMenuItemRadio:
          case ax::mojom::Role::kMenuListOption:
          case ax::mojom::Role::kRadioButton:
          case ax::mojom::Role::kRow:
          case ax::mojom::Role::kRowHeader:
          case ax::mojom::Role::kStaticText:
          case ax::mojom::Role::kSwitch:
          case ax::mojom::Role::kTab:
          case ax::mojom::Role::kTooltip:
          case ax::mojom::Role::kTreeItem:
            return false;
          default:
            break;
        }
        parent = FromNativeViewAccessible(parent->GetParent());
      }
    }
    return true;
  }
  // non web-content case
  const ui::AXNodeData& data = GetData();
  return !(ui::ShouldHaveReadonlyStateByDefault(data.role) ||
           data.GetRestriction() == ax::mojom::Restriction::kReadOnly ||
           data.HasState(ax::mojom::State::kInvisible) ||
           data.role == ax::mojom::Role::kIgnored);
}

base::Optional<LONG> AXPlatformNodeWin::ComputeUIALandmarkType() const {
  const AXNodeData& data = GetData();
  switch (data.role) {
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kHeader:
      return UIA_CustomLandmarkTypeId;

    case ax::mojom::Role::kForm:
      return UIA_FormLandmarkTypeId;

    case ax::mojom::Role::kMain:
      return UIA_MainLandmarkTypeId;

    case ax::mojom::Role::kNavigation:
      return UIA_NavigationLandmarkTypeId;

    case ax::mojom::Role::kSearch:
      return UIA_SearchLandmarkTypeId;

    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kSection:
      if (data.HasStringAttribute(ax::mojom::StringAttribute::kName))
        return UIA_CustomLandmarkTypeId;
      FALLTHROUGH;

    default:
      return {};
  }
}

bool AXPlatformNodeWin::IsInaccessibleDueToAncestor() const {
  AXPlatformNodeWin* parent = static_cast<AXPlatformNodeWin*>(
      AXPlatformNode::FromNativeViewAccessible(GetParent()));
  while (parent) {
    if (parent->ShouldHideChildren())
      return true;
    parent = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(parent->GetParent()));
  }
  return false;
}

bool AXPlatformNodeWin::ShouldHideChildren() const {
  switch (GetData().role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kGraphicsSymbol:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
      return true;
    default:
      return false;
  }
}

base::string16 AXPlatformNodeWin::GetValue() const {
  base::string16 value = AXPlatformNodeBase::GetValue();

  // If this doesn't have a value and is linked then set its value to the URL
  // attribute. This allows screen readers to read an empty link's
  // destination.
  // TODO(dougt): Look into ensuring that on click handlers correctly provide
  // a value here.
  if (value.empty() && (MSAAState() & STATE_SYSTEM_LINKED))
    value = GetString16Attribute(ax::mojom::StringAttribute::kUrl);

  return value;
}

bool AXPlatformNodeWin::ShouldNodeHaveFocusableState(
    const AXNodeData& data) const {
  switch (data.role) {
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kGraphicsDocument:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kWebArea:
      return true;

    case ax::mojom::Role::kIframe:
      return false;

    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuListOption:
      if (data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
        return true;
      break;

    default:
      break;
  }

  return data.HasState(ax::mojom::State::kFocusable);
}

int AXPlatformNodeWin::MSAAState() const {
  const AXNodeData& data = GetData();
  int msaa_state = 0;

  // Map the ax::mojom::State to MSAA state. Note that some of the states are
  // not currently handled.

  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    msaa_state |= STATE_SYSTEM_BUSY;

  if (data.HasState(ax::mojom::State::kCollapsed))
    msaa_state |= STATE_SYSTEM_COLLAPSED;

  if (data.HasState(ax::mojom::State::kDefault))
    msaa_state |= STATE_SYSTEM_DEFAULT;

  // TODO(dougt) unhandled ux::ax::mojom::State::kEditable

  if (data.HasState(ax::mojom::State::kExpanded))
    msaa_state |= STATE_SYSTEM_EXPANDED;

  if (ShouldNodeHaveFocusableState(data))
    msaa_state |= STATE_SYSTEM_FOCUSABLE;

  if (data.HasIntAttribute(ax::mojom::IntAttribute::kHasPopup) ||
      data.HasState(ax::mojom::State::kAutofillAvailable))
    msaa_state |= STATE_SYSTEM_HASPOPUP;

  // TODO(dougt) unhandled ux::ax::mojom::State::kHorizontal

  if (data.HasState(ax::mojom::State::kHovered)) {
    // Expose whether or not the mouse is over an element, but suppress
    // this for tests because it can make the test results flaky depending
    // on the position of the mouse.
    if (GetDelegate()->ShouldIgnoreHoveredStateForTesting())
      msaa_state |= STATE_SYSTEM_HOTTRACKED;
  }

  // If the role is IGNORED, we want these elements to be invisible so that
  // these nodes are hidden from the screen reader.
  if (IsInvisibleOrIgnored())
    msaa_state |= STATE_SYSTEM_INVISIBLE;

  if (data.HasState(ax::mojom::State::kLinked))
    msaa_state |= STATE_SYSTEM_LINKED;

  // TODO(dougt) unhandled ux::ax::mojom::State::kMultiline

  if (data.HasState(ax::mojom::State::kMultiselectable)) {
    msaa_state |= STATE_SYSTEM_EXTSELECTABLE;
    msaa_state |= STATE_SYSTEM_MULTISELECTABLE;
  }

  if (GetDelegate()->IsOffscreen())
    msaa_state |= STATE_SYSTEM_OFFSCREEN;

  if (data.HasState(ax::mojom::State::kProtected))
    msaa_state |= STATE_SYSTEM_PROTECTED;

  // TODO(dougt) unhandled ux::ax::mojom::State::kRequired
  // TODO(dougt) unhandled ux::ax::mojom::State::kRichlyEditable

  if (data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    msaa_state |= STATE_SYSTEM_SELECTABLE;

  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    msaa_state |= STATE_SYSTEM_SELECTED;

  // TODO(dougt) unhandled VERTICAL

  if (data.HasState(ax::mojom::State::kVisited))
    msaa_state |= STATE_SYSTEM_TRAVERSED;

  //
  // Checked state
  //

  switch (data.GetCheckedState()) {
    case ax::mojom::CheckedState::kNone:
    case ax::mojom::CheckedState::kFalse:
      break;
    case ax::mojom::CheckedState::kTrue:
      if (data.role == ax::mojom::Role::kToggleButton) {
        msaa_state |= STATE_SYSTEM_PRESSED;
      } else if (data.role == ax::mojom::Role::kSwitch) {
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
      // READONLY state is complex on windows.  We set STATE_SYSTEM_READONLY
      // on *some* document structure roles such as paragraph, heading or list
      // even if the node data isn't marked as read only, as long as the
      // node is not editable.
      if (!data.HasState(ax::mojom::State::kRichlyEditable) &&
          ShouldHaveReadonlyStateByDefault(data.role))
        msaa_state |= STATE_SYSTEM_READONLY;
      break;
  }

  //
  // Handle STATE_SYSTEM_FOCUSED
  //
  gfx::NativeViewAccessible focus = GetDelegate()->GetFocus();
  if (focus == const_cast<AXPlatformNodeWin*>(this)->GetNativeViewAccessible())
    msaa_state |= STATE_SYSTEM_FOCUSED;

  // In focused single selection UI menus and listboxes, mirror item selection
  // to focus. This helps NVDA read the selected option as it changes.
  if ((data.role == ax::mojom::Role::kListBoxOption || IsMenuItem(data.role)) &&
      data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    AXPlatformNodeBase* container = FromNativeViewAccessible(GetParent());
    if (container && container->GetParent() == focus) {
      ui::AXNodeData container_data = container->GetData();
      if ((container_data.role == ax::mojom::Role::kListBox ||
           container_data.role == ax::mojom::Role::kMenu) &&
          !container_data.HasState(ax::mojom::State::kMultiselectable)) {
        msaa_state |= STATE_SYSTEM_FOCUSED;
      }
    }
  }

  // On Windows, the "focus" bit should be set on certain containers, like
  // menu bars, when visible.
  //
  // TODO(dmazzoni): this should probably check if focus is actually inside
  // the menu bar, but we don't currently track focus inside menu pop-ups,
  // and Chrome only has one menu visible at a time so this works for now.
  if (data.role == ax::mojom::Role::kMenuBar &&
      !(data.HasState(ax::mojom::State::kInvisible))) {
    msaa_state |= STATE_SYSTEM_FOCUSED;
  }

  // Handle STATE_SYSTEM_LINKED
  if (GetData().role == ax::mojom::Role::kLink)
    msaa_state |= STATE_SYSTEM_LINKED;

  // Special case for indeterminate progressbar.
  if (GetData().role == ax::mojom::Role::kProgressIndicator &&
      !HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange))
    msaa_state |= STATE_SYSTEM_MIXED;

  return msaa_state;
}

// static
base::Optional<DWORD> AXPlatformNodeWin::MojoEventToMSAAEvent(
    ax::mojom::Event event) {
  if (::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return base::nullopt;

  switch (event) {
    case ax::mojom::Event::kAlert:
      return EVENT_SYSTEM_ALERT;
    case ax::mojom::Event::kCheckedStateChanged:
    case ax::mojom::Event::kExpandedChanged:
      return EVENT_OBJECT_STATECHANGE;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
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
      return base::nullopt;
  }
}

// static
base::Optional<EVENTID> AXPlatformNodeWin::MojoEventToUIAEvent(
    ax::mojom::Event event) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return base::nullopt;

  switch (event) {
    case ax::mojom::Event::kAlert:
      return UIA_SystemAlertEventId;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
      return UIA_AutomationFocusChangedEventId;
    case ax::mojom::Event::kLiveRegionChanged:
      return UIA_LiveRegionChangedEventId;
    case ax::mojom::Event::kSelection:
      return UIA_SelectionItem_ElementSelectedEventId;
    case ax::mojom::Event::kSelectionAdd:
      return UIA_SelectionItem_ElementAddedToSelectionEventId;
    case ax::mojom::Event::kSelectionRemove:
      return UIA_SelectionItem_ElementRemovedFromSelectionEventId;
    case ax::mojom::Event::kTooltipClosed:
      return UIA_ToolTipClosedEventId;
    case ax::mojom::Event::kTooltipOpened:
      return UIA_ToolTipOpenedEventId;
    default:
      return base::nullopt;
  }
}

// static
base::Optional<PROPERTYID> AXPlatformNodeWin::MojoEventToUIAProperty(
    ax::mojom::Event event) {
  if (!::switches::IsExperimentalAccessibilityPlatformUIAEnabled())
    return base::nullopt;

  switch (event) {
    case ax::mojom::Event::kControlsChanged:
      return UIA_ControllerForPropertyId;
    case ax::mojom::Event::kCheckedStateChanged:
      return UIA_ToggleToggleStatePropertyId;
    default:
      return base::nullopt;
  }
}

// static
BSTR AXPlatformNodeWin::GetValueAttributeAsBstr(AXPlatformNodeWin* target) {
  // GetValueAttributeAsBstr() has two sets of special cases depending on the
  // node's role.
  // The first set apply without regard for the nodes |value| attribute. That is
  // the nodes value attribute isn't consider for the first set of special
  // cases. For example, if the node role is ax::mojom::Role::kColorWell, we do
  // not care at all about the node's ax::mojom::StringAttribute::kValue
  // attribute. The second set of special cases only apply if the value
  // attribute for the node is empty.  That is, if
  // ax::mojom::StringAttribute::kValue is empty, we do something special.
  base::string16 result;

  //
  // Color Well special case (Use ax::mojom::IntAttribute::kColorValue)
  //
  if (target->GetData().role == ax::mojom::Role::kColorWell) {
    // static cast because SkColor is a 4-byte unsigned int
    unsigned int color = static_cast<unsigned int>(
        target->GetIntAttribute(ax::mojom::IntAttribute::kColorValue));

    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    base::string16 value_text;
    value_text = base::NumberToString16(red * 100 / 255) + L"% red " +
                 base::NumberToString16(green * 100 / 255) + L"% green " +
                 base::NumberToString16(blue * 100 / 255) + L"% blue";
    BSTR value = SysAllocString(value_text.c_str());
    DCHECK(value);
    return value;
  }

  //
  // Document special case (Use the document's URL)
  //
  if (target->GetData().role == ax::mojom::Role::kRootWebArea ||
      target->GetData().role == ax::mojom::Role::kWebArea) {
    result = base::UTF8ToUTF16(target->GetDelegate()->GetTreeData().url);
    BSTR value = SysAllocString(result.c_str());
    DCHECK(value);
    return value;
  }

  //
  // Links (Use ax::mojom::StringAttribute::kUrl)
  //
  if (target->GetData().role == ax::mojom::Role::kLink) {
    result = target->GetString16Attribute(ax::mojom::StringAttribute::kUrl);
    BSTR value = SysAllocString(result.c_str());
    DCHECK(value);
    return value;
  }

  // For range controls, e.g. sliders and spin buttons, |ax_attr_value| holds
  // the aria-valuetext if present but not the inner text. The actual value,
  // provided either via aria-valuenow or the actual control's value is held in
  // |ax::mojom::FloatAttribute::kValueForRange|.
  result = target->GetString16Attribute(ax::mojom::StringAttribute::kValue);
  if (result.empty() && IsRangeValueSupported(target->GetData())) {
    float fval;
    if (target->GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                  &fval)) {
      result = base::NumberToString16(fval);
      BSTR value = SysAllocString(result.c_str());
      DCHECK(value);
      return value;
    }
  }

  if (result.empty() && target->IsRichTextField())
    result = target->GetInnerText();

  BSTR value = SysAllocString(result.c_str());
  DCHECK(value);
  return value;
}

HRESULT AXPlatformNodeWin::GetStringAttributeAsBstr(
    ax::mojom::StringAttribute attribute,
    BSTR* value_bstr) const {
  base::string16 str;

  if (!GetString16Attribute(attribute, &str))
    return S_FALSE;

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

base::string16 AXPlatformNodeWin::TextForIAccessibleText() {
  // Special case allows us to get text even in non-HTML case, e.g. browser
  // UI.
  if (IsPlainTextField())
    return GetString16Attribute(ax::mojom::StringAttribute::kValue);
  return GetHypertext();
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

LONG AXPlatformNodeWin::FindBoundary(const base::string16& text,
                                     IA2TextBoundaryType ia2_boundary,
                                     LONG start_offset,
                                     AXTextBoundaryDirection direction) {
  HandleSpecialTextOffset(&start_offset);
  AXTextBoundary boundary = FromIA2TextBoundary(ia2_boundary);
  std::vector<int32_t> line_breaks;
  return static_cast<LONG>(FindAccessibleTextBoundary(
      text, line_breaks, boundary, start_offset, direction,
      ax::mojom::TextAffinity::kDownstream));
}

AXPlatformNodeWin* AXPlatformNodeWin::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (V_VT(&var_id) != VT_I4)
    return nullptr;

  LONG child_id = V_I4(&var_id);
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 && child_id <= GetDelegate()->GetChildCount()) {
    // Positive child ids are a 1-based child index, used by clients
    // that want to enumerate all immediate children.
    AXPlatformNodeBase* base =
        FromNativeViewAccessible(GetDelegate()->ChildAtIndex(child_id - 1));
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
  if (container && container->GetData().role == ax::mojom::Role::kGroup)
    container = FromNativeViewAccessible(container->GetParent());

  if (!container)
    return false;

  return container->GetData().role == ax::mojom::Role::kTreeGrid;
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

// TODO(dmazzoni): Remove this function once combo box refactoring is
// complete.
bool AXPlatformNodeWin::IsAncestorComboBox() {
  auto* parent =
      static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(GetParent()));
  if (!parent)
    return false;
  if (parent->MSAARole() == ROLE_SYSTEM_COMBOBOX)
    return true;
  return parent->IsAncestorComboBox();
}

bool AXPlatformNodeWin::IsPlaceholderText() const {
  if (GetData().role != ax::mojom::Role::kStaticText)
    return false;
  AXPlatformNodeWin* parent =
      static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(GetParent()));
  // Static text nodes are always expected to have a parent.
  DCHECK(parent);
  return (parent->IsPlainTextField() || parent->IsRichTextField()) &&
         parent->HasStringAttribute(ax::mojom::StringAttribute::kPlaceholder);
}

bool AXPlatformNodeWin::IsHyperlink() {
  int32_t hyperlink_index = -1;
  AXPlatformNodeWin* parent =
      static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(GetParent()));
  if (parent) {
    hyperlink_index = parent->GetHyperlinkIndexFromChild(this);
  }

  if (hyperlink_index >= 0)
    return true;
  return false;
}

void AXPlatformNodeWin::ComputeHypertextRemovedAndInserted(size_t* start,
                                                           size_t* old_len,
                                                           size_t* new_len) {
  AXPlatformNodeBase::ComputeHypertextRemovedAndInserted(old_hypertext_, start,
                                                         old_len, new_len);
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
  const base::string16 string =
      GetInheritedString16Attribute(ax::mojom::StringAttribute::kFontFamily);

  return SysAllocString(string.c_str());
}

BSTR AXPlatformNodeWin::GetStyleNameAttributeAsBSTR() const {
  base::string16 style_name =
      GetDelegate()->GetStyleNameAttributeAsLocalizedString();

  return SysAllocString(style_name.c_str());
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
  const AXNodeData& data = GetData();

  switch (pattern_id) {
    case UIA_ExpandCollapsePatternId:
      if (SupportsExpandCollapse(data)) {
        return &PatternProvider<IExpandCollapseProvider>;
      }
      break;

    case UIA_GridPatternId:
      if (IsTableLike(data.role)) {
        return &PatternProvider<IGridProvider>;
      }
      break;

    case UIA_GridItemPatternId:
      if (IsCellOrTableHeader(data.role)) {
        return &PatternProvider<IGridItemProvider>;
      }
      break;

    case UIA_InvokePatternId:
      if (IsInvokable(data)) {
        return &PatternProvider<IInvokeProvider>;
      }
      break;

    case UIA_RangeValuePatternId:
      if (IsRangeValueSupported(data)) {
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
      break;

    case UIA_SelectionItemPatternId:
      if (IsSelectionItemSupported()) {
        return &PatternProvider<ISelectionItemProvider>;
      }
      break;

    case UIA_SelectionPatternId:
      if (IsContainerWithSelectableChildren(data.role)) {
        return &PatternProvider<ISelectionProvider>;
      }
      break;

    case UIA_TablePatternId:
      if (IsTableLike(data.role)) {
        return &PatternProvider<ITableProvider>;
      }
      break;

    case UIA_TableItemPatternId:
      if (IsCellOrTableHeader(data.role)) {
        return &PatternProvider<ITableItemProvider>;
      }
      break;

    case UIA_TextChildPatternId:
      if (AXPlatformNodeTextChildProviderWin::GetTextContainer(this)) {
        return &AXPlatformNodeTextChildProviderWin::CreateIUnknown;
      }
      break;

    case UIA_TextEditPatternId:
    case UIA_TextPatternId:
      if (IsTextOnlyObject() || IsDocument() ||
          HasBoolAttribute(ax::mojom::BoolAttribute::kEditableRoot)) {
        return &AXPlatformNodeTextProviderWin::CreateIUnknown;
      }
      break;

    case UIA_TogglePatternId:
      if (SupportsToggle(data.role)) {
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
    case UIA_AnnotationPatternId:
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

void AXPlatformNodeWin::FireLiveRegionChangeRecursive() {
  const auto live_status_attr = ax::mojom::StringAttribute::kLiveStatus;
  if (HasStringAttribute(live_status_attr) &&
      GetStringAttribute(live_status_attr) != "off") {
    DCHECK(GetDelegate()->IsWebContent());
    ::UiaRaiseAutomationEvent(this, UIA_LiveRegionChangedEventId);
    return;
  }

  for (int index = 0; index < GetChildCount(); ++index) {
    auto* child = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(ChildAtIndex(index)));

    // We assume that only web-content will have live regions; also because
    // this will be called on each fragment-root, there is no need to walk
    // through non-content nodes.
    if (child->GetDelegate()->IsWebContent())
      child->FireLiveRegionChangeRecursive();
  }
}

AXPlatformNodeWin* AXPlatformNodeWin::GetLowestAccessibleElement() {
  if (!IsInaccessibleDueToAncestor())
    return this;

  AXPlatformNodeWin* parent = static_cast<AXPlatformNodeWin*>(
      AXPlatformNode::FromNativeViewAccessible(GetParent()));
  while (parent) {
    if (parent->ShouldHideChildren())
      return parent;
    parent = static_cast<AXPlatformNodeWin*>(
        AXPlatformNode::FromNativeViewAccessible(parent->GetParent()));
  }

  NOTREACHED();
  return nullptr;
}

void AXPlatformNodeWin::SanitizeTextAttributeValue(const std::string& input,
                                                   std::string* output) const {
  SanitizeStringAttributeForIA2(input, output);
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

}  // namespace ui
