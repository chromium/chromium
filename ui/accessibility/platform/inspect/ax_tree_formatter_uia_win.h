// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_UIA_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_UIA_WIN_H_

#include <ole2.h>

#include <stdint.h>
#include <wrl/client.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

#include <uiautomation.h>

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXTreeFormatterUia
    : public AXTreeFormatterBase {
 public:
  AXTreeFormatterUia();
  ~AXTreeFormatterUia() override;

  // AccessibilityTreeFormatterBase:
  base::DictValue BuildTree(AXPlatformNodeDelegate* start) const override;
  base::DictValue BuildTreeForSelector(
      const AXTreeSelector& selector) const override;
  base::DictValue BuildNode(AXPlatformNodeDelegate* node) const override;

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  static const long properties_[];
  static const long patterns_[];
  static const long pattern_properties_[];
  void RecursiveBuildTree(IUIAutomationElement* node,
                          int root_x,
                          int root_y,
                          base::DictValue* dict) const;
  void BuildCacheRequests();
  void BuildCustomPropertiesMap();
  void AddProperties(IUIAutomationElement* node,
                     int root_x,
                     int root_y,
                     base::DictValue* dict) const;
  void AddAnnotationProperties(IUIAutomationElement* node,
                               base::DictValue* dict) const;
  void AddExpandCollapseProperties(IUIAutomationElement* node,
                                   base::DictValue* dict) const;
  void AddGridProperties(IUIAutomationElement* node,
                         base::DictValue* dict) const;
  void AddGridItemProperties(IUIAutomationElement* node,
                             base::DictValue* dict) const;
  void AddRangeValueProperties(IUIAutomationElement* node,
                               base::DictValue* dict) const;
  void AddScrollProperties(IUIAutomationElement* node,
                           base::DictValue* dict) const;
  void AddSelectionProperties(IUIAutomationElement* node,
                              base::DictValue* dict) const;
  void AddSelectionItemProperties(IUIAutomationElement* node,
                                  base::DictValue* dict) const;
  void AddTableProperties(IUIAutomationElement* node,
                          base::DictValue* dict) const;
  void AddToggleProperties(IUIAutomationElement* node,
                           base::DictValue* dict) const;
  void AddValueProperties(IUIAutomationElement* node,
                          base::DictValue* dict) const;
  void AddWindowProperties(IUIAutomationElement* node,
                           base::DictValue* dict) const;
  void AddCustomProperties(IUIAutomationElement* node,
                           base::DictValue* dict) const;
  std::string GetPropertyName(long property_id) const;
  void WriteProperty(long propertyId,
                     const base::win::ScopedVariant& var,
                     base::DictValue* dict,
                     int root_x = 0,
                     int root_y = 0) const;
  // UIA enums have type I4, print formatted string for these when possible
  void WriteI4Property(long propertyId, long lval, base::DictValue* dict) const;
  void WriteUnknownProperty(long propertyId,
                            IUnknown* unk,
                            base::DictValue* dict) const;
  void WriteRectangleProperty(long propertyId,
                              const VARIANT& value,
                              int root_x,
                              int root_y,
                              base::DictValue* dict) const;
  void WriteElementArray(long propertyId,
                         IUIAutomationElementArray* array,
                         base::DictValue* dict) const;
  std::u16string GetNodeName(IUIAutomationElement* node) const;
  std::string ProcessTreeForOutput(const base::DictValue& node) const override;
  void ProcessPropertyForOutput(const std::string& property_name,
                                const base::DictValue& dict,
                                std::string& line) const;
  void ProcessValueForOutput(const std::string& name,
                             const base::Value& value,
                             std::string& line) const;
  std::map<long, std::string>& GetCustomPropertiesMap() const;

  Microsoft::WRL::ComPtr<IUIAutomation> uia_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> element_cache_request_;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> children_cache_request_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_UIA_WIN_H_
