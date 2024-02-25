// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_atk_hyperlink.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_enum_localization_util.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

struct _AXPlatformAtkHyperlinkPrivate {
  raw_ptr<AXPlatformNodeAuraLinux> platform_node = nullptr;
};

static gpointer kAXPlatformAtkHyperlinkParentClass = nullptr;

static AXPlatformNodeAuraLinux* ToAXPlatformNodeAuraLinux(
    AXPlatformAtkHyperlink* atk_hyperlink) {
  if (!atk_hyperlink)
    return nullptr;
  return atk_hyperlink->priv->platform_node;
}

static void AXPlatformAtkHyperlinkFinalize(GObject* self) {
  AX_PLATFORM_ATK_HYPERLINK(self)->priv->~AXPlatformAtkHyperlinkPrivate();
  G_OBJECT_CLASS(kAXPlatformAtkHyperlinkParentClass)->finalize(self);
}

static gchar* AXPlatformAtkHyperlinkGetUri(AtkHyperlink* atk_hyperlink,
                                           gint index) {
  AXPlatformNodeAuraLinux* obj =
      ToAXPlatformNodeAuraLinux(AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink));
  if (!obj)
    return nullptr;

  if (index != 0)
    return nullptr;

  return g_strdup(
      obj->GetStringAttribute(ax::mojom::StringAttribute::kUrl).c_str());
}

static AtkObject* AXPlatformAtkHyperlinkGetObject(AtkHyperlink* atk_hyperlink,
                                                  gint index) {
  AXPlatformNodeAuraLinux* obj =
      ToAXPlatformNodeAuraLinux(AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink));
  if (!obj)
    return nullptr;

  if (index != 0)
    return nullptr;

  return ATK_OBJECT(obj->GetNativeViewAccessible());
}

static gint AXPlatformAtkHyperlinkGetNAnchors(AtkHyperlink* atk_hyperlink) {
  AXPlatformNodeAuraLinux* obj =
      ToAXPlatformNodeAuraLinux(AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink));

  return obj ? 1 : 0;
}

static gboolean AXPlatformAtkHyperlinkIsValid(AtkHyperlink* atk_hyperlink) {
  AXPlatformNodeAuraLinux* obj =
      ToAXPlatformNodeAuraLinux(AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink));

  return obj ? TRUE : FALSE;
}

static gboolean AXPlatformAtkHyperlinkIsSelectedLink(
    AtkHyperlink* atk_hyperlink) {
  AXPlatformNodeAuraLinux* obj =
      ToAXPlatformNodeAuraLinux(AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink));
  if (!obj)
    return false;

  return obj->GetDelegate()->GetFocus() == obj->GetNativeViewAccessible();
}

static int AXPlatformAtkHyperlinkGetStartIndex(AtkHyperlink* atk_hyperlink) {
  g_return_val_if_fail(IS_AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink), 0);
  AXPlatformAtkHyperlink* link = AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink);
  std::optional<std::pair<int, int>> indices =
      link->priv->platform_node->GetEmbeddedObjectIndices();
  return indices.has_value() ? indices->first : 0;
}

static int AXPlatformAtkHyperlinkGetEndIndex(AtkHyperlink* atk_hyperlink) {
  g_return_val_if_fail(IS_AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink), 0);
  AXPlatformAtkHyperlink* link = AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink);
  std::optional<std::pair<int, int>> indices =
      link->priv->platform_node->GetEmbeddedObjectIndices();
  return indices.has_value() ? indices->second : 0;
}

static void AXPlatformAtkHyperlinkClassInit(AtkHyperlinkClass* klass) {
  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  kAXPlatformAtkHyperlinkParentClass = g_type_class_peek_parent(klass);

  g_type_class_add_private(gobject_class,
                           sizeof(AXPlatformAtkHyperlinkPrivate));

  gobject_class->finalize = AXPlatformAtkHyperlinkFinalize;
  klass->get_uri = AXPlatformAtkHyperlinkGetUri;
  klass->get_object = AXPlatformAtkHyperlinkGetObject;
  klass->is_valid = AXPlatformAtkHyperlinkIsValid;
  klass->get_n_anchors = AXPlatformAtkHyperlinkGetNAnchors;
  klass->is_selected_link = AXPlatformAtkHyperlinkIsSelectedLink;
  klass->get_start_index = AXPlatformAtkHyperlinkGetStartIndex;
  klass->get_end_index = AXPlatformAtkHyperlinkGetEndIndex;
}

void ax_platform_atk_hyperlink_set_object(
    AXPlatformAtkHyperlink* atk_hyperlink,
    AXPlatformNodeAuraLinux* platform_node) {
  g_return_if_fail(AX_PLATFORM_ATK_HYPERLINK(atk_hyperlink));
  atk_hyperlink->priv->platform_node = platform_node;
}

static void AXPlatformAtkHyperlinkInit(AXPlatformAtkHyperlink* self, gpointer) {
  AXPlatformAtkHyperlinkPrivate* priv =
      G_TYPE_INSTANCE_GET_PRIVATE(self, ax_platform_atk_hyperlink_get_type(),
                                  AXPlatformAtkHyperlinkPrivate);
  self->priv = priv;
  new (priv) AXPlatformAtkHyperlinkPrivate();
}

GType ax_platform_atk_hyperlink_get_type() {
  static gsize type_id = 0;

  AXPlatformNodeAuraLinux::EnsureGTypeInit();

  if (g_once_init_enter(&type_id)) {
    static const GTypeInfo tinfo = {
        sizeof(AXPlatformAtkHyperlinkClass),
        (GBaseInitFunc) nullptr,
        (GBaseFinalizeFunc) nullptr,
        (GClassInitFunc)AXPlatformAtkHyperlinkClassInit,
        (GClassFinalizeFunc) nullptr,
        nullptr,                        /* class data */
        sizeof(AXPlatformAtkHyperlink), /* instance size */
        0,                              /* nb preallocs */
        (GInstanceInitFunc)AXPlatformAtkHyperlinkInit,
        nullptr /* value table */
    };

    GType type = g_type_register_static(
        ATK_TYPE_HYPERLINK, "AXPlatformAtkHyperlink", &tinfo, GTypeFlags(0));
    g_once_init_leave(&type_id, type);
  }

  return type_id;
}

}  // namespace ui
