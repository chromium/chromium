// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_win.h"

#include <wrl/client.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/enum_variant.h"
#include "base/win/scoped_variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_relation_win.h"
#include "ui/base/win/atl_module.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

//
// Macros to use at the top of any AXPlatformNodeWin function that implements
// a COM interface. Because COM objects are reference counted and clients
// are completely untrusted, it's important to always first check that our
// object is still valid, and then check that all pointer arguments are
// not NULL.
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

typedef base::hash_set<AXPlatformNodeWin*> AXPlatformNodeWinSet;
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

IAccessible2UsageObserver::IAccessible2UsageObserver() {
}

IAccessible2UsageObserver::~IAccessible2UsageObserver() {
}

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

using UniqueIdMap = base::hash_map<int32_t, AXPlatformNode*>;
// Map from each AXPlatformNode's unique id to its instance.
base::LazyInstance<UniqueIdMap>::Leaky g_unique_id_map =
    LAZY_INSTANCE_INITIALIZER;

// static
AXPlatformNode* AXPlatformNodeWin::GetFromUniqueId(int32_t unique_id) {
  UniqueIdMap* unique_ids = g_unique_id_map.Pointer();
  auto iter = unique_ids->find(unique_id);
  if (iter != unique_ids->end())
    return iter->second;

  return nullptr;
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
  g_unique_id_map.Get()[GetUniqueId()] = this;
}

// static
size_t AXPlatformNodeWin::GetInstanceCountForTesting() {
  return g_unique_id_map.Get().size();
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
                         base::IntToString16(value));
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

SAFEARRAY* AXPlatformNodeWin::CreateUIAElementsArrayForRelation(
    const ax::mojom::IntListAttribute& attribute) {
  std::vector<int32_t> id_list = GetIntListAttribute(attribute);
  SAFEARRAY* propertyvalue = CreateUIAElementsArrayFromIdVector(id_list);
  return propertyvalue;
}

SAFEARRAY* AXPlatformNodeWin::CreateUIAElementsArrayFromIdVector(
    std::vector<int32_t>& ids) {
  SAFEARRAY* uia_array = SafeArrayCreateVector(VT_UNKNOWN, 0, ids.size());

  LONG i = 0;
  for (const auto& node_id : ids) {
    AXPlatformNodeWin* node_win =
        static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(node_id));
    DCHECK(node_win);
    node_win->AddRef();
    SafeArrayPutElement(uia_array, &i,
                        static_cast<IRawElementProviderSimple*>(node_win));
    ++i;
  }

  return uia_array;
}

gfx::Vector2d AXPlatformNodeWin::CalculateUIAScrollPoint(
    const ScrollAmount horizontal_amount,
    const ScrollAmount vertical_amount) const {
  if (!GetDelegate() || !IsScrollable())
    return {};

  const gfx::Rect bounds = GetDelegate()->GetClippedScreenBoundsRect();
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
  g_unique_id_map.Get().erase(GetUniqueId());

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
  HWND hwnd = GetDelegate()->GetTargetForNativeAccessibilityEvent();
  if (!hwnd)
    return;

  // Menu items fire selection events but Windows screen readers work reliably
  // with focus events. Remap here.
  if (event_type == ax::mojom::Event::kSelection) {
    // A menu item could have something other than a role of
    // |ROLE_SYSTEM_MENUITEM|. Zoom modification controls for example have a
    // role of button.
    auto* parent =
        static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(GetParent()));
    if (MSAARole() == ROLE_SYSTEM_MENUITEM ||
        (parent && parent->MSAARole() == ROLE_SYSTEM_MENUPOPUP)) {
      event_type = ax::mojom::Event::kFocus;
    }
  }

  int native_event = MSAAEvent(event_type);
  if (native_event < EVENT_MIN)
    return;

  ::NotifyWinEvent(native_event, hwnd, OBJID_CLIENT, -GetUniqueId());

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

base::string16 AXPlatformNodeWin::GetTextAsString16() {
  if (IsChildOfLeaf())
    return base::UTF8ToUTF16(AXPlatformNodeBase::GetText());

  return hypertext_.hypertext;
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
  if (!GetDelegate()->GetClippedScreenBoundsRect().Contains(point)) {
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

  gfx::Rect bounds = target->GetDelegate()->GetUnclippedScreenBoundsRect();
  *x_left = bounds.x();
  *y_top = bounds.y();
  *width  = bounds.width();
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
        result = GetDelegate()->ChildAtIndex(0);
      break;

    case NAVDIR_LASTCHILD:
      if (GetDelegate()->GetChildCount() > 0)
        result =
            GetDelegate()->ChildAtIndex(GetDelegate()->GetChildCount() - 1);
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
      if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
        return E_NOTIMPL;

      AXPlatformNodeBase* next = target->GetTableCell(
          GetTableRow() + GetTableRowSpan(), GetTableColumn());
      if (!next)
        return S_OK;

      result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_UP: {
      // This direction is not implemented except in tables.
      if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
        return E_NOTIMPL;

      AXPlatformNodeBase* next =
          target->GetTableCell(GetTableRow() - 1, GetTableColumn());
      if (!next)
        return S_OK;

      result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_LEFT: {
      // This direction is not implemented except in tables.
      if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
        return E_NOTIMPL;

      AXPlatformNodeBase* next =
          target->GetTableCell(GetTableRow(), GetTableColumn() - 1);
      if (!next)
        return S_OK;

      result = next->GetNativeViewAccessible();
      break;
    }

    case NAVDIR_RIGHT: {
      // This direction is not implemented except in tables.

      if (!IsTableLike(GetData().role) && !IsCellOrTableHeader(GetData().role))
        return E_NOTIMPL;

      AXPlatformNodeBase* next = target->GetTableCell(
          GetTableRow(), GetTableColumn() + GetTableColumnSpan());
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
    focus_child->vt = VT_DISPATCH;
    focus_child->pdispVal = focus_accessible;
    focus_child->pdispVal->AddRef();
    return S_OK;
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

IFACEMETHODIMP AXPlatformNodeWin::get_accName(VARIANT var_id, BSTR* name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ACC_NAME);
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_1_ARG_AND_GET_TARGET(var_id, name, target);

  for (IAccessible2UsageObserver& observer :
       GetIAccessible2UsageObserverList()) {
    observer.OnAccNameCalled();
  }

  // Ignored items are also marked invisible, but NVDA was not actually ignoring
  // them.
  // TODO(accessibility) Find a way to not expose ignored items at all, which
  // would be less hacky but more code. Using a nameless object is a workaround,
  // although it does not currently cause any known user-facing issues.
  if (target->GetData().role == ax::mojom::Role::kIgnored) {
    *name = nullptr;
    return S_FALSE;
  }

  HRESULT result =
      target->GetStringAttributeAsBstr(ax::mojom::StringAttribute::kName, name);
  if (FAILED(result) && MSAARole() == ROLE_SYSTEM_DOCUMENT && GetParent()) {
    // Hack: Some versions of JAWS crash if they get an empty name on
    // a document that's the child of an iframe, so always return a
    // nonempty string for this role.  https://crbug.com/583057
    base::string16 str = L" ";

    *name = SysAllocString(str.c_str());
    DCHECK(*name);
  }

  return result;
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

  // get_accValue() has two sets of special cases depending on the node's role.
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
    unsigned int color = static_cast<unsigned int>(target->GetIntAttribute(
        ax::mojom::IntAttribute::kColorValue));  // todo, why the static cast?

    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    base::string16 value_text;
    value_text = base::UintToString16(red * 100 / 255) + L"% red " +
                 base::UintToString16(green * 100 / 255) + L"% green " +
                 base::UintToString16(blue * 100 / 255) + L"% blue";
    *value = SysAllocString(value_text.c_str());
    DCHECK(*value);
    return S_OK;
  }

  //
  // Document special case (Use the document's URL)
  //
  if (target->GetData().role == ax::mojom::Role::kRootWebArea ||
      target->GetData().role == ax::mojom::Role::kWebArea) {
    result = base::UTF8ToUTF16(target->GetDelegate()->GetTreeData().url);
    *value = SysAllocString(result.c_str());
    DCHECK(*value);
    return S_OK;
  }

  //
  // Links (Use ax::mojom::StringAttribute::kUrl)
  //
  if (target->GetData().role == ax::mojom::Role::kLink) {
    result = target->GetString16Attribute(ax::mojom::StringAttribute::kUrl);
    *value = SysAllocString(result.c_str());
    DCHECK(*value);
    return S_OK;
  }

  // For range controls, e.g. sliders and spin buttons, |ax_attr_value| holds
  // the aria-valuetext if present but not the inner text. The actual value,
  // provided either via aria-valuenow or the actual control's value is held in
  // |ax::mojom::FloatAttribute::kValueForRange|.
  result = target->GetString16Attribute(ax::mojom::StringAttribute::kValue);
  if (result.empty() && target->IsRangeValueSupported()) {
    float fval;
    if (target->GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                  &fval)) {
      result = base::NumberToString16(fval);
      *value = SysAllocString(result.c_str());
      DCHECK(*value);
      return S_OK;
    }
  }

  if (result.empty() && target->IsRichTextField())
    result = base::UTF8ToUTF16(target->GetInnerText());

  *value = SysAllocString(result.c_str());
  DCHECK(*value);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::put_accValue(VARIANT var_id, BSTR new_value) {
  AXPlatformNodeWin* target;
  COM_OBJECT_VALIDATE_VAR_ID_AND_GET_TARGET(var_id, target);

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
        node->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
      selected_nodes.emplace_back(node);
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
      auto* enum_variant = new base::win::EnumVariant(selected_count);
      enum_variant->AddRef();
      for (LONG i = 0; i < selected_count; ++i) {
        enum_variant->ItemAt(i)->vt = VT_DISPATCH;
        enum_variant->ItemAt(i)->pdispVal = selected_nodes[i].Detach();
  }
  selected->vt = VT_UNKNOWN;
  HRESULT hr = enum_variant->QueryInterface(IID_PPV_ARGS(&V_UNKNOWN(selected)));
  enum_variant->Release();
  return hr;
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
  std::set<int32_t> target_ids;
  int found = AXPlatformRelationWin::EnumerateRelationships(
      GetData(), GetDelegate(), 0, type, &relation_type, &target_ids);
  if (found == 0)
    return S_FALSE;

  // Don't return more targets than max_targets - but note that the caller
  // is allowed to specify max_targets=0 to mean no limit.
  int count = static_cast<int>(target_ids.size());
  if (max_targets > 0 && count > max_targets)
    count = max_targets;

  // Allocate COM memory for the result array and populate it.
  *targets = static_cast<IUnknown**>(CoTaskMemAlloc(count * sizeof(IUnknown*)));
  int index = 0;
  for (int target_id : target_ids) {
    AXPlatformNodeWin* target = static_cast<AXPlatformNodeWin*>(
        GetDelegate()->GetFromNodeID(target_id));
    if (target) {
      (*targets)[index] = static_cast<IAccessible*>(target);
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
      GetData(), GetDelegate(), -1, base::string16(), nullptr, nullptr);
  *n_relations = count;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_relation(LONG relation_index,
                                               IAccessibleRelation** relation) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_RELATION);
  COM_OBJECT_VALIDATE_1_ARG(relation);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  base::string16 relation_type;
  std::set<int32_t> targets;
  int found = AXPlatformRelationWin::EnumerateRelationships(
      GetData(), GetDelegate(), relation_index, base::string16(),
      &relation_type, &targets);
  if (found == 0)
    return E_INVALIDARG;

  CComObject<AXPlatformRelationWin>* relation_obj;
  HRESULT hr = CComObject<AXPlatformRelationWin>::CreateInstance(&relation_obj);
  DCHECK(SUCCEEDED(hr));
  relation_obj->AddRef();
  relation_obj->Initialize(relation_type);
  for (int target_id : targets) {
    AXPlatformNodeWin* target = static_cast<AXPlatformNodeWin*>(
        GetDelegate()->GetFromNodeID(target_id));
    if (!target)
      continue;
    relation_obj->AddTarget(target);
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
  *similar_items_in_group = GetIntAttribute(ax::mojom::IntAttribute::kSetSize);
  *position_in_group = GetIntAttribute(ax::mojom::IntAttribute::kPosInSet);

  if (!*group_level && !*similar_items_in_group && !*position_in_group)
    return S_FALSE;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_localizedExtendedRole(
    BSTR* localized_extended_role) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALIZED_EXTENDED_ROLE);
  COM_OBJECT_VALIDATE_1_ARG(localized_extended_role);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  return GetStringAttributeAsBstr(ax::mojom::StringAttribute::kRoleDescription,
                                  localized_extended_role);
}

//
// IAccessible2 methods not implemented.
//

IFACEMETHODIMP AXPlatformNodeWin::get_attribute(BSTR name, VARIANT* attribute) {
  return E_NOTIMPL;
}
IFACEMETHODIMP AXPlatformNodeWin::get_extendedRole(BSTR* extended_role) {
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::scrollTo(enum IA2ScrollType scroll_type) {
  COM_OBJECT_VALIDATE();
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IA2_SCROLL_TO);

  // ax::mojom::Action::kScrollToMakeVisible wants a target rect in *local*
  // coords.
  gfx::Rect r = gfx::ToEnclosingRect(GetData().location);
  r -= r.OffsetFromOrigin();
  switch (scroll_type) {
    case IA2_SCROLL_TYPE_TOP_LEFT:
      r = gfx::Rect(r.x(), r.y(), 0, 0);
      break;
    case IA2_SCROLL_TYPE_BOTTOM_RIGHT:
      r = gfx::Rect(r.right(), r.bottom(), 0, 0);
      break;
    case IA2_SCROLL_TYPE_TOP_EDGE:
      r = gfx::Rect(r.x(), r.y(), r.width(), 0);
      break;
    case IA2_SCROLL_TYPE_BOTTOM_EDGE:
      r = gfx::Rect(r.x(), r.bottom(), r.width(), 0);
      break;
    case IA2_SCROLL_TYPE_LEFT_EDGE:
      r = gfx::Rect(r.x(), r.y(), 0, r.height());
      break;
    case IA2_SCROLL_TYPE_RIGHT_EDGE:
      r = gfx::Rect(r.right(), r.y(), 0, r.height());
      break;
    case IA2_SCROLL_TYPE_ANYWHERE:
      break;
  }

  ui::AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_rect = r;
  GetDelegate()->AccessibilityPerformAction(action_data);
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
                       ->GetUnclippedScreenBoundsRect()
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
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;

  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::Expand() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;

  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ExpandCollapseState(
    ExpandCollapseState* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  const AXNodeData& data = GetData();
  if (data.HasState(ax::mojom::State::kExpanded)) {
    *result = ExpandCollapseState_Expanded;
  } else if (data.HasState(ax::mojom::State::kCollapsed)) {
    *result = ExpandCollapseState_Collapsed;
  } else {
    NOTREACHED();
    return E_FAIL;
  }
  return S_OK;
}

//
// IGridItemProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::get_Column(int* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = GetTableColumn();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ColumnSpan(int* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = GetTableColumnSpan();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ContainingGrid(
    IRawElementProviderSimple** result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return E_FAIL;

  auto* node_win = static_cast<AXPlatformNodeWin*>(table);
  node_win->AddRef();
  *result = static_cast<IRawElementProviderSimple*>(node_win);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Row(int* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = GetTableRow();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowSpan(int* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = GetTableRowSpan();
  return S_OK;
}

//
// IGridProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::GetItem(int row,
                                          int column,
                                          IRawElementProviderSimple** result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

  AXPlatformNodeBase* cell = GetTableCell(row, column);
  if (!cell)
    return E_INVALIDARG;

  auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
  node_win->AddRef();
  *result = static_cast<IRawElementProviderSimple*>(node_win);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowCount(int* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = GetTableRowCount();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ColumnCount(int* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = GetTableColumnCount();
  return S_OK;
}

//
// IScrollItemProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::ScrollIntoView() {
  COM_OBJECT_VALIDATE();
  gfx::Rect r = gfx::ToEnclosingRect(GetData().location);
  r -= r.OffsetFromOrigin();

  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.target_rect = r;
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
  COM_OBJECT_VALIDATE();
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
  COM_OBJECT_VALIDATE();
  if (!IsScrollable())
    return E_FAIL;

  const double x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  const double x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  const double y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  const double y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
  const int x = gfx::ToRoundedInt(horizontal_percent * (x_max - x_min) + x_min);
  const int y = gfx::ToRoundedInt(vertical_percent * (y_max - y_min) + y_min);
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
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = IsHorizontallyScrollable();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_HorizontalScrollPercent(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  if (!IsHorizontallyScrollable()) {
    *result = UIA_ScrollPatternNoScroll;
    return S_OK;
  }

  float x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  float x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  float x = GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  *result = 100.0 * (x - x_min) / (x_max - x_min);
  return S_OK;
}

// Horizontal size of the viewable region as a percentage of the total content
// area.
IFACEMETHODIMP AXPlatformNodeWin::get_HorizontalViewSize(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  if (!IsHorizontallyScrollable()) {
    *result = 100.;
    return S_OK;
  }

  gfx::RectF clipped_bounds(GetDelegate()->GetClippedScreenBoundsRect());
  float x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  float x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  float total_width = clipped_bounds.width() + x_max - x_min;
  DCHECK_LE(clipped_bounds.width(), total_width);
  *result = 100.0 * clipped_bounds.width() / total_width;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_VerticallyScrollable(BOOL* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = IsVerticallyScrollable();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_VerticalScrollPercent(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  if (!IsVerticallyScrollable()) {
    *result = UIA_ScrollPatternNoScroll;
    return S_OK;
  }

  float y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  float y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
  float y = GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
  *result = 100.0 * (y - y_min) / (y_max - y_min);
  return S_OK;
}

// Vertical size of the viewable region as a percentage of the total content
// area.
IFACEMETHODIMP AXPlatformNodeWin::get_VerticalViewSize(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  if (!IsVerticallyScrollable()) {
    *result = 100.0;
    return S_OK;
  }

  gfx::RectF clipped_bounds(GetDelegate()->GetClippedScreenBoundsRect());
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

IFACEMETHODIMP AXPlatformNodeWin::AddToSelection() {
  if (!IsUIASelectable(GetData().role))
    return E_FAIL;

  bool selected;
  if (!GetBoolAttribute(ax::mojom::BoolAttribute::kSelected, &selected))
    return E_FAIL;
  if (selected)
    return S_OK;

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::RemoveFromSelection() {
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::Select() {
  if (!IsUIASelectable(GetData().role))
    return E_FAIL;

  bool selected;
  if (!GetBoolAttribute(ax::mojom::BoolAttribute::kSelected, &selected))
    return E_FAIL;
  if (selected)
    return S_OK;

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsSelected(BOOL* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

  if (!IsUIASelectable(GetData().role))
    return E_FAIL;

  bool selected;
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kSelected, &selected)) {
    *result = selected;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_SelectionContainer(
    IRawElementProviderSimple** result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

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
  COM_OBJECT_VALIDATE_1_ARG(result);
  int child_count = GetDelegate()->GetChildCount();
  *result = SafeArrayCreateVector(VT_UNKNOWN, 0, child_count);
  for (LONG i = 0; i < child_count; ++i) {
    auto* child = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(GetDelegate()->ChildAtIndex(i)));
    DCHECK(child);
    child->AddRef();
    SafeArrayPutElement(*result, &i,
                        static_cast<IRawElementProviderSimple*>(child));
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_CanSelectMultiple(BOOL* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  *result = GetData().HasState(ax::mojom::State::kMultiselectable);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsSelectionRequired(BOOL* result) {
  return E_NOTIMPL;
}

//
// ITableItemProvider methods.
//

IFACEMETHODIMP AXPlatformNodeWin::GetColumnHeaderItems(SAFEARRAY** result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

  if (!IsCellOrTableHeader(GetData().role) || !GetTable())
    return E_FAIL;

  std::vector<int32_t> column_header_ids =
      GetDelegate()->GetColHeaderNodeIds(GetTableColumn());
  if (column_header_ids.empty())
    return S_FALSE;
  *result = CreateUIAElementsArrayFromIdVector(column_header_ids);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetRowHeaderItems(SAFEARRAY** result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

  if (!IsCellOrTableHeader(GetData().role) || !GetTable())
    return E_FAIL;

  std::vector<int32_t> row_header_ids =
      GetDelegate()->GetRowHeaderNodeIds(GetTableRow());
  if (row_header_ids.empty())
    return S_FALSE;
  *result = CreateUIAElementsArrayFromIdVector(row_header_ids);
  return S_OK;
}

//
// ITableProvider methods.
//

IFACEMETHODIMP AXPlatformNodeWin::GetColumnHeaders(SAFEARRAY** result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

  if (!GetTable())
    return E_FAIL;

  std::vector<int32_t> column_header_ids = GetDelegate()->GetColHeaderNodeIds();
  *result = CreateUIAElementsArrayFromIdVector(column_header_ids);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetRowHeaders(SAFEARRAY** result) {
  COM_OBJECT_VALIDATE_1_ARG(result);

  if (!GetTable())
    return E_FAIL;

  std::vector<int32_t> row_header_ids = GetDelegate()->GetRowHeaderNodeIds();
  *result = CreateUIAElementsArrayFromIdVector(row_header_ids);
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_RowOrColumnMajor(
    RowOrColumnMajor* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  return E_NOTIMPL;
}

//
// IToggleProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::Toggle() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;

  if (GetDelegate()->AccessibilityPerformAction(action_data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ToggleState(ToggleState* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
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
  if (!value)
    return E_INVALIDARG;

  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = base::WideToUTF8(value);
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_IsReadOnly(BOOL* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  int restriction;
  if (GetIntAttribute(ax::mojom::IntAttribute::kRestriction, &restriction)) {
    *result = static_cast<ax::mojom::Restriction>(restriction) ==
              ax::mojom::Restriction::kReadOnly;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Value(BSTR* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  if (GetStringAttributeAsBstr(ax::mojom::StringAttribute::kValue, result))
    return S_OK;
  return E_FAIL;
}

//
// IRangeValueProvider implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::SetValue(double value) {
  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = base::NumberToString(value);
  if (GetDelegate()->AccessibilityPerformAction(data))
    return S_OK;
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_LargeChange(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange,
                        &attribute)) {
    *result = attribute * kLargeChangeScaleFactor;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Maximum(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Minimum(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_SmallChange(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_Value(double* result) {
  COM_OBJECT_VALIDATE_1_ARG(result);
  float attribute;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                        &attribute)) {
    *result = attribute;
    return S_OK;
  }
  return E_FAIL;
}

// IAccessibleEx methods not implemented.
IFACEMETHODIMP AXPlatformNodeWin::GetRuntimeId(SAFEARRAY** runtime_id) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_RUNTIME_ID);
  return E_NOTIMPL;
}

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
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!accessible)
    return E_INVALIDARG;

  AXPlatformNodeBase* cell =
      GetTableCell(static_cast<int>(row), static_cast<int>(column));
  if (cell) {
    auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
    node_win->AddRef();

    *accessible = static_cast<IAccessible*>(node_win);
    return S_OK;
  }

  *accessible = nullptr;
  return E_INVALIDARG;
}

IFACEMETHODIMP AXPlatformNodeWin::get_caption(IUnknown** accessible) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CAPTION);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  *accessible = nullptr;
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_childIndex(LONG row,
                                                 LONG column,
                                                 LONG* cell_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CHILD_INDEX);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!cell_index)
    return E_INVALIDARG;

  AXPlatformNodeBase* cell =
      GetTableCell(static_cast<int>(row), static_cast<int>(column));
  if (cell) {
    *cell_index = static_cast<LONG>(cell->GetTableCellIndex());
    return S_OK;
  }

  *cell_index = 0;
  return E_INVALIDARG;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnDescription(LONG column,
                                                        BSTR* description) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_DESCRIPTION);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!description)
    return E_INVALIDARG;

  int columns = GetTableColumnCount();
  if (column < 0 || column >= columns)
    return E_INVALIDARG;

  int rows = GetTableRowCount();
  if (rows <= 0) {
    *description = nullptr;
    return S_FALSE;
  }

  for (int r = 0; r < rows; ++r) {
    AXPlatformNodeBase* cell = GetTableCell(r, column);
    if (cell && cell->GetData().role == ax::mojom::Role::kColumnHeader) {
      base::string16 cell_name =
          cell->GetString16Attribute(ax::mojom::StringAttribute::kName);
      if (cell_name.size() > 0) {
        *description = SysAllocString(cell_name.c_str());
        return S_OK;
      }

      cell_name =
          cell->GetString16Attribute(ax::mojom::StringAttribute::kDescription);
      if (cell_name.size() > 0) {
        *description = SysAllocString(cell_name.c_str());
        return S_OK;
      }
    }
  }

  *description = nullptr;
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnExtentAt(LONG row,
                                                     LONG column,
                                                     LONG* n_columns_spanned) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_EXTENT_AT);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!n_columns_spanned)
    return E_INVALIDARG;

  AXPlatformNodeBase* cell =
      GetTableCell(static_cast<int>(row), static_cast<int>(column));
  if (!cell)
    return E_INVALIDARG;

  *n_columns_spanned = cell->GetTableColumnSpan();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnHeader(
    IAccessibleTable** accessible_table,
    LONG* starting_row_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_HEADER);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnIndex(LONG cell_index,
                                                  LONG* column_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COLUMN_INDEX);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!column_index)
    return E_INVALIDARG;

  AXPlatformNodeBase* cell = GetTableCell(cell_index);
  if (!cell)
    return E_INVALIDARG;
  *column_index = cell->GetTableColumn();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nColumns(LONG* column_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_COLUMNS);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!column_count)
    return E_INVALIDARG;

  *column_count = GetTableColumnCount();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nRows(LONG* row_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_ROWS);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!row_count)
    return E_INVALIDARG;

  *row_count = GetTableRowCount();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedChildren(LONG* cell_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_CHILDREN);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!cell_count)
    return E_INVALIDARG;
  *cell_count = 0;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0)
    return S_FALSE;

  LONG result = 0;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell &&
          cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
        result++;
    }
  }
  *cell_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedColumns(LONG* column_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_COLUMNS);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!column_count)
    return E_INVALIDARG;
  *column_count = 0;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0)
    return S_FALSE;

  // If every cell in a column is selected, then that column is selected.
  LONG result = 0;
  for (int c = 0; c < columns; ++c) {
    bool selected = true;
    for (int r = 0; r < rows && selected == true; ++r) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell || !(cell->GetData().GetBoolAttribute(
                       ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      result++;
  }

  *column_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_nSelectedRows(LONG* row_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_SELECTED_ROWS);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!row_count)
    return E_INVALIDARG;
  *row_count = 0;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0)
    return S_FALSE;

  // If every cell in a row is selected, then that row is selected.
  LONG result = 0;
  for (int r = 0; r < rows; ++r) {
    bool selected = true;
    for (int c = 0; c < columns && selected == true; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (!cell || !(cell->GetData().GetBoolAttribute(
                       ax::mojom::BoolAttribute::kSelected)))
        selected = false;
    }
    if (selected)
      result++;
  }

  *row_count = result;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowDescription(LONG row,
                                                     BSTR* description) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_DESCRIPTION);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!description)
    return E_INVALIDARG;

  if (row < 0 || row >= GetTableRowCount())
    return E_INVALIDARG;

  int columns = GetTableColumnCount();
  if (columns <= 0) {
    *description = nullptr;
    return S_FALSE;
  }

  for (int c = 0; c < columns; ++c) {
    AXPlatformNodeBase* cell = GetTableCell(row, c);
    if (cell && cell->GetData().role == ax::mojom::Role::kRowHeader) {
      base::string16 cell_name =
          cell->GetString16Attribute(ax::mojom::StringAttribute::kName);
      if (cell_name.size() > 0) {
        *description = SysAllocString(cell_name.c_str());
        return S_OK;
      }
      cell_name =
          cell->GetString16Attribute(ax::mojom::StringAttribute::kDescription);
      if (cell_name.size() > 0) {
        *description = SysAllocString(cell_name.c_str());
        return S_OK;
      }
    }
  }

  *description = nullptr;
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowExtentAt(LONG row,
                                                  LONG column,
                                                  LONG* n_rows_spanned) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_EXTENT_AT);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!n_rows_spanned)
    return E_INVALIDARG;

  AXPlatformNodeBase* cell = GetTableCell(row, column);
  if (!cell)
    return E_INVALIDARG;

  *n_rows_spanned = GetTableRowSpan();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowHeader(
    IAccessibleTable** accessible_table,
    LONG* starting_column_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ROW_HEADER);
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowIndex(LONG cell_index,
                                               LONG* row_index) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!row_index)
    return E_INVALIDARG;

  AXPlatformNodeBase* cell = GetTableCell(cell_index);
  if (!cell)
    return E_INVALIDARG;

  *row_index = cell->GetTableRow();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedChildren(LONG max_children,
                                                       LONG** children,
                                                       LONG* n_children) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!children || !n_children || max_children <= 0)
    return E_INVALIDARG;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0)
    return S_FALSE;

  std::vector<LONG> results;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell &&
          cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
        // index is row index * column count + column index.
        results.push_back(r * columns + c);
    }
  }

  return AllocateComArrayFromVector(results, max_children, children,
                                    n_children);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedColumns(LONG max_columns,
                                                      LONG** columns,
                                                      LONG* n_columns) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!columns || !n_columns || max_columns <= 0)
    return E_INVALIDARG;

  int column_count = GetTableColumnCount();
  int row_count = GetTableRowCount();
  if (column_count <= 0 || row_count <= 0)
    return S_FALSE;

  std::vector<LONG> results;
  for (int c = 0; c < column_count; ++c) {
    bool selected = true;
    for (int r = 0; r < row_count && selected == true; ++r) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
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
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!rows || !n_rows || max_rows <= 0)
    return E_INVALIDARG;

  int column_count = GetTableColumnCount();
  int row_count = GetTableRowCount();
  if (column_count <= 0 || row_count <= 0)
    return S_FALSE;

  std::vector<LONG> results;
  for (int r = 0; r < row_count; ++r) {
    bool selected = true;
    for (int c = 0; c < column_count && selected == true; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
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
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  *accessible = nullptr;
  return S_FALSE;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isColumnSelected(LONG column,
                                                       boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!is_selected)
    return E_INVALIDARG;
  *is_selected = false;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0 || column >= columns || column < 0)
    return S_FALSE;

  for (int r = 0; r < rows; ++r) {
    AXPlatformNodeBase* cell = GetTableCell(r, column);
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
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!is_selected)
    return E_INVALIDARG;
  *is_selected = false;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0 || row >= rows || row < 0)
    return S_FALSE;

  for (int c = 0; c < columns; ++c) {
    AXPlatformNodeBase* cell = GetTableCell(row, c);
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
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!is_selected)
    return E_INVALIDARG;
  *is_selected = false;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0 || row >= rows || row < 0 ||
      column >= columns || column < 0)
    return S_FALSE;

  AXPlatformNodeBase* cell = GetTableCell(row, column);
  if (cell &&
      cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
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
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!row || !column || !row_extents || !column_extents || !is_selected)
    return E_INVALIDARG;

  AXPlatformNodeBase* cell = GetTableCell(index);
  if (!cell)
    return E_INVALIDARG;

  *row = cell->GetTableRow();
  *column = cell->GetTableColumn();
  *row_extents = GetTableRowSpan();
  *column_extents = GetTableColumnSpan();
  *is_selected = false;  // Not supported.

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::selectRow(LONG row) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::selectColumn(LONG column) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::unselectRow(LONG row) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::unselectColumn(LONG column) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  return E_NOTIMPL;
}

IFACEMETHODIMP
AXPlatformNodeWin::get_modelChange(IA2TableModelChange* model_change) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  return E_NOTIMPL;
}

//
// IAccessibleTable2 methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_cellAt(LONG row,
                                             LONG column,
                                             IUnknown** cell) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(AXMode::kScreenReader);
  if (!cell)
    return E_INVALIDARG;

  AXPlatformNodeBase* table_cell =
      GetTableCell(static_cast<int>(row), static_cast<int>(column));
  if (table_cell) {
    auto* node_win = static_cast<AXPlatformNodeWin*>(table_cell);
    node_win->AddRef();
    *cell = static_cast<IAccessible*>(node_win);
    return S_OK;
  }

  *cell = nullptr;
  return E_INVALIDARG;
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
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!cells || !n_selected_cells)
    return E_INVALIDARG;

  *cells = nullptr;
  *n_selected_cells = 0;

  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0)
    return S_FALSE;

  std::vector<AXPlatformNodeBase*> selected;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      AXPlatformNodeBase* cell = GetTableCell(r, c);
      if (cell &&
          cell->GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
        selected.push_back(cell);
    }
  }

  *n_selected_cells = static_cast<LONG>(selected.size());
  *cells = static_cast<IUnknown**>(
      CoTaskMemAlloc((*n_selected_cells) * sizeof(cells[0])));

  for (size_t i = 0; i < selected.size(); ++i) {
    auto* node_win = static_cast<AXPlatformNodeWin*>(selected[i]);
    node_win->AddRef();
    (*cells)[i] = static_cast<IAccessible*>(node_win);
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedColumns(LONG** columns,
                                                      LONG* n_columns) {
  return get_selectedColumns(INT_MAX, columns, n_columns);
}

IFACEMETHODIMP AXPlatformNodeWin::get_selectedRows(LONG** rows, LONG* n_rows) {
  return get_selectedRows(INT_MAX, rows, n_rows);
}

//
// IAccessibleTableCell methods.
//

IFACEMETHODIMP AXPlatformNodeWin::get_columnExtent(LONG* n_columns_spanned) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!n_columns_spanned)
    return E_INVALIDARG;

  *n_columns_spanned = GetTableColumnSpan();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnHeaderCells(
    IUnknown*** cell_accessibles,
    LONG* n_column_header_cells) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!cell_accessibles || !n_column_header_cells)
    return E_INVALIDARG;

  *n_column_header_cells = 0;
  if (GetData().role != ax::mojom::Role::kCell)
    return S_FALSE;

  AXPlatformNodeBase* table = GetTable();
  if (!table) {
    return S_FALSE;
  }

  int column = GetTableColumn();
  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0 || column < 0 || column >= columns)
    return S_FALSE;

  for (int r = 0; r < rows; ++r) {
    AXPlatformNodeBase* cell = GetTableCell(r, column);
    if (cell && cell->GetData().role == ax::mojom::Role::kColumnHeader)
      (*n_column_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(
      CoTaskMemAlloc((*n_column_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int r = 0; r < rows; ++r) {
    AXPlatformNodeBase* cell = GetTableCell(r, column);
    if (cell && cell->GetData().role == ax::mojom::Role::kColumnHeader) {
      auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
      node_win->AddRef();

      (*cell_accessibles)[index] = static_cast<IAccessible*>(node_win);
      ++index;
    }
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_columnIndex(LONG* column_index) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!column_index)
    return E_INVALIDARG;

  *column_index = GetTableColumn();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowExtent(LONG* n_rows_spanned) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!n_rows_spanned)
    return E_INVALIDARG;

  *n_rows_spanned = GetTableRowSpan();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowHeaderCells(
    IUnknown*** cell_accessibles,
    LONG* n_row_header_cells) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!cell_accessibles || !n_row_header_cells)
    return E_INVALIDARG;

  *n_row_header_cells = 0;
  if (GetData().role != ax::mojom::Role::kCell)
    return S_FALSE;

  AXPlatformNodeBase* table = GetTable();
  if (!table) {
    return S_FALSE;
  }

  int row = GetTableRow();
  int columns = GetTableColumnCount();
  int rows = GetTableRowCount();
  if (columns <= 0 || rows <= 0 || row < 0 || row >= rows)
    return S_FALSE;

  for (int c = 0; c < columns; ++c) {
    AXPlatformNodeBase* cell = GetTableCell(row, c);
    if (cell && cell->GetData().role == ax::mojom::Role::kRowHeader)
      (*n_row_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(
      CoTaskMemAlloc((*n_row_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int c = 0; c < columns; ++c) {
    AXPlatformNodeBase* cell = GetTableCell(row, c);
    if (cell && cell->GetData().role == ax::mojom::Role::kRowHeader) {
      auto* node_win = static_cast<AXPlatformNodeWin*>(cell);
      node_win->AddRef();

      (*cell_accessibles)[index] = static_cast<IAccessible*>(node_win);
      ++index;
    }
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowIndex(LONG* row_index) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!row_index)
    return E_INVALIDARG;

  *row_index = GetTableRow();
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_isSelected(boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!is_selected)
    return E_INVALIDARG;

  *is_selected = false;
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_rowColumnExtents(LONG* row_index,
                                                       LONG* column_index,
                                                       LONG* row_extents,
                                                       LONG* column_extents,
                                                       boolean* is_selected) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!row_index || !column_index || !row_extents || !column_extents ||
      !is_selected) {
    return E_INVALIDARG;
  }

  *row_index = GetTableRow();
  *column_index = GetTableColumn();
  *row_extents = GetTableRowSpan();
  *column_extents = GetTableColumnSpan();
  *is_selected = false;  // Not supported.

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_table(IUnknown** table) {
  // TODO(dougt) WIN_ACCESSIBILITY_API_HISTOGRAM?
  AXPlatformNode::NotifyAddAXModeFlags(kScreenReaderAndHTMLAccessibilityModes);

  if (!table)
    return E_INVALIDARG;

  AXPlatformNodeBase* find_table = GetTable();
  if (!find_table) {
    *table = nullptr;
    return S_FALSE;
  }

  // The IAccessibleTable interface is still on the AXPlatformNodeWin
  // class.
  auto* node_win = static_cast<AXPlatformNodeWin*>(find_table);
  node_win->AddRef();

  *table = static_cast<IAccessibleTable*>(node_win);
  return S_OK;
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
    // end offsets. According to the IA2 Spec the end offset is always increased
    // by one past the end of the selection. This wouldn't make sense if
    // end < start.
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

  *start_offset =
      FindBoundary(text_str, boundary_type, offset, BACKWARDS_DIRECTION);
  *end_offset =
      FindBoundary(text_str, boundary_type, offset, FORWARDS_DIRECTION);
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

  *start_offset =
      FindBoundary(text_str, boundary_type, offset, BACKWARDS_DIRECTION);
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
  *end_offset =
      FindBoundary(text_str, boundary_type, offset, FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

IFACEMETHODIMP AXPlatformNodeWin::get_offsetAtPoint(
    LONG x,
    LONG y,
    enum IA2CoordinateType coord_type,
    LONG* offset) {
  COM_OBJECT_VALIDATE_1_ARG(offset);
  // We don't support this method, but we have to return something
  // rather than E_NOTIMPL or screen readers will complain.
  *offset = 0;
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

  if (SetTextSelection(static_cast<int>(start_offset),
                       static_cast<int>(end_offset))) {
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
// IRawElementProviderSimple implementation.
//

IFACEMETHODIMP AXPlatformNodeWin::GetPatternProvider(PATTERNID pattern_id,
                                                     IUnknown** result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PATTERN_PROVIDER);
  COM_OBJECT_VALIDATE_1_ARG(result);

  const AXNodeData& data = GetData();

  switch (pattern_id) {
    // Supported by IAccessibleEx.
    // TODO(suproteem): Implementations where applicable.
    case UIA_DockPatternId:
      break;

    case UIA_ExpandCollapsePatternId:
      if (SupportsExpandCollapse(data.role)) {
        AddRef();
        *result = static_cast<IExpandCollapseProvider*>(this);
      }
      break;

    case UIA_GridPatternId:
      if (IsTableLike(data.role)) {
        AddRef();
        *result = static_cast<IGridProvider*>(this);
      }
      break;

    case UIA_GridItemPatternId:
      if (IsCellOrTableHeader(data.role)) {
        AddRef();
        *result = static_cast<IGridItemProvider*>(this);
      }
      break;

    case UIA_MultipleViewPatternId:
      break;

    case UIA_RangeValuePatternId:
      AddRef();
      *result = static_cast<IRangeValueProvider*>(this);
      break;

    case UIA_ScrollPatternId:
      break;

    case UIA_ScrollItemPatternId:
      AddRef();
      *result = static_cast<IScrollItemProvider*>(this);
      break;

    case UIA_SynchronizedInputPatternId:
      break;

    case UIA_TablePatternId:
      if (IsTableLike(data.role)) {
        AddRef();
        *result = static_cast<ITableProvider*>(this);
      }
      break;

    case UIA_TableItemPatternId:
      if (IsCellOrTableHeader(data.role)) {
        AddRef();
        *result = static_cast<ITableItemProvider*>(this);
      }
      break;

    case UIA_TransformPatternId:
      break;

    // TODO(suproteem): Add checks for control role.
    case UIA_InvokePatternId:
      break;

    case UIA_SelectionItemPatternId:
      if (IsUIASelectable(data.role)) {
        AddRef();
        *result = static_cast<ISelectionItemProvider*>(this);
      }
      break;

    case UIA_SelectionPatternId:
      if (IsContainerWithSelectableChildren(data.role)) {
        AddRef();
        *result = static_cast<ISelectionProvider*>(this);
      }
      break;

    case UIA_TogglePatternId:
      if (SupportsToggle(data.role)) {
        AddRef();
        *result = static_cast<IToggleProvider*>(this);
      }
      break;

    case UIA_ValuePatternId:
      AddRef();
      *result = static_cast<IValueProvider*>(this);
      break;

    case UIA_WindowPatternId:
      break;

    // Overlap with MSAA, not supported.
    case UIA_AnnotationPatternId:
    case UIA_CustomNavigationPatternId:
    case UIA_DragPatternId:
    case UIA_DropTargetPatternId:
    case UIA_ItemContainerPatternId:
    case UIA_LegacyIAccessiblePatternId:
    case UIA_ObjectModelPatternId:
    case UIA_SpreadsheetPatternId:
    case UIA_SpreadsheetItemPatternId:
    case UIA_StylesPatternId:
    case UIA_TextChildPatternId:
    case UIA_TextEditPatternId:
    case UIA_TextPatternId:
    case UIA_TextPattern2Id:
    case UIA_TransformPattern2Id:
    case UIA_VirtualizedItemPatternId:
      break;
  }
  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::GetPropertyValue(PROPERTYID property_id,
                                                   VARIANT* result) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PROPERTY_VALUE);
  COM_OBJECT_VALIDATE_1_ARG(result);

  const AXNodeData& data = GetData();
  int int_attribute;
  ax::mojom::IntListAttribute relation_attribute;
  result->vt = VT_EMPTY;

  switch (property_id) {
    // Supported by IAccessibleEx.
    // TODO(suproteem): Implementations where applicable.
    case UIA_AriaPropertiesPropertyId:
      result->vt = VT_BSTR;
      result->bstrVal = SysAllocString(ComputeUIAProperties().c_str());
      break;

    case UIA_AriaRolePropertyId:
      result->vt = VT_BSTR;
      result->bstrVal = SysAllocString(UIAAriaRole().c_str());
      break;

    case UIA_AutomationIdPropertyId:
      result->vt = VT_BSTR;
      result->bstrVal =
          SysAllocString(base::NumberToString16(GetUniqueId()).c_str());
      break;

    case UIA_ClassNamePropertyId:
      result->vt = VT_BSTR;
      GetStringAttributeAsBstr(ax::mojom::StringAttribute::kName,
                               result->pbstrVal);
      break;

    case UIA_ClickablePointPropertyId:
      // TODO(suproteem)
      break;

    case UIA_ControllerForPropertyId:
      result->vt = VT_ARRAY;
      relation_attribute = ax::mojom::IntListAttribute::kControlsIds;
      result->parray = CreateUIAElementsArrayForRelation(relation_attribute);
      break;

    case UIA_ControlTypePropertyId:
      result->vt = VT_I4;
      result->lVal = ComputeUIAControlType();
      break;

    case UIA_CulturePropertyId:
      result->vt = VT_BSTR;
      GetStringAttributeAsBstr(ax::mojom::StringAttribute::kLanguage,
                               result->pbstrVal);
      break;

    case UIA_DescribedByPropertyId:
      result->vt = VT_ARRAY;
      relation_attribute = ax::mojom::IntListAttribute::kDescribedbyIds;
      result->parray = CreateUIAElementsArrayForRelation(relation_attribute);
      break;

    case UIA_FlowsToPropertyId:
      result->vt = VT_ARRAY;
      relation_attribute = ax::mojom::IntListAttribute::kFlowtoIds;
      result->parray = CreateUIAElementsArrayForRelation(relation_attribute);
      break;

    case UIA_FrameworkIdPropertyId:
    case UIA_IsContentElementPropertyId:
    case UIA_IsControlElementPropertyId:
      // TODO(suproteem)
      break;

    case UIA_IsDataValidForFormPropertyId:
      if (GetIntAttribute(ax::mojom::IntAttribute::kInvalidState,
                          &int_attribute)) {
        result->vt = VT_BOOL;
        result->boolVal = int_attribute ==
                          static_cast<int32_t>(ax::mojom::InvalidState::kFalse);
      }
      break;

    case UIA_IsRequiredForFormPropertyId:
      result->vt = VT_BOOL;
      if (data.HasState(ax::mojom::State::kRequired)) {
        result->boolVal = true;
      } else {
        result->boolVal = false;
      }
      break;

    case UIA_ItemStatusPropertyId:
    case UIA_ItemTypePropertyId:
      // TODO(suproteem)
      break;

    case UIA_LabeledByPropertyId:
      break;

    case UIA_LocalizedControlTypePropertyId:
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
      }
      break;

    // Covered by MSAA.
    case UIA_BoundingRectanglePropertyId:
    case UIA_HasKeyboardFocusPropertyId:
    case UIA_HelpTextPropertyId:
    case UIA_IsEnabledPropertyId:
    case UIA_IsKeyboardFocusablePropertyId:
    case UIA_IsOffscreenPropertyId:
    case UIA_IsPasswordPropertyId:
    case UIA_NamePropertyId:
    case UIA_NativeWindowHandlePropertyId:
    case UIA_ProcessIdPropertyId:
      break;

    // Overlap with MSAA, not supported.
    case UIA_AcceleratorKeyPropertyId:
    case UIA_AccessKeyPropertyId:
    case UIA_AnnotationObjectsPropertyId:
    case UIA_AnnotationTypesPropertyId:
    case UIA_CenterPointPropertyId:
    case UIA_CustomControlTypeId:
    case UIA_FillColorPropertyId:
    case UIA_FillTypePropertyId:
    case UIA_FlowsFromPropertyId:
    case UIA_FullDescriptionPropertyId:
    case UIA_GroupControlTypeId:
    case UIA_HeadingLevelPropertyId:
    case UIA_IsPeripheralPropertyId:
    case UIA_LandmarkTypePropertyId:
    case UIA_LevelPropertyId:
    case UIA_LiveSettingPropertyId:
    case UIA_LocalizedLandmarkTypePropertyId:
    case UIA_MenuControlTypeId:
    case UIA_OptimizeForVisualContentPropertyId:
    case UIA_OutlineColorPropertyId:
    case UIA_OutlineThicknessPropertyId:
    case UIA_PaneControlTypeId:
    case UIA_PositionInSetPropertyId:
    case UIA_ProviderDescriptionPropertyId:
    case UIA_RotationPropertyId:
    case UIA_RuntimeIdPropertyId:
    case UIA_SizeOfSetPropertyId:
    case UIA_SizePropertyId:
    case UIA_ToolBarControlTypeId:
    case UIA_ToolTipControlTypeId:
    case UIA_VisualEffectsPropertyId:
    case UIA_WindowControlTypeId:
      // MSAA-to-UIA Proxy.
      break;
  }

  return S_OK;
}

IFACEMETHODIMP AXPlatformNodeWin::get_ProviderOptions(ProviderOptions* ret) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PROVIDER_OPTIONS);
  return E_NOTIMPL;
}

IFACEMETHODIMP AXPlatformNodeWin::get_HostRawElementProvider(
    IRawElementProviderSimple** provider) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_HOST_RAW_ELEMENT_PROVIDER);
  return E_NOTIMPL;
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
      return ROLE_SYSTEM_ALERT;

    case ax::mojom::Role::kAlertDialog:
      // We temporarily use |ROLE_SYSTEM_ALERT| because some Windows screen
      // readers are not compatible with |ax::mojom::Role::kAlertDialog| yet.
      // TODO(aleventhal) modify this to return |ROLE_SYSTEM_DIALOG|.
      return ROLE_SYSTEM_ALERT;

    case ax::mojom::Role::kAnchor:
      return ROLE_SYSTEM_LINK;

    case ax::mojom::Role::kApplication:
      return ROLE_SYSTEM_APPLICATION;

    case ax::mojom::Role::kArticle:
      return ROLE_SYSTEM_DOCUMENT;

    case ax::mojom::Role::kAudio:
      return ROLE_SYSTEM_GROUPING;

    case ax::mojom::Role::kBanner:
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

    case ax::mojom::Role::kFooter:
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

    case ax::mojom::Role::kRegion: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "section" &&
          GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return ROLE_SYSTEM_GROUPING;
      }
      // Use ARIA mapping.
      return ROLE_SYSTEM_PANE;
    }

    case ax::mojom::Role::kRow: {
      // Role changes depending on whether row is inside a treegrid
      // https://www.w3.org/TR/core-aam-1.1/#role-map-row
      return IsInTreeGrid() ? ROLE_SYSTEM_OUTLINEITEM : ROLE_SYSTEM_ROW;
    }

    case ax::mojom::Role::kRowHeader:
      return ROLE_SYSTEM_ROWHEADER;

    case ax::mojom::Role::kRuby:
      return ROLE_SYSTEM_TEXT;

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

    case ax::mojom::Role::kAnnotation:
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
      // Do not return ROLE_SYSTEM_WINDOW as that is a special MSAA system role
      // used to indicate a real native window object. It is automatically
      // created by oleacc.dll as a parent of the root of our hierarchy,
      // matching the HWND.
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
      // Use html tag if available. In the case where there is no tag, e.g. for
      // anonymous content inserted by blink, treat it as a "div". This can
      // occur if the markup had a block and inline element as siblings -- blink
      // will wrap the inline with a block in this case.
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
    ia2_state |= IA2_STATE_SELECTABLE_TEXT;
  }

  // TODO(crbug.com/865101) Use
  // data.HasState(ax::mojom::State::kAutofillAvailable) instead of
  // IsFocusedInputWithSuggestions()
  if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete).empty() ||
      IsFocusedInputWithSuggestions()) {
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
    case ax::mojom::Role::kBanner:
      // CORE-AAM recommends IA2_ROLE_LANDMARK.
      ia2_role = IA2_ROLE_HEADER;
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
      // Note: IA2_ROLE_COMPLEMENTARY_CONTENT currently exists but CORE-AAM
      // maps this to more general IA2_ROLE_LANDMARK.
      ia2_role = IA2_ROLE_LANDMARK;
      break;
    case ax::mojom::Role::kContentDeletion:
      ia2_role = IA2_ROLE_CONTENT_DELETION;
      break;
    case ax::mojom::Role::kContentInsertion:
      ia2_role = IA2_ROLE_CONTENT_INSERTION;
      break;
    case ax::mojom::Role::kContentInfo:
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
    case ax::mojom::Role::kForm:
      ia2_role = IA2_ROLE_FORM;
      break;
    case ax::mojom::Role::kFooter:
      ia2_role = IA2_ROLE_FOOTER;
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
      // adopt it. Currently, a <meter> ends up being spoken as a progress bar,
      // which is confusing.
      // IA2_ROLE_LEVEL_BAR is the correct mapping according to CORE-AAM.
      // ia2_role = IA2_ROLE_LEVEL_BAR;
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
    case ax::mojom::Role::kRegion: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "section" &&
          GetData()
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
    case ax::mojom::Role::kRuby:
      ia2_role = IA2_ROLE_TEXT_FRAME;
      break;
    case ax::mojom::Role::kSearch:
      ia2_role = IA2_ROLE_LANDMARK;
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

    case ax::mojom::Role::kApplication:
      return L"application";

    case ax::mojom::Role::kArticle:
      return L"article";

    case ax::mojom::Role::kAudio:
      return L"group";

    case ax::mojom::Role::kBanner:
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
      return L"contentinfo";

    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      return L"list";

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

    case ax::mojom::Role::kFeed:
      return L"group";

    case ax::mojom::Role::kFigcaption:
      return L"group";

    case ax::mojom::Role::kFigure:
      return L"group";

    case ax::mojom::Role::kFooter:
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

    case ax::mojom::Role::kListItem:
      return L"listitem";

    case ax::mojom::Role::kListBox:
      return L"listbox";

    case ax::mojom::Role::kListBoxOption:
      return L"option";

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

    case ax::mojom::Role::kRegion: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "section" &&
          GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return L"group";
      }
      // Use ARIA mapping.
      return L"region";
    }

    case ax::mojom::Role::kRow: {
      // Role changes depending on whether row is inside a treegrid
      // https://www.w3.org/TR/core-aam-1.1/#role-map-row
      return IsInTreeGrid() ? L"treeitem" : L"row";
    }

    case ax::mojom::Role::kRowHeader:
      return L"rowheader";

    case ax::mojom::Role::kRuby:
      return L"region";

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

    case ax::mojom::Role::kSwitch:
      return L"checkbox";

    case ax::mojom::Role::kAnnotation:
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
    case ax::mojom::Role::kSearchBox:
      return L"textbox";

    case ax::mojom::Role::kTextFieldWithComboBox:
      return L"combobox";

    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kTime:
      return L"description";

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
  HtmlAttributeToUIAAriaProperty(properties, "aria-channel", "channel");

  switch (data.GetCheckedState()) {
    case ax::mojom::CheckedState::kNone:
      break;
    case ax::mojom::CheckedState::kFalse:
      if (data.role == ax::mojom::Role::kToggleButton) {
        properties.emplace_back(L"pressed=false");
      } else if (data.role == ax::mojom::Role::kSwitch) {
        // ARIA switches are exposed to Windows accessibility as toggle buttons.
        // For maximum compatibility with ATs, we expose both the pressed and
        // checked states.
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
        // ARIA switches are exposed to Windows accessibility as toggle buttons.
        // For maximum compatibility with ATs, we expose both the pressed and
        // checked states.
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
          ShouldNodeHaveReadonlyStateByDefault(data))
        properties.push_back(L"readonly=true");
      break;
  }

  HtmlAttributeToUIAAriaProperty(properties, "aria-dropeffect", "dropeffect");
  StateToUIAAriaProperty(properties, ax::mojom::State::kExpanded, "expanded");
  HtmlAttributeToUIAAriaProperty(properties, "aria-grabbed", "grabbed");

  // TODO(crbug.com/865101) Use
  // data.HasState(ax::mojom::State::kAutofillAvailable) instead of
  // IsFocusedInputWithSuggestions()
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kHasPopup)) {
    properties.push_back(L"haspopup=true");
  }

  if (data.HasState(ax::mojom::State::kInvisible) ||
      GetData().role == ax::mojom::Role::kIgnored) {
    properties.push_back(L"hidden=true");
  }

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
  HtmlAttributeToUIAAriaProperty(properties, "aria-secret", "secret");

  int32_t sort_direction;
  if ((data.role == ax::mojom::Role::kColumnHeader ||
       data.role == ax::mojom::Role::kRowHeader) &&
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

  HtmlAttributeToUIAAriaProperty(properties, "aria-tabindex", "tabindex");

  if (IsRangeValueSupported()) {
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

    case ax::mojom::Role::kApplication:
      return UIA_PaneControlTypeId;

    case ax::mojom::Role::kArticle:
      return UIA_DocumentControlTypeId;

    case ax::mojom::Role::kAudio:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kBanner:
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

    case ax::mojom::Role::kColorWell:
      return UIA_DocumentControlTypeId;

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
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
      return UIA_ListControlTypeId;

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

    case ax::mojom::Role::kFeed:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kFigcaption:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kFigure:
      return UIA_GroupControlTypeId;

    case ax::mojom::Role::kFooter:
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
      return UIA_DataGridControlTypeId;

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

    case ax::mojom::Role::kListItem:
      return UIA_ListItemControlTypeId;

    case ax::mojom::Role::kListBox:
      return UIA_ListControlTypeId;

    case ax::mojom::Role::kListBoxOption:
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

    case ax::mojom::Role::kRegion: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "section" &&
          GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return UIA_GroupControlTypeId;
      }
      // Use ARIA mapping.
      return UIA_PaneControlTypeId;
    }

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
      return UIA_CheckBoxControlTypeId;

    case ax::mojom::Role::kAnnotation:
    case ax::mojom::Role::kListMarker:
    case ax::mojom::Role::kStaticText:
      return UIA_TextControlTypeId;

    case ax::mojom::Role::kStatus:
      return UIA_StatusBarControlTypeId;

    case ax::mojom::Role::kSplitter:
      return UIA_SeparatorControlTypeId;

    case ax::mojom::Role::kSvgRoot:
      return UIA_ImageControlTypeId;

    case ax::mojom::Role::kTab:
      return UIA_TabItemControlTypeId;

    case ax::mojom::Role::kTable:
      return UIA_DataGridControlTypeId;

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
      return UIA_DocumentControlTypeId;

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

base::string16 AXPlatformNodeWin::GetValue() {
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

bool AXPlatformNodeWin::ShouldNodeHaveReadonlyStateByDefault(
    const AXNodeData& data) const {
  switch (data.role) {
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kDescriptionListTerm:
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kGraphicsDocument:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kImageMap:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kTerm:
    case ax::mojom::Role::kTimer:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTooltip:
    case ax::mojom::Role::kWebArea:
      return true;

    case ax::mojom::Role::kGrid:
      // TODO(aleventhal) this changed between ARIA 1.0 and 1.1,
      // need to determine whether grids/treegrids should really be readonly
      // or editable by default
      // msaa_state |= STATE_SYSTEM_READONLY;
      break;

    default:
      break;
  }
  return false;
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

int AXPlatformNodeWin::MSAAState() {
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

  // TODO(crbug.com/865101) Use
  // data.HasState(ax::mojom::State::kAutofillAvailable) instead of
  // IsFocusedInputWithSuggestions() and rmove the below comment: Note:
  // suggestions are special-cased here because there is no way for the browser
  // to know when a suggestion popup is available.
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kHasPopup) ||
      IsFocusedInputWithSuggestions())
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
  if (data.HasState(ax::mojom::State::kInvisible) ||
      GetData().role == ax::mojom::Role::kIgnored) {
    msaa_state |= STATE_SYSTEM_INVISIBLE;
  }
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
        // ARIA switches are exposed to Windows accessibility as toggle buttons.
        // For maximum compatibility with ATs, we expose both the pressed and
        // checked states.
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
          ShouldNodeHaveReadonlyStateByDefault(data))
        msaa_state |= STATE_SYSTEM_READONLY;
      break;
  }

  //
  // Handle STATE_SYSTEM_FOCUSED
  //
  gfx::NativeViewAccessible focus = GetDelegate()->GetFocus();
  if (focus == GetNativeViewAccessible())
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

int AXPlatformNodeWin::MSAAEvent(ax::mojom::Event event) {
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
    case ax::mojom::Event::kValueChanged:
      return EVENT_OBJECT_VALUECHANGE;
    default:
      return -1;
  }
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
  // Special case allows us to get text even in non-HTML case, e.g. browser UI.
  if (IsPlainTextField())
    return GetString16Attribute(ax::mojom::StringAttribute::kValue);
  return GetTextAsString16();
}

void AXPlatformNodeWin::HandleSpecialTextOffset(LONG* offset) {
  if (*offset == IA2_TEXT_OFFSET_LENGTH) {
    *offset = static_cast<LONG>(GetTextAsString16().length());
  } else if (*offset == IA2_TEXT_OFFSET_CARET) {
    int selection_start, selection_end;
    GetSelectionOffsets(&selection_start, &selection_end);
    // TODO(nektar): Deprecate selection_start and selection_end in favor of
    // sel_anchor_offset/sel_focus_offset. See https://crbug.com/645596.
    if (selection_end < 0)
      *offset = 0;
    else
      *offset = static_cast<LONG>(selection_end);
  }
}

TextBoundaryType AXPlatformNodeWin::IA2TextBoundaryToTextBoundary(
    IA2TextBoundaryType ia2_boundary) {
  switch (ia2_boundary) {
    case IA2_TEXT_BOUNDARY_CHAR:
      return CHAR_BOUNDARY;
    case IA2_TEXT_BOUNDARY_WORD:
      return WORD_BOUNDARY;
    case IA2_TEXT_BOUNDARY_LINE:
      return LINE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_SENTENCE:
      return SENTENCE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_PARAGRAPH:
      return PARAGRAPH_BOUNDARY;
    case IA2_TEXT_BOUNDARY_ALL:
      return ALL_BOUNDARY;
    default:
      NOTREACHED();
      return CHAR_BOUNDARY;
  }
}

LONG AXPlatformNodeWin::FindBoundary(const base::string16& text,
                                     IA2TextBoundaryType ia2_boundary,
                                     LONG start_offset,
                                     TextBoundaryDirection direction) {
  HandleSpecialTextOffset(&start_offset);
  TextBoundaryType boundary = IA2TextBoundaryToTextBoundary(ia2_boundary);
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

// TODO(dmazzoni): Remove this function once combo box refactoring is complete.
bool AXPlatformNodeWin::IsAncestorComboBox() {
  auto* parent =
      static_cast<AXPlatformNodeWin*>(FromNativeViewAccessible(GetParent()));
  if (!parent)
    return false;
  if (parent->MSAARole() == ROLE_SYSTEM_COMBOBOX)
    return true;
  return parent->IsAncestorComboBox();
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

AXPlatformNodeWin* AXPlatformNodeWin::GetHyperlinkFromHypertextOffset(
    int offset) {
  std::map<int32_t, int32_t>::iterator iterator =
      hypertext_.hyperlink_offset_to_index.find(offset);
  if (iterator == hypertext_.hyperlink_offset_to_index.end())
    return nullptr;

  int32_t index = iterator->second;
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int32_t>(hypertext_.hyperlinks.size()));
  int32_t id = hypertext_.hyperlinks[index];
  auto* hyperlink =
      static_cast<AXPlatformNodeWin*>(AXPlatformNodeWin::GetFromUniqueId(id));
  if (!hyperlink)
    return nullptr;
  return hyperlink;
}

int32_t AXPlatformNodeWin::GetHyperlinkIndexFromChild(
    AXPlatformNodeWin* child) {
  if (hypertext_.hyperlinks.empty())
    return -1;

  auto iterator = std::find(hypertext_.hyperlinks.begin(),
                            hypertext_.hyperlinks.end(), child->GetUniqueId());
  if (iterator == hypertext_.hyperlinks.end())
    return -1;

  return static_cast<int32_t>(iterator - hypertext_.hyperlinks.begin());
}

int32_t AXPlatformNodeWin::GetHypertextOffsetFromHyperlinkIndex(
    int32_t hyperlink_index) {
  for (auto& offset_index : hypertext_.hyperlink_offset_to_index) {
    if (offset_index.second == hyperlink_index)
      return offset_index.first;
  }
  return -1;
}

int32_t AXPlatformNodeWin::GetHypertextOffsetFromChild(
    AXPlatformNodeWin* child) {
  // TODO(dougt) DCHECK(child.owner()->PlatformGetParent() == owner());

  // Handle the case when we are dealing with a text-only child.
  // Note that this object might be a platform leaf, e.g. an ARIA searchbox.
  // Also, text-only children should not be present at tree roots and so no
  // cross-tree traversal is necessary.
  if (child->IsTextOnlyObject()) {
    int32_t hypertext_offset = 0;
    int32_t index_in_parent = child->GetDelegate()->GetIndexInParent();
    DCHECK_GE(index_in_parent, 0);
    DCHECK_LT(index_in_parent,
              static_cast<int32_t>(GetDelegate()->GetChildCount()));
    for (uint32_t i = 0; i < static_cast<uint32_t>(index_in_parent); ++i) {
      auto* sibling = static_cast<AXPlatformNodeWin*>(
          FromNativeViewAccessible(GetDelegate()->ChildAtIndex(i)));
      DCHECK(sibling);
      if (sibling->IsTextOnlyObject())
        hypertext_offset += (int32_t)sibling->GetTextAsString16().size();
      else
        ++hypertext_offset;
    }
    return hypertext_offset;
  }

  int32_t hyperlink_index = GetHyperlinkIndexFromChild(child);
  if (hyperlink_index < 0)
    return -1;

  return GetHypertextOffsetFromHyperlinkIndex(hyperlink_index);
}

int32_t AXPlatformNodeWin::GetHypertextOffsetFromDescendant(
    AXPlatformNodeWin* descendant) {
  auto* parent_object = static_cast<AXPlatformNodeWin*>(
      FromNativeViewAccessible(descendant->GetDelegate()->GetParent()));
  while (parent_object && parent_object != this) {
    descendant = parent_object;
    parent_object = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(descendant->GetParent()));
  }
  if (!parent_object)
    return -1;

  return parent_object->GetHypertextOffsetFromChild(descendant);
}

int AXPlatformNodeWin::GetHypertextOffsetFromEndpoint(
    AXPlatformNodeWin* endpoint_object,
    int endpoint_offset) {
  // There are three cases:
  // 1. Either the selection endpoint is inside this object or is an ancestor of
  // of this object. endpoint_offset should be returned.
  // 2. The selection endpoint is a pure descendant of this object. The offset
  // of the character corresponding to the subtree in which the endpoint is
  // located should be returned.
  // 3. The selection endpoint is in a completely different part of the tree.
  // Either 0 or text_length should be returned depending on the direction that
  // one needs to travel to find the endpoint.

  // Case 1.
  //
  // IsDescendantOf includes the case when endpoint_object == this.
  if (IsDescendantOf(endpoint_object))
    return endpoint_offset;

  AXPlatformNodeWin* common_parent = this;
  int32_t index_in_common_parent = GetDelegate()->GetIndexInParent();
  while (common_parent && !endpoint_object->IsDescendantOf(common_parent)) {
    index_in_common_parent = common_parent->GetDelegate()->GetIndexInParent();
    common_parent = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(common_parent->GetParent()));
  }
  if (!common_parent)
    return -1;

  DCHECK_GE(index_in_common_parent, 0);
  DCHECK(!(common_parent->IsTextOnlyObject()));

  // Case 2.
  //
  // We already checked in case 1 if our endpoint is inside this object.
  // We can safely assume that it is a descendant or in a completely different
  // part of the tree.
  if (common_parent == this) {
    int32_t hypertext_offset =
        GetHypertextOffsetFromDescendant(endpoint_object);
    auto* parent = static_cast<AXPlatformNodeWin*>(
        FromNativeViewAccessible(endpoint_object->GetParent()));
    if (parent == this && endpoint_object->IsTextOnlyObject()) {
      hypertext_offset += endpoint_offset;
    }

    return hypertext_offset;
  }

  // Case 3.
  //
  // We can safely assume that the endpoint is in another part of the tree or
  // at common parent, and that this object is a descendant of common parent.
  int32_t endpoint_index_in_common_parent = -1;
  for (int i = 0; i < common_parent->GetDelegate()->GetChildCount(); ++i) {
    auto* child = static_cast<AXPlatformNodeWin*>(
        common_parent->GetDelegate()->ChildAtIndex(i));
    DCHECK(child);
    if (endpoint_object->IsDescendantOf(child)) {
      endpoint_index_in_common_parent =
          child->GetDelegate()->GetIndexInParent();
      break;
    }
  }
  DCHECK_GE(endpoint_index_in_common_parent, 0);

  if (endpoint_index_in_common_parent < index_in_common_parent)
    return 0;
  if (endpoint_index_in_common_parent > index_in_common_parent)
    return (int32_t)GetTextAsString16().size();

  NOTREACHED();
  return -1;
}

bool AXPlatformNodeWin::IsSameHypertextCharacter(size_t old_char_index,
                                                 size_t new_char_index) {
  if (old_char_index >= old_hypertext_.hypertext.size() ||
      new_char_index >= hypertext_.hypertext.size()) {
    return false;
  }

  // For anything other than the "embedded character", we just compare the
  // characters directly.
  base::char16 old_ch = old_hypertext_.hypertext[old_char_index];
  base::char16 new_ch = hypertext_.hypertext[new_char_index];
  if (old_ch != new_ch)
    return false;
  if (new_ch != kEmbeddedCharacter)
    return true;

  // If it's an embedded character, they're only identical if the child id
  // the hyperlink points to is the same.
  std::map<int32_t, int32_t>& old_offset_to_index =
      old_hypertext_.hyperlink_offset_to_index;
  std::vector<int32_t>& old_hyperlinks = old_hypertext_.hyperlinks;
  int32_t old_hyperlinkscount = static_cast<int32_t>(old_hyperlinks.size());
  std::map<int32_t, int32_t>::iterator iter;
  iter = old_offset_to_index.find((int32_t)old_char_index);
  int old_index = (iter != old_offset_to_index.end()) ? iter->second : -1;
  int old_child_id = (old_index >= 0 && old_index < old_hyperlinkscount)
                         ? old_hyperlinks[old_index]
                         : -1;

  std::map<int32_t, int32_t>& new_offset_to_index =
      hypertext_.hyperlink_offset_to_index;
  std::vector<int32_t>& new_hyperlinks = hypertext_.hyperlinks;
  int32_t new_hyperlinkscount = static_cast<int32_t>(new_hyperlinks.size());
  iter = new_offset_to_index.find((int32_t)new_char_index);
  int new_index = (iter != new_offset_to_index.end()) ? iter->second : -1;
  int new_child_id = (new_index >= 0 && new_index < new_hyperlinkscount)
                         ? new_hyperlinks[new_index]
                         : -1;

  return old_child_id == new_child_id;
}

// Return true if the index represents a text character.
bool AXPlatformNodeWin::IsText(const base::string16& text,
                               size_t index,
                               bool is_indexed_from_end) {
  size_t text_len = text.size();
  if (index == text_len)
    return false;
  auto ch = text[is_indexed_from_end ? text_len - index - 1 : index];
  return ch != kEmbeddedCharacter;
}

void AXPlatformNodeWin::ComputeHypertextRemovedAndInserted(size_t* start,
                                                           size_t* old_len,
                                                           size_t* new_len) {
  *start = 0;
  *old_len = 0;
  *new_len = 0;

  // Do not compute for static text objects, otherwise redundant text change
  // announcements will occur in live regions, as the parent hypertext also
  // changes.
  if (GetData().role == ax::mojom::Role::kStaticText)
    return;

  const base::string16& old_text = old_hypertext_.hypertext;
  const base::string16& new_text = hypertext_.hypertext;

  // TODO(accessibility) Plumb through which part of text changed so we don't
  // have to guess what changed based on character differences. This can be
  // wrong in some cases as follows:
  // -- EDITABLE --
  // If editable: when part of the text node changes, assume only that part
  // changed, and not the entire thing. For example, if "car" changes to
  // "cat", assume only 1 letter changed. This code compares common characters
  // to guess what has changed.
  // -- NOT EDITABLE --
  // When part of the text changes, assume the entire node's text changed. For
  // example, if "car" changes to "cat" then assume all 3 letters changed. Note,
  // it is possible (though rare) that CharacterData methods are used to remove,
  // insert, replace or append a substring.
  bool allow_partial_text_node_changes =
      GetData().HasState(ax::mojom::State::kEditable);
  size_t prefix_index = 0;
  size_t common_prefix = 0;
  while (prefix_index < old_text.size() && prefix_index < new_text.size() &&
         IsSameHypertextCharacter(prefix_index, prefix_index)) {
    ++prefix_index;
    if (allow_partial_text_node_changes ||
        (!IsText(old_text, prefix_index) && !IsText(new_text, prefix_index))) {
      common_prefix = prefix_index;
    }
  }

  size_t suffix_index = 0;
  size_t common_suffix = 0;
  while (common_prefix + suffix_index < old_text.size() &&
         common_prefix + suffix_index < new_text.size() &&
         IsSameHypertextCharacter(old_text.size() - suffix_index - 1,
                                  new_text.size() - suffix_index - 1)) {
    ++suffix_index;
    if (allow_partial_text_node_changes ||
        (!IsText(old_text, suffix_index, true) &&
         !IsText(new_text, suffix_index, true))) {
      common_suffix = suffix_index;
    }
  }

  *start = common_prefix;
  *old_len = old_text.size() - common_prefix - common_suffix;
  *new_len = new_text.size() - common_prefix - common_suffix;
}

int AXPlatformNodeWin::GetSelectionAnchor() {
  int32_t anchor_id = GetDelegate()->GetTreeData().sel_anchor_object_id;
  AXPlatformNodeWin* anchor_object =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(anchor_id));
  if (!anchor_object)
    return -1;

  int anchor_offset = GetDelegate()->GetTreeData().sel_anchor_offset;
  return GetHypertextOffsetFromEndpoint(anchor_object, anchor_offset);
}

int AXPlatformNodeWin::GetSelectionFocus() {
  int32_t focus_id = GetDelegate()->GetTreeData().sel_focus_object_id;
  AXPlatformNodeWin* focus_object =
      static_cast<AXPlatformNodeWin*>(GetDelegate()->GetFromNodeID(focus_id));
  if (!focus_object)
    return -1;

  int focus_offset = GetDelegate()->GetTreeData().sel_focus_offset;
  return GetHypertextOffsetFromEndpoint(focus_object, focus_offset);
}

void AXPlatformNodeWin::GetSelectionOffsets(int* selection_start,
                                            int* selection_end) {
  DCHECK(selection_start && selection_end);

  if (IsPlainTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart,
                      selection_start) &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, selection_end)) {
    return;
  }

  *selection_start = GetSelectionAnchor();
  *selection_end = GetSelectionFocus();
  if (*selection_start < 0 || *selection_end < 0)
    return;

  // There are three cases when a selection would start and end on the same
  // character:
  // 1. Anchor and focus are both in a subtree that is to the right of this
  // object.
  // 2. Anchor and focus are both in a subtree that is to the left of this
  // object.
  // 3. Anchor and focus are in a subtree represented by a single embedded
  // object character.
  // Only case 3 refers to a valid selection because cases 1 and 2 fall
  // outside this object in their entirety.
  // Selections that span more than one character are by definition inside this
  // object, so checking them is not necessary.
  if (*selection_start == *selection_end && !HasCaret()) {
    *selection_start = -1;
    *selection_end = -1;
    return;
  }

  // The IA2 Spec says that if the largest of the two offsets falls on an
  // embedded object character and if there is a selection in that embedded
  // object, it should be incremented by one so that it points after the
  // embedded object character.
  // This is a signal to AT software that the embedded object is also part of
  // the selection.
  int* largest_offset =
      (*selection_start <= *selection_end) ? selection_end : selection_start;
  AXPlatformNodeWin* hyperlink =
      GetHyperlinkFromHypertextOffset(*largest_offset);
  if (!hyperlink)
    return;

  LONG n_selections = 0;
  HRESULT hr = hyperlink->get_nSelections(&n_selections);
  DCHECK(SUCCEEDED(hr));
  if (n_selections > 0)
    ++(*largest_offset);
}

}  // namespace ui
