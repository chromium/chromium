// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_

#include <atk/atk.h>

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

// Some ATK interfaces require returning a (const gchar*), use
// this macro to make it safe to return a pointer to a temporary
// string.
#define ATK_AURALINUX_RETURN_STRING(str_expr) \
  {                                           \
    static std::string result;                \
    result = (str_expr);                      \
    return result.c_str();                    \
  }

namespace ui {

// Implements accessibility on Aura Linux using ATK.
class AX_EXPORT AXPlatformNodeAuraLinux : public AXPlatformNodeBase {
 public:
  AXPlatformNodeAuraLinux();
  ~AXPlatformNodeAuraLinux() override;

  // Set or get the root-level Application object that's the parent of all
  // top-level windows.
  static void SetApplication(AXPlatformNode* application);
  static AXPlatformNode* application();

  static void EnsureGTypeInit();

  // Do asynchronous static initialization.
  static void StaticInitialize();

  void DataChanged();
  void Destroy() override;
  void AddAccessibilityTreeProperties(base::DictionaryValue* dict);

  AtkRole GetAtkRole();
  void GetAtkState(AtkStateSet* state_set);
  void GetAtkRelations(AtkRelationSet* atk_relation_set);
  void GetExtents(gint* x, gint* y, gint* width, gint* height,
                  AtkCoordType coord_type);
  void GetPosition(gint* x, gint* y, AtkCoordType coord_type);
  void GetSize(gint* width, gint* height);
  gfx::NativeViewAccessible HitTestSync(gint x,
                                        gint y,
                                        AtkCoordType coord_type);
  bool GrabFocus();
  bool DoDefaultAction();
  const gchar* GetDefaultActionName();
  AtkAttributeSet* GetAtkAttributes();

  void SetExtentsRelativeToAtkCoordinateType(
      gint* x, gint* y, gint* width, gint* height,
      AtkCoordType coord_type);

  static AXPlatformNodeAuraLinux* GetFromUniqueId(int32_t unique_id);

  // AtkDocument helpers
  const gchar* GetDocumentAttributeValue(const gchar* attribute) const;
  AtkAttributeSet* GetDocumentAttributes() const;

  // AtkHyperlink helpers
  AtkHyperlink* GetAtkHyperlink();

  // Misc helpers
  void GetFloatAttributeInGValue(ax::mojom::FloatAttribute attr, GValue* value);

  // Event helpers
  void OnCheckedStateChanged();
  void OnExpandedStateChanged(bool is_expanded);
  void OnFocused();
  void OnSelected();
  void OnValueChanged();

  bool SelectionAndFocusAreTheSame();

  // AXPlatformNode overrides.
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ax::mojom::Event event_type) override;

  // AXPlatformNodeBase overrides.
  void Init(AXPlatformNodeDelegate* delegate) override;
  int GetIndexInParent() override;

  std::string GetTextForATK();

  void UpdateHypertext();
  const AXHypertext& GetHypertext();

 protected:
  AXHypertext hypertext_;

  void AddAttributeToList(const char* name,
                          const char* value,
                          PlatformAttributeList* attributes) override;

 private:
  enum AtkInterfaces {
    ATK_ACTION_INTERFACE,
    ATK_COMPONENT_INTERFACE,
    ATK_DOCUMENT_INTERFACE,
    ATK_EDITABLE_TEXT_INTERFACE,
    ATK_HYPERLINK_INTERFACE,
    ATK_HYPERTEXT_INTERFACE,
    ATK_IMAGE_INTERFACE,
    ATK_SELECTION_INTERFACE,
    ATK_TABLE_INTERFACE,
    ATK_TEXT_INTERFACE,
    ATK_VALUE_INTERFACE,
  };

  int GetGTypeInterfaceMask();
  GType GetAccessibilityGType();
  AtkObject* CreateAtkObject();
  void DestroyAtkObjects();

  // The AtkStateType for a checkable node can vary depending on the role.
  AtkStateType GetAtkStateTypeForCheckableNode();

  // Keep information of latest AtkInterfaces mask to refresh atk object
  // interfaces accordingly if needed.
  int interface_mask_ = 0;

  // We own a reference to these ref-counted objects.
  AtkObject* atk_object_ = nullptr;
  AtkHyperlink* atk_hyperlink_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeAuraLinux);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_AURALINUX_H_
