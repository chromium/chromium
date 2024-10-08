// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_atk_hyperlink.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_text_boundary.h"
#include "ui/accessibility/platform/child_iterator.h"
#include "ui/gfx/geometry/rect_conversions.h"

// Function availability can be tested by checking whether its address is not
// nullptr.
#define WEAK_ATK_FN(x) extern "C" __attribute__((weak)) decltype(x) x

// TODO(https://crbug.com/40549424): This may be removed when support for
// Ubuntu 18.04 is dropped.
WEAK_ATK_FN(atk_component_scroll_to_point);
WEAK_ATK_FN(atk_text_scroll_substring_to_point);

namespace ui {

namespace {

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
//
// LINT.IfChange(UmaAtkApi)
enum class UmaAtkApi {
  kGetName = 0,
  kGetDescription = 1,
  kGetNChildren = 2,
  kRefChild = 3,
  kGetIndexInParent = 4,
  kGetParent = 5,
  kRefRelationSet = 6,
  kGetAttributes = 7,
  kGetRole = 8,
  kRefStateSet = 9,
  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  kMaxValue = kRefStateSet,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityATKAPIEnum)

void RecordAccessibilityAtkApi(UmaAtkApi enum_value) {
  UMA_HISTOGRAM_ENUMERATION("Accessibility.ATK-APIs", enum_value);
}

// When accepting input from clients calling the API, an ATK character offset
// of -1 can often represent the length of the string.
static const int kStringLengthOffset = -1;

// We must forward declare this because it is used by the traditional GObject
// type manipulation macros.
namespace atk_object {
GType GetType();
}  // namespace atk_object

//
// ax_platform_node_auralinux AtkObject definition and implementation.
//
#define AX_PLATFORM_NODE_AURALINUX_TYPE (atk_object::GetType())
#define AX_PLATFORM_NODE_AURALINUX(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                              AXPlatformNodeAuraLinuxObject))
#define AX_PLATFORM_NODE_AURALINUX_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                           AXPlatformNodeAuraLinuxClass))
#define IS_AX_PLATFORM_NODE_AURALINUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define IS_AX_PLATFORM_NODE_AURALINUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define AX_PLATFORM_NODE_AURALINUX_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), AX_PLATFORM_NODE_AURALINUX_TYPE, \
                             AXPlatformNodeAuraLinuxClass))

typedef struct _AXPlatformNodeAuraLinuxObject AXPlatformNodeAuraLinuxObject;
typedef struct _AXPlatformNodeAuraLinuxClass AXPlatformNodeAuraLinuxClass;

struct _AXPlatformNodeAuraLinuxObject {
  AtkObject parent;
  raw_ptr<AXPlatformNodeAuraLinux> m_object;
};

struct _AXPlatformNodeAuraLinuxClass {
  AtkObjectClass parent_class;
};

// The root-level Application object that's the parent of all top-level windows.
AXPlatformNode* g_root_application = nullptr;

// The last AtkObject with keyboard focus. Tracking this is required to emit the
// ATK_STATE_FOCUSED change to false.
AtkObject* g_current_focused = nullptr;

// The last object which was selected. Tracking this is required because
// widgets in the browser UI only emit notifications upon becoming selected,
// but clients also expect notifications when items become unselected.
AXPlatformNodeAuraLinux* g_current_selected = nullptr;

// The AtkObject with role=ATK_ROLE_FRAME that represents the toplevel desktop
// window with focus. If this window is not one of our windows, this value
// should be null. This is a weak pointer as well, so its value will also be
// null if if the AtkObject is destroyed.
AtkObject* g_active_top_level_frame = nullptr;

AtkObject* g_active_views_dialog = nullptr;

constexpr AtkRole kStaticRole = ATK_ROLE_STATIC;
constexpr AtkRole kSubscriptRole = ATK_ROLE_SUBSCRIPT;
constexpr AtkRole kSuperscriptRole = ATK_ROLE_SUPERSCRIPT;

constexpr AtkRole kAtkFootnoteRole = ATK_ROLE_FOOTNOTE;

using GetTypeFunc = GType (*)();
using GetColumnHeaderCellsFunc = GPtrArray* (*)(AtkTableCell* cell);
using GetRowHeaderCellsFunc = GPtrArray* (*)(AtkTableCell* cell);
using GetRowColumnSpanFunc = bool (*)(AtkTableCell* cell,
                                      gint* row,
                                      gint* column,
                                      gint* row_span,
                                      gint* col_span);

// The ATK API often requires pointers to be used as out arguments, while
// allowing for those pointers to be null if the caller is not interested in
// the value. This function is a simpler helper to avoid continually checking
// for null and to help prevent forgetting to check for null.
void SetIntPointerValueIfNotNull(int* pointer, int value) {
  if (pointer)
    *pointer = value;
}

// TODO(https://crbug.com/40549424): This may be removed when support for
// Ubuntu 18.04 is dropped.
bool SupportsAtkComponentScrollingInterface() {
  return atk_component_scroll_to_point;
}

// TODO(https://crbug.com/40549424): This may be removed when support for
// Ubuntu 18.04 is dropped.
bool SupportsAtkTextScrollingInterface() {
  return atk_text_scroll_substring_to_point;
}

// TODO(https://crbug.com/40549424): This may be removed when support for
// Ubuntu 18.04 is dropped.
AtkRole GetAtkRoleContentDeletion() {
  base::Version atk_version(atk_get_version());
  return atk_version.CompareTo(base::Version("2.34.0")) >= 0
             ? ATK_ROLE_CONTENT_DELETION
             : ATK_ROLE_SECTION;
}

// TODO(https://crbug.com/40549424): This may be removed when support for
// Ubuntu 18.04 is dropped.
AtkRole GetAtkRoleContentInsertion() {
  base::Version atk_version(atk_get_version());
  return atk_version.CompareTo(base::Version("2.34.0")) >= 0
             ? ATK_ROLE_CONTENT_INSERTION
             : ATK_ROLE_SECTION;
}

AtkObject* FindAtkObjectParentFrame(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* node =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  while (node) {
    if (node->GetAtkRole() == ATK_ROLE_FRAME)
      return node->GetNativeViewAccessible();
    node = AXPlatformNodeAuraLinux::FromAtkObject(node->GetParent());
  }
  return nullptr;
}

AtkObject* FindAtkObjectToplevelParentDocument(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* node =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  AtkObject* toplevel_document = nullptr;
  while (node) {
    if (node->GetAtkRole() == ATK_ROLE_DOCUMENT_WEB)
      toplevel_document = node->GetNativeViewAccessible();
    node = AXPlatformNodeAuraLinux::FromAtkObject(node->GetParent());
  }
  return toplevel_document;
}

bool IsFrameAncestorOfAtkObject(AtkObject* frame, AtkObject* atk_object) {
  AtkObject* current_frame = FindAtkObjectParentFrame(atk_object);
  while (current_frame) {
    if (current_frame == frame)
      return true;
    AXPlatformNodeAuraLinux* frame_node =
        AXPlatformNodeAuraLinux::FromAtkObject(current_frame);
    current_frame = FindAtkObjectParentFrame(frame_node->GetParent());
  }
  return false;
}

// Returns a stack of AtkObjects of activated popup menus. Since each popup
// menu and submenu has its own native window, we want to properly manage the
// activated state for their containing frames.
std::vector<AtkObject*>& GetActiveMenus() {
  static base::NoDestructor<std::vector<AtkObject*>> active_menus;
  return *active_menus;
}

std::map<AtkObject*, FindInPageResultInfo>& GetActiveFindInPageResults() {
  static base::NoDestructor<std::map<AtkObject*, FindInPageResultInfo>>
      active_results;
  return *active_results;
}

// The currently active frame is g_active_top_level_frame, unless there is an
// active menu. If there is an active menu the parent frame of the
// most-recently opened active menu should be the currently active frame.
AtkObject* ComputeActiveTopLevelFrame() {
  if (!GetActiveMenus().empty())
    return FindAtkObjectParentFrame(GetActiveMenus().back());
  return g_active_top_level_frame;
}

const char* GetUniqueAccessibilityGTypeName(
    ImplementedAtkInterfaces interface_mask) {
  // 37 characters is enough for "AXPlatformNodeAuraLinux%x" with any integer
  // value.
  static char name[37];
  snprintf(name, sizeof(name), "AXPlatformNodeAuraLinux%x",
           interface_mask.value());
  return name;
}

void SetWeakGPtrToAtkObject(AtkObject** weak_pointer, AtkObject* new_value) {
  DCHECK(weak_pointer);
  if (*weak_pointer == new_value)
    return;

  if (*weak_pointer) {
    g_object_remove_weak_pointer(G_OBJECT(*weak_pointer),
                                 reinterpret_cast<void**>(weak_pointer));
  }

  *weak_pointer = new_value;

  if (new_value) {
    g_object_add_weak_pointer(G_OBJECT(new_value),
                              reinterpret_cast<void**>(weak_pointer));
  }
}

void SetActiveTopLevelFrame(AtkObject* new_top_level_frame) {
  SetWeakGPtrToAtkObject(&g_active_top_level_frame, new_top_level_frame);
}

AXCoordinateSystem AtkCoordTypeToAXCoordinateSystem(
    AtkCoordType coordinate_type) {
  switch (coordinate_type) {
    case ATK_XY_SCREEN:
      return AXCoordinateSystem::kScreenDIPs;
    case ATK_XY_WINDOW:
      return AXCoordinateSystem::kRootFrame;
    case ATK_XY_PARENT:
      // AXCoordinateSystem does not support parent coordinates.
      NOTIMPLEMENTED();
      return AXCoordinateSystem::kFrame;
    default:
      return AXCoordinateSystem::kScreenDIPs;
  }
}

const char* BuildDescriptionFromHeaders(AXPlatformNodeDelegate* delegate,
                                        const std::vector<int32_t>& ids) {
  std::vector<std::string> names;
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = delegate->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        if (const gchar* name = atk_object_get_name(atk_header))
          names.push_back(name);
      }
    }
  }

  std::string result = base::JoinString(names, " ");

#if defined(LEAK_SANITIZER) && !BUILDFLAG(IS_NACL)
  // http://crbug.com/982839
  // atk_table_get_column_description and atk_table_get_row_description return
  // const gchar*, which suggests the caller does not gain ownership of the
  // returned string. The g_strdup below causes a new allocation, which does not
  // fit that pattern and causes a leak in tests.
  ScopedLeakSanitizerDisabler lsan_disabler;
#endif

  return g_strdup(result.c_str());
}

AtkAttributeSet* PrependAtkAttributeToAtkAttributeSet(
    const char* name,
    const char* value,
    AtkAttributeSet* attribute_set) {
  AtkAttribute* attribute =
      static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
  attribute->name = g_strdup(name);
  attribute->value = g_strdup(value);
  return g_slist_prepend(attribute_set, attribute);
}

void PrependTextAttributeToSet(const std::string& attribute,
                               const std::string& value,
                               AtkAttributeSet** attributes) {
  DCHECK(attributes);

  AtkAttribute* new_attribute =
      static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
  new_attribute->name = g_strdup(attribute.c_str());
  new_attribute->value = g_strdup(value.c_str());
  *attributes = g_slist_prepend(*attributes, new_attribute);
}

void PrependAtkTextAttributeToSet(const AtkTextAttribute attribute,
                                  const std::string& value,
                                  AtkAttributeSet** attributes) {
  PrependTextAttributeToSet(atk_text_attribute_get_name(attribute), value,
                            attributes);
}

std::string ToAtkTextAttributeColor(const std::string& color) {
  // The platform-independent color string is in the form "rgb(r, g, b)",
  // but ATK expects a string like "r, g, b". We convert the string here
  // by stripping away the unnecessary characters.
  DCHECK(base::StartsWith(color, "rgb(", base::CompareCase::INSENSITIVE_ASCII));
  DCHECK(base::EndsWith(color, ")", base::CompareCase::INSENSITIVE_ASCII));
  return color.substr(4, color.length() - 5);
}

AtkAttributeSet* ToAtkAttributeSet(const TextAttributeList& attributes) {
  AtkAttributeSet* copied_attributes = nullptr;
  for (const auto& attribute : attributes) {
    if (attribute.first == "background-color") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_BG_COLOR,
                                   ToAtkTextAttributeColor(attribute.second),
                                   &copied_attributes);
    } else if (attribute.first == "color") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_FG_COLOR,
                                   ToAtkTextAttributeColor(attribute.second),
                                   &copied_attributes);
    } else if (attribute.first == "font-family") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_FAMILY_NAME, attribute.second,
                                   &copied_attributes);
    } else if (attribute.first == "font-size") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_SIZE, attribute.second,
                                   &copied_attributes);
    } else if (attribute.first == "font-weight" && attribute.second == "bold") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_WEIGHT, "700",
                                   &copied_attributes);
    } else if (attribute.first == "font-style") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_STYLE, "italic",
                                   &copied_attributes);
    } else if (attribute.first == "text-line-through-style") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_STRIKETHROUGH, "true",
                                   &copied_attributes);
    } else if (attribute.first == "text-underline-style") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_UNDERLINE, "single",
                                   &copied_attributes);
    } else if (attribute.first == "invalid") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_INVALID, attribute.second,
                                   &copied_attributes);
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_UNDERLINE, "error",
                                   &copied_attributes);
    } else if (attribute.first == "language") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_LANGUAGE, attribute.second,
                                   &copied_attributes);
    } else if (attribute.first == "writing-mode") {
      PrependAtkTextAttributeToSet(ATK_TEXT_ATTR_DIRECTION, attribute.second,
                                   &copied_attributes);
    } else if (attribute.first == "text-position") {
      PrependTextAttributeToSet(attribute.first, attribute.second,
                                &copied_attributes);
    }
  }

  return g_slist_reverse(copied_attributes);
}

namespace atk_component {

void GetExtents(AtkComponent* atk_component,
                gint* x,
                gint* y,
                gint* width,
                gint* height,
                AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;
  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetExtents(x, y, width, height, coord_type);
}

void GetPosition(AtkComponent* atk_component,
                 gint* x,
                 gint* y,
                 AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

void GetSize(AtkComponent* atk_component, gint* width, gint* height) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

AtkObject* RefAccesibleAtPoint(AtkComponent* atk_component,
                               gint x,
                               gint y,
                               AtkCoordType coord_type) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), nullptr);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  AtkObject* result = obj->HitTestSync(x, y, coord_type);
  if (result)
    g_object_ref(result);
  return result;
}

gboolean GrabFocus(AtkComponent* atk_component) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return FALSE;

  return obj->GrabFocus();
}

gboolean ScrollTo(AtkComponent* atk_component, AtkScrollType scroll_type) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_component));
  if (!obj)
    return FALSE;

  obj->ScrollNodeIntoView(scroll_type);
  return TRUE;
}

gboolean ScrollToPoint(AtkComponent* atk_component,
                       AtkCoordType atk_coord_type,
                       gint x,
                       gint y) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_component));
  if (!obj)
    return FALSE;

  obj->ScrollToPoint(atk_coord_type, x, y);
  return TRUE;
}

void Init(AtkComponentIface* iface) {
  iface->get_extents = GetExtents;
  iface->get_position = GetPosition;
  iface->get_size = GetSize;
  iface->ref_accessible_at_point = RefAccesibleAtPoint;
  iface->grab_focus = GrabFocus;
  if (SupportsAtkComponentScrollingInterface()) {
    iface->scroll_to = ScrollTo;
    iface->scroll_to_point = ScrollToPoint;
  }
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_component

namespace atk_action {

gboolean DoAction(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), FALSE);
  g_return_val_if_fail(index >= 0, FALSE);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return FALSE;

  const std::vector<ax::mojom::Action> actions =
      obj->GetDelegate()->GetSupportedActions();
  g_return_val_if_fail(index < static_cast<gint>(actions.size()), FALSE);

  AXActionData data;
  data.action = actions[index];
  return obj->GetDelegate()->AccessibilityPerformAction(data);
}

gint GetNActions(AtkAction* atk_action) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return 0;

  return static_cast<gint>(obj->GetDelegate()->GetSupportedActions().size());
}

const gchar* GetDescription(AtkAction*, gint) {
  // Not implemented. Right now Orca does not provide this and
  // Chromium is not providing a string for the action description.
  return nullptr;
}

const gchar* GetName(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  const std::vector<ax::mojom::Action> actions =
      obj->GetDelegate()->GetSupportedActions();
  g_return_val_if_fail(index < static_cast<gint>(actions.size()), nullptr);

  if (index == 0 && obj->GetDelegate()->HasDefaultActionVerb()) {
    // If there is a default action, it will always be at index 0.
    return obj->GetDefaultActionName();
  }
  return ToString(actions[index]);
}

const gchar* GetKeybinding(AtkAction* atk_action, gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  const std::vector<ax::mojom::Action> actions =
      obj->GetDelegate()->GetSupportedActions();
  g_return_val_if_fail(index < static_cast<gint>(actions.size()), nullptr);

  if (index == 0 && obj->GetDelegate()->HasDefaultActionVerb()) {
    // If there is a default action, it will always be at index 0. Only the
    // default action has a key binding.
    return obj->GetStringAttribute(ax::mojom::StringAttribute::kAccessKey)
        .c_str();
  }
  return nullptr;
}

void Init(AtkActionIface* iface) {
  iface->do_action = DoAction;
  iface->get_n_actions = GetNActions;
  iface->get_description = GetDescription;
  iface->get_name = GetName;
  iface->get_keybinding = GetKeybinding;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_action

namespace atk_document {

const gchar* GetDocumentAttributeValue(AtkDocument* atk_doc,
                                       const gchar* attribute) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributeValue(attribute);
}

AtkAttributeSet* GetDocumentAttributes(AtkDocument* atk_doc) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributes();
}

void Init(AtkDocumentIface* iface) {
  iface->get_document_attribute_value = GetDocumentAttributeValue;
  iface->get_document_attributes = GetDocumentAttributes;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_document

namespace atk_image {

void GetImagePosition(AtkImage* atk_img,
                      gint* x,
                      gint* y,
                      AtkCoordType coord_type) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

const gchar* GetImageDescription(AtkImage* atk_img) {
  g_return_val_if_fail(ATK_IMAGE(atk_img), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

void GetImageSize(AtkImage* atk_img, gint* width, gint* height) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

void Init(AtkImageIface* iface) {
  iface->get_image_position = GetImagePosition;
  iface->get_image_description = GetImageDescription;
  iface->get_image_size = GetImageSize;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_image

namespace atk_value {

void GetCurrentValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_IS_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kValueForRange,
                                 value);
}

void GetMinimumValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_IS_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMinValueForRange,
                                 value);
}

void GetMaximumValue(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_IS_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMaxValueForRange,
                                 value);
}

void GetMinimumIncrement(AtkValue* atk_value, GValue* value) {
  g_return_if_fail(ATK_IS_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kStepValueForRange,
                                 value);
}

gboolean SetCurrentValue(AtkValue* atk_value, const GValue* value) {
  g_return_val_if_fail(ATK_IS_VALUE(atk_value), FALSE);

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return FALSE;

  std::string new_value;
  switch (G_VALUE_TYPE(value)) {
    case G_TYPE_FLOAT:
      new_value = base::NumberToString(g_value_get_float(value));
      break;
    case G_TYPE_INT:
      new_value = base::NumberToString(g_value_get_int(value));
      break;
    case G_TYPE_INT64:
      new_value = base::NumberToString(g_value_get_int64(value));
      break;
    case G_TYPE_STRING:
      new_value = g_value_get_string(value);
      break;
    default:
      return FALSE;
  }

  AXActionData data;
  data.action = ax::mojom::Action::kSetValue;
  data.value = new_value;
  obj->GetDelegate()->AccessibilityPerformAction(data);
  return TRUE;
}

void Init(AtkValueIface* iface) {
  iface->get_current_value = GetCurrentValue;
  iface->get_maximum_value = GetMaximumValue;
  iface->get_minimum_value = GetMinimumValue;
  iface->get_minimum_increment = GetMinimumIncrement;
  iface->set_current_value = SetCurrentValue;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_value

namespace atk_hyperlink {

AtkHyperlink* GetHyperlink(AtkHyperlinkImpl* atk_hyperlink_impl) {
  g_return_val_if_fail(ATK_HYPERLINK_IMPL(atk_hyperlink_impl), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_hyperlink_impl);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return 0;

  AtkHyperlink* atk_hyperlink = obj->GetAtkHyperlink();
  g_object_ref(atk_hyperlink);

  return atk_hyperlink;
}

void Init(AtkHyperlinkImplIface* iface) {
  iface->get_hyperlink = GetHyperlink;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_hyperlink

namespace atk_hypertext {

AtkHyperlink* GetLink(AtkHypertext* hypertext, int index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(hypertext));
  if (!obj)
    return nullptr;

  const AXLegacyHypertext& ax_hypertext = obj->GetAXHypertext();
  if (index >= static_cast<int>(ax_hypertext.hyperlinks.size()) || index < 0)
    return nullptr;

  int32_t id = ax_hypertext.hyperlinks[index];
  auto* link = static_cast<AXPlatformNodeAuraLinux*>(
      AXPlatformNodeBase::GetFromUniqueId(id));
  if (!link)
    return nullptr;

  return link->GetAtkHyperlink();
}

int GetNLinks(AtkHypertext* hypertext) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(hypertext));
  return obj ? obj->GetAXHypertext().hyperlinks.size() : 0;
}

int GetLinkIndex(AtkHypertext* hypertext, int char_index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(hypertext));
  if (!obj)
    return -1;

  auto it = obj->GetAXHypertext().hyperlink_offset_to_index.find(char_index);
  if (it == obj->GetAXHypertext().hyperlink_offset_to_index.end())
    return -1;
  return it->second;
}

void Init(AtkHypertextIface* iface) {
  iface->get_link = GetLink;
  iface->get_n_links = GetNLinks;
  iface->get_link_index = GetLinkIndex;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_hypertext

namespace atk_text {

gchar* GetText(AtkText* atk_text, gint start_offset, gint end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  std::u16string text = obj->GetHypertext();

  start_offset = obj->UnicodeToUTF16OffsetInText(start_offset);
  if (start_offset < 0 || start_offset >= static_cast<int>(text.size()))
    return nullptr;

  if (end_offset < 0) {
    end_offset = text.size();
  } else {
    end_offset = obj->UnicodeToUTF16OffsetInText(end_offset);
    end_offset =
        std::clamp(static_cast<int>(text.size()), start_offset, end_offset);
  }

  DCHECK_GE(start_offset, 0);
  DCHECK_GE(end_offset, start_offset);

  return g_strdup(
      base::UTF16ToUTF8(text.substr(start_offset, end_offset - start_offset))
          .c_str());
}

gint GetCharacterCount(AtkText* atk_text) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return 0;

  return obj->UTF16ToUnicodeOffsetInText(obj->GetHypertext().length());
}

gunichar GetCharacterAtOffset(AtkText* atk_text, int offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return 0;

  std::u16string text = obj->GetHypertext();
  size_t text_length = text.length();

  offset = obj->UnicodeToUTF16OffsetInText(offset);
  offset = std::max(offset, 0);
  size_t limited_offset = std::min(static_cast<size_t>(offset), text_length);

  base_icu::UChar32 code_point;
  base::ReadUnicodeCharacter(text.c_str(), text_length + 1, &limited_offset,
                             &code_point);
  return code_point;
}

gint GetOffsetAtPoint(AtkText* text, gint x, gint y, AtkCoordType coords) {
  g_return_val_if_fail(ATK_IS_TEXT(text), -1);

  AtkObject* atk_object = ATK_OBJECT(text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return -1;

  return obj->GetTextOffsetAtPoint(x, y, coords);
}

// This function returns a single character as a UTF-8 encoded C string because
// the character may be encoded into more than one byte.
char* GetCharacter(AtkText* atk_text,
                   int offset,
                   int* start_offset,
                   int* end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  *start_offset = -1;
  *end_offset = -1;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  if (offset < 0 || offset >= GetCharacterCount(atk_text))
    return nullptr;

  char* text = GetText(atk_text, offset, offset + 1);
  if (!text)
    return nullptr;

  *start_offset = offset;
  *end_offset = offset + 1;
  return text;
}

char* GetTextWithBoundaryType(AtkText* atk_text,
                              int offset,
                              ax::mojom::TextBoundary boundary,
                              int* start_offset_ptr,
                              int* end_offset_ptr) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  if (offset < 0 || offset >= atk_text_get_character_count(atk_text))
    return nullptr;

  // The offset that we receive from the API is a Unicode character offset.
  // Since we calculate boundaries in terms of UTF-16 code point offsets, we
  // need to convert this input value.
  offset = obj->UnicodeToUTF16OffsetInText(offset);

  int start_offset = obj->FindTextBoundary(
      boundary, offset, ax::mojom::MoveDirection::kBackward,
      ax::mojom::TextAffinity::kDownstream);
  int end_offset = obj->FindTextBoundary(boundary, offset,
                                         ax::mojom::MoveDirection::kForward,
                                         ax::mojom::TextAffinity::kDownstream);
  if (start_offset < 0 || end_offset < 0)
    return nullptr;

  DCHECK_LE(start_offset, end_offset)
      << "Start offset should be less than or equal the end offset.";

  // The ATK API is also expecting Unicode character offsets as output
  // values.
  *start_offset_ptr = obj->UTF16ToUnicodeOffsetInText(start_offset);
  *end_offset_ptr = obj->UTF16ToUnicodeOffsetInText(end_offset);

  std::u16string text = obj->GetHypertext();
  DCHECK_LE(end_offset, static_cast<int>(text.size()));

  std::u16string substr = text.substr(start_offset, end_offset - start_offset);
  return g_strdup(base::UTF16ToUTF8(substr).c_str());
}

char* GetTextAtOffset(AtkText* atk_text,
                      int offset,
                      AtkTextBoundary atk_boundary,
                      int* start_offset,
                      int* end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);
  ax::mojom::TextBoundary boundary = FromAtkTextBoundary(atk_boundary);
  return GetTextWithBoundaryType(atk_text, offset, boundary, start_offset,
                                 end_offset);
}

char* GetTextAfterOffset(AtkText* atk_text,
                         int offset,
                         AtkTextBoundary boundary,
                         int* start_offset,
                         int* end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  if (boundary != ATK_TEXT_BOUNDARY_CHAR) {
    *start_offset = -1;
    *end_offset = -1;
    return nullptr;
  }

  // ATK does not offer support for the special negative index and we don't
  // want to do arithmetic on that value below.
  if (offset == kStringLengthOffset)
    return nullptr;

  return GetCharacter(atk_text, offset + 1, start_offset, end_offset);
}

char* GetTextBeforeOffset(AtkText* atk_text,
                          int offset,
                          AtkTextBoundary boundary,
                          int* start_offset,
                          int* end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  if (boundary != ATK_TEXT_BOUNDARY_CHAR) {
    *start_offset = -1;
    *end_offset = -1;
    return nullptr;
  }

  // ATK does not offer support for the special negative index and we don't
  // want to do arithmetic on that value below.
  if (offset == kStringLengthOffset)
    return nullptr;

  return GetCharacter(atk_text, offset - 1, start_offset, end_offset);
}

gint GetCaretOffset(AtkText* atk_text) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), -1);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return -1;
  return obj->GetCaretOffset();
}

gboolean SetCaretOffset(AtkText* atk_text, gint offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;
  if (!obj->SetCaretOffset(offset))
    return FALSE;

  // Orca expects atk_text_set_caret_offset to either focus the target element
  // or set the sequential focus navigation starting point there.
  int utf16_offset = obj->UnicodeToUTF16OffsetInText(offset);
  obj->GrabFocusOrSetSequentialFocusNavigationStartingPointAtOffset(
      utf16_offset);

  return TRUE;
}

int GetNSelections(AtkText* atk_text) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), 0);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return 0;

  if (obj->HasSelection())
    return 1;

  std::optional<FindInPageResultInfo> result =
      obj->GetSelectionOffsetsFromFindInPage();
  if (result.has_value() && result->node == ATK_OBJECT(atk_text))
    return 1;

  return 0;
}

gchar* GetSelection(AtkText* atk_text,
                    int selection_num,
                    int* start_offset,
                    int* end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return nullptr;
  if (selection_num != 0)
    return nullptr;

  return obj->GetSelectionWithText(start_offset, end_offset);
}

gboolean RemoveSelection(AtkText* atk_text, int selection_num) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), FALSE);

  if (selection_num != 0)
    return FALSE;

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  // Simply collapse the selection to the position of the caret if a caret is
  // visible, otherwise set the selection to 0.
  int selection_end = obj->UTF16ToUnicodeOffsetInText(
      obj->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd));
  return SetCaretOffset(atk_text, selection_end);
}

gboolean SetSelection(AtkText* atk_text,
                      int selection_num,
                      int start_offset,
                      int end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), FALSE);

  if (selection_num != 0)
    return FALSE;

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  return obj->SetTextSelectionForAtkText(start_offset, end_offset);
}

gboolean AddSelection(AtkText* atk_text, int start_offset, int end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), FALSE);

  // We only support one selection.
  return SetSelection(atk_text, 0, start_offset, end_offset);
}

char* GetStringAtOffset(AtkText* atk_text,
                        int offset,
                        AtkTextGranularity atk_granularity,
                        int* start_offset,
                        int* end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  *start_offset = -1;
  *end_offset = -1;

  ax::mojom::TextBoundary boundary = FromAtkTextGranularity(atk_granularity);
  return GetTextWithBoundaryType(atk_text, offset, boundary, start_offset,
                                 end_offset);
}

gfx::Rect GetUnclippedParentHypertextRangeBoundsRect(
    AXPlatformNodeDelegate* ax_platform_node_delegate,
    const int start_offset,
    const int end_offset) {
  const AXPlatformNode* parent_platform_node =
      AXPlatformNode::FromNativeViewAccessible(
          ax_platform_node_delegate->GetParent());
  if (!parent_platform_node)
    return gfx::Rect();

  const AXPlatformNodeDelegate* parent_ax_platform_node_delegate =
      parent_platform_node->GetDelegate();
  if (!parent_ax_platform_node_delegate)
    return gfx::Rect();

  return ax_platform_node_delegate->GetHypertextRangeBoundsRect(
             start_offset, end_offset, AXCoordinateSystem::kRootFrame,
             AXClippingBehavior::kUnclipped) -
         parent_ax_platform_node_delegate
             ->GetBoundsRect(AXCoordinateSystem::kRootFrame,
                             AXClippingBehavior::kClipped)
             .OffsetFromOrigin();
}

void GetCharacterExtents(AtkText* atk_text,
                         int offset,
                         int* x,
                         int* y,
                         int* width,
                         int* height,
                         AtkCoordType coordinate_type) {
  g_return_if_fail(ATK_IS_TEXT(atk_text));

  gfx::Rect rect;
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (obj) {
    switch (coordinate_type) {
      case ATK_XY_PARENT:
        rect = GetUnclippedParentHypertextRangeBoundsRect(obj->GetDelegate(),
                                                          offset, offset + 1);
        break;
      default:
        rect = obj->GetDelegate()->GetHypertextRangeBoundsRect(
            obj->UnicodeToUTF16OffsetInText(offset),
            obj->UnicodeToUTF16OffsetInText(offset + 1),
            AtkCoordTypeToAXCoordinateSystem(coordinate_type),
            AXClippingBehavior::kUnclipped);
        break;
    }
  }

  if (x)
    *x = rect.x();
  if (y)
    *y = rect.y();
  if (width)
    *width = rect.width();
  if (height)
    *height = rect.height();
}

void GetRangeExtents(AtkText* atk_text,
                     int start_offset,
                     int end_offset,
                     AtkCoordType coordinate_type,
                     AtkTextRectangle* out_rectangle) {
  g_return_if_fail(ATK_IS_TEXT(atk_text));

  if (!out_rectangle)
    return;

  gfx::Rect rect;
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (obj) {
    switch (coordinate_type) {
      case ATK_XY_PARENT:
        rect = GetUnclippedParentHypertextRangeBoundsRect(
            obj->GetDelegate(), start_offset, end_offset);
        break;
      default:
        rect = obj->GetDelegate()->GetHypertextRangeBoundsRect(
            obj->UnicodeToUTF16OffsetInText(start_offset),
            obj->UnicodeToUTF16OffsetInText(end_offset),
            AtkCoordTypeToAXCoordinateSystem(coordinate_type),
            AXClippingBehavior::kUnclipped);
        break;
    }
  }

  out_rectangle->x = rect.x();
  out_rectangle->y = rect.y();
  out_rectangle->width = rect.width();
  out_rectangle->height = rect.height();
}

AtkAttributeSet* GetRunAttributes(AtkText* atk_text,
                                  gint offset,
                                  gint* start_offset,
                                  gint* end_offset) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  SetIntPointerValueIfNotNull(start_offset, -1);
  SetIntPointerValueIfNotNull(end_offset, -1);

  if (offset < 0 || offset > GetCharacterCount(atk_text))
    return nullptr;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  return ToAtkAttributeSet(
      obj->GetTextAttributes(offset, start_offset, end_offset));
}

AtkAttributeSet* GetDefaultAttributes(AtkText* atk_text) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;
  return ToAtkAttributeSet(obj->GetDefaultTextAttributes());
}

gboolean ScrollSubstringTo(AtkText* atk_text,
                           gint start_offset,
                           gint end_offset,
                           AtkScrollType scroll_type) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  return obj->ScrollSubstringIntoView(scroll_type, start_offset, end_offset);
}

gboolean ScrollSubstringToPoint(AtkText* atk_text,
                                gint start_offset,
                                gint end_offset,
                                AtkCoordType atk_coord_type,
                                gint x,
                                gint y) {
  g_return_val_if_fail(ATK_IS_TEXT(atk_text), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(atk_text));
  if (!obj)
    return FALSE;

  return obj->ScrollSubstringToPoint(start_offset, end_offset, atk_coord_type,
                                     x, y);
}

void Init(AtkTextIface* iface) {
  iface->get_text = GetText;
  iface->get_character_count = GetCharacterCount;
  iface->get_character_at_offset = GetCharacterAtOffset;
  iface->get_offset_at_point = GetOffsetAtPoint;
  iface->get_text_after_offset = GetTextAfterOffset;
  iface->get_text_before_offset = GetTextBeforeOffset;
  iface->get_text_at_offset = GetTextAtOffset;
  iface->get_caret_offset = GetCaretOffset;
  iface->set_caret_offset = SetCaretOffset;
  iface->get_character_extents = GetCharacterExtents;
  iface->get_range_extents = GetRangeExtents;
  iface->get_n_selections = GetNSelections;
  iface->get_selection = GetSelection;
  iface->add_selection = AddSelection;
  iface->remove_selection = RemoveSelection;
  iface->set_selection = SetSelection;

  iface->get_run_attributes = GetRunAttributes;
  iface->get_default_attributes = GetDefaultAttributes;

  iface->get_string_at_offset = GetStringAtOffset;

  if (SupportsAtkTextScrollingInterface()) {
    iface->scroll_substring_to = ScrollSubstringTo;
    iface->scroll_substring_to_point = ScrollSubstringToPoint;
  }
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_text

namespace atk_window {
void Init(AtkWindowIface* iface) {}
const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};
}  // namespace atk_window

namespace atk_selection {

gboolean AddSelection(AtkSelection* selection, gint index) {
  g_return_val_if_fail(ATK_IS_SELECTION(selection), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;
  if (index < 0 || static_cast<size_t>(index) >= obj->GetChildCount())
    return FALSE;

  AXPlatformNodeAuraLinux* child =
      AXPlatformNodeAuraLinux::FromAtkObject(obj->ChildAtIndex(index));
  if (!child)
    return FALSE;

  if (!child->SupportsSelectionWithAtkSelection())
    return FALSE;

  bool selected = child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  if (selected)
    return TRUE;

  AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  return child->GetDelegate()->AccessibilityPerformAction(data);
}

gboolean ClearSelection(AtkSelection* selection) {
  g_return_val_if_fail(ATK_IS_SELECTION(selection), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  bool success = true;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AXPlatformNodeAuraLinux::FromAtkObject(obj->ChildAtIndex(i));
    if (!child)
      continue;

    if (!child->SupportsSelectionWithAtkSelection())
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (!selected)
      continue;

    AXActionData data;
    data.action = ax::mojom::Action::kDoDefault;
    success = success && child->GetDelegate()->AccessibilityPerformAction(data);
  }

  return success;
}

AtkObject* RefSelection(AtkSelection* selection, gint requested_child_index) {
  g_return_val_if_fail(ATK_IS_SELECTION(selection), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(selection));
  if (!obj)
    return nullptr;

  if (auto* selected_child = obj->GetSelectedItem(requested_child_index)) {
    if (AtkObject* atk_object = selected_child->GetNativeViewAccessible()) {
      g_object_ref(atk_object);
      return atk_object;
    }
  }

  return nullptr;
}

gint GetSelectionCount(AtkSelection* selection) {
  g_return_val_if_fail(ATK_IS_SELECTION(selection), 0);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(selection));
  if (!obj)
    return 0;

  return obj->GetSelectionCount();
}

gboolean IsChildSelected(AtkSelection* selection, gint index) {
  g_return_val_if_fail(ATK_IS_SELECTION(selection), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;
  if (index < 0 || static_cast<size_t>(index) >= obj->GetChildCount())
    return FALSE;

  AXPlatformNodeAuraLinux* child =
      AXPlatformNodeAuraLinux::FromAtkObject(obj->ChildAtIndex(index));
  return child && child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

gboolean RemoveSelection(AtkSelection* selection,
                         gint index_into_selected_children) {
  g_return_val_if_fail(ATK_IS_SELECTION(selection), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AXPlatformNodeAuraLinux::FromAtkObject(obj->ChildAtIndex(i));
    if (!child)
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (selected && index_into_selected_children == 0) {
      if (!child->SupportsSelectionWithAtkSelection())
        return FALSE;

      AXActionData data;
      data.action = ax::mojom::Action::kDoDefault;
      return child->GetDelegate()->AccessibilityPerformAction(data);
    } else if (selected) {
      index_into_selected_children--;
    }
  }

  return FALSE;
}

gboolean SelectAllSelection(AtkSelection* selection) {
  g_return_val_if_fail(ATK_IS_SELECTION(selection), FALSE);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(selection));
  if (!obj)
    return FALSE;

  int child_count = obj->GetChildCount();
  bool success = true;
  for (int i = 0; i < child_count; ++i) {
    AXPlatformNodeAuraLinux* child =
        AXPlatformNodeAuraLinux::FromAtkObject(obj->ChildAtIndex(i));
    if (!child)
      continue;

    if (!child->SupportsSelectionWithAtkSelection())
      continue;

    bool selected =
        child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    if (selected)
      continue;

    AXActionData data;
    data.action = ax::mojom::Action::kDoDefault;
    success = success && child->GetDelegate()->AccessibilityPerformAction(data);
  }

  return success;
}

void Init(AtkSelectionIface* iface) {
  iface->add_selection = AddSelection;
  iface->clear_selection = ClearSelection;
  iface->ref_selection = RefSelection;
  iface->get_selection_count = GetSelectionCount;
  iface->is_child_selected = IsChildSelected;
  iface->remove_selection = RemoveSelection;
  iface->select_all_selection = SelectAllSelection;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_selection

namespace atk_table {

AtkObject* RefAt(AtkTable* table, gint row, gint column) {
  g_return_val_if_fail(ATK_IS_TABLE(table), nullptr);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    if (AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      if (AtkObject* atk_cell = cell->GetNativeViewAccessible()) {
        g_object_ref(atk_cell);
        return atk_cell;
      }
    }
  }

  return nullptr;
}

gint GetIndexAt(AtkTable* table, gint row, gint column) {
  g_return_val_if_fail(ATK_IS_TABLE(table), -1);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      DCHECK(cell->GetTableCellIndex().has_value());
      return cell->GetTableCellIndex().value();
    }
  }

  return -1;
}

gint GetColumnAtIndex(AtkTable* table, gint index) {
  g_return_val_if_fail(ATK_IS_TABLE(table), -1);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(index)) {
      DCHECK(cell->GetTableColumn().has_value());
      return cell->GetTableColumn().value();
    }
  }

  return -1;
}

gint GetRowAtIndex(AtkTable* table, gint index) {
  g_return_val_if_fail(ATK_IS_TABLE(table), -1);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(index)) {
      DCHECK(cell->GetTableRow().has_value());
      return cell->GetTableRow().value();
    }
  }

  return -1;
}

gint GetNColumns(AtkTable* table) {
  g_return_val_if_fail(ATK_IS_TABLE(table), 0);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    // If the object is not a table, we return 0.
    return obj->GetTableColumnCount().value_or(0);
  }

  return 0;
}

gint GetNRows(AtkTable* table) {
  g_return_val_if_fail(ATK_IS_TABLE(table), 0);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    // If the object is not a table, we return 0.
    return obj->GetTableRowCount().value_or(0);
  }

  return 0;
}

gint GetColumnExtentAt(AtkTable* table, gint row, gint column) {
  g_return_val_if_fail(ATK_IS_TABLE(table), 0);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      DCHECK(cell->GetTableColumnSpan().has_value());
      return cell->GetTableColumnSpan().value();
    }
  }

  return 0;
}

gint GetRowExtentAt(AtkTable* table, gint row, gint column) {
  g_return_val_if_fail(ATK_IS_TABLE(table), 0);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    if (const AXPlatformNodeBase* cell = obj->GetTableCell(row, column)) {
      DCHECK(cell->GetTableRowSpan().has_value());
      return cell->GetTableRowSpan().value();
    }
  }

  return 0;
}

AtkObject* GetColumnHeader(AtkTable* table, gint column) {
  g_return_val_if_fail(ATK_IS_TABLE(table), nullptr);

  auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  // AtkTable supports only one column header object. So return the first one
  // we find. In the case of multiple headers, ATs can fall back on the column
  // description.
  std::vector<int32_t> ids = obj->GetDelegate()->GetColHeaderNodeIds(column);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        g_object_ref(atk_header);
        return atk_header;
      }
    }
  }

  return nullptr;
}

AtkObject* GetRowHeader(AtkTable* table, gint row) {
  g_return_val_if_fail(ATK_IS_TABLE(table), nullptr);

  auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  // AtkTable supports only one row header object. So return the first one
  // we find. In the case of multiple headers, ATs can fall back on the row
  // description.
  std::vector<int32_t> ids = obj->GetDelegate()->GetRowHeaderNodeIds(row);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* header = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_header = header->GetNativeViewAccessible()) {
        g_object_ref(atk_header);
        return atk_header;
      }
    }
  }

  return nullptr;
}

AtkObject* GetCaption(AtkTable* table) {
  g_return_val_if_fail(ATK_IS_TABLE(table), nullptr);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table))) {
    if (auto* caption = obj->GetTableCaption())
      return caption->GetNativeViewAccessible();
  }

  return nullptr;
}

const gchar* GetColumnDescription(AtkTable* table, gint column) {
  g_return_val_if_fail(ATK_IS_TABLE(table), nullptr);

  auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  std::vector<int32_t> ids = obj->GetDelegate()->GetColHeaderNodeIds(column);
  return BuildDescriptionFromHeaders(obj->GetDelegate(), ids);
}

const gchar* GetRowDescription(AtkTable* table, gint row) {
  g_return_val_if_fail(ATK_IS_TABLE(table), nullptr);

  auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(table));
  if (!obj)
    return nullptr;

  std::vector<int32_t> ids = obj->GetDelegate()->GetRowHeaderNodeIds(row);
  return BuildDescriptionFromHeaders(obj->GetDelegate(), ids);
}

void Init(AtkTableIface* iface) {
  iface->ref_at = RefAt;
  iface->get_index_at = GetIndexAt;
  iface->get_column_at_index = GetColumnAtIndex;
  iface->get_row_at_index = GetRowAtIndex;
  iface->get_n_columns = GetNColumns;
  iface->get_n_rows = GetNRows;
  iface->get_column_extent_at = GetColumnExtentAt;
  iface->get_row_extent_at = GetRowExtentAt;
  iface->get_column_header = GetColumnHeader;
  iface->get_row_header = GetRowHeader;
  iface->get_caption = GetCaption;
  iface->get_column_description = GetColumnDescription;
  iface->get_row_description = GetRowDescription;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_table

namespace atk_table_cell {

gint GetColumnSpan(AtkTableCell* cell) {
  g_return_val_if_fail(
      G_TYPE_CHECK_INSTANCE_TYPE((cell), atk_table_cell_get_type()), 0);

  if (const AXPlatformNodeBase* obj =
          AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(cell))) {
    // If the object is not a cell, we return 0.
    return obj->GetTableColumnSpan().value_or(0);
  }

  return 0;
}

GPtrArray* GetColumnHeaderCells(AtkTableCell* cell) {
  g_return_val_if_fail(
      G_TYPE_CHECK_INSTANCE_TYPE((cell), atk_table_cell_get_type()), nullptr);

  GPtrArray* array = g_ptr_array_new_with_free_func(g_object_unref);

  auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(cell));
  if (!obj)
    return array;

  // AtkTableCell is implemented on cells, row headers, and column headers.
  // Calling GetColHeaderNodeIds() on a column header cell will include that
  // column header, along with any other column headers in the column which
  // may or may not describe the header cell in question. Therefore, just return
  // headers for non-header cells.
  if (obj->GetAtkRole() != ATK_ROLE_TABLE_CELL)
    return array;

  std::optional<int> col_index = obj->GetTableColumn();
  if (!col_index)
    return array;

  const std::vector<int32_t> ids =
      obj->GetDelegate()->GetColHeaderNodeIds(*col_index);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* node = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_node = node->GetNativeViewAccessible()) {
        g_ptr_array_add(array, g_object_ref(atk_node));
      }
    }
  }

  return array;
}

gboolean GetCellPosition(AtkTableCell* cell, gint* row, gint* column) {
  g_return_val_if_fail(
      G_TYPE_CHECK_INSTANCE_TYPE((cell), atk_table_cell_get_type()), FALSE);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(cell))) {
    std::optional<int> row_index = obj->GetTableRow();
    std::optional<int> col_index = obj->GetTableColumn();
    if (!row_index || !col_index)
      return false;

    *row = *row_index;
    *column = *col_index;
    return true;
  }

  return false;
}

gint GetRowSpan(AtkTableCell* cell) {
  g_return_val_if_fail(
      G_TYPE_CHECK_INSTANCE_TYPE((cell), atk_table_cell_get_type()), 0);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(cell))) {
    // If the object is not a cell, we return 0.
    return obj->GetTableRowSpan().value_or(0);
  }

  return 0;
}

GPtrArray* GetRowHeaderCells(AtkTableCell* cell) {
  g_return_val_if_fail(
      G_TYPE_CHECK_INSTANCE_TYPE((cell), atk_table_cell_get_type()), nullptr);

  GPtrArray* array = g_ptr_array_new_with_free_func(g_object_unref);

  auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(cell));
  if (!obj)
    return array;

  // AtkTableCell is implemented on cells, row headers, and column headers.
  // Calling GetRowHeaderNodeIds() on a row header cell will include that
  // row header, along with any other row headers in the row which may or
  // may not describe the header cell in question. Therefore, just return
  // headers for non-header cells.
  if (obj->GetAtkRole() != ATK_ROLE_TABLE_CELL)
    return array;

  std::optional<int> row_index = obj->GetTableRow();
  if (!row_index)
    return array;

  const std::vector<int32_t> ids =
      obj->GetDelegate()->GetRowHeaderNodeIds(*row_index);
  for (const auto& node_id : ids) {
    if (AXPlatformNode* node = obj->GetDelegate()->GetFromNodeID(node_id)) {
      if (AtkObject* atk_node = node->GetNativeViewAccessible()) {
        g_ptr_array_add(array, g_object_ref(atk_node));
      }
    }
  }

  return array;
}

AtkObject* GetTable(AtkTableCell* cell) {
  g_return_val_if_fail(
      G_TYPE_CHECK_INSTANCE_TYPE((cell), atk_table_cell_get_type()), nullptr);

  if (auto* obj = AXPlatformNodeAuraLinux::FromAtkObject(ATK_OBJECT(cell))) {
    if (auto* table = obj->GetTable())
      return table->GetNativeViewAccessible();
  }

  return nullptr;
}

using AtkTableCellIface = struct _AtkTableCellIface;

void Init(AtkTableCellIface* iface) {
  iface->get_column_span = GetColumnSpan;
  iface->get_column_header_cells = GetColumnHeaderCells;
  iface->get_position = GetCellPosition;
  iface->get_row_span = GetRowSpan;
  iface->get_row_header_cells = GetRowHeaderCells;
  iface->get_table = GetTable;
}

const GInterfaceInfo Info = {reinterpret_cast<GInterfaceInitFunc>(Init),
                             nullptr, nullptr};

}  // namespace atk_table_cell

namespace atk_object {

gpointer kAXPlatformNodeAuraLinuxParentClass = nullptr;

const gchar* GetName(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  if (!obj->IsNameExposed())
    return nullptr;

  ax::mojom::NameFrom name_from = obj->GetNameFrom();
  if (obj->GetName().empty() &&
      name_from != ax::mojom::NameFrom::kAttributeExplicitlyEmpty) {
    return nullptr;
  }

  obj->accessible_name_ = obj->GetName();
  return obj->accessible_name_.c_str();
}

const gchar* AtkGetName(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kGetName);
  return GetName(atk_object);
}

const gchar* GetDescription(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

const gchar* AtkGetDescription(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kGetDescription);
  return GetDescription(atk_object);
}

gint GetNChildren(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), 0);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return 0;

  return obj->GetChildCount();
}

gint AtkGetNChildren(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kGetNChildren);
  return GetNChildren(atk_object);
}

AtkObject* RefChild(AtkObject* atk_object, gint index) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  if (index < 0 || static_cast<size_t>(index) >= obj->GetChildCount())
    return nullptr;

  AtkObject* result = obj->ChildAtIndex(index);
  if (result)
    g_object_ref(result);
  return result;
}

AtkObject* AtkRefChild(AtkObject* atk_object, gint index) {
  RecordAccessibilityAtkApi(UmaAtkApi::kRefChild);
  return RefChild(atk_object, index);
}

gint GetIndexInParent(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), -1);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return -1;

  auto index_in_parent = obj->GetIndexInParent();
  return index_in_parent.has_value()
             ? static_cast<gint>(index_in_parent.value())
             : -1;
}

gint AtkGetIndexInParent(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kGetIndexInParent);
  return GetIndexInParent(atk_object);
}

AtkObject* GetParent(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetParent();
}

AtkObject* AtkGetParent(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kGetParent);
  return GetParent(atk_object);
}

AtkRelationSet* RefRelationSet(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return atk_relation_set_new();
  return obj->GetAtkRelations();
}

AtkRelationSet* AtkRefRelationSet(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kRefRelationSet);
  // Enables AX mode. Most AT does not call AtkRefRelationSet, but Orca does,
  // which is why it's a good signal to enable accessibility for Orca users
  // without too many false positives.
  AXPlatformNodeAuraLinux::EnableAXMode();
  return RefRelationSet(atk_object);
}

AtkAttributeSet* GetAttributes(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), nullptr);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetAtkAttributes();
}

AtkAttributeSet* AtkGetAttributes(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kGetAttributes);
  // Enables AX mode. Most AT does not call AtkGetAttributes, but Orca does,
  // which is why it's a good signal to enable accessibility for Orca users
  // without too many false positives.
  AXPlatformNodeAuraLinux::EnableAXMode();
  return GetAttributes(atk_object);
}

AtkRole GetRole(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), ATK_ROLE_INVALID);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->GetAtkRole();
}

AtkRole AtkGetRole(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kGetRole);
  return GetRole(atk_object);
}

AtkStateSet* RefStateSet(AtkObject* atk_object) {
  g_return_val_if_fail(ATK_IS_OBJECT(atk_object), nullptr);

  AtkStateSet* atk_state_set =
      ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
          ->ref_state_set(atk_object);

  AXPlatformNodeAuraLinux* obj =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  if (!obj) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFUNCT);
  } else {
    obj->GetAtkState(atk_state_set);
  }
  return atk_state_set;
}

AtkStateSet* AtkRefStateSet(AtkObject* atk_object) {
  RecordAccessibilityAtkApi(UmaAtkApi::kRefStateSet);
  return RefStateSet(atk_object);
}

void Initialize(AtkObject* atk_object, gpointer data) {
  if (ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->initialize) {
    ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
        ->initialize(atk_object, data);
  }

  AX_PLATFORM_NODE_AURALINUX(atk_object)->m_object =
      reinterpret_cast<AXPlatformNodeAuraLinux*>(data);
}

void Finalize(GObject* atk_object) {
  G_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->finalize(atk_object);
}

void ClassInit(gpointer class_pointer, gpointer /* class_data */) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(class_pointer);
  kAXPlatformNodeAuraLinuxParentClass = g_type_class_peek_parent(gobject_class);
  gobject_class->finalize = Finalize;

  AtkObjectClass* atk_object_class = ATK_OBJECT_CLASS(gobject_class);
  atk_object_class->initialize = Initialize;
  atk_object_class->get_name = AtkGetName;
  atk_object_class->get_description = AtkGetDescription;
  atk_object_class->get_parent = AtkGetParent;
  atk_object_class->get_n_children = AtkGetNChildren;
  atk_object_class->ref_child = AtkRefChild;
  atk_object_class->get_role = AtkGetRole;
  atk_object_class->ref_state_set = AtkRefStateSet;
  atk_object_class->get_index_in_parent = AtkGetIndexInParent;
  atk_object_class->ref_relation_set = AtkRefRelationSet;
  atk_object_class->get_attributes = AtkGetAttributes;
}

GType GetType() {
  AXPlatformNodeAuraLinux::EnsureGTypeInit();

  static gsize type_id = 0;
  if (g_once_init_enter(&type_id)) {
    static const GTypeInfo type_info = {
        sizeof(AXPlatformNodeAuraLinuxClass),  // class_size
        nullptr,                               // base_init
        nullptr,                               // base_finalize
        atk_object::ClassInit,
        nullptr,                                // class_finalize
        nullptr,                                // class_data
        sizeof(AXPlatformNodeAuraLinuxObject),  // instance_size
        0,                                      // n_preallocs
        nullptr,                                // instance_init
        nullptr                                 // value_table
    };

    GType type = g_type_register_static(
        ATK_TYPE_OBJECT, "AXPlatformNodeAuraLinux", &type_info, GTypeFlags(0));
    g_once_init_leave(&type_id, type);
  }

  return type_id;
}

void Detach(AXPlatformNodeAuraLinuxObject* atk_object) {
  if (!atk_object->m_object)
    return;

  atk_object->m_object = nullptr;
}

}  //  namespace atk_object

}  // namespace

void AXPlatformNodeAuraLinux::EnsureGTypeInit() {
#if !GLIB_CHECK_VERSION(2, 36, 0)
  static bool first_time = true;
  if (first_time) [[unlikely]] {
    g_type_init();
    first_time = false;
  }
#endif
}

// static
ImplementedAtkInterfaces AXPlatformNodeAuraLinux::GetGTypeInterfaceMask(
    const AXNodeData& data) {
  // The default implementation set includes the AtkComponent and AtkAction
  // interfaces, which are provided by all the AtkObjects that we produce.
  ImplementedAtkInterfaces interface_mask;

  if (!IsImageOrVideo(data.role)) {
    interface_mask.Add(ImplementedAtkInterfaces::Value::kText);
    if (!data.IsAtomicTextField())
      interface_mask.Add(ImplementedAtkInterfaces::Value::kHypertext);
  }

  if (data.IsRangeValueSupported())
    interface_mask.Add(ImplementedAtkInterfaces::Value::kValue);

  if (ui::IsPlatformDocument(data.role))
    interface_mask.Add(ImplementedAtkInterfaces::Value::kDocument);

  if (IsImage(data.role))
    interface_mask.Add(ImplementedAtkInterfaces::Value::kImage);

  // The AtkHyperlinkImpl interface allows getting a AtkHyperlink from an
  // AtkObject. It is indeed implemented by actual web hyperlinks, but also by
  // objects that will become embedded objects in ATK hypertext, so the name is
  // a bit of a misnomer from the ATK API.
  if (IsLink(data.role) || !ui::IsText(data.role))
    interface_mask.Add(ImplementedAtkInterfaces::Value::kHyperlink);

  if (data.role == ax::mojom::Role::kWindow)
    interface_mask.Add(ImplementedAtkInterfaces::Value::kWindow);

  if (IsContainerWithSelectableChildren(data.role))
    interface_mask.Add(ImplementedAtkInterfaces::Value::kSelection);

  if (IsTableLike(data.role))
    interface_mask.Add(ImplementedAtkInterfaces::Value::kTable);

  // Because the TableCell Interface is only supported in ATK version 2.12 and
  // later, GetAccessibilityGType has a runtime check to verify we have a recent
  // enough version. If we don't, GetAccessibilityGType will exclude
  // AtkTableCell from the supported interfaces and none of its methods or
  // properties will be exposed to assistive technologies.
  if (IsCellOrTableHeader(data.role))
    interface_mask.Add(ImplementedAtkInterfaces::Value::kTableCell);

  return interface_mask;
}

GType AXPlatformNodeAuraLinux::GetAccessibilityGType() {
  static const GTypeInfo type_info = {
      sizeof(AXPlatformNodeAuraLinuxClass),
      (GBaseInitFunc) nullptr,
      (GBaseFinalizeFunc) nullptr,
      (GClassInitFunc) nullptr,
      (GClassFinalizeFunc) nullptr,
      nullptr,                               /* class data */
      sizeof(AXPlatformNodeAuraLinuxObject), /* instance size */
      0,                                     /* nb preallocs */
      (GInstanceInitFunc) nullptr,
      nullptr /* value table */
  };

  const char* atk_type_name = GetUniqueAccessibilityGTypeName(interface_mask_);
  GType type = g_type_from_name(atk_type_name);
  if (type)
    return type;

  type = g_type_register_static(AX_PLATFORM_NODE_AURALINUX_TYPE, atk_type_name,
                                &type_info, GTypeFlags(0));

  // The AtkComponent and AtkAction interfaces are always supported.
  g_type_add_interface_static(type, ATK_TYPE_COMPONENT, &atk_component::Info);
  g_type_add_interface_static(type, ATK_TYPE_ACTION, &atk_action::Info);

  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kDocument))
    g_type_add_interface_static(type, ATK_TYPE_DOCUMENT, &atk_document::Info);
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kImage))
    g_type_add_interface_static(type, ATK_TYPE_IMAGE, &atk_image::Info);
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kValue))
    g_type_add_interface_static(type, ATK_TYPE_VALUE, &atk_value::Info);
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kHyperlink)) {
    g_type_add_interface_static(type, ATK_TYPE_HYPERLINK_IMPL,
                                &atk_hyperlink::Info);
  }
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kHypertext))
    g_type_add_interface_static(type, ATK_TYPE_HYPERTEXT, &atk_hypertext::Info);
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kText))
    g_type_add_interface_static(type, ATK_TYPE_TEXT, &atk_text::Info);
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kWindow))
    g_type_add_interface_static(type, ATK_TYPE_WINDOW, &atk_window::Info);
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kSelection))
    g_type_add_interface_static(type, ATK_TYPE_SELECTION, &atk_selection::Info);
  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kTable))
    g_type_add_interface_static(type, ATK_TYPE_TABLE, &atk_table::Info);

  if (interface_mask_.Implements(ImplementedAtkInterfaces::Value::kTableCell)) {
    g_type_add_interface_static(type, atk_table_cell_get_type(),
                                &atk_table_cell::Info);
  }

  return type;
}

void AXPlatformNodeAuraLinux::SetDocumentParentOnFrameIfNecessary() {
  if (GetRole() != ax::mojom::Role::kRootWebArea) {
    return;
  }

  if (!GetDelegate()->IsWebContent())
    return;

  // If there is a parent, then this is not the root document.
  if (GetDelegate()->node()->GetUnignoredParent()) {
    return;
  }

  // Get the ATK parent, which will cross over into the UI hierarchy.
  AtkObject* parent_atk_object = GetParent();
  AXPlatformNodeAuraLinux* parent =
      AXPlatformNodeAuraLinux::FromAtkObject(parent_atk_object);
  if (!parent)
    return;

  if (parent->GetDelegate()->IsWebContent())
    return;

  AXPlatformNodeAuraLinux* frame = AXPlatformNodeAuraLinux::FromAtkObject(
      FindAtkObjectParentFrame(parent_atk_object));
  if (!frame)
    return;

  frame->SetDocumentParent(parent_atk_object);
}

AtkObject* AXPlatformNodeAuraLinux::FindPrimaryWebContentDocument() {
  // It could get multiple web contents since additional web content is added,
  // when the DevTools window is opened.
  std::vector<AtkObject*> web_content_candidates;
  for (auto child_iterator_ptr = GetDelegate()->ChildrenBegin();
       *child_iterator_ptr != *GetDelegate()->ChildrenEnd();
       ++(*child_iterator_ptr)) {
    AtkObject* child = child_iterator_ptr->GetNativeViewAccessible();
    auto* child_node = AXPlatformNodeAuraLinux::FromAtkObject(child);
    if (!child_node)
      continue;
    if (!child_node->GetDelegate()->IsWebContent())
      continue;
    if (child_node->GetAtkRole() != ATK_ROLE_DOCUMENT_WEB)
      continue;
    web_content_candidates.push_back(child);
  }

  if (web_content_candidates.empty())
    return nullptr;

  // If it finds just one web content, return it.
  if (web_content_candidates.size() == 1)
    return web_content_candidates[0];

  for (auto* object : web_content_candidates) {
    auto* child_node = AXPlatformNodeAuraLinux::FromAtkObject(object);
    // If it is a primary web contents, return it.
    if (child_node->GetDelegate()->IsPrimaryWebContentsForWindow()) {
      return object;
    }
  }
  return nullptr;
}

bool AXPlatformNodeAuraLinux::IsWebDocumentForRelations() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return false;
  AXPlatformNodeAuraLinux* parent = FromAtkObject(GetParent());
  if (!parent || !GetDelegate()->IsWebContent() ||
      GetAtkRole() != ATK_ROLE_DOCUMENT_WEB)
    return false;
  return parent->FindPrimaryWebContentDocument() == atk_object;
}

AtkObject* AXPlatformNodeAuraLinux::CreateAtkObject() {
  if (GetRole() != ax::mojom::Role::kApplication &&
      !GetDelegate()->IsToplevelBrowserWindow() &&
      !AXPlatform::GetInstance().GetMode().has_mode(AXMode::kNativeAPIs)) {
    return nullptr;
  }
  if (GetDelegate()->IsChildOfLeaf())
    return nullptr;
  EnsureGTypeInit();
  interface_mask_ = GetGTypeInterfaceMask(GetData());
  GType type = GetAccessibilityGType();
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, nullptr));

  atk_object_initialize(atk_object, this);

  SetDocumentParentOnFrameIfNecessary();

  return ATK_OBJECT(atk_object);
}

void AXPlatformNodeAuraLinux::DestroyAtkObjects() {
  if (atk_hyperlink_) {
    ax_platform_atk_hyperlink_set_object(
        AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_.get()), nullptr);
    g_object_unref(atk_hyperlink_);
    atk_hyperlink_ = nullptr;
  }

  if (atk_object_) {
    // We explicitly clear g_current_focused just in case there is another
    // reference to atk_object_ somewhere.
    if (atk_object_ == g_current_focused)
      SetWeakGPtrToAtkObject(&g_current_focused, nullptr);
    atk_object::Detach(AX_PLATFORM_NODE_AURALINUX(atk_object_.get()));

    g_object_unref(atk_object_);
    atk_object_ = nullptr;
  }
}

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeAuraLinux* node = new AXPlatformNodeAuraLinux();
  node->Init(delegate);
  return node;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return AXPlatformNodeAuraLinux::FromAtkObject(accessible);
}

//
// AXPlatformNodeAuraLinux implementation.
//

// static
AXPlatformNodeAuraLinux* AXPlatformNodeAuraLinux::FromAtkObject(
    const AtkObject* atk_object) {
  if (!atk_object)
    return nullptr;

  if (IS_AX_PLATFORM_NODE_AURALINUX(atk_object)) {
    AXPlatformNodeAuraLinuxObject* platform_object =
        AX_PLATFORM_NODE_AURALINUX(atk_object);
    return platform_object->m_object;
  }

  return nullptr;
}

// static
void AXPlatformNodeAuraLinux::SetApplication(AXPlatformNode* application) {
  g_root_application = application;
}

// static
AXPlatformNode* AXPlatformNodeAuraLinux::application() {
  return g_root_application;
}

// static
void AXPlatformNodeAuraLinux::StaticInitialize() {
  AtkUtilAuraLinux::GetInstance()->InitializeAsync();
}

// static
void AXPlatformNodeAuraLinux::EnableAXMode() {
  AXPlatformNode::NotifyAddAXModeFlags(kAXModeComplete);
}

AtkRole AXPlatformNodeAuraLinux::GetAtkRole() const {
  switch (GetRole()) {
    case ax::mojom::Role::kAlert:
      return ATK_ROLE_NOTIFICATION;
    case ax::mojom::Role::kAlertDialog:
      return ATK_ROLE_ALERT;
    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kSuggestion:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kApplication:
      // Only use ATK_ROLE_APPLICATION for elements with no parent, since it
      // is only for top level app windows and not ARIA applications.
      if (!GetParent()) {
        return ATK_ROLE_APPLICATION;
      } else {
        return ATK_ROLE_EMBEDDED;
      }
    case ax::mojom::Role::kArticle:
      return ATK_ROLE_ARTICLE;
    case ax::mojom::Role::kAudio:
      return ATK_ROLE_AUDIO;
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kBlockquote:
      return ATK_ROLE_BLOCK_QUOTE;
    case ax::mojom::Role::kCaret:
      return ATK_ROLE_UNKNOWN;
    case ax::mojom::Role::kButton:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kCanvas:
      return ATK_ROLE_CANVAS;
    case ax::mojom::Role::kCaption:
      return ATK_ROLE_CAPTION;
    case ax::mojom::Role::kCell:
      return ATK_ROLE_TABLE_CELL;
    case ax::mojom::Role::kCheckBox:
      return ATK_ROLE_CHECK_BOX;
    case ax::mojom::Role::kSwitch:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kColorWell:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kColumn:
      return ATK_ROLE_UNKNOWN;
    case ax::mojom::Role::kColumnHeader:
      return ATK_ROLE_COLUMN_HEADER;
    case ax::mojom::Role::kComboBoxGrouping:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kComboBoxMenuButton:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kComboBoxSelect:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kComplementary:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kContentDeletion:
      return GetAtkRoleContentDeletion();
    case ax::mojom::Role::kContentInsertion:
      return GetAtkRoleContentInsertion();
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kDate:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDateTime:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDefinition:
      return ATK_ROLE_DESCRIPTION_VALUE;
    case ax::mojom::Role::kDescriptionList:
      return ATK_ROLE_DESCRIPTION_LIST;
    case ax::mojom::Role::kDetails:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kDialog:
      return ATK_ROLE_DIALOG;
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kDisclosureTriangleGrouped:
      return ::features::IsAccessibilityExposeSummaryAsHeadingEnabled()
                 ? ATK_ROLE_HEADING
                 : ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kDocCover:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kDocNotice:
    case ax::mojom::Role::kDocTip:
      return ATK_ROLE_COMMENT;
    case ax::mojom::Role::kDocFootnote:
      return kAtkFootnoteRole;
    case ax::mojom::Role::kDocPageBreak:
      return ATK_ROLE_SEPARATOR;
    case ax::mojom::Role::kDocPageFooter:
      return ATK_ROLE_FOOTER;
    case ax::mojom::Role::kDocPageHeader:
      return ATK_ROLE_HEADER;
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
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kDocSubtitle:
      return ATK_ROLE_HEADING;
    case ax::mojom::Role::kDocument:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kEmbeddedObject:
      return ATK_ROLE_EMBEDDED;
    case ax::mojom::Role::kForm:
      // TODO(accessibility) Forms which lack an accessible name are no longer
      // exposed as forms. http://crbug.com/874384. Forms which have accessible
      // names should be exposed as ATK_ROLE_LANDMARK according to Core AAM.
      return ATK_ROLE_FORM;
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kFeed:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kGenericContainer:
    case ax::mojom::Role::kRuby:
    case ax::mojom::Role::kSectionWithoutName:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kGraphicsDocument:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kGraphicsObject:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kGraphicsSymbol:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kGrid:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kGridCell:
      return ATK_ROLE_TABLE_CELL;
    case ax::mojom::Role::kGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kHeading:
      return ATK_ROLE_HEADING;
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kIframePresentational:
      return ATK_ROLE_INTERNAL_FRAME;
    case ax::mojom::Role::kImage:
      return IsImageWithMap() ? ATK_ROLE_IMAGE_MAP : ATK_ROLE_IMAGE;
    case ax::mojom::Role::kInlineTextBox:
      return kStaticRole;
    case ax::mojom::Role::kInputTime:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kLabelText:
      return ATK_ROLE_LABEL;
    case ax::mojom::Role::kLegend:
      return ATK_ROLE_LABEL;
    // Layout table objects are treated the same as Role::kGenericContainer.
    case ax::mojom::Role::kLayoutTable:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableCell:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableRow:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLineBreak:
      // TODO(Accessibility) Having a separate accessible object for line breaks
      // is inconsistent with other implementations. http://crbug.com/873144#c1.
      return kStaticRole;
    case ax::mojom::Role::kLink:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kList:
      return ATK_ROLE_LIST;
    case ax::mojom::Role::kListBox:
      return ATK_ROLE_LIST_BOX;
    // TODO(Accessibility) Use ATK_ROLE_MENU_ITEM inside a combo box, see how
    // ax_platform_node_win.cc code does this.
    case ax::mojom::Role::kListBoxOption:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kListGrid:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kListItem:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kListMarker:
      // Regular list markers only expose their alternative text, but do not
      // expose their descendants; and the descendants should be ignored. This
      // is because the alternative text depends on the counter style and can
      // be different from the actual (visual) marker text, and hence,
      // inconsistent with the descendants. We treat a list marker as non-text
      // only if it still has non-ignored descendants, which happens only when:
      // - The list marker itself is ignored but the descendants are not
      // - Or the list marker contains images
      if (!GetChildCount())
        return kStaticRole;
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kLog:
      return ATK_ROLE_LOG;
    case ax::mojom::Role::kMain:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kMark:
      return kStaticRole;
    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMathMLMath:
      return ATK_ROLE_MATH;
    // https://w3c.github.io/mathml-aam/#mathml-element-mappings
    case ax::mojom::Role::kMathMLFraction:
      return ATK_ROLE_MATH_FRACTION;
    case ax::mojom::Role::kMathMLIdentifier:
      return ATK_ROLE_STATIC;
    case ax::mojom::Role::kMathMLMultiscripts:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLNoneScript:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLNumber:
      return ATK_ROLE_STATIC;
    case ax::mojom::Role::kMathMLPrescriptDelimiter:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLOperator:
      return ATK_ROLE_STATIC;
    case ax::mojom::Role::kMathMLOver:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLRoot:
      return ATK_ROLE_MATH_ROOT;
    case ax::mojom::Role::kMathMLRow:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLSquareRoot:
      return ATK_ROLE_MATH_ROOT;
    case ax::mojom::Role::kMathMLStringLiteral:
      return ATK_ROLE_STATIC;
    case ax::mojom::Role::kMathMLSub:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLSubSup:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLSup:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLTable:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kMathMLTableCell:
      return ATK_ROLE_TABLE_CELL;
    case ax::mojom::Role::kMathMLTableRow:
      return ATK_ROLE_TABLE_ROW;
    case ax::mojom::Role::kMathMLText:
      return ATK_ROLE_STATIC;
    case ax::mojom::Role::kMathMLUnder:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMathMLUnderOver:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kMarquee:
      return ATK_ROLE_MARQUEE;
    case ax::mojom::Role::kMenu:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuBar:
      return ATK_ROLE_MENU_BAR;
    case ax::mojom::Role::kMenuItem:
      return ATK_ROLE_MENU_ITEM;
    case ax::mojom::Role::kMenuItemCheckBox:
      return ATK_ROLE_CHECK_MENU_ITEM;
    case ax::mojom::Role::kMenuItemRadio:
      return ATK_ROLE_RADIO_MENU_ITEM;
    case ax::mojom::Role::kMenuListPopup:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuListOption:
      return ATK_ROLE_MENU_ITEM;
    case ax::mojom::Role::kMeter:
      return ATK_ROLE_LEVEL_BAR;
    case ax::mojom::Role::kNavigation:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kNote:
      return ATK_ROLE_COMMENT;
    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kScrollView:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kParagraph:
      return ATK_ROLE_PARAGRAPH;
    case ax::mojom::Role::kPdfActionableHighlight:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kPdfRoot:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kPluginObject:
      return ATK_ROLE_EMBEDDED;
    case ax::mojom::Role::kPopUpButton:
      return ATK_ROLE_PUSH_BUTTON;
    case ax::mojom::Role::kProgressIndicator:
      return ATK_ROLE_PROGRESS_BAR;
    case ax::mojom::Role::kRadioButton:
      return ATK_ROLE_RADIO_BUTTON;
    case ax::mojom::Role::kRadioGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kRegion:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kRootWebArea:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kRow:
      return ATK_ROLE_TABLE_ROW;
    case ax::mojom::Role::kRowGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kRowHeader:
      return ATK_ROLE_ROW_HEADER;
    case ax::mojom::Role::kRubyAnnotation:
      // Generally exposed as description on <ruby> (Role::kRuby) element, not
      // as its own object in the tree.
      // However, it's possible to make a kRubyAnnotation element show up in the
      // AX tree, for example by adding tabindex="0" to the source <rp> or <rt>
      // element or making the source element the target of an aria-owns.
      // Therefore, browser side needs to gracefully handle it if it actually
      // shows up in the tree.
      return kStaticRole;
    case ax::mojom::Role::kSection:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kSectionFooter:
      return ATK_ROLE_FOOTER;
    case ax::mojom::Role::kSectionHeader:
      return ATK_ROLE_HEADER;
    case ax::mojom::Role::kScrollBar:
      return ATK_ROLE_SCROLL_BAR;
    case ax::mojom::Role::kSearch:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kSlider:
      return ATK_ROLE_SLIDER;
    case ax::mojom::Role::kSpinButton:
      return ATK_ROLE_SPIN_BUTTON;
    case ax::mojom::Role::kSplitter:
      return ATK_ROLE_SEPARATOR;
    case ax::mojom::Role::kStaticText:
      return kStaticRole;
    case ax::mojom::Role::kStatus:
      return ATK_ROLE_STATUSBAR;
    case ax::mojom::Role::kSubscript:
      return kSubscriptRole;
    case ax::mojom::Role::kSuperscript:
      return kSuperscriptRole;
    case ax::mojom::Role::kSvgRoot:
      return ATK_ROLE_DOCUMENT_FRAME;
    case ax::mojom::Role::kTab:
      return ATK_ROLE_PAGE_TAB;
    case ax::mojom::Role::kTable:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kTableHeaderContainer:
      // TODO(accessibility) This mapping is correct, but it doesn't seem to be
      // used. We don't necessarily want to always expose these containers, but
      // we must do so if they are focusable. http://crbug.com/874043
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kTabList:
      return ATK_ROLE_PAGE_TAB_LIST;
    case ax::mojom::Role::kTabPanel:
      return ATK_ROLE_SCROLL_PANE;
    case ax::mojom::Role::kTerm:
      return ATK_ROLE_DESCRIPTION_TERM;
    case ax::mojom::Role::kTitleBar:
      return ATK_ROLE_TITLE_BAR;
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kSearchBox:
      if (HasState(ax::mojom::State::kProtected))
        return ATK_ROLE_PASSWORD_TEXT;
      return ATK_ROLE_ENTRY;
    case ax::mojom::Role::kTextFieldWithComboBox:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kCode:
    case ax::mojom::Role::kEmphasis:
    case ax::mojom::Role::kStrong:
    case ax::mojom::Role::kTime:
      return kStaticRole;
    case ax::mojom::Role::kTimer:
      return ATK_ROLE_TIMER;
    case ax::mojom::Role::kToggleButton:
      return ATK_ROLE_TOGGLE_BUTTON;
    case ax::mojom::Role::kToolbar:
      return ATK_ROLE_TOOL_BAR;
    case ax::mojom::Role::kTooltip:
      return ATK_ROLE_TOOL_TIP;
    case ax::mojom::Role::kTree:
      return ATK_ROLE_TREE;
    case ax::mojom::Role::kTreeItem:
      return ATK_ROLE_TREE_ITEM;
    case ax::mojom::Role::kTreeGrid:
      return ATK_ROLE_TREE_TABLE;
    case ax::mojom::Role::kVideo:
      return ATK_ROLE_VIDEO;
    case ax::mojom::Role::kWindow:
      // In ATK elements with ATK_ROLE_FRAME are windows with titles and
      // buttons, while those with ATK_ROLE_WINDOW are windows without those
      // elements.
      return ATK_ROLE_FRAME;
    case ax::mojom::Role::kClient:
    case ax::mojom::Role::kDesktop:
    case ax::mojom::Role::kWebView:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kFigcaption:
      return ATK_ROLE_CAPTION;
    case ax::mojom::Role::kUnknown:
      // When we are not in web content, assume that a node with an unknown
      // role is a view (which often have the unknown role).
      return !GetDelegate()->IsWebContent() ? ATK_ROLE_PANEL : ATK_ROLE_UNKNOWN;
    case ax::mojom::Role::kImeCandidate:
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
      return ATK_ROLE_REDUNDANT_OBJECT;
    case ax::mojom::Role::kDescriptionListTermDeprecated:
    case ax::mojom::Role::kPreDeprecated:
    case ax::mojom::Role::kPortalDeprecated:
    case ax::mojom::Role::kDescriptionListDetailDeprecated:
    case ax::mojom::Role::kDirectoryDeprecated:
      NOTREACHED();
  }
}

// If we were compiled with a newer version of ATK than the runtime version,
// it's possible that the state we want to expose and/or emit an event for
// is not present. This will generate a runtime error.
bool PlatformSupportsState(AtkStateType atk_state_type) {
  static std::optional<int> max_state_type = std::nullopt;
  if (!max_state_type.has_value()) {
    GEnumClass* enum_class =
        G_ENUM_CLASS(g_type_class_ref(atk_state_type_get_type()));
    max_state_type = enum_class->maximum;
    g_type_class_unref(enum_class);
  }
  return atk_state_type < max_state_type.value();
}

void AXPlatformNodeAuraLinux::GetAtkState(AtkStateSet* atk_state_set) {
  bool menu_active = !GetActiveMenus().empty();
  if (!menu_active && atk_object_ == g_active_top_level_frame)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);
  if (menu_active &&
      FindAtkObjectParentFrame(GetActiveMenus().back()) == atk_object_)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);

  if (atk_object_ && atk_object_ == g_active_views_dialog)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);

  bool is_minimized = delegate_->IsMinimized();
  if (is_minimized && GetRole() == ax::mojom::Role::kWindow)
    atk_state_set_add_state(atk_state_set, ATK_STATE_ICONIFIED);

  if (HasState(ax::mojom::State::kCollapsed))
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
  if (HasState(ax::mojom::State::kDefault))
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFAULT);
  if ((HasState(ax::mojom::State::kEditable) ||
       HasState(ax::mojom::State::kRichlyEditable)) &&
      GetData().GetRestriction() != ax::mojom::Restriction::kReadOnly) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EDITABLE);
  }
  if (HasState(ax::mojom::State::kExpanded)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDED);
  }
  if (IsFocused())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSED);
  if (IsFocusable())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSABLE);
  if (HasState(ax::mojom::State::kHorizontal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HORIZONTAL);
  if (!IsInvisibleOrIgnored()) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISIBLE);
    if (!delegate_->IsOffscreen() && !is_minimized)
      atk_state_set_add_state(atk_state_set, ATK_STATE_SHOWING);
  }
  if (HasState(ax::mojom::State::kMultiselectable))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MULTISELECTABLE);
  if (HasState(ax::mojom::State::kRequired))
    atk_state_set_add_state(atk_state_set, ATK_STATE_REQUIRED);
  if (HasState(ax::mojom::State::kVertical))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VERTICAL);
  if (HasState(ax::mojom::State::kVisited))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISITED);
  if (HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      GetIntAttribute(ax::mojom::IntAttribute::kInvalidState) !=
          static_cast<int32_t>(ax::mojom::InvalidState::kFalse)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_INVALID_ENTRY);
  }
  if (HasIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState) &&
      GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState) !=
          static_cast<int32_t>(ax::mojom::AriaCurrentState::kFalse)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_ACTIVE);
  }
  // Runtime checks in case we were compiled with a newer version of ATK.
  if (IsPlatformCheckable() && PlatformSupportsState(ATK_STATE_CHECKABLE))
    atk_state_set_add_state(atk_state_set, ATK_STATE_CHECKABLE);
  if (HasIntAttribute(ax::mojom::IntAttribute::kHasPopup) &&
      PlatformSupportsState(ATK_STATE_HAS_POPUP))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HAS_POPUP);
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    atk_state_set_add_state(atk_state_set, ATK_STATE_BUSY);
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kModal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MODAL);
  if (GetData().IsSelectable())
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE);
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTED);

  if (IsTextField()) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE_TEXT);
    if (HasState(ax::mojom::State::kMultiline))
      atk_state_set_add_state(atk_state_set, ATK_STATE_MULTI_LINE);
    else
      atk_state_set_add_state(atk_state_set, ATK_STATE_SINGLE_LINE);
  }

  // Special case for indeterminate progressbar.
  if (GetRole() == ax::mojom::Role::kProgressIndicator &&
      !HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_INDETERMINATE);
  }

  if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete).empty() ||
      HasState(ax::mojom::State::kAutofillAvailable)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_SUPPORTS_AUTOCOMPLETION);
  }

  // Checked state
  const auto checked_state = GetData().GetCheckedState();
  if (checked_state == ax::mojom::CheckedState::kTrue ||
      checked_state == ax::mojom::CheckedState::kMixed) {
    atk_state_set_add_state(atk_state_set, GetAtkStateTypeForCheckableNode());
  }

  if (GetData().GetRestriction() != ax::mojom::Restriction::kDisabled) {
    if (GetDelegate()->IsReadOnlySupported() &&
        GetDelegate()->IsReadOnlyOrDisabled()) {
      // Runtime check in case we were compiled with a newer version of ATK.
      if (PlatformSupportsState(ATK_STATE_READ_ONLY))
        atk_state_set_add_state(atk_state_set, ATK_STATE_READ_ONLY);
    } else {
      atk_state_set_add_state(atk_state_set, ATK_STATE_ENABLED);
      atk_state_set_add_state(atk_state_set, ATK_STATE_SENSITIVE);
    }
  }
}

// Some relations only exist in a high enough ATK version.
// If a relation has a version requirement, it will be documented in
// the link below.
// https://docs.gtk.org/atk/enum.RelationType.html
struct AtkIntRelation {
  ax::mojom::IntAttribute attribute;
  AtkRelationType relation;
  std::optional<AtkRelationType> reverse_relation;
};

static AtkIntRelation kIntRelations[] = {
    {ax::mojom::IntAttribute::kMemberOfId, ATK_RELATION_MEMBER_OF,
     std::nullopt},
    {ax::mojom::IntAttribute::kPopupForId, ATK_RELATION_POPUP_FOR,
     std::nullopt},
};

struct AtkIntListRelation {
  ax::mojom::IntListAttribute attribute;
  AtkRelationType relation;
  std::optional<AtkRelationType> reverse_relation;
};

static AtkIntListRelation kIntListRelations[] = {
    {ax::mojom::IntListAttribute::kControlsIds, ATK_RELATION_CONTROLLER_FOR,
     ATK_RELATION_CONTROLLED_BY},
    {ax::mojom::IntListAttribute::kDetailsIds, ATK_RELATION_DETAILS,
     ATK_RELATION_DETAILS_FOR},
    {ax::mojom::IntListAttribute::kDescribedbyIds, ATK_RELATION_DESCRIBED_BY,
     ATK_RELATION_DESCRIPTION_FOR},
    {ax::mojom::IntListAttribute::kErrormessageIds, ATK_RELATION_ERROR_MESSAGE,
     ATK_RELATION_ERROR_FOR},
    {ax::mojom::IntListAttribute::kFlowtoIds, ATK_RELATION_FLOWS_TO,
     ATK_RELATION_FLOWS_FROM},
    {ax::mojom::IntListAttribute::kLabelledbyIds, ATK_RELATION_LABELLED_BY,
     ATK_RELATION_LABEL_FOR},
};

void AXPlatformNodeAuraLinux::AddRelationToSet(AtkRelationSet* relation_set,
                                               AtkRelationType relation,
                                               AXPlatformNode* target) {
  DCHECK(target);
  DCHECK(GetDelegate()->IsValidRelationTarget(target));

  // If we were compiled with a newer version of ATK than the runtime version,
  // it's possible that we might try to add a relation that doesn't exist in
  // the runtime version of the AtkRelationType enum. This will cause a runtime
  // error, so return early here if we are about to do that.
  static std::optional<int> max_relation_type = std::nullopt;
  if (!max_relation_type.has_value()) {
    GEnumClass* enum_class =
        G_ENUM_CLASS(g_type_class_ref(atk_relation_type_get_type()));
    max_relation_type = enum_class->maximum;
    g_type_class_unref(enum_class);
  }
  if (relation >= max_relation_type.value())
    return;

  atk_relation_set_add_relation_by_type(relation_set, relation,
                                        target->GetNativeViewAccessible());
}

AtkRelationSet* AXPlatformNodeAuraLinux::GetAtkRelations() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return nullptr;

  AtkRelationSet* relation_set = atk_relation_set_new();

  if (IsWebDocumentForRelations()) {
    AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object);
    if (parent_frame) {
      atk_relation_set_add_relation_by_type(
          relation_set, ATK_RELATION_EMBEDDED_BY, parent_frame);
    }
  }

  if (auto* document_parent = FromAtkObject(document_parent_)) {
    AtkObject* document = document_parent->FindPrimaryWebContentDocument();
    if (document) {
      atk_relation_set_add_relation_by_type(relation_set, ATK_RELATION_EMBEDS,
                                            document);
    }
  }

  // For each possible relation defined by an IntAttribute, we test that
  // attribute and then look for reverse relations.
  for (auto relation : kIntRelations) {
    if (AXPlatformNode* target =
            GetDelegate()->GetTargetNodeForRelation(relation.attribute))
      AddRelationToSet(relation_set, relation.relation, target);

    if (!relation.reverse_relation.has_value())
      continue;

    std::vector<AXPlatformNode*> target_ids =
        GetDelegate()->GetSourceNodesForReverseRelations(relation.attribute);
    for (AXPlatformNode* target : target_ids) {
      AddRelationToSet(relation_set, relation.reverse_relation.value(), target);
    }
  }

  // Now we do the same for each possible relation defined by an
  // IntListAttribute. In this case we need to handle each target in the list.
  for (const auto& relation : kIntListRelations) {
    std::vector<AXPlatformNode*> targets =
        GetDelegate()->GetTargetNodesForRelation(relation.attribute);
    for (AXPlatformNode* target : targets) {
      AddRelationToSet(relation_set, relation.relation, target);
    }

    if (!relation.reverse_relation.has_value())
      continue;

    std::vector<AXPlatformNode*> reverse_target_ids =
        GetDelegate()->GetSourceNodesForReverseRelations(relation.attribute);
    for (AXPlatformNode* target : reverse_target_ids) {
      AddRelationToSet(relation_set, relation.reverse_relation.value(), target);
    }
  }

  return relation_set;
}

AXPlatformNodeAuraLinux::AXPlatformNodeAuraLinux() = default;

AXPlatformNodeAuraLinux::~AXPlatformNodeAuraLinux() {
  if (g_current_selected == this)
    g_current_selected = nullptr;

  DestroyAtkObjects();

  if (window_activate_event_postponed_)
    AtkUtilAuraLinux::GetInstance()->CancelPostponedEventsFor(this);

  SetWeakGPtrToAtkObject(&document_parent_, nullptr);
}

void AXPlatformNodeAuraLinux::Destroy() {
  DestroyAtkObjects();
  AXPlatformNodeBase::Destroy();
}

void AXPlatformNodeAuraLinux::Init(AXPlatformNodeDelegate* delegate) {
  // Initialize ATK.
  AXPlatformNodeBase::Init(delegate);

  // Only create the AtkObject if we know enough information.
  if (GetRole() != ax::mojom::Role::kUnknown)
    GetOrCreateAtkObject();
}

bool AXPlatformNodeAuraLinux::IsPlatformCheckable() const {
  if (GetRole() == ax::mojom::Role::kToggleButton)
    return false;

  return AXPlatformNodeBase::IsPlatformCheckable();
}

std::optional<size_t> AXPlatformNodeAuraLinux::GetIndexInParent() {
  AXPlatformNode* parent =
      AXPlatformNode::FromNativeViewAccessible(GetParent());
  // Even though the node doesn't have its parent, GetParent() could return the
  // application. Since the detached view has the kUnknown role and the
  // restriction is kDisabled, it early returns before finding the index.
  if (parent == AXPlatformNodeAuraLinux::application() &&
      GetRole() == ax::mojom::Role::kUnknown &&
      GetData().GetRestriction() == ax::mojom::Restriction::kDisabled) {
    return std::nullopt;
  }

  return AXPlatformNodeBase::GetIndexInParent();
}

void AXPlatformNodeAuraLinux::EnsureAtkObjectIsValid() {
  if (atk_object_) {
    // If the object's role changes and that causes its
    // interface mask to change, we need to create a new
    // AtkObject for it.
    ImplementedAtkInterfaces interface_mask = GetGTypeInterfaceMask(GetData());
    if (interface_mask != interface_mask_)
      DestroyAtkObjects();
  }

  if (!atk_object_) {
    GetOrCreateAtkObject();
  }
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetNativeViewAccessible() {
  return GetOrCreateAtkObject();
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetOrCreateAtkObject() {
  if (!atk_object_) {
    atk_object_ = CreateAtkObject();
  }
  return atk_object_;
}

void AXPlatformNodeAuraLinux::OnCheckedStateChanged() {
  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  atk_object_notify_state_change(
      ATK_OBJECT(obj), GetAtkStateTypeForCheckableNode(),
      GetData().GetCheckedState() != ax::mojom::CheckedState::kFalse);
}

void AXPlatformNodeAuraLinux::OnEnabledChanged() {
  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  atk_object_notify_state_change(
      obj, ATK_STATE_ENABLED,
      GetData().GetRestriction() != ax::mojom::Restriction::kDisabled);

  atk_object_notify_state_change(
      obj, ATK_STATE_SENSITIVE,
      GetData().GetRestriction() != ax::mojom::Restriction::kDisabled);
}

void AXPlatformNodeAuraLinux::OnBusyStateChanged(bool is_busy) {
  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  atk_object_notify_state_change(obj, ATK_STATE_BUSY, is_busy);
}

void AXPlatformNodeAuraLinux::OnExpandedStateChanged(bool is_expanded) {
  // When a list box is expanded, it becomes visible. This means that it might
  // now have a different role (the role for hidden Views is kUnknown).  We
  // need to recreate the AtkObject in this case because a change in roles
  // might imply a change in ATK interfaces implemented.
  EnsureAtkObjectIsValid();

  DCHECK(HasState(ax::mojom::State::kCollapsed) ||
         HasState(ax::mojom::State::kExpanded));

  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  atk_object_notify_state_change(obj, ATK_STATE_EXPANDED, is_expanded);
}

void AXPlatformNodeAuraLinux::OnShowingStateChanged(bool is_showing) {
  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  atk_object_notify_state_change(obj, ATK_STATE_SHOWING, is_showing);
}

void AXPlatformNodeAuraLinux::OnMenuPopupStart() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object);
  if (!parent_frame)
    return;

  // Exit early if kMenuPopupStart is sent multiple times for the same menu.
  std::vector<AtkObject*>& active_menus = GetActiveMenus();
  bool menu_already_open = !active_menus.empty();
  if (menu_already_open && active_menus.back() == atk_object)
    return;

  // We also want to inform the AT that menu the is now showing. Normally this
  // event is not fired because the menu will be created with the
  // ATK_STATE_SHOWING already set to TRUE.
  atk_object_notify_state_change(atk_object, ATK_STATE_SHOWING, TRUE);

  // We need to compute this before modifying the active menu stack.
  AtkObject* previous_active_frame = ComputeActiveTopLevelFrame();

  active_menus.push_back(atk_object);

  // We exit early if the newly activated menu has the same AtkWindow as the
  // previous one.
  if (previous_active_frame == parent_frame)
    return;
  if (previous_active_frame) {
    g_signal_emit_by_name(previous_active_frame, "deactivate");
    atk_object_notify_state_change(previous_active_frame, ATK_STATE_ACTIVE,
                                   FALSE);
  }
  g_signal_emit_by_name(parent_frame, "activate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, TRUE);
}

void AXPlatformNodeAuraLinux::OnMenuPopupEnd() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  AtkObject* parent_frame = FindAtkObjectParentFrame(atk_object);
  if (!parent_frame)
    return;

  atk_object_notify_state_change(atk_object, ATK_STATE_SHOWING, FALSE);

  // kMenuPopupHide may be called multiple times for the same menu, so only
  // remove it if our parent frame matches the most recently opened menu.
  std::vector<AtkObject*>& active_menus = GetActiveMenus();
  DCHECK(!active_menus.empty())
      << "Asymmetrical menupopupend events -- too many";

  active_menus.pop_back();
  AtkObject* new_active_item = ComputeActiveTopLevelFrame();
  if (new_active_item != parent_frame) {
    // Newly activated menu has the different AtkWindow as the previous one.
    g_signal_emit_by_name(parent_frame, "deactivate");
    atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, FALSE);
    if (new_active_item) {
      g_signal_emit_by_name(new_active_item, "activate");
      atk_object_notify_state_change(new_active_item, ATK_STATE_ACTIVE, TRUE);
    }
  }

  // All menus are closed.
  if (active_menus.empty())
    OnAllMenusEnded();
}

void AXPlatformNodeAuraLinux::ResendFocusSignalsForCurrentlyFocusedNode() {
  auto* frame = FromAtkObject(g_active_top_level_frame);
  if (!frame)
    return;

  AtkObject* focused_node = frame->GetDelegate()->GetFocus();
  if (!focused_node)
    return;

  g_signal_emit_by_name(focused_node, "focus-event", true);
  atk_object_notify_state_change(focused_node, ATK_STATE_FOCUSED, true);
}

void AXPlatformNodeAuraLinux::SetAsCurrentlyFocusedNode() {
  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  SetWeakGPtrToAtkObject(&g_current_focused, obj);
}

// All menus have closed.
void AXPlatformNodeAuraLinux::OnAllMenusEnded() {
  if (!GetActiveMenus().empty() && g_active_top_level_frame &&
      ComputeActiveTopLevelFrame() != g_active_top_level_frame) {
    g_signal_emit_by_name(g_active_top_level_frame, "activate");
    atk_object_notify_state_change(g_active_top_level_frame, ATK_STATE_ACTIVE,
                                   TRUE);
  }

  GetActiveMenus().clear();
  ResendFocusSignalsForCurrentlyFocusedNode();
}

void AXPlatformNodeAuraLinux::OnWindowActivated() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(GetOrCreateAtkObject());
  if (!parent_frame || parent_frame == g_active_top_level_frame)
    return;

  SetActiveTopLevelFrame(parent_frame);

  g_signal_emit_by_name(parent_frame, "activate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, TRUE);

  // We also send a focus event for the currently focused element, so that
  // the user knows where the focus is when the toplevel window regains focus.
  if (g_current_focused &&
      IsFrameAncestorOfAtkObject(parent_frame, g_current_focused)) {
    g_signal_emit_by_name(g_current_focused, "focus-event", true);
    atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                   ATK_STATE_FOCUSED, true);
  }
}

void AXPlatformNodeAuraLinux::OnWindowDeactivated() {
  AtkObject* parent_frame = FindAtkObjectParentFrame(GetOrCreateAtkObject());
  if (!parent_frame || parent_frame != g_active_top_level_frame)
    return;

  SetActiveTopLevelFrame(nullptr);

  g_signal_emit_by_name(parent_frame, "deactivate");
  atk_object_notify_state_change(parent_frame, ATK_STATE_ACTIVE, FALSE);
}

void AXPlatformNodeAuraLinux::OnWindowVisibilityChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  if (GetAtkRole() != ATK_ROLE_FRAME)
    return;

  bool minimized = delegate_->IsMinimized();
  if (minimized == was_minimized_)
    return;

  was_minimized_ = minimized;
  if (minimized)
    g_signal_emit_by_name(atk_object, "minimize");
  else
    g_signal_emit_by_name(atk_object, "restore");
  atk_object_notify_state_change(atk_object, ATK_STATE_ICONIFIED, minimized);
}

void AXPlatformNodeAuraLinux::OnScrolledToAnchor() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;
  // The text-caret-moved event is used to signal a scroll to anchor event.
  if (ATK_IS_TEXT(atk_object)) {
    g_signal_emit_by_name(atk_object, "text-caret-moved", 0);
  }
}

void AXPlatformNodeAuraLinux::SetActiveViewsDialog() {
  AtkObject* old_views_dialog = g_active_views_dialog;
  AtkObject* new_views_dialog = nullptr;

  AtkObject* parent = GetOrCreateAtkObject();
  if (!parent)
    return;

  if (!GetDelegate()->IsWebContent()) {
    while (parent) {
      if (atk_object::GetRole(parent) == ATK_ROLE_DIALOG) {
        new_views_dialog = parent;
        break;
      }
      parent = atk_object::GetParent(parent);
    }
  }

  if (old_views_dialog == new_views_dialog)
    return;

  SetWeakGPtrToAtkObject(&g_active_views_dialog, new_views_dialog);
  if (old_views_dialog)
    atk_object_notify_state_change(old_views_dialog, ATK_STATE_ACTIVE, FALSE);
  if (new_views_dialog)
    atk_object_notify_state_change(new_views_dialog, ATK_STATE_ACTIVE, TRUE);
}

void AXPlatformNodeAuraLinux::OnFocused() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  if (atk_object::GetRole(atk_object) == ATK_ROLE_FRAME) {
    OnWindowActivated();
    return;
  }

  if (atk_object == g_current_focused)
    return;

  SetActiveViewsDialog();

  if (g_current_focused) {
    g_signal_emit_by_name(g_current_focused, "focus-event", false);
    atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                   ATK_STATE_FOCUSED, false);
  }

  SetWeakGPtrToAtkObject(&g_current_focused, atk_object);

  g_signal_emit_by_name(g_current_focused, "focus-event", true);
  atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                 ATK_STATE_FOCUSED, true);
}

void AXPlatformNodeAuraLinux::OnSelected() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;
  if (g_current_selected && !g_current_selected->GetBoolAttribute(
                                ax::mojom::BoolAttribute::kSelected)) {
    atk_object_notify_state_change(
        ATK_OBJECT(g_current_selected->GetOrCreateAtkObject()),
        ATK_STATE_SELECTED, false);
  }

  g_current_selected = this;
  if (ATK_IS_OBJECT(atk_object)) {
    atk_object_notify_state_change(ATK_OBJECT(atk_object), ATK_STATE_SELECTED,
                                   true);
  }
}

void AXPlatformNodeAuraLinux::OnSelectedChildrenChanged() {
  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  g_signal_emit_by_name(obj, "selection-changed", true);
}

bool AXPlatformNodeAuraLinux::EmitsAtkTextEvents() const {
  // Objects which do not implement AtkText cannot emit AtkText events.
  if (!atk_object_ || !ATK_IS_TEXT(atk_object_.get())) {
    return false;
  }

  // Objects which do implement AtkText, but are ignored or invisible should not
  // emit AtkText events.
  if (IsInvisibleOrIgnored())
    return false;

  // If this node is not a static text node, it supports the full AtkText
  // interface.
  if (GetAtkRole() != kStaticRole)
    return true;

  // If this node has children it is not a static text leaf node and supports
  // the full AtkText interface.
  if (GetChildCount())
    return true;

  return false;
}

void AXPlatformNodeAuraLinux::GetFullSelection(int32_t* anchor_node_id,
                                               int* anchor_offset,
                                               int32_t* focus_node_id,
                                               int* focus_offset) {
  DCHECK(anchor_node_id);
  DCHECK(anchor_offset);
  DCHECK(focus_node_id);
  DCHECK(focus_offset);

  if (IsAtomicTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart, anchor_offset) &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, focus_offset)) {
    int32_t node_id = GetData().id != -1 ? GetData().id : GetUniqueId();
    *anchor_node_id = *focus_node_id = node_id;
    return;
  }

  AXSelection selection = GetDelegate()->GetUnignoredSelection();
  *anchor_node_id = selection.anchor_object_id;
  *anchor_offset = selection.anchor_offset;
  *focus_node_id = selection.focus_object_id;
  *focus_offset = selection.focus_offset;
}

AXPlatformNodeAuraLinux& AXPlatformNodeAuraLinux::FindEditableRootOrDocument() {
  if (GetAtkRole() == ATK_ROLE_DOCUMENT_WEB)
    return *this;
  if (GetBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot) &&
      HasState(ax::mojom::State::kEditable))
    return *this;
  if (auto* parent = FromAtkObject(GetParent()))
    return parent->FindEditableRootOrDocument();
  return *this;
}

AXPlatformNodeAuraLinux* AXPlatformNodeAuraLinux::FindCommonAncestor(
    AXPlatformNodeAuraLinux* other) {
  if (this == other || other->IsDescendantOf(this))
    return this;
  if (auto* parent = FromAtkObject(GetParent()))
    return parent->FindCommonAncestor(other);
  return nullptr;
}

void AXPlatformNodeAuraLinux::UpdateSelectionInformation(int32_t anchor_node_id,
                                                         int anchor_offset,
                                                         int32_t focus_node_id,
                                                         int focus_offset) {
  had_nonzero_width_selection =
      focus_node_id != anchor_node_id || focus_offset != anchor_offset;
  current_caret_ = std::make_pair(focus_node_id, focus_offset);
}

void AXPlatformNodeAuraLinux::EmitSelectionChangedSignal(bool had_selection) {
  if (!EmitsAtkTextEvents()) {
    if (auto* parent = FromAtkObject(GetParent()))
      parent->EmitSelectionChangedSignal(had_selection);
    return;
  }

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;
  DCHECK(ATK_IS_TEXT(atk_object));

  // ATK does not consider a collapsed selection a selection, so
  // when the collapsed selection changes (caret movement), we should
  // avoid sending text-selection-changed events.
  if (HasSelection() || had_selection)
    g_signal_emit_by_name(atk_object, "text-selection-changed");
}

void AXPlatformNodeAuraLinux::EmitCaretChangedSignal() {
  if (!EmitsAtkTextEvents()) {
    if (auto* parent = FromAtkObject(GetParent()))
      parent->EmitCaretChangedSignal();
    return;
  }

  std::pair<int, int> selection = GetSelectionOffsetsForAtk();
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  DCHECK(ATK_IS_TEXT(atk_object));
  g_signal_emit_by_name(atk_object, "text-caret-moved",
                        UTF16ToUnicodeOffsetInText(selection.second));
}

void AXPlatformNodeAuraLinux::OnTextAttributesChanged() {
  if (!EmitsAtkTextEvents()) {
    if (auto* parent = FromAtkObject(GetParent()))
      parent->OnTextAttributesChanged();
    return;
  }

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  DCHECK(ATK_IS_TEXT(atk_object));
  g_signal_emit_by_name(atk_object, "text-attributes-changed");
}

void AXPlatformNodeAuraLinux::OnTextSelectionChanged() {
  int32_t anchor_node_id, focus_node_id;
  int anchor_offset, focus_offset;
  GetFullSelection(&anchor_node_id, &anchor_offset, &focus_node_id,
                   &focus_offset);

  auto* anchor_node = static_cast<AXPlatformNodeAuraLinux*>(
      GetDelegate()->GetFromNodeID(anchor_node_id));
  auto* focus_node = static_cast<AXPlatformNodeAuraLinux*>(
      GetDelegate()->GetFromNodeID(focus_node_id));
  if (!anchor_node || !focus_node)
    return;

  AXPlatformNodeAuraLinux& editable_root = FindEditableRootOrDocument();
  AXPlatformNodeAuraLinux* common_ancestor =
      focus_node->FindCommonAncestor(anchor_node);
  if (common_ancestor) {
    common_ancestor->EmitSelectionChangedSignal(
        editable_root.HadNonZeroWidthSelection());
  }

  // It's possible for the selection to change and for the caret to stay in
  // place. This might happen if the selection is totally reset with a
  // different anchor node, but the same focus node. We should avoid sending a
  // caret changed signal in that case.
  std::pair<int32_t, int> prev_caret = editable_root.GetCurrentCaret();
  if (prev_caret.first != focus_node_id || prev_caret.second != focus_offset)
    focus_node->EmitCaretChangedSignal();

  editable_root.UpdateSelectionInformation(anchor_node_id, anchor_offset,
                                           focus_node_id, focus_offset);
}

bool AXPlatformNodeAuraLinux::SupportsSelectionWithAtkSelection() {
  return SupportsToggle(GetRole()) ||
         GetRole() == ax::mojom::Role::kListBoxOption;
}

void AXPlatformNodeAuraLinux::OnDescriptionChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  std::string description;
  GetStringAttribute(ax::mojom::StringAttribute::kDescription, &description);

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-description";
  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_STRING);
  g_value_set_string(&property_values.new_value, description.c_str());
  g_signal_emit_by_name(G_OBJECT(atk_object),
                        "property-change::accessible-description",
                        &property_values, nullptr);
  g_value_unset(&property_values.new_value);
}

void AXPlatformNodeAuraLinux::OnSortDirectionChanged() {
  AXPlatformNodeBase* table = GetTable();
  if (!table)
    return;

  AtkObject* atk_table = table->GetNativeViewAccessible();
  DCHECK(ATK_IS_TABLE(atk_table));

  if (GetRole() == ax::mojom::Role::kColumnHeader)
    g_signal_emit_by_name(atk_table, "row-reordered");
  else if (GetRole() == ax::mojom::Role::kRowHeader)
    g_signal_emit_by_name(atk_table, "column-reordered");
}

void AXPlatformNodeAuraLinux::OnValueChanged() {
  // For the AtkText interface to work on non-web content nodes, we need to
  // update the nodes' hypertext and trigger text change signals when the value
  // changes. Otherwise, for web and PDF content, this is handled by
  // "BrowserAccessibilityAuraLinux".
  if (!GetDelegate()->IsWebContent())
    UpdateHypertext();

  if (!GetData().IsRangeValueSupported())
    return;

  float float_val;
  if (!GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, &float_val))
    return;

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-value";

  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_DOUBLE);
  g_value_set_double(&property_values.new_value,
                     static_cast<double>(float_val));
  g_signal_emit_by_name(G_OBJECT(atk_object),
                        "property-change::accessible-value", &property_values,
                        nullptr);
}

void AXPlatformNodeAuraLinux::OnNameChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object) {
    return;
  }
  std::string previous_accessible_name = accessible_name_;
  // Calling atk_object_get_name will update the value of accessible_name_.
  if (!g_strcmp0(atk_object::GetName(atk_object),
                 previous_accessible_name.c_str()))
    return;

  g_object_notify(G_OBJECT(atk_object), "accessible-name");
}

void AXPlatformNodeAuraLinux::OnDocumentTitleChanged() {
  if (!g_active_top_level_frame)
    return;

  // We always want to notify on the top frame.
  AXPlatformNodeAuraLinux* window = FromAtkObject(g_active_top_level_frame);
  if (window)
    window->OnNameChanged();
}

void AXPlatformNodeAuraLinux::OnSubtreeCreated() {
  // We might not have a parent, in that case we don't need to send the event.
  // We also don't want to notify if this is an ignored node
  if (!GetParent() || GetData().IsIgnored())
    return;

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  auto index_in_parent = GetIndexInParent();
  gint index_gint = index_in_parent.has_value()
                        ? static_cast<gint>(index_in_parent.value())
                        : -1;
  g_signal_emit_by_name(GetParent(), "children-changed::add", index_gint,
                        atk_object);
}

void AXPlatformNodeAuraLinux::OnSubtreeWillBeDeleted() {
  // There is a chance there won't be a parent as we're in the deletion process.
  // We also don't want to notify if this is an ignored node
  if (!GetParent() || GetData().IsIgnored())
    return;

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  auto index_in_parent = GetIndexInParent();
  gint index_gint = index_in_parent.has_value()
                        ? static_cast<gint>(index_in_parent.value())
                        : -1;
  g_signal_emit_by_name(GetParent(), "children-changed::remove", index_gint,
                        atk_object);
}

void AXPlatformNodeAuraLinux::OnParentChanged() {
  if (!atk_object_)
    return;

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-parent";
  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_OBJECT);
  g_value_set_object(&property_values.new_value, GetParent());
  g_signal_emit_by_name(G_OBJECT(atk_object_.get()),
                        "property-change::accessible-parent", &property_values,
                        nullptr);
  g_value_unset(&property_values.new_value);
}

void AXPlatformNodeAuraLinux::OnReadonlyChanged() {
  AtkObject* obj = GetOrCreateAtkObject();
  if (!obj)
    return;

  // Runtime check in case we were compiled with a newer version of ATK.
  if (!PlatformSupportsState(ATK_STATE_READ_ONLY))
    return;

  atk_object_notify_state_change(
      obj, ATK_STATE_READ_ONLY,
      GetData().GetRestriction() == ax::mojom::Restriction::kReadOnly);
}

void AXPlatformNodeAuraLinux::OnInvalidStatusChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  atk_object_notify_state_change(
      ATK_OBJECT(atk_object), ATK_STATE_INVALID_ENTRY,
      GetData().GetInvalidState() != ax::mojom::InvalidState::kFalse);
}

void AXPlatformNodeAuraLinux::OnAriaCurrentChanged() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  ax::mojom::AriaCurrentState aria_current =
      static_cast<ax::mojom::AriaCurrentState>(
          GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState));
  atk_object_notify_state_change(
      ATK_OBJECT(atk_object), ATK_STATE_ACTIVE,
      aria_current != ax::mojom::AriaCurrentState::kNone &&
          aria_current != ax::mojom::AriaCurrentState::kFalse);
}

void AXPlatformNodeAuraLinux::OnAlertShown() {
  atk_object_notify_state_change(ATK_OBJECT(GetOrCreateAtkObject()),
                                 ATK_STATE_SHOWING, TRUE);
}

void AXPlatformNodeAuraLinux::RunPostponedEvents() {
  if (window_activate_event_postponed_) {
    OnWindowActivated();
    window_activate_event_postponed_ = false;
  }
}

void AXPlatformNodeAuraLinux::NotifyAccessibilityEvent(
    ax::mojom::Event event_type) {
  if (!GetOrCreateAtkObject())
    return;
  AXPlatformNodeBase::NotifyAccessibilityEvent(event_type);
  switch (event_type) {
    // kMenuStart/kMenuEnd: the menu system has started / stopped.
    // kMenuPopupStart/kMenuPopupEnd: an individual menu/submenu has
    // opened/closed.
    case ax::mojom::Event::kMenuPopupStart:
      OnMenuPopupStart();
      break;
    case ax::mojom::Event::kMenuPopupEnd:
      OnMenuPopupEnd();
      break;
    case ax::mojom::Event::kCheckedStateChanged:
      OnCheckedStateChanged();
      break;
    case ax::mojom::Event::kExpandedChanged:
      OnExpandedStateChanged(HasState(ax::mojom::State::kExpanded));
      break;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
      OnFocused();
      break;
    case ax::mojom::Event::kFocusAfterMenuClose:
      // The saved focused object is not always getting cleared when a popup
      // becomes active. As a result, when the popup is dismissed, OnFocused()
      // will return early thinking focus has not changed. Rather than trying
      // to catch every case, take kFocusAfterMenuClose as a clear indication
      // that a focus change should be presented and reset the saved focus.
      g_current_focused = nullptr;
      OnFocused();
      break;
    case ax::mojom::Event::kSelection:
      OnSelected();
      // When changing tabs also fire a name changed event.
      if (GetRole() == ax::mojom::Role::kTab)
        OnDocumentTitleChanged();
      break;
    case ax::mojom::Event::kSelectedChildrenChanged:
      OnSelectedChildrenChanged();
      break;
    case ax::mojom::Event::kStateChanged:
      // We need to know what state changed and fire an event for that specific
      // state. Because we don't know what state changed, we deliberately do
      // nothing here.
      break;
    case ax::mojom::Event::kTextChanged:
      OnNameChanged();
      break;
    case ax::mojom::Event::kTextSelectionChanged:
      OnTextSelectionChanged();
      break;
    case ax::mojom::Event::kValueChanged:
      OnValueChanged();
      break;
    case ax::mojom::Event::kWindowActivated:
      if (AtkUtilAuraLinux::GetInstance()->IsAtSpiReady()) {
        OnWindowActivated();
      } else {
        AtkUtilAuraLinux::GetInstance()->PostponeEventsFor(this);
        window_activate_event_postponed_ = true;
      }
      break;
    case ax::mojom::Event::kWindowDeactivated:
      if (AtkUtilAuraLinux::GetInstance()->IsAtSpiReady()) {
        OnWindowDeactivated();
      } else {
        AtkUtilAuraLinux::GetInstance()->CancelPostponedEventsFor(this);
        window_activate_event_postponed_ = false;
      }
      break;
    case ax::mojom::Event::kWindowVisibilityChanged:
      OnWindowVisibilityChanged();
      break;
    case ax::mojom::Event::kLoadComplete:
    case ax::mojom::Event::kDocumentTitleChanged:
      // Sometimes, e.g. upon navigating away from the page, the tree is
      // rebuilt rather than modified. The kDocumentTitleChanged event occurs
      // prior to the rebuild and so is added on the previous root node. When
      // the tree is rebuilt and the old node removed, the events on the old
      // node are removed and no new kDocumentTitleChanged will be emitted. To
      // ensure we still fire the event, though, we also pay attention to
      // kLoadComplete.
      OnDocumentTitleChanged();
      break;
    case ax::mojom::Event::kAlert:
      OnAlertShown();
      break;
    default:
      break;
  }
}

std::optional<std::pair<int, int>>
AXPlatformNodeAuraLinux::GetEmbeddedObjectIndicesForId(int id) {
  auto iterator = base::ranges::find(hypertext_.hyperlinks, id);
  if (iterator == hypertext_.hyperlinks.end())
    return std::nullopt;
  int hyperlink_index = std::distance(hypertext_.hyperlinks.begin(), iterator);

  auto offset =
      base::ranges::find(hypertext_.hyperlink_offset_to_index, hyperlink_index,
                         &AXLegacyHypertext::OffsetToIndex::value_type::second);
  if (offset == hypertext_.hyperlink_offset_to_index.end())
    return std::nullopt;

  return std::make_pair(UTF16ToUnicodeOffsetInText(offset->first),
                        UTF16ToUnicodeOffsetInText(offset->first + 1));
}

std::optional<std::pair<int, int>>
AXPlatformNodeAuraLinux::GetEmbeddedObjectIndices() {
  auto* parent = FromAtkObject(GetParent());
  if (!parent)
    return std::nullopt;
  return parent->GetEmbeddedObjectIndicesForId(GetUniqueId());
}

void AXPlatformNodeAuraLinux::UpdateHypertext() {
  EnsureAtkObjectIsValid();
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  AXLegacyHypertext old_hypertext = hypertext_;
  base::OffsetAdjuster::Adjustments old_adjustments = GetHypertextAdjustments();

  UpdateComputedHypertext();
  text_unicode_adjustments_ = std::nullopt;
  offset_to_text_attributes_.clear();

  if ((!HasState(ax::mojom::State::kEditable) ||
       GetData().GetRestriction() == ax::mojom::Restriction::kReadOnly) &&
      !IsInLiveRegion()) {
    return;
  }

  if (!EmitsAtkTextEvents())
    return;

  size_t shared_prefix, old_len, new_len;
  ComputeHypertextRemovedAndInserted(old_hypertext, &shared_prefix, &old_len,
                                     &new_len);
  if (old_len > 0) {
    std::u16string removed_substring =
        old_hypertext.hypertext.substr(shared_prefix, old_len);

    size_t shared_unicode_prefix = shared_prefix;
    base::OffsetAdjuster::AdjustOffset(old_adjustments, &shared_unicode_prefix);
    size_t shared_unicode_suffix = shared_prefix + old_len;
    base::OffsetAdjuster::AdjustOffset(old_adjustments, &shared_unicode_suffix);

    g_signal_emit_by_name(
        atk_object, "text-remove",
        shared_unicode_prefix,                  // position of removal
        shared_unicode_suffix - shared_prefix,  // length of removal
        base::UTF16ToUTF8(removed_substring).c_str());
  }

  if (new_len > 0) {
    std::u16string inserted_substring =
        hypertext_.hypertext.substr(shared_prefix, new_len);
    size_t shared_unicode_prefix = UTF16ToUnicodeOffsetInText(shared_prefix);
    size_t shared_unicode_suffix =
        UTF16ToUnicodeOffsetInText(shared_prefix + new_len);
    g_signal_emit_by_name(
        atk_object, "text-insert",
        shared_unicode_prefix,                          // position of insertion
        shared_unicode_suffix - shared_unicode_prefix,  // length of insertion
        base::UTF16ToUTF8(inserted_substring).c_str());
  }
}

const AXLegacyHypertext& AXPlatformNodeAuraLinux::GetAXHypertext() {
  return hypertext_;
}

const base::OffsetAdjuster::Adjustments&
AXPlatformNodeAuraLinux::GetHypertextAdjustments() {
  if (text_unicode_adjustments_.has_value())
    return *text_unicode_adjustments_;

  text_unicode_adjustments_.emplace();

  std::u16string text = GetHypertext();
  size_t text_length = text.size();
  for (size_t i = 0; i < text_length; i++) {
    base_icu::UChar32 code_point;
    size_t original_i = i;
    base::ReadUnicodeCharacter(text.c_str(), text_length + 1, &i, &code_point);

    if ((i - original_i + 1) != 1) {
      text_unicode_adjustments_->push_back(
          base::OffsetAdjuster::Adjustment(original_i, i - original_i + 1, 1));
    }
  }

  return *text_unicode_adjustments_;
}

size_t AXPlatformNodeAuraLinux::UTF16ToUnicodeOffsetInText(
    size_t utf16_offset) {
  size_t unicode_offset = utf16_offset;
  base::OffsetAdjuster::AdjustOffset(GetHypertextAdjustments(),
                                     &unicode_offset);
  return unicode_offset;
}

size_t AXPlatformNodeAuraLinux::UnicodeToUTF16OffsetInText(int unicode_offset) {
  if (unicode_offset == kStringLengthOffset)
    return GetHypertext().size();

  size_t utf16_offset = unicode_offset;
  base::OffsetAdjuster::UnadjustOffset(GetHypertextAdjustments(),
                                       &utf16_offset);
  return utf16_offset;
}

int AXPlatformNodeAuraLinux::GetTextOffsetAtPoint(int x,
                                                  int y,
                                                  AtkCoordType atk_coord_type) {
  if (!GetExtentsRelativeToAtkCoordinateType(atk_coord_type).Contains(x, y))
    return -1;

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return -1;

  int count = atk_text::GetCharacterCount(ATK_TEXT(atk_object));
  for (int i = 0; i < count; i++) {
    int out_x, out_y, out_width, out_height;
    atk_text::GetCharacterExtents(ATK_TEXT(atk_object), i, &out_x, &out_y,
                                  &out_width, &out_height, atk_coord_type);
    gfx::Rect rect(out_x, out_y, out_width, out_height);
    if (rect.Contains(x, y))
      return i;
  }
  return -1;
}

gfx::Vector2d AXPlatformNodeAuraLinux::GetParentOriginInScreenCoordinates()
    const {
  AtkObject* parent = GetParent();
  if (!parent)
    return gfx::Vector2d();

  const AXPlatformNode* parent_node =
      AXPlatformNode::FromNativeViewAccessible(parent);
  if (!parent)
    return gfx::Vector2d();

  return parent_node->GetDelegate()
      ->GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                      AXClippingBehavior::kUnclipped)
      .OffsetFromOrigin();
}

gfx::Vector2d AXPlatformNodeAuraLinux::GetParentFrameOriginInScreenCoordinates()
    const {
  AtkObject* frame = FindAtkObjectParentFrame(atk_object_);
  if (!frame)
    return gfx::Vector2d();

  const AXPlatformNode* frame_node =
      AXPlatformNode::FromNativeViewAccessible(frame);
  if (!frame_node)
    return gfx::Vector2d();

  return frame_node->GetDelegate()
      ->GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                      AXClippingBehavior::kUnclipped)
      .OffsetFromOrigin();
}

gfx::Rect AXPlatformNodeAuraLinux::GetExtentsRelativeToAtkCoordinateType(
    AtkCoordType coord_type) const {
  gfx::Rect extents = delegate_->GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                                               AXClippingBehavior::kUnclipped);
  switch (coord_type) {
    case ATK_XY_SCREEN:
      break;
    case ATK_XY_WINDOW: {
      gfx::Vector2d window_origin = -GetParentFrameOriginInScreenCoordinates();
      extents.Offset(window_origin);
      break;
    }
    case ATK_XY_PARENT: {
      gfx::Vector2d parent_origin = -GetParentOriginInScreenCoordinates();
      extents.Offset(parent_origin);
      break;
    }
  }

  return extents;
}

void AXPlatformNodeAuraLinux::GetExtents(gint* x,
                                         gint* y,
                                         gint* width,
                                         gint* height,
                                         AtkCoordType coord_type) {
  gfx::Rect extents = GetExtentsRelativeToAtkCoordinateType(coord_type);
  if (x)
    *x = extents.x();
  if (y)
    *y = extents.y();
  if (width)
    *width = extents.width();
  if (height)
    *height = extents.height();
}

void AXPlatformNodeAuraLinux::GetPosition(gint* x,
                                          gint* y,
                                          AtkCoordType coord_type) {
  gfx::Rect extents = GetExtentsRelativeToAtkCoordinateType(coord_type);
  if (x)
    *x = extents.x();
  if (y)
    *y = extents.y();
}

void AXPlatformNodeAuraLinux::GetSize(gint* width, gint* height) {
  gfx::Rect rect_size = gfx::ToEnclosingRect(GetData().relative_bounds.bounds);
  if (width)
    *width = rect_size.width();
  if (height)
    *height = rect_size.height();
}

gfx::NativeViewAccessible
AXPlatformNodeAuraLinux::HitTestSync(gint x, gint y, AtkCoordType coord_type) {
  gfx::Point scroll_to(x, y);
  scroll_to = ConvertPointToScreenCoordinates(scroll_to, coord_type);

  AXPlatformNode* current_result = this;
  while (true) {
    gfx::NativeViewAccessible hit_child =
        current_result->GetDelegate()->HitTestSync(scroll_to.x(),
                                                   scroll_to.y());
    if (!hit_child)
      return nullptr;
    AXPlatformNode* hit_child_node =
        AXPlatformNode::FromNativeViewAccessible(hit_child);
    if (!hit_child_node || !hit_child_node->IsDescendantOf(current_result))
      break;

    // If we get the same node, we're done.
    if (hit_child_node == current_result)
      break;

    // Continue to check recursively. That's because HitTestSync may have
    // returned the best result within a particular accessibility tree,
    // but we might need to recurse further in a tree of a different type
    // (for example, from Views to Web).
    current_result = hit_child_node;
  }
  return current_result->GetNativeViewAccessible();
}

bool AXPlatformNodeAuraLinux::GrabFocus() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeAuraLinux::FocusFirstFocusableAncestorInWebContent() {
  if (!GetDelegate()->IsWebContent())
    return false;

  // Don't cross document boundaries in order to avoid having this operation
  // cross iframe boundaries or escape to non-document UI elements.
  if (GetAtkRole() == ATK_ROLE_DOCUMENT_WEB)
    return false;

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return false;

  if (IsFocusable()) {
    if (g_current_focused != atk_object)
      GrabFocus();
    return true;
  }

  auto* parent = FromAtkObject(GetParent());
  if (!parent)
    return false;

  // If any of the siblings of this element are focusable, focusing the parent
  // would be like moving the focus position backward, so we should fall back
  // to setting the sequential focus navigation starting point.
  for (auto child_iterator_ptr = parent->GetDelegate()->ChildrenBegin();
       *child_iterator_ptr != *parent->GetDelegate()->ChildrenEnd();
       ++(*child_iterator_ptr)) {
    auto* child = FromAtkObject(child_iterator_ptr->GetNativeViewAccessible());
    if (!child || child == this)
      continue;

    if (child->IsFocusable())
      return false;
  }

  return parent->FocusFirstFocusableAncestorInWebContent();
}

bool AXPlatformNodeAuraLinux::SetSequentialFocusNavigationStartingPoint() {
  AXActionData action_data;
  action_data.action =
      ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeAuraLinux::
    GrabFocusOrSetSequentialFocusNavigationStartingPoint() {
  // First we try to grab focus on this node if any ancestor in the same
  // document is focusable. Otherwise we set the sequential navigation starting
  // point.
  if (!FocusFirstFocusableAncestorInWebContent())
    return SetSequentialFocusNavigationStartingPoint();
  else
    return true;
}

bool AXPlatformNodeAuraLinux::
    GrabFocusOrSetSequentialFocusNavigationStartingPointAtOffset(int offset) {
  int child_count = delegate_->GetChildCount();
  if (IsAtomicTextField() || child_count == 0)
    return GrabFocusOrSetSequentialFocusNavigationStartingPoint();

  // When this node has children, we walk through them to figure out what child
  // node should get focus. We are essentially repeating the process used when
  // building the hypertext here.
  int current_offset = 0;
  for (int i = 0; i < child_count; ++i) {
    auto* child = FromAtkObject(delegate_->ChildAtIndex(i));
    if (!child)
      continue;

    if (child->IsText()) {
      current_offset += child->GetName().size();
    } else {
      // Add an offset for the embedded character.
      current_offset += 1;
    }

    // If the offset is larger than our size, try to work with the last child,
    // which is also the behavior of SetCaretOffset.
    if (offset <= current_offset || i == child_count - 1)
      return child->GrabFocusOrSetSequentialFocusNavigationStartingPoint();
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

const gchar* AXPlatformNodeAuraLinux::GetDefaultActionName() {
  int action;
  if (!GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb, &action))
    return nullptr;

  // If this object cannot receive focus and has a button role, use click as
  // the default action. On the AuraLinux platform, the press action is a
  // signal to users that they can trigger the action using the keyboard, while
  // a click action means the user should trigger the action via a simulated
  // click. If this object cannot receive focus, it's impossible to trigger it
  // with a key press.
  if (GetRole() == ax::mojom::Role::kButton &&
      action == static_cast<int>(ax::mojom::DefaultActionVerb::kPress) &&
      !IsFocusable()) {
    action = static_cast<int>(ax::mojom::DefaultActionVerb::kClick);
  }

  std::string action_verb =
      ui::ToString(static_cast<ax::mojom::DefaultActionVerb>(action));

  ATK_AURALINUX_RETURN_STRING(action_verb);
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetAtkAttributes() {
  AtkAttributeSet* attribute_list = nullptr;
  ComputeAttributes(&attribute_list);
  return attribute_list;
}

AtkStateType AXPlatformNodeAuraLinux::GetAtkStateTypeForCheckableNode() {
  if (GetData().GetCheckedState() == ax::mojom::CheckedState::kMixed)
    return ATK_STATE_INDETERMINATE;
  if (IsPlatformCheckable())
    return ATK_STATE_CHECKED;
  return ATK_STATE_PRESSED;
}

// AtkDocumentHelpers

const gchar* AXPlatformNodeAuraLinux::GetDocumentAttributeValue(
    const gchar* attribute) const {
  if (!g_ascii_strcasecmp(attribute, "DocType"))
    return delegate_->GetTreeData().doctype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "MimeType"))
    return delegate_->GetTreeData().mimetype.c_str();
  else if (!g_ascii_strcasecmp(attribute, "Title"))
    return delegate_->GetTreeData().title.c_str();
  else if (!g_ascii_strcasecmp(attribute, "URI"))
    return delegate_->GetTreeData().url.c_str();

  return nullptr;
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetDocumentAttributes() const {
  AtkAttributeSet* attribute_set = nullptr;
  const gchar* doc_attributes[] = {"DocType", "MimeType", "Title", "URI"};
  const gchar* value = nullptr;

  for (unsigned i = 0; i < G_N_ELEMENTS(doc_attributes); i++) {
    value = GetDocumentAttributeValue(doc_attributes[i]);
    if (value) {
      attribute_set = PrependAtkAttributeToAtkAttributeSet(
          doc_attributes[i], value, attribute_set);
    }
  }

  return attribute_set;
}

//
// AtkHyperlink helpers
//

AtkHyperlink* AXPlatformNodeAuraLinux::GetAtkHyperlink() {
  if (atk_hyperlink_)
    return atk_hyperlink_;

  atk_hyperlink_ =
      ATK_HYPERLINK(g_object_new(AX_PLATFORM_ATK_HYPERLINK_TYPE, 0));
  ax_platform_atk_hyperlink_set_object(
      AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_.get()), this);
  return atk_hyperlink_;
}

//
// Misc helpers
//

void AXPlatformNodeAuraLinux::GetFloatAttributeInGValue(
    ax::mojom::FloatAttribute attr,
    GValue* value) {
  float float_val;
  if (GetFloatAttribute(attr, &float_val)) {
    memset(value, 0, sizeof(*value));
    g_value_init(value, G_TYPE_FLOAT);
    g_value_set_float(value, float_val);
  }
}

void AXPlatformNodeAuraLinux::AddAttributeToList(const char* name,
                                                 const char* value,
                                                 AtkAttributeSet** attributes) {
  *attributes = PrependAtkAttributeToAtkAttributeSet(name, value, *attributes);
}

void AXPlatformNodeAuraLinux::SetDocumentParent(
    AtkObject* new_document_parent) {
  DCHECK(GetAtkRole() == ATK_ROLE_FRAME);
  SetWeakGPtrToAtkObject(&document_parent_, new_document_parent);
}

bool AXPlatformNodeAuraLinux::IsNameExposed() {
  switch (GetRole()) {
    case ax::mojom::Role::kListMarker:
      return !GetChildCount();
    default:
      return true;
  }
}

int AXPlatformNodeAuraLinux::GetCaretOffset() {
  if (!HasVisibleCaretOrSelection()) {
    std::optional<FindInPageResultInfo> result =
        GetSelectionOffsetsFromFindInPage();
    AtkObject* atk_object = GetOrCreateAtkObject();
    if (!atk_object)
      return -1;
    if (result.has_value() && result->node == atk_object)
      return UTF16ToUnicodeOffsetInText(result->end_offset);
    return -1;
  }

  std::pair<int, int> selection = GetSelectionOffsetsForAtk();
  return UTF16ToUnicodeOffsetInText(selection.second);
}

bool AXPlatformNodeAuraLinux::SetCaretOffset(int offset) {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return false;

  int character_count = atk_text_get_character_count(ATK_TEXT(atk_object));
  if (offset < 0 || offset > character_count)
    offset = character_count;

  // Even if we don't change anything, we still want to act like we
  // were successful.
  if (offset == GetCaretOffset() && !HasSelection())
    return true;

  offset = UnicodeToUTF16OffsetInText(offset);
  if (!SetHypertextSelection(offset, offset))
    return false;

  return true;
}

bool AXPlatformNodeAuraLinux::SetTextSelectionForAtkText(int start_offset,
                                                         int end_offset) {
  start_offset = UnicodeToUTF16OffsetInText(start_offset);
  end_offset = UnicodeToUTF16OffsetInText(end_offset);

  std::u16string text = GetHypertext();
  if (start_offset < 0 || start_offset > static_cast<int>(text.length()))
    return false;
  if (end_offset < 0 || end_offset > static_cast<int>(text.length()))
    return false;

  // We must put these in the correct order so that we can do
  // a comparison with the existing start and end below.
  if (end_offset < start_offset)
    std::swap(start_offset, end_offset);

  // Even if we don't change anything, we still want to act like we
  // were successful.
  std::pair<int, int> old_offsets = GetSelectionOffsetsForAtk();
  if (old_offsets.first == start_offset && old_offsets.second == end_offset)
    return true;

  if (!SetHypertextSelection(start_offset, end_offset))
    return false;

  return true;
}

bool AXPlatformNodeAuraLinux::HasSelection() {
  std::pair<int, int> selection = GetSelectionOffsetsForAtk();
  return selection.first >= 0 && selection.second >= 0 &&
         selection.first != selection.second;
}

void AXPlatformNodeAuraLinux::GetSelectionExtents(int* start_offset,
                                                  int* end_offset) {
  if (start_offset)
    *start_offset = 0;
  if (end_offset)
    *end_offset = 0;

  std::pair<int, int> selection = GetSelectionOffsetsForAtk();
  if (selection.first < 0 || selection.second < 0 ||
      selection.first == selection.second)
    return;

  // We should ignore the direction of the selection when exposing start and
  // end offsets. According to the ATK documentation the end offset is always
  // the offset immediately past the end of the selection. This wouldn't make
  // sense if end < start.
  if (selection.second < selection.first)
    std::swap(selection.first, selection.second);

  selection.first = UTF16ToUnicodeOffsetInText(selection.first);
  selection.second = UTF16ToUnicodeOffsetInText(selection.second);

  if (start_offset)
    *start_offset = selection.first;
  if (end_offset)
    *end_offset = selection.second;
}

// Since this method doesn't return a static gchar*, we expect the caller of
// atk_text_get_selection to free the return value.
gchar* AXPlatformNodeAuraLinux::GetSelectionWithText(int* start_offset,
                                                     int* end_offset) {
  int selection_start, selection_end;
  GetSelectionExtents(&selection_start, &selection_end);

  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return nullptr;

  if (selection_start < 0 || selection_end < 0 ||
      selection_start == selection_end) {
    std::optional<FindInPageResultInfo> find_in_page_result =
        GetSelectionOffsetsFromFindInPage();
    if (!find_in_page_result.has_value() ||
        find_in_page_result->node != atk_object) {
      *start_offset = 0;
      *end_offset = 0;
      return nullptr;
    }

    selection_start = find_in_page_result->start_offset;
    selection_end = find_in_page_result->end_offset;
  }

  selection_start = UTF16ToUnicodeOffsetInText(selection_start);
  selection_end = UTF16ToUnicodeOffsetInText(selection_end);
  if (selection_start < 0 || selection_end < 0 ||
      selection_start == selection_end) {
    return nullptr;
  }

  if (start_offset)
    *start_offset = selection_start;
  if (end_offset)
    *end_offset = selection_end;
  return atk_text::GetText(ATK_TEXT(atk_object), selection_start,
                           selection_end);
}

bool AXPlatformNodeAuraLinux::IsInLiveRegion() {
  return HasStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus);
}

void AXPlatformNodeAuraLinux::ScrollToPoint(AtkCoordType atk_coord_type,
                                            int x,
                                            int y) {
  gfx::Point scroll_to(x, y);
  scroll_to = ConvertPointToScreenCoordinates(scroll_to, atk_coord_type);

  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToPoint;
  action_data.target_point = scroll_to;
  GetDelegate()->AccessibilityPerformAction(action_data);
}

void AXPlatformNodeAuraLinux::ScrollNodeRectIntoView(
    gfx::Rect rect,
    AtkScrollType atk_scroll_type) {
  AXActionData action_data;
  action_data.target_node_id = GetData().id;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_rect = rect;

  action_data.scroll_behavior = ax::mojom::ScrollBehavior::kScrollIfVisible;
  action_data.horizontal_scroll_alignment = ax::mojom::ScrollAlignment::kNone;
  action_data.vertical_scroll_alignment = ax::mojom::ScrollAlignment::kNone;

  switch (atk_scroll_type) {
    case ATK_SCROLL_TOP_LEFT:
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentTop;
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentLeft;
      break;
    case ATK_SCROLL_BOTTOM_RIGHT:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentRight;
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentBottom;
      break;
    case ATK_SCROLL_TOP_EDGE:
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentTop;
      break;
    case ATK_SCROLL_BOTTOM_EDGE:
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentBottom;
      break;
    case ATK_SCROLL_LEFT_EDGE:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentLeft;
      break;
    case ATK_SCROLL_RIGHT_EDGE:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentRight;
      break;
    case ATK_SCROLL_ANYWHERE:
      action_data.horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge;
      action_data.vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge;
      break;
  }

  GetDelegate()->AccessibilityPerformAction(action_data);
}

void AXPlatformNodeAuraLinux::ScrollNodeIntoView(
    AtkScrollType atk_scroll_type) {
  gfx::Rect rect = GetDelegate()->GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                                                AXClippingBehavior::kUnclipped);
  rect -= rect.OffsetFromOrigin();
  ScrollNodeRectIntoView(rect, atk_scroll_type);
}

std::optional<gfx::Rect>
AXPlatformNodeAuraLinux::GetUnclippedHypertextRangeBoundsRect(int start_offset,
                                                              int end_offset) {
  start_offset = UnicodeToUTF16OffsetInText(start_offset);
  end_offset = UnicodeToUTF16OffsetInText(end_offset);

  std::u16string text = GetHypertext();
  if (start_offset < 0 || start_offset > static_cast<int>(text.length()))
    return std::nullopt;
  if (end_offset < 0 || end_offset > static_cast<int>(text.length()))
    return std::nullopt;

  if (end_offset < start_offset)
    std::swap(start_offset, end_offset);

  return GetDelegate()->GetHypertextRangeBoundsRect(
      UnicodeToUTF16OffsetInText(start_offset),
      UnicodeToUTF16OffsetInText(end_offset), AXCoordinateSystem::kScreenDIPs,
      AXClippingBehavior::kUnclipped);
}

bool AXPlatformNodeAuraLinux::ScrollSubstringIntoView(
    AtkScrollType atk_scroll_type,
    int start_offset,
    int end_offset) {
  std::optional<gfx::Rect> optional_rect =
      GetUnclippedHypertextRangeBoundsRect(start_offset, end_offset);
  if (!optional_rect.has_value())
    return false;

  gfx::Rect rect = *optional_rect;
  gfx::Rect node_rect = GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreenDIPs, AXClippingBehavior::kUnclipped);
  rect -= node_rect.OffsetFromOrigin();
  ScrollNodeRectIntoView(rect, atk_scroll_type);

  return true;
}

bool AXPlatformNodeAuraLinux::ScrollSubstringToPoint(
    int start_offset,
    int end_offset,
    AtkCoordType atk_coord_type,
    int x,
    int y) {
  std::optional<gfx::Rect> optional_rect =
      GetUnclippedHypertextRangeBoundsRect(start_offset, end_offset);
  if (!optional_rect.has_value())
    return false;

  gfx::Rect rect = *optional_rect;
  gfx::Rect node_rect = GetDelegate()->GetBoundsRect(
      AXCoordinateSystem::kScreenDIPs, AXClippingBehavior::kUnclipped);
  ScrollToPoint(atk_coord_type, x - (rect.x() - node_rect.x()),
                y - (rect.y() - node_rect.y()));

  return true;
}

void AXPlatformNodeAuraLinux::ComputeStylesIfNeeded() {
  if (!offset_to_text_attributes_.empty())
    return;

  default_text_attributes_ = ComputeTextAttributes();
  TextAttributeMap attributes_map =
      GetDelegate()->ComputeTextAttributeMap(default_text_attributes_);
  offset_to_text_attributes_.swap(attributes_map);
}

int AXPlatformNodeAuraLinux::FindStartOfStyle(
    int start_offset,
    ax::mojom::MoveDirection direction) {
  int text_length = GetHypertext().length();
  DCHECK_GE(start_offset, 0);
  DCHECK_LE(start_offset, text_length);
  DCHECK(!offset_to_text_attributes_.empty());

  switch (direction) {
    case ax::mojom::MoveDirection::kNone:
      NOTREACHED_IN_MIGRATION();
      return start_offset;
    case ax::mojom::MoveDirection::kBackward: {
      auto iterator = offset_to_text_attributes_.upper_bound(start_offset);
      --iterator;
      return iterator->first;
    }
    case ax::mojom::MoveDirection::kForward: {
      const auto iterator =
          offset_to_text_attributes_.upper_bound(start_offset);
      if (iterator == offset_to_text_attributes_.end())
        return text_length;
      return iterator->first;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return start_offset;
}

const TextAttributeList& AXPlatformNodeAuraLinux::GetTextAttributes(
    int offset,
    int* start_offset,
    int* end_offset) {
  ComputeStylesIfNeeded();
  DCHECK(!offset_to_text_attributes_.empty());

  int utf16_offset = UnicodeToUTF16OffsetInText(offset);
  int style_start =
      FindStartOfStyle(utf16_offset, ax::mojom::MoveDirection::kBackward);
  int style_end =
      FindStartOfStyle(utf16_offset, ax::mojom::MoveDirection::kForward);

  auto iterator = offset_to_text_attributes_.find(style_start);
  CHECK(iterator != offset_to_text_attributes_.end(),
        base::NotFatalUntil::M130);

  SetIntPointerValueIfNotNull(start_offset,
                              UTF16ToUnicodeOffsetInText(style_start));
  SetIntPointerValueIfNotNull(end_offset,
                              UTF16ToUnicodeOffsetInText(style_end));

  if (iterator == offset_to_text_attributes_.end())
    return default_text_attributes_;

  return iterator->second;
}

const TextAttributeList& AXPlatformNodeAuraLinux::GetDefaultTextAttributes() {
  ComputeStylesIfNeeded();
  return default_text_attributes_;
}

void AXPlatformNodeAuraLinux::TerminateFindInPage() {
  ForgetCurrentFindInPageResult();
}

void AXPlatformNodeAuraLinux::ActivateFindInPageResult(int start_offset,
                                                       int end_offset) {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  DCHECK(ATK_IS_TEXT(atk_object));

  if (!EmitsAtkTextEvents()) {
    ActivateFindInPageInParent(start_offset, end_offset);
    return;
  }

  AtkObject* parent_doc = FindAtkObjectToplevelParentDocument(atk_object);
  if (!parent_doc)
    return;

  std::map<AtkObject*, FindInPageResultInfo>& active_results =
      GetActiveFindInPageResults();
  auto iterator = active_results.find(parent_doc);
  FindInPageResultInfo new_info = {atk_object, start_offset, end_offset};
  if (iterator != active_results.end() && iterator->second == new_info)
    return;

  active_results[parent_doc] = new_info;
  g_signal_emit_by_name(atk_object, "text-selection-changed");
  g_signal_emit_by_name(atk_object, "text-caret-moved",
                        UTF16ToUnicodeOffsetInText(end_offset));
}

std::optional<std::pair<int, int>>
AXPlatformNodeAuraLinux::GetHypertextExtentsOfChild(
    AXPlatformNodeAuraLinux* child_to_find) {
  int current_offset = 0;
  for (auto child_iterator_ptr = GetDelegate()->ChildrenBegin();
       *child_iterator_ptr != *GetDelegate()->ChildrenEnd();
       ++(*child_iterator_ptr)) {
    auto* child = FromAtkObject(child_iterator_ptr->GetNativeViewAccessible());
    if (!child)
      continue;

    // If this object is a text only object, it is included directly into this
    // node's hypertext, otherwise it is represented as an embedded object
    // character.
    int size = child->IsText() ? child->GetName().size() : 1;
    if (child == child_to_find)
      return std::make_pair(current_offset, current_offset + size);
    current_offset += size;
  }

  return std::nullopt;
}

void AXPlatformNodeAuraLinux::ActivateFindInPageInParent(int start_offset,
                                                         int end_offset) {
  auto* parent = FromAtkObject(GetParent());
  if (!parent)
    return;

  std::optional<std::pair<int, int>> extents_in_parent =
      parent->GetHypertextExtentsOfChild(this);
  if (!extents_in_parent.has_value())
    return;

  DCHECK(IsText());
  parent->ActivateFindInPageResult(extents_in_parent->first + start_offset,
                                   extents_in_parent->first + end_offset);
}

void AXPlatformNodeAuraLinux::ForgetCurrentFindInPageResult() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return;

  AtkObject* parent_doc = FindAtkObjectToplevelParentDocument(atk_object);
  if (parent_doc)
    GetActiveFindInPageResults().erase(parent_doc);
}

std::optional<FindInPageResultInfo>
AXPlatformNodeAuraLinux::GetSelectionOffsetsFromFindInPage() {
  AtkObject* atk_object = GetOrCreateAtkObject();
  if (!atk_object)
    return std::nullopt;

  AtkObject* parent_doc = FindAtkObjectToplevelParentDocument(atk_object);
  if (!parent_doc)
    return std::nullopt;

  std::map<AtkObject*, FindInPageResultInfo>& active_results =
      GetActiveFindInPageResults();
  auto iterator = active_results.find(parent_doc);
  if (iterator == active_results.end())
    return std::nullopt;

  return iterator->second;
}

gfx::Point AXPlatformNodeAuraLinux::ConvertPointToScreenCoordinates(
    const gfx::Point& point,
    AtkCoordType atk_coord_type) {
  switch (atk_coord_type) {
    case ATK_XY_WINDOW:
      return point + GetParentFrameOriginInScreenCoordinates();
    case ATK_XY_PARENT:
      return point + GetParentOriginInScreenCoordinates();
    case ATK_XY_SCREEN:
    default:
      return point;
  }
}

std::pair<int, int> AXPlatformNodeAuraLinux::GetSelectionOffsetsForAtk() {
  // In web content we always want to look at the selection from the tree
  // instead of the selection that might be set via node attributes. This is
  // because the tree selection is the absolute truth about what is visually
  // selected, whereas node attributes might contain selection extents that are
  // no longer part of the visual selection.
  std::pair<int, int> selection;
  if (GetDelegate()->IsWebContent()) {
    AXSelection unignored_selection = GetDelegate()->GetUnignoredSelection();
    GetSelectionOffsetsFromTree(&unignored_selection, &selection.first,
                                &selection.second);
  } else {
    GetSelectionOffsets(&selection.first, &selection.second);
  }
  return selection;
}

}  // namespace ui
