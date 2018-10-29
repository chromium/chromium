// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_atk_hyperlink.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

//
// ax_platform_node_auralinux AtkObject definition and implementation.
//

G_BEGIN_DECLS

#define AX_PLATFORM_NODE_AURALINUX_TYPE (ax_platform_node_auralinux_get_type())
#define AX_PLATFORM_NODE_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST( \
        (obj), AX_PLATFORM_NODE_AURALINUX_TYPE, AXPlatformNodeAuraLinuxObject))
#define AX_PLATFORM_NODE_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST( \
        (klass), AX_PLATFORM_NODE_AURALINUX_TYPE, AXPlatformNodeAuraLinuxClass))
#define IS_AX_PLATFORM_NODE_AURALINUX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define IS_AX_PLATFORM_NODE_AURALINUX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), AX_PLATFORM_NODE_AURALINUX_TYPE))
#define AX_PLATFORM_NODE_AURALINUX_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS( \
        (obj), AX_PLATFORM_NODE_AURALINUX_TYPE, AXPlatformNodeAuraLinuxClass))

typedef struct _AXPlatformNodeAuraLinuxObject AXPlatformNodeAuraLinuxObject;
typedef struct _AXPlatformNodeAuraLinuxClass AXPlatformNodeAuraLinuxClass;

// TODO(aleventhal) Remove this and use atk_role_get_name() once the following
// GNOME bug is fixed: https://bugzilla.gnome.org/show_bug.cgi?id=795983
const char* const kRoleNames[] = {
    "invalid",  // ATK_ROLE_INVALID.
    "accelerator label",
    "alert",
    "animation",
    "arrow",
    "calendar",
    "canvas",
    "check box",
    "check menu item",
    "color chooser",
    "column header",
    "combo box",
    "dateeditor",
    "desktop icon",
    "desktop frame",
    "dial",
    "dialog",
    "directory pane",
    "drawing area",
    "file chooser",
    "filler",
    "fontchooser",
    "frame",
    "glass pane",
    "html container",
    "icon",
    "image",
    "internal frame",
    "label",
    "layered pane",
    "list",
    "list item",
    "menu",
    "menu bar",
    "menu item",
    "option pane",
    "page tab",
    "page tab list",
    "panel",
    "password text",
    "popup menu",
    "progress bar",
    "push button",
    "radio button",
    "radio menu item",
    "root pane",
    "row header",
    "scroll bar",
    "scroll pane",
    "separator",
    "slider",
    "split pane",
    "spin button",
    "statusbar",
    "table",
    "table cell",
    "table column header",
    "table row header",
    "tear off menu item",
    "terminal",
    "text",
    "toggle button",
    "tool bar",
    "tool tip",
    "tree",
    "tree table",
    "unknown",
    "viewport",
    "window",
    "header",
    "footer",
    "paragraph",
    "ruler",
    "application",
    "autocomplete",
    "edit bar",
    "embedded component",
    "entry",
    "chart",
    "caption",
    "document frame",
    "heading",
    "page",
    "section",
    "redundant object",
    "form",
    "link",
    "input method window",
    "table row",
    "tree item",
    "document spreadsheet",
    "document presentation",
    "document text",
    "document web",
    "document email",
    "comment",
    "list box",
    "grouping",
    "image map",
    "notification",
    "info bar",
    "level bar",
    "title bar",
    "block quote",
    "audio",
    "video",
    "definition",
    "article",
    "landmark",
    "log",
    "marquee",
    "math",
    "rating",
    "timer",
    "description list",
    "description term",
    "description value",
    "static",
    "math fraction",
    "math root",
    "subscript",
    "superscript",
    "footnote",  // ATK_ROLE_FOOTNOTE = 122.
};

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 16, 0)
#define ATK_216
#endif

#if defined(ATK_CHECK_VERSION) && ATK_CHECK_VERSION(2, 26, 0)
#define ATK_226
#endif

#if defined(ATK_216)
constexpr AtkRole kStaticRole = ATK_ROLE_STATIC;
constexpr AtkRole kSubscriptRole = ATK_ROLE_SUBSCRIPT;
constexpr AtkRole kSuperscriptRole = ATK_ROLE_SUPERSCRIPT;
#else
constexpr AtkRole kStaticRole = ATK_ROLE_TEXT;
constexpr AtkRole kSubscriptRole = ATK_ROLE_TEXT;
constexpr AtkRole kSuperscriptRole = ATK_ROLE_TEXT;
#endif

#if defined(ATK_226)
constexpr AtkRole kAtkFootnoteRole = ATK_ROLE_FOOTNOTE;
#else
constexpr AtkRole kAtkFootnoteRole = ATK_ROLE_LIST_ITEM;
#endif

struct _AXPlatformNodeAuraLinuxObject {
  AtkObject parent;
  AXPlatformNodeAuraLinux* m_object;
};

struct _AXPlatformNodeAuraLinuxClass {
  AtkObjectClass parent_class;
};

GType ax_platform_node_auralinux_get_type();

static gpointer kAXPlatformNodeAuraLinuxParentClass = nullptr;

static AXPlatformNodeAuraLinux* ToAXPlatformNodeAuraLinux(
    AXPlatformNodeAuraLinuxObject* atk_object) {
  if (!atk_object)
    return nullptr;

  return atk_object->m_object;
}

static AXPlatformNodeAuraLinux* AtkObjectToAXPlatformNodeAuraLinux(
    AtkObject* atk_object) {
  if (!atk_object)
    return nullptr;

  if (IS_AX_PLATFORM_NODE_AURALINUX(atk_object))
    return ToAXPlatformNodeAuraLinux(AX_PLATFORM_NODE_AURALINUX(atk_object));

  return nullptr;
}

static const gchar* AXPlatformNodeAuraLinuxGetName(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  ax::mojom::NameFrom name_from = obj->GetData().GetNameFrom();
  if (obj->GetStringAttribute(ax::mojom::StringAttribute::kName).empty() &&
      name_from != ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kName).c_str();
}

static const gchar* AXPlatformNodeAuraLinuxGetDescription(
    AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

static gint AXPlatformNodeAuraLinuxGetIndexInParent(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);

  if (!obj)
    return -1;

  return obj->GetIndexInParent();
}

static AtkObject* AXPlatformNodeAuraLinuxGetParent(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetParent();
}

static gint AXPlatformNodeAuraLinuxGetNChildren(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return obj->GetChildCount();
}

static AtkObject* AXPlatformNodeAuraLinuxRefChild(AtkObject* atk_object,
                                                  gint index) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  AtkObject* result = obj->ChildAtIndex(index);
  if (result)
    g_object_ref(result);
  return result;
}

static AtkRelationSet* AXPlatformNodeAuraLinuxRefRelationSet(
    AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  AtkRelationSet* atk_relation_set =
      ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
          ->ref_relation_set(atk_object);

  if (!obj)
    return atk_relation_set;

  obj->GetAtkRelations(atk_relation_set);
  return atk_relation_set;
}

static AtkAttributeSet* AXPlatformNodeAuraLinuxGetAttributes(
    AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetAtkAttributes();
}

static AtkRole AXPlatformNodeAuraLinuxGetRole(AtkObject* atk_object) {
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return ATK_ROLE_INVALID;
  return obj->GetAtkRole();
}

static AtkStateSet* AXPlatformNodeAuraLinuxRefStateSet(AtkObject* atk_object) {
  AtkStateSet* atk_state_set =
      ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
          ->ref_state_set(atk_object);

  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFUNCT);
  } else {
    obj->GetAtkState(atk_state_set);
  }
  return atk_state_set;
}

//
// AtkComponent interface
//

static gfx::Point FindAtkObjectParentCoords(AtkObject* atk_object) {
  if (!atk_object)
    return gfx::Point(0, 0);

  if (atk_object_get_role(atk_object) == ATK_ROLE_FRAME) {
    int x, y;
    atk_component_get_extents(ATK_COMPONENT(atk_object),
        &x, &y, nullptr, nullptr, ATK_XY_WINDOW);
    gfx::Point window_coords(x, y);
    return window_coords;
  }
  atk_object = atk_object_get_parent(atk_object);

  return FindAtkObjectParentCoords(atk_object);
}

static void AXPlatformNodeAuraLinuxGetExtents(AtkComponent* atk_component,
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
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetExtents(x, y, width, height, coord_type);
}

static void AXPlatformNodeAuraLinuxGetPosition(AtkComponent* atk_component,
                                               gint* x,
                                               gint* y,
                                               AtkCoordType coord_type) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (x)
    *x = 0;
  if (y)
    *y = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

static void AXPlatformNodeAuraLinuxGetSize(AtkComponent* atk_component,
                                           gint* width,
                                           gint* height) {
  g_return_if_fail(ATK_IS_COMPONENT(atk_component));

  if (width)
    *width = 0;
  if (height)
    *height = 0;

  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

static AtkObject* AXPlatformNodeAuraLinuxRefAccessibleAtPoint(
    AtkComponent* atk_component,
    gint x,
    gint y,
    AtkCoordType coord_type) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), nullptr);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  AtkObject* result = obj->HitTestSync(x, y, coord_type);
  if (result)
    g_object_ref(result);
  return result;
}

static gboolean AXPlatformNodeAuraLinuxGrabFocus(AtkComponent* atk_component) {
  g_return_val_if_fail(ATK_IS_COMPONENT(atk_component), FALSE);
  AtkObject* atk_object = ATK_OBJECT(atk_component);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->GrabFocus();
}

void AXComponentInterfaceBaseInit(AtkComponentIface* iface) {
  iface->get_extents = AXPlatformNodeAuraLinuxGetExtents;
  iface->get_position = AXPlatformNodeAuraLinuxGetPosition;
  iface->get_size = AXPlatformNodeAuraLinuxGetSize;
  iface->ref_accessible_at_point = AXPlatformNodeAuraLinuxRefAccessibleAtPoint;
  iface->grab_focus = AXPlatformNodeAuraLinuxGrabFocus;
}

static const GInterfaceInfo ComponentInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXComponentInterfaceBaseInit), 0, 0};

//
// AtkAction interface
//

static gboolean AXPlatformNodeAuraLinuxDoAction(AtkAction* atk_action,
                                                gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), FALSE);
  g_return_val_if_fail(!index, FALSE);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return FALSE;

  return obj->DoDefaultAction();
}

static gint AXPlatformNodeAuraLinuxGetNActions(AtkAction* atk_action) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  return 1;
}

static const gchar* AXPlatformNodeAuraLinuxGetActionDescription(AtkAction*,
                                                                gint) {
  // Not implemented. Right now Orca does not provide this and
  // Chromium is not providing a string for the action description.
  return nullptr;
}

static const gchar* AXPlatformNodeAuraLinuxGetActionName(AtkAction* atk_action,
                                                         gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDefaultActionName();
}

static const gchar* AXPlatformNodeAuraLinuxGetActionKeybinding(
    AtkAction* atk_action,
    gint index) {
  g_return_val_if_fail(ATK_IS_ACTION(atk_action), nullptr);
  g_return_val_if_fail(!index, nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_action);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kAccessKey)
      .c_str();
}

void AXActionInterfaceBaseInit(AtkActionIface* iface) {
  iface->do_action = AXPlatformNodeAuraLinuxDoAction;
  iface->get_n_actions = AXPlatformNodeAuraLinuxGetNActions;
  iface->get_description = AXPlatformNodeAuraLinuxGetActionDescription;
  iface->get_name = AXPlatformNodeAuraLinuxGetActionName;
  iface->get_keybinding = AXPlatformNodeAuraLinuxGetActionKeybinding;
}

static const GInterfaceInfo ActionInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXActionInterfaceBaseInit), nullptr,
    nullptr};

// AtkDocument interface.

static const gchar* AXPlatformNodeAuraLinuGetDocumentAttributeValue(
    AtkDocument* atk_doc,
    const gchar* attribute) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributeValue(attribute);
}

static AtkAttributeSet* AXPlatformNodeAuraLinuxGetDocumentAttributes(
    AtkDocument* atk_doc) {
  g_return_val_if_fail(ATK_IS_DOCUMENT(atk_doc), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_doc);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetDocumentAttributes();
}

void AXDocumentInterfaceBaseInit(AtkDocumentIface* iface) {
  iface->get_document_attribute_value =
      AXPlatformNodeAuraLinuGetDocumentAttributeValue;
  iface->get_document_attributes = AXPlatformNodeAuraLinuxGetDocumentAttributes;
}

static const GInterfaceInfo DocumentInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXDocumentInterfaceBaseInit), nullptr,
    nullptr};

//
// AtkImage interface.
//

static void AXPlatformNodeGetImagePosition(AtkImage* atk_img,
                                           gint* x,
                                           gint* y,
                                           AtkCoordType coord_type) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetPosition(x, y, coord_type);
}

static const gchar* AXPlatformNodeAuraLinuxGetImageDescription(
    AtkImage* atk_img) {
  g_return_val_if_fail(ATK_IMAGE(atk_img), nullptr);

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  return obj->GetStringAttribute(ax::mojom::StringAttribute::kDescription)
      .c_str();
}

static void AXPlatformNodeAuraLinuxGetImageSize(AtkImage* atk_img,
                                                gint* width,
                                                gint* height) {
  g_return_if_fail(ATK_IMAGE(atk_img));

  AtkObject* atk_object = ATK_OBJECT(atk_img);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetSize(width, height);
}

void AXImageInterfaceBaseInit(AtkImageIface* iface) {
  iface->get_image_position = AXPlatformNodeGetImagePosition;
  iface->get_image_description = AXPlatformNodeAuraLinuxGetImageDescription;
  iface->get_image_size = AXPlatformNodeAuraLinuxGetImageSize;
}

static const GInterfaceInfo ImageInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXImageInterfaceBaseInit), nullptr,
    nullptr};

//
// AtkValue interface
//

static void AXPlatformNodeAuraLinuxGetCurrentValue(AtkValue* atk_value,
                                                   GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kValueForRange,
                                 value);
}

static void AXPlatformNodeAuraLinuxGetMinimumValue(AtkValue* atk_value,
                                                   GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMinValueForRange,
                                 value);
}

static void AXPlatformNodeAuraLinuxGetMaximumValue(AtkValue* atk_value,
                                                   GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kMaxValueForRange,
                                 value);
}

static void AXPlatformNodeAuraLinuxGetMinimumIncrement(AtkValue* atk_value,
                                                       GValue* value) {
  g_return_if_fail(ATK_VALUE(atk_value));

  AtkObject* atk_object = ATK_OBJECT(atk_value);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return;

  obj->GetFloatAttributeInGValue(ax::mojom::FloatAttribute::kStepValueForRange,
                                 value);
}

static void AXValueInterfaceBaseInit(AtkValueIface* iface) {
  iface->get_current_value = AXPlatformNodeAuraLinuxGetCurrentValue;
  iface->get_maximum_value = AXPlatformNodeAuraLinuxGetMaximumValue;
  iface->get_minimum_value = AXPlatformNodeAuraLinuxGetMinimumValue;
  iface->get_minimum_increment = AXPlatformNodeAuraLinuxGetMinimumIncrement;
}

static const GInterfaceInfo ValueInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXValueInterfaceBaseInit), nullptr,
    nullptr};

//
// AtkHyperlinkImpl interface.
//

static AtkHyperlink* AXPlatformNodeAuraLinuxGetHyperlink(
    AtkHyperlinkImpl* atk_hyperlink_impl) {
  g_return_val_if_fail(ATK_HYPERLINK_IMPL(atk_hyperlink_impl), 0);

  AtkObject* atk_object = ATK_OBJECT(atk_hyperlink_impl);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  AtkHyperlink* atk_hyperlink = obj->GetAtkHyperlink();
  g_object_ref(atk_hyperlink);

  return atk_hyperlink;
}

void AXHyperlinkImplInterfaceBaseInit(AtkHyperlinkImplIface* iface) {
  iface->get_hyperlink = AXPlatformNodeAuraLinuxGetHyperlink;
}

static const GInterfaceInfo HyperlinkImplInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXHyperlinkImplInterfaceBaseInit),
    nullptr, nullptr};

//
// AtkHypertext interface.
//

static AtkHyperlink* AXPlatformNodeAuraLinuxHypertextGetLink(
    AtkHypertext* hypertext,
    int index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  auto* obj = AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));
  if (!obj)
    return nullptr;

  const AXHypertext& ax_hypertext = obj->GetHypertext();
  if (index > static_cast<int>(ax_hypertext.hyperlinks.size()) || index < 0)
    return nullptr;

  int32_t id = ax_hypertext.hyperlinks[index];
  auto* link = AXPlatformNodeAuraLinux::GetFromUniqueId(id);
  if (!link)
    return nullptr;

  AtkHyperlink* atk_hyperlink = link->GetAtkHyperlink();
  for (const auto& key_value : ax_hypertext.hyperlink_offset_to_index) {
    if (key_value.second == index) {
      ax_platform_atk_hyperlink_set_indices(
          AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink), key_value.first,
          key_value.first + 1);
    }
  }

  return atk_hyperlink;
}

static int AXPlatformNodeAuraLinuxGetNLinks(AtkHypertext* hypertext) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));
  return obj ? obj->GetHypertext().hyperlinks.size() : 0;
}

static int AXPlatformNodeAuraLinuxGetLinkIndex(AtkHypertext* hypertext,
                                               int char_index) {
  g_return_val_if_fail(ATK_HYPERTEXT(hypertext), 0);
  AXPlatformNodeAuraLinux* obj =
      AtkObjectToAXPlatformNodeAuraLinux(ATK_OBJECT(hypertext));

  auto it = obj->GetHypertext().hyperlink_offset_to_index.find(char_index);
  if (it == obj->GetHypertext().hyperlink_offset_to_index.end())
    return -1;
  return it->second;
}

void AXHypertextInterfaceBaseInit(AtkHypertextIface* iface) {
  iface->get_link = AXPlatformNodeAuraLinuxHypertextGetLink;
  iface->get_n_links = AXPlatformNodeAuraLinuxGetNLinks;
  iface->get_link_index = AXPlatformNodeAuraLinuxGetLinkIndex;
}

static const GInterfaceInfo HypertextInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXHypertextInterfaceBaseInit), nullptr,
    nullptr};

//
// AtkText interface.
//

static gchar* AXPlatformNodeAuraLinuxGetText(AtkText* atk_text,
                                             gint start_offset,
                                             gint end_offset) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  std::string text = obj->GetTextForATK();
  if (end_offset < 0)
    end_offset = g_utf8_strlen(text.c_str(), -1);

  return g_utf8_substring(text.c_str(), start_offset, end_offset);
}

static gint AXPlatformNodeAuraLinuxGetCharacterCount(AtkText* atk_text) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  std::string text = obj->GetTextForATK();
  return g_utf8_strlen(text.c_str(), -1);
}

static AtkAttributeSet* AXPlatformNodeAuraLinuxGetRunAttributes(
    AtkText* atk_text,
    gint offset,
    gint* start_offset,
    gint* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  *start_offset = 0;
  *end_offset = AXPlatformNodeAuraLinuxGetCharacterCount(atk_text);

  return nullptr;
}

static gunichar AXPlatformNodeAuraLinuxGetCharacterAtOffset(AtkText* atk_text,
                                                            int offset) {
  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return 0;

  std::string text = obj->GetTextForATK();
  size_t limited_offset = std::max(0L, std::min(g_utf8_strlen(text.c_str(), -1),
                                                static_cast<glong>(offset)));

  // According to the C++ documentation, the pointer returned by c_str() should
  // be valid as long as any non-const operations are not performed on the
  // std::string in question.
  return g_utf8_get_char(
      g_utf8_offset_to_pointer(text.c_str(), limited_offset));
}

// This function returns a single character as a UTF-8 encoded C string because
// the character may be encoded into more than one byte.
static char* AXPlatformNodeAuraLinuxGetCharacter(AtkText* atk_text,
                                                 int offset,
                                                 int* start_offset,
                                                 int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  AtkObject* atk_object = ATK_OBJECT(atk_text);
  AXPlatformNodeAuraLinux* obj = AtkObjectToAXPlatformNodeAuraLinux(atk_object);
  if (!obj)
    return nullptr;

  std::string text = obj->GetTextForATK();
  int text_length = static_cast<int>(g_utf8_strlen(text.c_str(), -1));
  *start_offset = std::max(0, std::min(text_length, offset));
  *end_offset = std::max(0, std::min(text_length, *start_offset + 1));

  return g_utf8_substring(text.c_str(), *start_offset, *end_offset);
}

static char* AXPlatformNodeAuraLinuxGetTextAtOffset(
    AtkText* atk_text,
    int offset,
    AtkTextBoundary boundary_type,
    int* start_offset,
    int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  if (boundary_type != ATK_TEXT_BOUNDARY_CHAR) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  return AXPlatformNodeAuraLinuxGetCharacter(atk_text, offset, start_offset,
                                             end_offset);
}

static char* AXPlatformNodeAuraLinuxGetTextAfterOffset(
    AtkText* atk_text,
    int offset,
    AtkTextBoundary boundary_type,
    int* start_offset,
    int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  if (boundary_type != ATK_TEXT_BOUNDARY_CHAR) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  return AXPlatformNodeAuraLinuxGetCharacter(atk_text, offset + 1, start_offset,
                                             end_offset);
}

static char* AXPlatformNodeAuraLinuxGetTextBeforeOffset(
    AtkText* atk_text,
    int offset,
    AtkTextBoundary boundary_type,
    int* start_offset,
    int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  if (boundary_type != ATK_TEXT_BOUNDARY_CHAR) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  return AXPlatformNodeAuraLinuxGetCharacter(atk_text, offset - 1, start_offset,
                                             end_offset);
}

#if ATK_CHECK_VERSION(2, 10, 0)
static char* AXPlatformNodeAuraLinuxGetStringAtOffset(
    AtkText* atk_text,
    int offset,
    AtkTextGranularity granularity,
    int* start_offset,
    int* end_offset) {
  *start_offset = -1;
  *end_offset = -1;

  if (granularity != ATK_TEXT_GRANULARITY_CHAR) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  return AXPlatformNodeAuraLinuxGetCharacter(atk_text, offset, start_offset,
                                             end_offset);
}
#endif

static void AXTextInterfaceBaseInit(AtkTextIface* iface) {
  iface->get_text = AXPlatformNodeAuraLinuxGetText;
  iface->get_run_attributes = AXPlatformNodeAuraLinuxGetRunAttributes;
  iface->get_character_count = AXPlatformNodeAuraLinuxGetCharacterCount;
  iface->get_character_at_offset = AXPlatformNodeAuraLinuxGetCharacterAtOffset;
  iface->get_text_after_offset = AXPlatformNodeAuraLinuxGetTextAfterOffset;
  iface->get_text_before_offset = AXPlatformNodeAuraLinuxGetTextBeforeOffset;
  iface->get_text_at_offset = AXPlatformNodeAuraLinuxGetTextAtOffset;

#if ATK_CHECK_VERSION(2, 10, 0)
  iface->get_string_at_offset = AXPlatformNodeAuraLinuxGetStringAtOffset;
#endif
}

static const GInterfaceInfo TextInfo = {
    reinterpret_cast<GInterfaceInitFunc>(AXTextInterfaceBaseInit), nullptr,
    nullptr};

//
// The rest of the AXPlatformNodeAtk code, not specific to one
// of the Atk* interfaces.
//

static void AXPlatformNodeAuraLinuxInit(AtkObject* atk_object, gpointer data) {
  if (ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->initialize) {
    ATK_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)
        ->initialize(atk_object, data);
  }

  AX_PLATFORM_NODE_AURALINUX(atk_object)->m_object =
      reinterpret_cast<AXPlatformNodeAuraLinux*>(data);
}

static void AXPlatformNodeAuraLinuxFinalize(GObject* atk_object) {
  G_OBJECT_CLASS(kAXPlatformNodeAuraLinuxParentClass)->finalize(atk_object);
}

static void AXPlatformNodeAuraLinuxClassInit(AtkObjectClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  kAXPlatformNodeAuraLinuxParentClass = g_type_class_peek_parent(klass);

  gobject_class->finalize = AXPlatformNodeAuraLinuxFinalize;
  klass->initialize = AXPlatformNodeAuraLinuxInit;
  klass->get_name = AXPlatformNodeAuraLinuxGetName;
  klass->get_description = AXPlatformNodeAuraLinuxGetDescription;
  klass->get_parent = AXPlatformNodeAuraLinuxGetParent;
  klass->get_n_children = AXPlatformNodeAuraLinuxGetNChildren;
  klass->ref_child = AXPlatformNodeAuraLinuxRefChild;
  klass->get_role = AXPlatformNodeAuraLinuxGetRole;
  klass->ref_state_set = AXPlatformNodeAuraLinuxRefStateSet;
  klass->get_index_in_parent = AXPlatformNodeAuraLinuxGetIndexInParent;
  klass->ref_relation_set = AXPlatformNodeAuraLinuxRefRelationSet;
  klass->get_attributes = AXPlatformNodeAuraLinuxGetAttributes;
}

GType ax_platform_node_auralinux_get_type() {
  AXPlatformNodeAuraLinux::EnsureGTypeInit();

  static volatile gsize type_volatile = 0;
  if (g_once_init_enter(&type_volatile)) {
    static const GTypeInfo tinfo = {
        sizeof(AXPlatformNodeAuraLinuxClass),
        (GBaseInitFunc) nullptr,
        (GBaseFinalizeFunc) nullptr,
        (GClassInitFunc)AXPlatformNodeAuraLinuxClassInit,
        (GClassFinalizeFunc) nullptr,
        nullptr,                               /* class data */
        sizeof(AXPlatformNodeAuraLinuxObject), /* instance size */
        0,                                     /* nb preallocs */
        (GInstanceInitFunc) nullptr,
        nullptr /* value table */
    };

    GType type = g_type_register_static(
        ATK_TYPE_OBJECT, "AXPlatformNodeAuraLinux", &tinfo, GTypeFlags(0));
    g_once_init_leave(&type_volatile, type);
  }

  return type_volatile;
}

void AXPlatformNodeAuraLinuxDetach(AXPlatformNodeAuraLinuxObject* atk_object) {
  if (atk_object->m_object) {
    atk_object_notify_state_change(ATK_OBJECT(atk_object), ATK_STATE_DEFUNCT,
                                   TRUE);
  }
  atk_object->m_object = nullptr;
}

G_END_DECLS

namespace {

// The root-level Application object that's the parent of all top-level windows.
AXPlatformNode* g_root_application = nullptr;

// The last AtkObject with keyboard focus. Tracking this is required to emit the
// ATK_STATE_FOCUSED change to false.
AtkObject* g_current_focused = nullptr;

// The last object which was selected. Tracking this is required because
// widgets in the browser UI only emit notifications upon becoming selected,
// but clients also expect notifications when items become unselected.
AXPlatformNodeAuraLinux* g_current_selected = nullptr;

const char* GetUniqueAccessibilityGTypeName(int interface_mask) {
  // 37 characters is enough for "AXPlatformNodeAuraLinux%x" with any integer
  // value.
  static char name[37];
  snprintf(name, sizeof(name), "AXPlatformNodeAuraLinux%x", interface_mask);
  return name;
}

bool IsRoleWithValueInterface(AtkRole role) {
  return role == ATK_ROLE_SCROLL_BAR || role == ATK_ROLE_SLIDER ||
         role == ATK_ROLE_PROGRESS_BAR || role == ATK_ROLE_SEPARATOR ||
         role == ATK_ROLE_SPIN_BUTTON;
}

}  // namespace

void AXPlatformNodeAuraLinux::EnsureGTypeInit() {
#if !GLIB_CHECK_VERSION(2, 36, 0)
  static bool first_time = true;
  if (UNLIKELY(first_time)) {
    g_type_init();
    first_time = false;
  }
#endif
}

int AXPlatformNodeAuraLinux::GetGTypeInterfaceMask() {
  int interface_mask = 0;

  // Component interface is always supported.
  interface_mask |= 1 << ATK_COMPONENT_INTERFACE;

  // Action interface is basic one. It just relays on executing default action
  // for each object.
  interface_mask |= 1 << ATK_ACTION_INTERFACE;

  // TODO(accessibility): We should only expose this for some elements, but
  // it might be better to do this after exposing the hypertext interface
  // as well.
  interface_mask |= 1 << ATK_TEXT_INTERFACE;

  if (!IsPlainTextField() && !IsChildOfLeaf())
    interface_mask |= 1 << ATK_HYPERTEXT_INTERFACE;

  // Value Interface
  AtkRole role = GetAtkRole();
  if (IsRoleWithValueInterface(role)) {
    interface_mask |= 1 << ATK_VALUE_INTERFACE;
  }

  // Document Interface
  if (role == ATK_ROLE_DOCUMENT_WEB)
    interface_mask |= 1 << ATK_DOCUMENT_INTERFACE;

  // Image Interface
  if (role == ATK_ROLE_IMAGE || role == ATK_ROLE_IMAGE_MAP)
    interface_mask |= 1 << ATK_IMAGE_INTERFACE;

  // HyperlinkImpl interface
  if (role == ATK_ROLE_LINK)
    interface_mask |= 1 << ATK_HYPERLINK_INTERFACE;

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
  if (interface_mask_ & (1 << ATK_COMPONENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_COMPONENT, &ComponentInfo);
  if (interface_mask_ & (1 << ATK_ACTION_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_ACTION, &ActionInfo);
  if (interface_mask_ & (1 << ATK_DOCUMENT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_DOCUMENT, &DocumentInfo);
  if (interface_mask_ & (1 << ATK_IMAGE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_IMAGE, &ImageInfo);
  if (interface_mask_ & (1 << ATK_VALUE_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_VALUE, &ValueInfo);
  if (interface_mask_ & (1 << ATK_HYPERLINK_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_HYPERLINK_IMPL,
                                &HyperlinkImplInfo);
  if (interface_mask_ & (1 << ATK_HYPERTEXT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_HYPERTEXT, &HypertextInfo);
  if (interface_mask_ & (1 << ATK_TEXT_INTERFACE))
    g_type_add_interface_static(type, ATK_TYPE_TEXT, &TextInfo);

  return type;
}

AtkObject* AXPlatformNodeAuraLinux::CreateAtkObject() {
  EnsureGTypeInit();
  interface_mask_ = GetGTypeInterfaceMask();
  GType type = GetAccessibilityGType();
  AtkObject* atk_object = static_cast<AtkObject*>(g_object_new(type, nullptr));

  atk_object_initialize(atk_object, this);

  return ATK_OBJECT(atk_object);
}

void AXPlatformNodeAuraLinux::DestroyAtkObjects() {
  if (atk_hyperlink_) {
    ax_platform_atk_hyperlink_set_object(
        AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_), nullptr);
    g_object_unref(atk_hyperlink_);
    atk_hyperlink_ = nullptr;
  }
  if (atk_object_) {
    if (atk_object_ == g_current_focused)
      g_current_focused = nullptr;
    AXPlatformNodeAuraLinuxDetach(AX_PLATFORM_NODE_AURALINUX(atk_object_));
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
  return AtkObjectToAXPlatformNodeAuraLinux(accessible);
}

using UniqueIdMap = base::hash_map<int32_t, AXPlatformNodeAuraLinux*>;
// Map from each AXPlatformNode's unique id to its instance.
base::LazyInstance<UniqueIdMap>::Leaky g_unique_id_map =
    LAZY_INSTANCE_INITIALIZER;

// static
AXPlatformNodeAuraLinux* AXPlatformNodeAuraLinux::GetFromUniqueId(
    int32_t unique_id) {
  UniqueIdMap* unique_ids = g_unique_id_map.Pointer();
  auto iter = unique_ids->find(unique_id);
  if (iter != unique_ids->end())
    return iter->second;

  return nullptr;
}

//
// AXPlatformNodeAuraLinux implementation.
//

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

AtkRole AXPlatformNodeAuraLinux::GetAtkRole() {
  switch (GetData().role) {
    case ax::mojom::Role::kAlert:
      return ATK_ROLE_ALERT;
    case ax::mojom::Role::kAlertDialog:
      return ATK_ROLE_ALERT;
    case ax::mojom::Role::kAnchor:
      return ATK_ROLE_LINK;
    case ax::mojom::Role::kAnnotation:
      // TODO(accessibility) Panels are generally for containers of widgets.
      // This should probably be a section (if a container) or static if text.
      return ATK_ROLE_PANEL;
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
    case ax::mojom::Role::kComplementary:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
      // TODO(accessibility) https://github.com/w3c/html-aam/issues/141
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kContentInfo:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kDate:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDateTime:
      return ATK_ROLE_DATE_EDITOR;
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDescriptionListDetail:
      return ATK_ROLE_DESCRIPTION_VALUE;
    case ax::mojom::Role::kDescriptionList:
      return ATK_ROLE_DESCRIPTION_LIST;
    case ax::mojom::Role::kDescriptionListTerm:
      return ATK_ROLE_DESCRIPTION_TERM;
    case ax::mojom::Role::kDetails:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kDialog:
      return ATK_ROLE_DIALOG;
    case ax::mojom::Role::kDirectory:
      return ATK_ROLE_LIST;
    case ax::mojom::Role::kDisclosureTriangle:
      return ATK_ROLE_TOGGLE_BUTTON;
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
      return ATK_ROLE_DOCUMENT_WEB;
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
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kGraphicsDocument:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kGraphicsObject:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kGraphicsSymbol:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kGrid:
      return ATK_ROLE_TABLE;
    case ax::mojom::Role::kGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kHeading:
      return ATK_ROLE_HEADING;
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kIframePresentational:
      return ATK_ROLE_INTERNAL_FRAME;
    case ax::mojom::Role::kIgnored:
      return ATK_ROLE_REDUNDANT_OBJECT;
    case ax::mojom::Role::kImage:
      return ATK_ROLE_IMAGE;
    case ax::mojom::Role::kImageMap:
      return ATK_ROLE_IMAGE_MAP;
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
    case ax::mojom::Role::kLayoutTableColumn:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLayoutTableRow:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kLineBreak:
      // TODO(Accessibility) Having a separate accessible object for line breaks
      // is inconsistent with other implementations. http://crbug.com/873144#c1.
      return ATK_ROLE_TEXT;
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
    case ax::mojom::Role::kListMarker:
      // TODO(Accessibility) Having a separate accessible object for the marker
      // is inconsistent with other implementations. http://crbug.com/873144.
      return kStaticRole;
    case ax::mojom::Role::kListItem:
      return ATK_ROLE_LIST_ITEM;
    case ax::mojom::Role::kLog:
      return ATK_ROLE_LOG;
    case ax::mojom::Role::kMain:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kMark:
      return kStaticRole;
    case ax::mojom::Role::kMath:
      return ATK_ROLE_MATH;
    case ax::mojom::Role::kMarquee:
      return ATK_ROLE_MARQUEE;
    case ax::mojom::Role::kMenu:
      return ATK_ROLE_MENU;
    case ax::mojom::Role::kMenuButton:
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
    case ax::mojom::Role::kPopUpButton: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "select")
        return ATK_ROLE_COMBO_BOX;
      return ATK_ROLE_PUSH_BUTTON;
    }
    case ax::mojom::Role::kPre:
      return ATK_ROLE_SECTION;
    case ax::mojom::Role::kProgressIndicator:
      return ATK_ROLE_PROGRESS_BAR;
    case ax::mojom::Role::kRadioButton:
      return ATK_ROLE_RADIO_BUTTON;
    case ax::mojom::Role::kRadioGroup:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kRegion: {
      std::string html_tag =
          GetData().GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
      if (html_tag == "section" &&
          GetData()
              .GetString16Attribute(ax::mojom::StringAttribute::kName)
              .empty()) {
        // Do not use ARIA mapping for nameless <section>.
        return ATK_ROLE_SECTION;
      } else {
        // Use ARIA mapping.
        return ATK_ROLE_LANDMARK;
      }
    }
    case ax::mojom::Role::kRootWebArea:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kRow:
      return ATK_ROLE_TABLE_ROW;
    case ax::mojom::Role::kRowHeader:
      return ATK_ROLE_ROW_HEADER;
    case ax::mojom::Role::kRuby:
      return kStaticRole;
    case ax::mojom::Role::kScrollBar:
      return ATK_ROLE_SCROLL_BAR;
    case ax::mojom::Role::kSearch:
      return ATK_ROLE_LANDMARK;
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSliderThumb:
      return ATK_ROLE_SLIDER;
    case ax::mojom::Role::kSpinButton:
      return ATK_ROLE_SPIN_BUTTON;
    case ax::mojom::Role::kSplitter:
      return ATK_ROLE_SEPARATOR;
    case ax::mojom::Role::kStaticText: {
      switch (static_cast<ax::mojom::TextPosition>(
          GetIntAttribute(ax::mojom::IntAttribute::kTextPosition))) {
        case ax::mojom::TextPosition::kSubscript:
          return kSubscriptRole;
        case ax::mojom::TextPosition::kSuperscript:
          return kSuperscriptRole;
        default:
          break;
      }
      return ATK_ROLE_TEXT;
    }
    case ax::mojom::Role::kStatus:
      return ATK_ROLE_STATUSBAR;
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
      // TODO(accessibility) This mapping should also be applied to the dfn
      // element. http://crbug.com/874411
      return ATK_ROLE_DESCRIPTION_TERM;
    case ax::mojom::Role::kTitleBar:
      return ATK_ROLE_TITLE_BAR;
    case ax::mojom::Role::kInlineTextBox:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kSearchBox:
      if (GetData().HasState(ax::mojom::State::kProtected))
        return ATK_ROLE_PASSWORD_TEXT;
      // TODO(crbug.com/865101) Use
      // GetData().HasState(ax::mojom::State::kAutofillAvailable) instead of
      // IsFocusedInputWithSuggestions()
      if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete)
               .empty() ||
          IsFocusedInputWithSuggestions()) {
        return ATK_ROLE_AUTOCOMPLETE;
      }
      return ATK_ROLE_ENTRY;
    case ax::mojom::Role::kTextFieldWithComboBox:
      return ATK_ROLE_COMBO_BOX;
    case ax::mojom::Role::kAbbr:
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
    case ax::mojom::Role::kWebArea:
    case ax::mojom::Role::kWebView:
      return ATK_ROLE_DOCUMENT_WEB;
    case ax::mojom::Role::kWindow:
      // In ATK elements with ATK_ROLE_FRAME are windows with titles and
      // buttons, while those with ATK_ROLE_WINDOW are windows without those
      // elements.
      return ATK_ROLE_FRAME;
    case ax::mojom::Role::kClient:
    case ax::mojom::Role::kDesktop:
      return ATK_ROLE_PANEL;
    case ax::mojom::Role::kFigcaption:
      return ATK_ROLE_CAPTION;
    case ax::mojom::Role::kFooter:
      return ATK_ROLE_FOOTER;
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kPresentational:
    case ax::mojom::Role::kUnknown:
      return ATK_ROLE_REDUNDANT_OBJECT;
  }
}

void AXPlatformNodeAuraLinux::GetAtkState(AtkStateSet* atk_state_set) {
  AXNodeData data = GetData();
  if (data.HasState(ax::mojom::State::kCollapsed))
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
  if (data.HasState(ax::mojom::State::kDefault))
    atk_state_set_add_state(atk_state_set, ATK_STATE_DEFAULT);
  if (data.HasState(ax::mojom::State::kEditable) &&
      data.GetRestriction() != ax::mojom::Restriction::kReadOnly) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EDITABLE);
  }
  if (data.HasState(ax::mojom::State::kExpanded)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDABLE);
    atk_state_set_add_state(atk_state_set, ATK_STATE_EXPANDED);
  }
  if (data.HasState(ax::mojom::State::kFocusable) ||
      SelectionAndFocusAreTheSame())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSABLE);
  if (data.HasState(ax::mojom::State::kHorizontal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HORIZONTAL);
  if (!data.HasState(ax::mojom::State::kInvisible)) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISIBLE);
    if (!delegate_->IsOffscreen())
      atk_state_set_add_state(atk_state_set, ATK_STATE_SHOWING);
  }
  if (data.HasState(ax::mojom::State::kMultiselectable))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MULTISELECTABLE);
  if (data.HasState(ax::mojom::State::kRequired))
    atk_state_set_add_state(atk_state_set, ATK_STATE_REQUIRED);
  if (data.HasState(ax::mojom::State::kVertical))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VERTICAL);
  if (data.HasState(ax::mojom::State::kVisited))
    atk_state_set_add_state(atk_state_set, ATK_STATE_VISITED);
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      data.GetIntAttribute(ax::mojom::IntAttribute::kInvalidState) !=
          static_cast<int32_t>(ax::mojom::InvalidState::kFalse))
    atk_state_set_add_state(atk_state_set, ATK_STATE_INVALID_ENTRY);
#if defined(ATK_216)
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState) &&
      data.role != ax::mojom::Role::kToggleButton) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_CHECKABLE);
  }
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kHasPopup))
    atk_state_set_add_state(atk_state_set, ATK_STATE_HAS_POPUP);
#endif
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    atk_state_set_add_state(atk_state_set, ATK_STATE_BUSY);
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kModal))
    atk_state_set_add_state(atk_state_set, ATK_STATE_MODAL);
  if (data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE);
  if (data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTED);

  if (IsPlainTextField() || IsRichTextField()) {
    atk_state_set_add_state(atk_state_set, ATK_STATE_SELECTABLE_TEXT);
    if (data.HasState(ax::mojom::State::kMultiline))
      atk_state_set_add_state(atk_state_set, ATK_STATE_MULTI_LINE);
    else
      atk_state_set_add_state(atk_state_set, ATK_STATE_SINGLE_LINE);
  }

  // TODO(crbug.com/865101) Use
  // GetData().HasState(ax::mojom::State::kAutofillAvailable) instead of
  // IsFocusedInputWithSuggestions()
  if (!GetStringAttribute(ax::mojom::StringAttribute::kAutoComplete).empty() ||
      IsFocusedInputWithSuggestions())
    atk_state_set_add_state(atk_state_set, ATK_STATE_SUPPORTS_AUTOCOMPLETION);

  // Checked state
  const auto checked_state = GetData().GetCheckedState();
  if (checked_state == ax::mojom::CheckedState::kTrue ||
      checked_state == ax::mojom::CheckedState::kMixed) {
    atk_state_set_add_state(atk_state_set, GetAtkStateTypeForCheckableNode());
  }

  switch (GetData().GetRestriction()) {
    case ax::mojom::Restriction::kNone:
      atk_state_set_add_state(atk_state_set, ATK_STATE_ENABLED);
      atk_state_set_add_state(atk_state_set, ATK_STATE_SENSITIVE);
      break;
    case ax::mojom::Restriction::kReadOnly:
#if defined(ATK_216)
      atk_state_set_add_state(atk_state_set, ATK_STATE_READ_ONLY);
#endif
      break;
    default:
      break;
  }

  if (delegate_->GetFocus() == GetNativeViewAccessible())
    atk_state_set_add_state(atk_state_set, ATK_STATE_FOCUSED);
}

void AXPlatformNodeAuraLinux::GetAtkRelations(
    AtkRelationSet* atk_relation_set) {
}

AXPlatformNodeAuraLinux::AXPlatformNodeAuraLinux() = default;

AXPlatformNodeAuraLinux::~AXPlatformNodeAuraLinux() {
  if (g_current_selected == this)
    g_current_selected = nullptr;

  DestroyAtkObjects();
}

void AXPlatformNodeAuraLinux::Destroy() {
  g_unique_id_map.Get().erase(GetUniqueId());

  DestroyAtkObjects();
  AXPlatformNodeBase::Destroy();
}

void AXPlatformNodeAuraLinux::Init(AXPlatformNodeDelegate* delegate) {
  // Initialize ATK.
  AXPlatformNodeBase::Init(delegate);
  g_unique_id_map.Get()[GetUniqueId()] = this;
  DataChanged();
}

void AXPlatformNodeAuraLinux::DataChanged() {
  if (atk_object_) {
    // If the object's role changes and that causes its
    // interface mask to change, we need to create a new
    // AtkObject for it.
    int interface_mask = GetGTypeInterfaceMask();
    if (interface_mask != interface_mask_)
      DestroyAtkObjects();
  }

  if (!atk_object_) {
    atk_object_ = CreateAtkObject();
  }
}

void AXPlatformNodeAuraLinux::AddAccessibilityTreeProperties(
    base::DictionaryValue* dict) {
  AtkRole role = GetAtkRole();
  if (role != ATK_ROLE_UNKNOWN) {
    int role_index = static_cast<int>(role);
    dict->SetString("role", kRoleNames[role_index]);
  }
  const gchar* name = atk_object_get_name(atk_object_);
  if (name)
    dict->SetString("name", std::string(name));
  const gchar* description = atk_object_get_description(atk_object_);
  if (description)
    dict->SetString("description", std::string(description));

  AtkStateSet* state_set = atk_object_ref_state_set(atk_object_);
  auto states = std::make_unique<base::ListValue>();
  for (int i = ATK_STATE_INVALID; i < ATK_STATE_LAST_DEFINED; i++) {
    AtkStateType state_type = static_cast<AtkStateType>(i);
    if (atk_state_set_contains_state(state_set, state_type))
      states->AppendString(atk_state_type_get_name(state_type));
  }
  dict->Set("states", std::move(states));

  AtkAttributeSet* attributes = atk_object_get_attributes(atk_object_);
  for (AtkAttributeSet* attr = attributes; attr; attr = attr->next) {
    AtkAttribute* attribute = static_cast<AtkAttribute*>(attr->data);
    dict->SetString(attribute->name, attribute->value);
  }
  atk_attribute_set_free(attributes);
}

gfx::NativeViewAccessible AXPlatformNodeAuraLinux::GetNativeViewAccessible() {
  return atk_object_;
}

void AXPlatformNodeAuraLinux::OnCheckedStateChanged() {
  DCHECK(atk_object_);

  atk_object_notify_state_change(
      ATK_OBJECT(atk_object_), GetAtkStateTypeForCheckableNode(),
      GetData().GetCheckedState() != ax::mojom::CheckedState::kFalse);
}

void AXPlatformNodeAuraLinux::OnExpandedStateChanged(bool is_expanded) {
  DCHECK(atk_object_);

  atk_object_notify_state_change(ATK_OBJECT(atk_object_), ATK_STATE_EXPANDED,
                                 is_expanded);
}

void AXPlatformNodeAuraLinux::OnFocused() {
  DCHECK(atk_object_);

  if (atk_object_ == g_current_focused)
    return;

  if (g_current_focused) {
    g_signal_emit_by_name(g_current_focused, "focus-event", false);
    atk_object_notify_state_change(ATK_OBJECT(g_current_focused),
                                   ATK_STATE_FOCUSED, false);
  }

  g_current_focused = atk_object_;
  g_signal_emit_by_name(atk_object_, "focus-event", true);
  atk_object_notify_state_change(ATK_OBJECT(atk_object_), ATK_STATE_FOCUSED,
                                 true);
}

void AXPlatformNodeAuraLinux::OnSelected() {
  if (g_current_selected && !g_current_selected->GetData().GetBoolAttribute(
                                ax::mojom::BoolAttribute::kSelected)) {
    atk_object_notify_state_change(ATK_OBJECT(g_current_selected->atk_object_),
                                   ATK_STATE_SELECTED, false);
  }

  g_current_selected = this;
  if (ATK_IS_OBJECT(atk_object_)) {
    atk_object_notify_state_change(ATK_OBJECT(atk_object_), ATK_STATE_SELECTED,
                                   true);
  }

  if (SelectionAndFocusAreTheSame())
    OnFocused();
}

bool AXPlatformNodeAuraLinux::SelectionAndFocusAreTheSame() {
  if (AXPlatformNodeBase* container = GetSelectionContainer()) {
    ax::mojom::Role role = container->GetData().role;
    if (role == ax::mojom::Role::kMenuBar || role == ax::mojom::Role::kMenu)
      return true;
    if (role == ax::mojom::Role::kListBox &&
        !container->GetData().HasState(ax::mojom::State::kMultiselectable)) {
      return container->GetDelegate()->GetFocus() ==
             container->GetNativeViewAccessible();
    }
  }

  // TODO(accessibility): GetSelectionContainer returns nullptr when the current
  // object is a descendant of a select element with a size of 1. Intentional?
  // For now, handle that scenario here.
  //
  // If the selection is changing on a collapsed select element, focus remains
  // on the select element and not the newly-selected descendant.
  if (AXPlatformNodeBase* parent = FromNativeViewAccessible(GetParent())) {
    if (parent->GetData().role == ax::mojom::Role::kMenuListPopup)
      return !parent->GetData().HasState(ax::mojom::State::kInvisible);
  }

  return false;
}

void AXPlatformNodeAuraLinux::OnValueChanged() {
  DCHECK(atk_object_);

  if (!IsRoleWithValueInterface(GetAtkRole()))
    return;

  float float_val;
  if (!GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, &float_val))
    return;

  AtkPropertyValues property_values;
  property_values.property_name = "accessible-value";

  property_values.new_value = G_VALUE_INIT;
  g_value_init(&property_values.new_value, G_TYPE_DOUBLE);
  g_value_set_double(&property_values.new_value,
                     static_cast<double>(float_val));
  g_signal_emit_by_name(G_OBJECT(atk_object_),
                        "property-change::accessible-value", &property_values,
                        nullptr);
}

void AXPlatformNodeAuraLinux::NotifyAccessibilityEvent(
    ax::mojom::Event event_type) {
  switch (event_type) {
    case ax::mojom::Event::kCheckedStateChanged:
      OnCheckedStateChanged();
      break;
    case ax::mojom::Event::kExpandedChanged:
      OnExpandedStateChanged(GetData().HasState(ax::mojom::State::kExpanded));
      break;
    case ax::mojom::Event::kFocus:
    case ax::mojom::Event::kFocusContext:
      OnFocused();
      break;
    case ax::mojom::Event::kSelection:
      OnSelected();
      break;
    case ax::mojom::Event::kValueChanged:
      OnValueChanged();
      break;
    default:
      break;
  }
}

void AXPlatformNodeAuraLinux::UpdateHypertext() {
  hypertext_ = ComputeHypertext();
}

const AXHypertext& AXPlatformNodeAuraLinux::GetHypertext() {
  return hypertext_;
}

int AXPlatformNodeAuraLinux::GetIndexInParent() {
  if (!GetParent())
    return -1;

  return delegate_->GetIndexInParent();
}

void AXPlatformNodeAuraLinux::SetExtentsRelativeToAtkCoordinateType(
    gint* x, gint* y, gint* width, gint* height, AtkCoordType coord_type) {
  gfx::Rect extents = delegate_->GetUnclippedScreenBoundsRect();

  if (x)
    *x = extents.x();
  if (y)
    *y = extents.y();
  if (width)
    *width = extents.width();
  if (height)
    *height = extents.height();

  if (coord_type == ATK_XY_WINDOW) {
    if (AtkObject* atk_object = GetParent()) {
      gfx::Point window_coords = FindAtkObjectParentCoords(atk_object);
      if (x)
        *x -= window_coords.x();
      if (y)
        *y -= window_coords.y();
    }
  }
}

void AXPlatformNodeAuraLinux::GetExtents(gint* x, gint* y,
                                         gint* width, gint* height,
                                         AtkCoordType coord_type) {
  SetExtentsRelativeToAtkCoordinateType(x, y,
                                        width, height,
                                        coord_type);
}

void AXPlatformNodeAuraLinux::GetPosition(gint* x, gint* y,
                                          AtkCoordType coord_type) {
  SetExtentsRelativeToAtkCoordinateType(x, y,
                                        nullptr, nullptr,
                                        coord_type);
}

void AXPlatformNodeAuraLinux::GetSize(gint* width, gint* height) {
  gfx::Rect rect_size = gfx::ToEnclosingRect(GetData().location);
  if (width)
    *width = rect_size.width();
  if (height)
    *height = rect_size.height();
}

gfx::NativeViewAccessible
AXPlatformNodeAuraLinux::HitTestSync(gint x, gint y, AtkCoordType coord_type) {
  if (coord_type == ATK_XY_WINDOW) {
    if (AtkObject* atk_object = GetParent()) {
      gfx::Point window_coords = FindAtkObjectParentCoords(atk_object);
      x += window_coords.x();
      y += window_coords.y();
    }
  }

  return delegate_->HitTestSync(x, y);
}

bool AXPlatformNodeAuraLinux::GrabFocus() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  return delegate_->AccessibilityPerformAction(action_data);
}

bool AXPlatformNodeAuraLinux::DoDefaultAction() {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  return delegate_->AccessibilityPerformAction(action_data);
}

const gchar* AXPlatformNodeAuraLinux::GetDefaultActionName() {
  int action;
  if (!GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb, &action))
    return nullptr;

  base::string16 action_verb = ActionVerbToUnlocalizedString(
      static_cast<ax::mojom::DefaultActionVerb>(action));

  ATK_AURALINUX_RETURN_STRING(base::UTF16ToUTF8(action_verb));
}

AtkAttributeSet* AXPlatformNodeAuraLinux::GetAtkAttributes() {
  AtkAttributeSet* attribute_list = nullptr;
  ComputeAttributes(&attribute_list);
  return attribute_list;
}

AtkStateType AXPlatformNodeAuraLinux::GetAtkStateTypeForCheckableNode() {
  if (GetData().GetCheckedState() == ax::mojom::CheckedState::kMixed)
    return ATK_STATE_INDETERMINATE;
  if (GetData().role == ax::mojom::Role::kToggleButton)
    return ATK_STATE_PRESSED;
  return ATK_STATE_CHECKED;
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

static AtkAttributeSet* PrependAtkAttributeToAtkAttributeSet(
    const char* name,
    const char* value,
    AtkAttributeSet* attribute_set) {
  AtkAttribute* attribute =
      static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
  attribute->name = g_strdup(name);
  attribute->value = g_strdup(value);
  return g_slist_prepend(attribute_set, attribute);
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
  if (!atk_hyperlink_) {
    atk_hyperlink_ =
        ATK_HYPERLINK(g_object_new(AX_PLATFORM_ATK_HYPERLINK_TYPE, 0));
    ax_platform_atk_hyperlink_set_object(
        AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink_), this);
  }

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

std::string AXPlatformNodeAuraLinux::GetTextForATK() {
  // Special case allows us to get text even in non-HTML case, e.g. browser UI.
  if (IsPlainTextField())
    return GetStringAttribute(ax::mojom::StringAttribute::kValue);

  if (IsChildOfLeaf())
    return AXPlatformNodeBase::GetText();

  return base::UTF16ToUTF8(hypertext_.hypertext);
}

}  // namespace ui
