// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/inspect/ax_tree_formatter_uia_win.h"

#include <math.h>
#include <oleacc.h>
#include <stddef.h>
#include <stdint.h>
#include <wrl/client.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_win.h"
#include "ui/accessibility/platform/uia_registrar_win.h"
#include "ui/gfx/win/hwnd_util.h"

#include <uiautomation.h>

namespace {

std::string UiaIdentifierToCondensedString(int32_t id) {
  std::string identifier = ui::UiaIdentifierToStringUTF8(id);
  if (id >= UIA_RuntimeIdPropertyId && id <= UIA_HeadingLevelPropertyId) {
    // remove leading 'UIA_' and trailing 'PropertyId'
    return identifier.substr(4, identifier.size() - 14);
  }
  if (id >= UIA_ButtonControlTypeId && id <= UIA_AppBarControlTypeId) {
    // remove leading 'UIA_' and trailing 'ControlTypeId'
    return identifier.substr(4, identifier.size() - 17);
  }
  return identifier;
}

// The RuntimeId returned from IUIAutomationElement is different than the one
// we hand out in IRawElementProviderFragment::GetRuntimeId, as UIA modifies it.
// This function takes an existing IUIAutomationElement and swaps in the
// Chromium specific bits of the internal runtime id so that the returned
// SAFEARRAY can be used in IUIAutomationElement-based search/conditionals.
void GetUIARuntimeId(IUIAutomationElement* first_child,
                     IRawElementProviderFragment* start_fragment,
                     SAFEARRAY** runtime_id_out) {
  DCHECK(first_child && start_fragment && runtime_id_out);
  std::array<int, 3> internal_id;
  {
    // Get the internal runtime id from the IRawElementProviderFragment based
    // GetRuntimeId.
    base::win::ScopedSafearray start_fragment_runtime_id;
    start_fragment->GetRuntimeId(start_fragment_runtime_id.Receive());
    LONG lower_bound = 0;
    HRESULT hr =
        ::SafeArrayGetLBound(start_fragment_runtime_id.Get(), 1, &lower_bound);
    CHECK(SUCCEEDED(hr));
    LONG upper_bound = 0;
    hr = ::SafeArrayGetUBound(start_fragment_runtime_id.Get(), 1, &upper_bound);
    CHECK(SUCCEEDED(hr));
    CHECK(lower_bound >= 0);
    LONG fragment_id_length = (upper_bound - lower_bound) + 1;
    CHECK(fragment_id_length == 4);

    int32_t* fragment_id_array = nullptr;
    ::SafeArrayAccessData(start_fragment_runtime_id.Get(),
                          reinterpret_cast<void**>(&fragment_id_array));
    CHECK(fragment_id_array);
    // Grab out the last three ints from the internal runtime id. This should
    // correspond with the frame tree id and DOM id.
    internal_id = {fragment_id_array[1], fragment_id_array[2],
                   fragment_id_array[3]};

    ::SafeArrayUnaccessData(start_fragment_runtime_id.Get());
  }

  base::win::ScopedSafearray runtime_id;
  first_child->GetRuntimeId(runtime_id.Receive());
  CHECK(runtime_id.Get());
  LONG lower_bound = 0;
  HRESULT hr = ::SafeArrayGetLBound(runtime_id.Get(), 1, &lower_bound);
  CHECK(SUCCEEDED(hr));
  LONG upper_bound = 0;
  hr = ::SafeArrayGetUBound(runtime_id.Get(), 1, &upper_bound);
  CHECK(SUCCEEDED(hr));
  LONG runtime_id_length = upper_bound - lower_bound + 1;
  CHECK(runtime_id_length >= 4);
  {
    int32_t* runtime_id_array = nullptr;
    ::SafeArrayAccessData(runtime_id.Get(),
                          reinterpret_cast<void**>(&runtime_id_array));
    CHECK(runtime_id_array);

    // Stuff the internal id values in the last three spots in the grabbed
    // UIA-based runtime id.
    runtime_id_array[upper_bound - 2] = internal_id[0];
    runtime_id_array[upper_bound - 1] = internal_id[1];
    runtime_id_array[upper_bound] = internal_id[2];
    ::SafeArrayUnaccessData(runtime_id.Get());
  }

  *runtime_id_out = runtime_id.Release();
}

void GetUIARoot(ui::AXPlatformNodeDelegate* start,
                IUIAutomation* uia,
                IUIAutomationElement** root) {
  // If dumping when the page or iframe is reloading, the
  // tree manager may have been removed.
  ui::AXTreeManager* tree_manager = start->GetTreeManager();
  if (!tree_manager) {
    return;
  }

  // Start by getting the root element for the HWND hosting the web content.
  HWND hwnd = static_cast<ui::AXPlatformTreeManager*>(tree_manager)
                  ->RootDelegate()
                  ->GetTargetForNativeAccessibilityEvent();
  uia->ElementFromHandle(hwnd, root);
}

void GetUIAElementFromDelegate(ui::AXPlatformNodeDelegate* start,
                               IUIAutomation* uia,
                               IUIAutomationElement** element) {
  // We use the UI Automation client API to produce the tree dump, but
  // BrowserAccessibility has a pointer to a provider API implementation, and
  // we can't directly relate the two -- the OS manages the relationship.
  // To locate the client element we want, we'll construct a RuntimeId
  // corresponding to our provider element, then search for that.
  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  // If dumping when the page or iframe is reloading, we may encounter
  // a brief moment when the root cannot be found through the tree_manager.
  GetUIARoot(start, uia, &root);
  if (!root.Get()) {
    return;
  }

  // The root element is provided by AXFragmentRootWin, whose RuntimeId is not
  // in the same form as elements provided by BrowserAccessibility.
  // Find the root element's first child, which should be provided by
  // BrowserAccessibility. We'll use that element's RuntimeId as a template for
  // the RuntimeId of the element we're looking for.
  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  uia->get_RawViewWalker(&tree_walker);
  Microsoft::WRL::ComPtr<IUIAutomationElement> first_child;
  tree_walker->GetFirstChildElement(root.Get(), &first_child);
  CHECK(first_child.Get());

  // Get first_child's RuntimeId and swap out the last element in its SAFEARRAY
  // for the UniqueId of the element we want to start from.
  Microsoft::WRL::ComPtr<IUnknown> start_unknown =
      start->GetNativeViewAccessible();
  Microsoft::WRL::ComPtr<IRawElementProviderFragment> start_fragment;
  start_unknown.As(&start_fragment);
  CHECK(start_fragment.Get());
  base::win::ScopedSafearray uia_runtime_id;
  GetUIARuntimeId(first_child.Get(), start_fragment.Get(),
                  uia_runtime_id.Receive());

  // Find the element with the desired RuntimeId.
  base::win::ScopedVariant runtime_id_variant(uia_runtime_id.Release());
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
  uia->CreatePropertyCondition(UIA_RuntimeIdPropertyId, runtime_id_variant,
                               &condition);
  CHECK(condition);

  root->FindFirst(TreeScope_Subtree, condition.Get(), element);
}

RECT GetUIARootBounds(ui::AXPlatformNodeDelegate* delegate,
                      IUIAutomation* uia) {
  Microsoft::WRL::ComPtr<IUIAutomationElement> root;
  GetUIARoot(delegate, uia, &root);
  CHECK(root.Get());
  RECT root_bounds = {0};
  root->get_CurrentBoundingRectangle(&root_bounds);

  return root_bounds;
}

}  // namespace

namespace ui {

// This is the list of interesting properties to dump.
//
// Certain properties are skipped because they are known to cause crashes if the
// underlying pattern isn't implemented (e.g., LegacyIAccessibleSelection will
// crash on Win7 if the LegacyIAccessible pattern isn't implemented).
//
// Other properties aren't interesting in a tree-dump context (e.g., ProcessId).
//
// Finally, certain properties are dumped as part of a pattern, and don't need
// to be dumped a second time here (e.g., Grid*, GridItem*, RangeValue*, etc.).

// static
const long AXTreeFormatterUia::properties_[] = {
    // UIA_RuntimeIdPropertyId                          // 30000
    UIA_BoundingRectanglePropertyId,  // 30001
    // UIA_ProcessIdPropertyId                          // 30002
    UIA_ControlTypePropertyId,           // 30003
    UIA_LocalizedControlTypePropertyId,  // 30004
    UIA_NamePropertyId,                  // 30005
    UIA_AcceleratorKeyPropertyId,        // 30006
    UIA_AccessKeyPropertyId,             // 30007
    UIA_HasKeyboardFocusPropertyId,      // 30008
    UIA_IsKeyboardFocusablePropertyId,   // 30009
    UIA_IsEnabledPropertyId,             // 30010
    UIA_AutomationIdPropertyId,          // 30011
    UIA_ClassNamePropertyId,             // 30012
    UIA_HelpTextPropertyId,              // 30013
    UIA_ClickablePointPropertyId,        // 30014
    UIA_CulturePropertyId,               // 30015
    UIA_IsControlElementPropertyId,      // 30016
    UIA_IsContentElementPropertyId,      // 30017
    UIA_LabeledByPropertyId,             // 30018
    UIA_IsPasswordPropertyId,            // 30019
    // UIA_NativeWindowHandlePropertyId                 // 30020
    UIA_ItemTypePropertyId,                          // 30021
    UIA_IsOffscreenPropertyId,                       // 30022
    UIA_OrientationPropertyId,                       // 30023
    UIA_FrameworkIdPropertyId,                       // 30024
    UIA_IsRequiredForFormPropertyId,                 // 30025
    UIA_ItemStatusPropertyId,                        // 30026
    UIA_IsDockPatternAvailablePropertyId,            // 30027
    UIA_IsExpandCollapsePatternAvailablePropertyId,  // 30028
    UIA_IsGridItemPatternAvailablePropertyId,        // 30029
    UIA_IsGridPatternAvailablePropertyId,            // 30030
    UIA_IsInvokePatternAvailablePropertyId,          // 30031
    UIA_IsMultipleViewPatternAvailablePropertyId,    // 30032
    UIA_IsRangeValuePatternAvailablePropertyId,      // 30033
    UIA_IsScrollPatternAvailablePropertyId,          // 30034
    UIA_IsScrollItemPatternAvailablePropertyId,      // 30035
    UIA_IsSelectionItemPatternAvailablePropertyId,   // 30036
    UIA_IsSelectionPatternAvailablePropertyId,       // 30037
    UIA_IsTablePatternAvailablePropertyId,           // 30038
    UIA_IsTableItemPatternAvailablePropertyId,       // 30039
    UIA_IsTextPatternAvailablePropertyId,            // 30040
    UIA_IsTogglePatternAvailablePropertyId,          // 30041
    UIA_IsTransformPatternAvailablePropertyId,       // 30042
    UIA_IsValuePatternAvailablePropertyId,           // 30043
    UIA_IsWindowPatternAvailablePropertyId,          // 30044
    // UIA_Value*                                       // 30045-30046
    // UIA_RangeValue*                                  // 30047-30052
    // UIA_Scroll*                                      // 30053-30058
    // UIA_Selection*                                   // 30059-30061
    // UIA_Grid*                                        // 30062-30068
    // UIA_DockDockPositionPropertyId,                  // 30069
    // UIA_ExpandCollapseExpandCollapseStatePropertyId, // 30070
    // UIA_MultipleViewCurrentViewPropertyId,           // 30071
    // UIA_MultipleViewSupportedViewsPropertyId,        // 30072
    // UIA_WindowCanMaximizePropertyId,                 // 30073
    // UIA_WindowCanMinimizePropertyId,                 // 30074
    // UIA_WindowWindowVisualStatePropertyId,           // 30075
    // UIA_WindowWindowInteractionStatePropertyId,      // 30076
    // UIA_WindowIsModalPropertyId                      // 30077
    // UIA_WindowIsTopmostPropertyId,                   // 30078
    // UIA_SelectionItem*                               // 30079-30080
    // UIA_TableRowHeadersPropertyId,                   // 30081
    // UIA_TableColumnHeadersPropertyId,                // 30082
    // UIA_TableRowOrColumnMajorPropertyId              // 30083
    // UIA_TableItemRowHeaderItemsPropertyId,           // 30084
    // UIA_TableItemColumnHeaderItemsPropertyId,        // 30085
    // UIA_ToggleToggleStatePropertyId                  // 30086
    // UIA_TransformCanMovePropertyId,                  // 30087
    // UIA_TransformCanResizePropertyId,                // 30088
    // UIA_TransformCanRotatePropertyId,                // 30089
    UIA_IsLegacyIAccessiblePatternAvailablePropertyId,  // 30090
    // UIA_LegacyIAccessible*                           // 30091-30100
    UIA_AriaRolePropertyId,            // 30101
    UIA_AriaPropertiesPropertyId,      // 30102
    UIA_IsDataValidForFormPropertyId,  // 30103
    UIA_ControllerForPropertyId,       // 30104
    UIA_DescribedByPropertyId,         // 30105
    UIA_FlowsToPropertyId,             // 30106
    // UIA_ProviderDescriptionPropertyId                // 30107
    UIA_IsItemContainerPatternAvailablePropertyId,      // 30108
    UIA_IsVirtualizedItemPatternAvailablePropertyId,    // 30109
    UIA_IsSynchronizedInputPatternAvailablePropertyId,  // 30110
    UIA_OptimizeForVisualContentPropertyId,             // 30111
    UIA_IsObjectModelPatternAvailablePropertyId,        // 30112
    UIA_AnnotationAnnotationTypeIdPropertyId,           // 30113
    UIA_AnnotationAnnotationTypeNamePropertyId,         // 30114
    UIA_AnnotationAuthorPropertyId,                     // 30115
    UIA_AnnotationDateTimePropertyId,                   // 30116
    UIA_AnnotationTargetPropertyId,                     // 30117
    UIA_IsAnnotationPatternAvailablePropertyId,         // 30118
    UIA_IsTextPattern2AvailablePropertyId,              // 30119
    UIA_StylesStyleIdPropertyId,                        // 30120
    UIA_StylesStyleNamePropertyId,                      // 30121
    UIA_StylesFillColorPropertyId,                      // 30122
    UIA_StylesFillPatternStylePropertyId,               // 30123
    UIA_StylesShapePropertyId,                          // 30124
    UIA_StylesFillPatternColorPropertyId,               // 30125
    UIA_StylesExtendedPropertiesPropertyId,             // 30126
    UIA_IsStylesPatternAvailablePropertyId,             // 30127
    UIA_IsSpreadsheetPatternAvailablePropertyId,        // 30128
    UIA_SpreadsheetItemFormulaPropertyId,               // 30129
    UIA_SpreadsheetItemAnnotationObjectsPropertyId,     // 30130
    UIA_SpreadsheetItemAnnotationTypesPropertyId,       // 30131
    UIA_IsSpreadsheetItemPatternAvailablePropertyId,    // 30132
    UIA_Transform2CanZoomPropertyId,                    // 30133
    UIA_IsTransformPattern2AvailablePropertyId,         // 30134
    UIA_LiveSettingPropertyId,                          // 30135
    UIA_IsTextChildPatternAvailablePropertyId,          // 30136
    UIA_IsDragPatternAvailablePropertyId,               // 30137
    UIA_DragIsGrabbedPropertyId,                        // 30138
    UIA_DragDropEffectPropertyId,                       // 30139
    UIA_DragDropEffectsPropertyId,                      // 30140
    UIA_IsDropTargetPatternAvailablePropertyId,         // 30141
    UIA_DropTargetDropTargetEffectPropertyId,           // 30142
    UIA_DropTargetDropTargetEffectsPropertyId,          // 30143
    UIA_DragGrabbedItemsPropertyId,                     // 30144
    UIA_Transform2ZoomLevelPropertyId,                  // 30145
    UIA_Transform2ZoomMinimumPropertyId,                // 30146
    UIA_Transform2ZoomMaximumPropertyId,                // 30147
    UIA_FlowsFromPropertyId,                            // 30148
    UIA_IsTextEditPatternAvailablePropertyId,           // 30149
    UIA_IsPeripheralPropertyId,                         // 30150
    UIA_IsCustomNavigationPatternAvailablePropertyId,   // 30151
    UIA_PositionInSetPropertyId,                        // 30152
    UIA_SizeOfSetPropertyId,                            // 30153
    UIA_LevelPropertyId,                                // 30154
    UIA_AnnotationTypesPropertyId,                      // 30155
    UIA_AnnotationObjectsPropertyId,                    // 30156
    UIA_LandmarkTypePropertyId,                         // 30157
    UIA_LocalizedLandmarkTypePropertyId,                // 30158
    UIA_FullDescriptionPropertyId,                      // 30159
    UIA_FillColorPropertyId,                            // 30160
    UIA_OutlineColorPropertyId,                         // 30161
    UIA_FillTypePropertyId,                             // 30162
    UIA_VisualEffectsPropertyId,                        // 30163
    UIA_OutlineThicknessPropertyId,                     // 30164
    UIA_CenterPointPropertyId,                          // 30165
    UIA_RotationPropertyId,                             // 30166
    UIA_SizePropertyId,                                 // 30167
    UIA_IsSelectionPattern2AvailablePropertyId,         // 30168
    UIA_Selection2FirstSelectedItemPropertyId,          // 30169
    UIA_Selection2LastSelectedItemPropertyId,           // 30170
    UIA_Selection2CurrentSelectedItemPropertyId,        // 30171
    UIA_Selection2ItemCountPropertyId,                  // 30172
    UIA_HeadingLevelPropertyId,                         // 30173
};

const long AXTreeFormatterUia::patterns_[] = {
    UIA_SelectionPatternId,       // 10001
    UIA_ValuePatternId,           // 10002
    UIA_RangeValuePatternId,      // 10003
    UIA_ScrollPatternId,          // 10004
    UIA_ExpandCollapsePatternId,  // 10005
    UIA_GridPatternId,            // 10006
    UIA_GridItemPatternId,        // 10007
    UIA_WindowPatternId,          // 10009
    UIA_SelectionItemPatternId,   // 10010
    UIA_TablePatternId,           // 10012
    UIA_TogglePatternId,          // 10015
    UIA_AnnotationPatternId,      // 10023
};

const long AXTreeFormatterUia::pattern_properties_[] = {
    UIA_ValueValuePropertyId,                         // 30045
    UIA_ValueIsReadOnlyPropertyId,                    // 30046
    UIA_RangeValueValuePropertyId,                    // 30047
    UIA_RangeValueIsReadOnlyPropertyId,               // 30048
    UIA_RangeValueMinimumPropertyId,                  // 30049
    UIA_RangeValueMaximumPropertyId,                  // 30050
    UIA_RangeValueLargeChangePropertyId,              // 30051
    UIA_RangeValueSmallChangePropertyId,              // 30052
    UIA_ScrollHorizontalScrollPercentPropertyId,      // 30053
    UIA_ScrollHorizontalViewSizePropertyId,           // 30054
    UIA_ScrollVerticalScrollPercentPropertyId,        // 30055
    UIA_ScrollVerticalViewSizePropertyId,             // 30056
    UIA_ScrollHorizontallyScrollablePropertyId,       // 30057
    UIA_ScrollVerticallyScrollablePropertyId,         // 30058
    UIA_SelectionCanSelectMultiplePropertyId,         // 30060
    UIA_SelectionIsSelectionRequiredPropertyId,       // 30061
    UIA_GridRowCountPropertyId,                       // 30062
    UIA_GridColumnCountPropertyId,                    // 30063
    UIA_GridItemRowPropertyId,                        // 30064
    UIA_GridItemColumnPropertyId,                     // 30065
    UIA_GridItemRowSpanPropertyId,                    // 30066
    UIA_GridItemColumnSpanPropertyId,                 // 30067
    UIA_GridItemContainingGridPropertyId,             // 30068
    UIA_ExpandCollapseExpandCollapseStatePropertyId,  // 30070
    UIA_WindowIsModalPropertyId,                      // 30077
    UIA_SelectionItemIsSelectedPropertyId,            // 30079
    UIA_SelectionItemSelectionContainerPropertyId,    // 30080
    UIA_TableRowOrColumnMajorPropertyId,              // 30083
    UIA_ToggleToggleStatePropertyId,                  // 30086
};

AXTreeFormatterUia::AXTreeFormatterUia() {
  // Create an instance of the CUIAutomation class.
  CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                   IID_IUIAutomation, &uia_);
  CHECK(uia_.Get());
  BuildCacheRequests();
  BuildCustomPropertiesMap();
}

AXTreeFormatterUia::~AXTreeFormatterUia() {}

void AXTreeFormatterUia::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  // Too noisy: IsKeyboardFocusable, IsDataValidForForm, UIA_ScrollPatternId,
  //  Value.IsReadOnly

  // properties not exposed through a pattern
  AddPropertyFilter(property_filters, "Name=*");
  AddPropertyFilter(property_filters, "ItemStatus=*");
  AddPropertyFilter(property_filters, "Orientation=OrientationType_Horizontal");
  AddPropertyFilter(property_filters, "IsPassword=true");
  AddPropertyFilter(property_filters, "IsControlElement=false");
  AddPropertyFilter(property_filters, "IsEnabled=false");
  AddPropertyFilter(property_filters, "IsRequiredForForm=true");

  // UIA_ExpandCollapsePatternId
  AddPropertyFilter(property_filters, "ExpandCollapse.ExpandCollapseState=*");

  // UIA_GridPatternId
  AddPropertyFilter(property_filters, "Grid.ColumnCount=*");
  AddPropertyFilter(property_filters, "Grid.RowCount=*");
  // UIA_GridItemPatternId
  AddPropertyFilter(property_filters, "GridItem.Column=*");
  AddPropertyFilter(property_filters, "GridItem.ColumnSpan=*");
  AddPropertyFilter(property_filters, "GridItem.Row=*");
  AddPropertyFilter(property_filters, "GridItem.RowSpan=*");
  AddPropertyFilter(property_filters, "GridItem.ContainingGrid=*");
  // UIA_RangeValuePatternId
  AddPropertyFilter(property_filters, "RangeValue.IsReadOnly=*");
  AddPropertyFilter(property_filters, "RangeValue.LargeChange=*");
  AddPropertyFilter(property_filters, "RangeValue.SmallChange=*");
  AddPropertyFilter(property_filters, "RangeValue.Maximum=*");
  AddPropertyFilter(property_filters, "RangeValue.Minimum=*");
  AddPropertyFilter(property_filters, "RangeValue.Value=*");
  // UIA_SelectionPatternId
  AddPropertyFilter(property_filters, "Selection.CanSelectMultiple=*");
  AddPropertyFilter(property_filters, "Selection.IsSelectionRequired=*");
  // UIA_SelectionItemPatternId
  AddPropertyFilter(property_filters, "SelectionItem.IsSelected=*");
  AddPropertyFilter(property_filters, "SelectionItem.SelectionContainer=*");
  // UIA_TablePatternId
  AddPropertyFilter(property_filters, "Table.RowOrColumnMajor=*");
  // UIA_TogglePatternId
  AddPropertyFilter(property_filters, "Toggle.ToggleState=*");
  // UIA_ValuePatternId
  AddPropertyFilter(property_filters, "Value.Value=*");
  AddPropertyFilter(property_filters, "Value.Value='http*'",
                    AXPropertyFilter::DENY);
  // UIA_WindowPatternId
  AddPropertyFilter(property_filters, "Window.IsModal=*");

  // Custom properties.
  AddPropertyFilter(
      property_filters,
      GetPropertyName(
          UiaRegistrarWin::GetInstance().GetVirtualContentPropertyId()) +
          "=*");
}

base::Value::Dict AXTreeFormatterUia::BuildTree(
    AXPlatformNodeDelegate* start) const {
  Microsoft::WRL::ComPtr<IUIAutomationElement> start_element;
  GetUIAElementFromDelegate(start, uia_.Get(), &start_element);

  base::Value::Dict tree;
  if (!start_element) {
    return tree;
  }

  RECT root_bounds = GetUIARootBounds(start, uia_.Get());
  if (start_element.Get()) {
    // Build an accessibility tree starting from that element.
    RecursiveBuildTree(start_element.Get(), root_bounds.left, root_bounds.top,
                       &tree);
  } else {
    // If the search failed, start dumping with the first thing that isn't a
    // Pane.
    // TODO(http://crbug.com/1071188): Figure out why the original FindFirst
    // fails and remove this fallback codepath.
    Microsoft::WRL::ComPtr<IUIAutomationElement> non_pane_descendant;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> is_pane_condition;
    base::win::ScopedVariant pane_control_type_variant(UIA_PaneControlTypeId);
    uia_->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                  pane_control_type_variant,
                                  &is_pane_condition);
    Microsoft::WRL::ComPtr<IUIAutomationCondition> not_is_pane_condition;
    uia_->CreateNotCondition(is_pane_condition.Get(), &not_is_pane_condition);
    Microsoft::WRL::ComPtr<IUIAutomationElement> root;
    GetUIARoot(start, uia_.Get(), &root);
    CHECK(root.Get());
    root->FindFirst(TreeScope_Subtree, not_is_pane_condition.Get(),
                    &non_pane_descendant);

    DCHECK(non_pane_descendant.Get());
    RecursiveBuildTree(non_pane_descendant.Get(), root_bounds.left,
                       root_bounds.top, &tree);
  }
  return tree;
}

base::Value::Dict AXTreeFormatterUia::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  HWND hwnd = GetHWNDBySelector(selector);

  base::Value::Dict tree;
  if (hwnd) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> root;
    uia_->ElementFromHandle(hwnd, &root);
    CHECK(root.Get());

    RECT root_bounds = {0};
    root->get_CurrentBoundingRectangle(&root_bounds);

    RecursiveBuildTree(root.Get(), root_bounds.left, root_bounds.top, &tree);
  }

  return tree;
}

base::Value::Dict AXTreeFormatterUia::BuildNode(
    AXPlatformNodeDelegate* node) const {
  Microsoft::WRL::ComPtr<IUIAutomationElement> uia_element;
  GetUIAElementFromDelegate(node, uia_.Get(), &uia_element);
  // Note that we have to go through external UIA APIs to get a reference to
  // the given node's UIA Element. This requires that the node is marked as
  // a content/control element (see IsUIAControl for more details). If you see
  // the following CHECK hit, most likely the node is not a UIA control and thus
  // not exposed via the Find* APIs.
  CHECK(uia_element.Get());

  RECT root_bounds = GetUIARootBounds(node, uia_.Get());
  base::Value::Dict tree;
  AddProperties(uia_element.Get(), root_bounds.left, root_bounds.top, &tree);
  return tree;
}

void AXTreeFormatterUia::RecursiveBuildTree(IUIAutomationElement* uncached_node,
                                            int root_x,
                                            int root_y,
                                            base::Value::Dict* dict) const {
  // Process this node.
  AddProperties(uncached_node, root_x, root_y, dict);

  // Update the cache to get children
  Microsoft::WRL::ComPtr<IUIAutomationElement> parent;
  uncached_node->BuildUpdatedCache(children_cache_request_.Get(), &parent);

  Microsoft::WRL::ComPtr<IUIAutomationElementArray> children;
  if (!SUCCEEDED(parent->GetCachedChildren(&children)) || !children)
    return;
  // Process children.
  base::Value::List child_list;
  int child_count;
  children->get_Length(&child_count);
  for (int i = 0; i < child_count; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> child;
    base::Value::Dict child_dict;
    if (SUCCEEDED(children->GetElement(i, &child))) {
      RecursiveBuildTree(child.Get(), root_x, root_y, &child_dict);
    } else {
      child_dict.Set("error", "[Error retrieving child]");
    }
    child_list.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(child_list));
}

void AXTreeFormatterUia::AddProperties(IUIAutomationElement* uncached_node,
                                       int root_x,
                                       int root_y,
                                       base::Value::Dict* dict) const {
  // Update the cache for this node's information.
  Microsoft::WRL::ComPtr<IUIAutomationElement> node;
  uncached_node->BuildUpdatedCache(element_cache_request_.Get(), &node);

  // Get all properties that may be on this node.
  for (long i : properties_) {
    base::win::ScopedVariant variant;
    if (SUCCEEDED(node->GetCachedPropertyValue(i, variant.Receive()))) {
      WriteProperty(i, variant, dict, root_x, root_y);
    }
  }
  // Add control pattern specific properties
  AddAnnotationProperties(node.Get(), dict);
  AddExpandCollapseProperties(node.Get(), dict);
  AddGridProperties(node.Get(), dict);
  AddGridItemProperties(node.Get(), dict);
  AddRangeValueProperties(node.Get(), dict);
  AddScrollProperties(node.Get(), dict);
  AddSelectionProperties(node.Get(), dict);
  AddSelectionItemProperties(node.Get(), dict);
  AddTableProperties(node.Get(), dict);
  AddToggleProperties(node.Get(), dict);
  AddValueProperties(node.Get(), dict);
  AddValueProperties(node.Get(), dict);
  AddWindowProperties(node.Get(), dict);
  AddCustomProperties(node.Get(), dict);
}

void AXTreeFormatterUia::AddAnnotationProperties(
    IUIAutomationElement* node,
    base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationAnnotationPattern> annotation_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_AnnotationPatternId,
                                         IID_PPV_ARGS(&annotation_pattern))) &&
      annotation_pattern) {
    int type_id;
    if (SUCCEEDED(annotation_pattern->get_CachedAnnotationTypeId(&type_id))) {
      const char* type_id_string;
      switch (type_id) {
        case AnnotationType_Comment:
          type_id_string = "Comment";
          break;
        case AnnotationType_Endnote:
          type_id_string = "Endnote";
          break;
        case AnnotationType_Footnote:
          type_id_string = "Footnote";
          break;
        case AnnotationType_Highlighted:
          type_id_string = "Highlighted";
          break;
        case AnnotationType_Unknown:
          type_id_string = "Unknown";
          break;
      }
      dict->SetByDottedPath("Annotation.AnnotationTypeId", type_id_string);
    }

    base::win::ScopedBstr type_name;
    if (SUCCEEDED(annotation_pattern->get_CachedAnnotationTypeName(
            type_name.Receive()))) {
      dict->SetByDottedPath("Annotation.AnnotationTypeName",
                            BstrToUTF8(type_name.Get()));
    }
  }
}

void AXTreeFormatterUia::AddExpandCollapseProperties(
    IUIAutomationElement* node,
    base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationExpandCollapsePattern>
      expand_collapse_pattern;
  if (SUCCEEDED(
          node->GetCachedPatternAs(UIA_ExpandCollapsePatternId,
                                   IID_PPV_ARGS(&expand_collapse_pattern))) &&
      expand_collapse_pattern) {
    ExpandCollapseState current_state;
    if (SUCCEEDED(expand_collapse_pattern->get_CachedExpandCollapseState(
            &current_state))) {
      std::string state;
      switch (current_state) {
        case ExpandCollapseState_Collapsed:
          state = "Collapsed";
          break;
        case ExpandCollapseState_Expanded:
          state = "Expanded";
          break;
        case ExpandCollapseState_PartiallyExpanded:
          state = "PartiallyExpanded";
          break;
        case ExpandCollapseState_LeafNode:
          state = "LeafNode";
          break;
      }
      dict->SetByDottedPath("ExpandCollapse.ExpandCollapseState", state);
    }
  }
}

void AXTreeFormatterUia::AddGridProperties(IUIAutomationElement* node,
                                           base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationGridPattern> grid_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_GridPatternId,
                                         IID_PPV_ARGS(&grid_pattern))) &&
      grid_pattern) {
    int column_count;
    if (SUCCEEDED(grid_pattern->get_CachedColumnCount(&column_count))) {
      dict->SetByDottedPath("Grid.ColumnCount", column_count);
    }
    int row_count;
    if (SUCCEEDED(grid_pattern->get_CachedRowCount(&row_count))) {
      dict->SetByDottedPath("Grid.RowCount", row_count);
    }
  }
}

void AXTreeFormatterUia::AddGridItemProperties(IUIAutomationElement* node,
                                               base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationGridItemPattern> grid_item_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_GridItemPatternId,
                                         IID_PPV_ARGS(&grid_item_pattern))) &&
      grid_item_pattern) {
    int column;
    if (SUCCEEDED(grid_item_pattern->get_CachedColumn(&column))) {
      dict->SetByDottedPath("GridItem.Column", column);
    }
    int column_span;
    if (SUCCEEDED(grid_item_pattern->get_CachedColumnSpan(&column_span))) {
      dict->SetByDottedPath("GridItem.ColumnSpan", column_span);
    }
    int row;
    if (SUCCEEDED(grid_item_pattern->get_CachedRow(&row))) {
      dict->SetByDottedPath("GridItem.Row", row);
    }
    int row_span;
    if (SUCCEEDED(grid_item_pattern->get_CachedRowSpan(&row_span))) {
      dict->SetByDottedPath("GridItem.RowSpan", row_span);
    }
    Microsoft::WRL::ComPtr<IUIAutomationElement> containing_grid;
    if (SUCCEEDED(
            grid_item_pattern->get_CachedContainingGrid(&containing_grid))) {
      dict->SetByDottedPath("GridItem.ContainingGrid",
                            GetNodeName(containing_grid.Get()));
    }
  }
}

void AXTreeFormatterUia::AddRangeValueProperties(
    IUIAutomationElement* node,
    base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationRangeValuePattern> range_value_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_RangeValuePatternId,
                                         IID_PPV_ARGS(&range_value_pattern))) &&
      range_value_pattern) {
    BOOL is_read_only;
    if (SUCCEEDED(range_value_pattern->get_CachedIsReadOnly(&is_read_only))) {
      dict->SetByDottedPath("RangeValue.IsReadOnly", !!is_read_only);
    }
    double large_change;
    if (SUCCEEDED(range_value_pattern->get_CachedLargeChange(&large_change))) {
      dict->SetByDottedPath("RangeValue.LargeChange", large_change);
    }
    double small_change;
    if (SUCCEEDED(range_value_pattern->get_CachedSmallChange(&small_change))) {
      dict->SetByDottedPath("RangeValue.SmallChange", small_change);
    }
    double maximum;
    if (SUCCEEDED(range_value_pattern->get_CachedMaximum(&maximum))) {
      dict->SetByDottedPath("RangeValue.Maximum", maximum);
    }
    double minimum;
    if (SUCCEEDED(range_value_pattern->get_CachedMinimum(&minimum))) {
      dict->SetByDottedPath("RangeValue.Minimum", minimum);
    }
    double value;
    if (SUCCEEDED(range_value_pattern->get_CachedValue(&value))) {
      dict->SetByDottedPath("RangeValue.Value", value);
    }
  }
}

void AXTreeFormatterUia::AddScrollProperties(IUIAutomationElement* node,
                                             base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationScrollPattern> scroll_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_ScrollPatternId,
                                         IID_PPV_ARGS(&scroll_pattern))) &&
      scroll_pattern) {
    double horizontal_scroll_percent;
    if (SUCCEEDED(scroll_pattern->get_CachedHorizontalScrollPercent(
            &horizontal_scroll_percent))) {
      dict->SetByDottedPath("Scroll.HorizontalScrollPercent",
                            horizontal_scroll_percent);
    }

    double horizontal_view_size;
    if (SUCCEEDED(scroll_pattern->get_CachedHorizontalViewSize(
            &horizontal_view_size))) {
      dict->SetByDottedPath("Scroll.HorizontalViewSize", horizontal_view_size);
    }
    BOOL horizontally_scrollable;
    if (SUCCEEDED(scroll_pattern->get_CachedHorizontallyScrollable(
            &horizontally_scrollable))) {
      dict->SetByDottedPath("Scroll.HorizontallyScrollable",
                            !!horizontally_scrollable);
    }

    double vertical_scroll_percent;
    if (SUCCEEDED(scroll_pattern->get_CachedVerticalScrollPercent(
            &vertical_scroll_percent))) {
      dict->SetByDottedPath("Scroll.VerticalScrollPercent",
                            vertical_scroll_percent);
    }

    double vertical_view_size;
    if (SUCCEEDED(
            scroll_pattern->get_CachedVerticalViewSize(&vertical_view_size))) {
      dict->SetByDottedPath("Scroll.VerticalViewSize", vertical_view_size);
    }
    BOOL vertically_scrollable;
    if (SUCCEEDED(scroll_pattern->get_CachedVerticallyScrollable(
            &vertically_scrollable))) {
      dict->SetByDottedPath("Scroll.VerticallyScrollable",
                            !!vertically_scrollable);
    }
  }
}

void AXTreeFormatterUia::AddSelectionProperties(IUIAutomationElement* node,
                                                base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationSelectionPattern> selection_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_SelectionPatternId,
                                         IID_PPV_ARGS(&selection_pattern))) &&
      selection_pattern) {
    BOOL can_select_multiple;
    if (SUCCEEDED(selection_pattern->get_CachedCanSelectMultiple(
            &can_select_multiple))) {
      dict->SetByDottedPath("Selection.CanSelectMultiple",
                            !!can_select_multiple);
    }
    BOOL is_selection_required;
    if (SUCCEEDED(selection_pattern->get_CachedIsSelectionRequired(
            &is_selection_required))) {
      dict->SetByDottedPath("Selection.IsSelectionRequired",
                            !!is_selection_required);
    }
  }
}

void AXTreeFormatterUia::AddSelectionItemProperties(
    IUIAutomationElement* node,
    base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationSelectionItemPattern>
      selection_item_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(
          UIA_SelectionItemPatternId, IID_PPV_ARGS(&selection_item_pattern))) &&
      selection_item_pattern) {
    BOOL is_selected;
    if (SUCCEEDED(selection_item_pattern->get_CachedIsSelected(&is_selected))) {
      dict->SetByDottedPath("SelectionItem.IsSelected", !!is_selected);
    }
    Microsoft::WRL::ComPtr<IUIAutomationElement> selection_container;
    if (SUCCEEDED(selection_item_pattern->get_CachedSelectionContainer(
            &selection_container))) {
      dict->SetByDottedPath("SelectionItem.SelectionContainer",
                            GetNodeName(selection_container.Get()));
    }
  }
}

void AXTreeFormatterUia::AddTableProperties(IUIAutomationElement* node,
                                            base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationTablePattern> table_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_TablePatternId,
                                         IID_PPV_ARGS(&table_pattern))) &&
      table_pattern) {
    RowOrColumnMajor row_or_column_major;
    if (SUCCEEDED(
            table_pattern->get_CachedRowOrColumnMajor(&row_or_column_major))) {
      std::string row_or_column_string;
      switch (row_or_column_major) {
        case RowOrColumnMajor_RowMajor:
          row_or_column_string = "RowMajor";
          break;
        case RowOrColumnMajor_ColumnMajor:
          row_or_column_string = "ColumnMajor";
          break;
        case RowOrColumnMajor_Indeterminate:
          row_or_column_string = "Indeterminate";
          break;
      }
      dict->SetByDottedPath("Table.RowOrColumnMajor", row_or_column_string);
    }
  }
}

void AXTreeFormatterUia::AddToggleProperties(IUIAutomationElement* node,
                                             base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationTogglePattern> toggle_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_TogglePatternId,
                                         IID_PPV_ARGS(&toggle_pattern))) &&
      toggle_pattern) {
    ToggleState toggle_state;
    if (SUCCEEDED(toggle_pattern->get_CachedToggleState(&toggle_state))) {
      std::string toggle_state_string;
      switch (toggle_state) {
        case ToggleState_Off:
          toggle_state_string = "Off";
          break;
        case ToggleState_On:
          toggle_state_string = "On";
          break;
        case ToggleState_Indeterminate:
          toggle_state_string = "Indeterminate";
          break;
      }
      dict->SetByDottedPath("Toggle.ToggleState", toggle_state_string);
    }
  }
}

void AXTreeFormatterUia::AddValueProperties(IUIAutomationElement* node,
                                            base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationValuePattern> value_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_ValuePatternId,
                                         IID_PPV_ARGS(&value_pattern))) &&
      value_pattern) {
    BOOL is_read_only;
    if (SUCCEEDED(value_pattern->get_CachedIsReadOnly(&is_read_only))) {
      dict->SetByDottedPath("Value.IsReadOnly", !!is_read_only);
    }
    base::win::ScopedBstr value;
    if (SUCCEEDED(value_pattern->get_CachedValue(value.Receive()))) {
      dict->SetByDottedPath("Value.Value", BstrToUTF8(value.Get()));
    }
  }
}

void AXTreeFormatterUia::AddWindowProperties(IUIAutomationElement* node,
                                             base::Value::Dict* dict) const {
  Microsoft::WRL::ComPtr<IUIAutomationWindowPattern> window_pattern;
  if (SUCCEEDED(node->GetCachedPatternAs(UIA_WindowPatternId,
                                         IID_PPV_ARGS(&window_pattern))) &&
      window_pattern) {
    BOOL is_modal;
    if (SUCCEEDED(window_pattern->get_CachedIsModal(&is_modal))) {
      dict->SetByDottedPath("Window.IsModal", !!is_modal);
    }
  }
}

std::map<long, std::string>& AXTreeFormatterUia::GetCustomPropertiesMap()
    const {
  static base::NoDestructor<std::map<long, std::string>> custom_properties_map;
  return *custom_properties_map;
}

void AXTreeFormatterUia::AddCustomProperties(IUIAutomationElement* node,
                                             base::Value::Dict* dict) const {
  // Custom properties need to be added separately.
  for (const auto& property : GetCustomPropertiesMap()) {
    base::win::ScopedVariant variant;
    if (SUCCEEDED(
            node->GetCurrentPropertyValue(property.first, variant.Receive()))) {
      WriteProperty(property.first, variant, dict);
    }
  }
}

std::string AXTreeFormatterUia::GetPropertyName(long property_id) const {
  // We cannot infer the property name from a custom property id, so we get it
  // from the map we created manually in `BuildCustomPropertiesMap()`.
  auto property = GetCustomPropertiesMap().find(property_id);
  if (property != GetCustomPropertiesMap().end()) {
    return property->second;
  }
  return UiaIdentifierToCondensedString(property_id);
}

void AXTreeFormatterUia::WriteProperty(long propertyId,
                                       const base::win::ScopedVariant& var,
                                       base::Value::Dict* dict,
                                       int root_x,
                                       int root_y) const {
  switch (var.type()) {
    case VT_EMPTY:
    case VT_NULL:
      break;
    case VT_I2:
      dict->SetByDottedPath(GetPropertyName(propertyId), var.ptr()->iVal);
      break;
    case VT_I4:
      WriteI4Property(propertyId, var.ptr()->lVal, dict);
      break;
    case VT_R4:
      dict->SetByDottedPath(GetPropertyName(propertyId), var.ptr()->fltVal);
      break;
    case VT_R8:
      dict->SetByDottedPath(GetPropertyName(propertyId), var.ptr()->dblVal);
      break;
    case VT_I1:
      dict->SetByDottedPath(GetPropertyName(propertyId), var.ptr()->cVal);
      break;
    case VT_UI1:
      dict->SetByDottedPath(GetPropertyName(propertyId), var.ptr()->bVal);
      break;
    case VT_UI2:
      dict->SetByDottedPath(GetPropertyName(propertyId), var.ptr()->uiVal);
      break;
    case VT_UI4:
      dict->SetByDottedPath(GetPropertyName(propertyId),
                            static_cast<int>(var.ptr()->ulVal));
      break;
    case VT_BSTR:
      dict->SetByDottedPath(GetPropertyName(propertyId),
                            BstrToUTF8(var.ptr()->bstrVal));
      break;
    case VT_BOOL:
      dict->SetByDottedPath(GetPropertyName(propertyId),
                            var.ptr()->boolVal == VARIANT_TRUE ? true : false);
      break;
    case VT_UNKNOWN:
      WriteUnknownProperty(propertyId, var.ptr()->punkVal, dict);
      break;
    default:
      switch (propertyId) {
        case UIA_BoundingRectanglePropertyId:
          WriteRectangleProperty(propertyId, var, root_x, root_y, dict);
          break;
        default:
          break;
      }
      break;
  }
}

void AXTreeFormatterUia::WriteI4Property(long propertyId,
                                         long lval,
                                         base::Value::Dict* dict) const {
  switch (propertyId) {
    case UIA_ControlTypePropertyId:
      dict->SetByDottedPath(GetPropertyName(propertyId),
                            UiaIdentifierToCondensedString(lval));
      break;
    case UIA_OrientationPropertyId:
      dict->SetByDottedPath(GetPropertyName(propertyId),
                            base::WideToUTF8(UiaOrientationToString(lval)));
      break;
    case UIA_LiveSettingPropertyId:
      dict->SetByDottedPath(GetPropertyName(propertyId),
                            base::WideToUTF8(UiaLiveSettingToString(lval)));
      break;
    default:
      dict->SetByDottedPath(GetPropertyName(propertyId),
                            static_cast<int>(lval));
      break;
  }
}

void AXTreeFormatterUia::WriteUnknownProperty(long propertyId,
                                              IUnknown* unk,
                                              base::Value::Dict* dict) const {
  switch (propertyId) {
    case UIA_ControllerForPropertyId:
    case UIA_DescribedByPropertyId:
    case UIA_FlowsFromPropertyId:
    case UIA_FlowsToPropertyId: {
      Microsoft::WRL::ComPtr<IUIAutomationElementArray> array;
      if (unk && SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&array))))
        WriteElementArray(propertyId, array.Get(), dict);
      break;
    }
    case UIA_LabeledByPropertyId: {
      Microsoft::WRL::ComPtr<IUIAutomationElement> node;
      if (unk && SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&node)))) {
        dict->SetByDottedPath(GetPropertyName(propertyId),
                              GetNodeName(node.Get()));
      }
      break;
    }
    default:
      break;
  }
}

void AXTreeFormatterUia::WriteRectangleProperty(long propertyId,
                                                const VARIANT& value,
                                                int root_x,
                                                int root_y,
                                                base::Value::Dict* dict) const {
  CHECK(value.vt == (VT_ARRAY | VT_R8));

  double* data = nullptr;
  SafeArrayAccessData(value.parray, reinterpret_cast<void**>(&data));

  base::Value::Dict rectangle;
  rectangle.Set("left", static_cast<int>(data[0] - root_x));
  rectangle.Set("top", static_cast<int>(data[1] - root_y));
  rectangle.Set("width", static_cast<int>(data[2]));
  rectangle.Set("height", static_cast<int>(data[3]));
  dict->SetByDottedPath(GetPropertyName(propertyId), std::move(rectangle));

  SafeArrayUnaccessData(value.parray);
}

void AXTreeFormatterUia::WriteElementArray(long propertyId,
                                           IUIAutomationElementArray* array,
                                           base::Value::Dict* dict) const {
  int count;
  array->get_Length(&count);
  std::u16string element_list;
  for (int i = 0; i < count; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> element;
    if (SUCCEEDED(array->GetElement(i, &element))) {
      if (element_list != u"") {
        element_list += u", ";
      }
      auto name = GetNodeName(element.Get());
      if (name.empty()) {
        base::win::ScopedBstr role;
        element->get_CurrentAriaRole(role.Receive());
        name = u"{" + base::WideToUTF16(role.Get()) + u"}";
      }
      element_list += name;
    }
  }
  if (!element_list.empty()) {
    dict->SetByDottedPath(GetPropertyName(propertyId), element_list);
  }
}

std::u16string AXTreeFormatterUia::GetNodeName(
    IUIAutomationElement* uncached_node) const {
  // Update the cache for this node.
  if (uncached_node) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> node;
    uncached_node->BuildUpdatedCache(element_cache_request_.Get(), &node);

    base::win::ScopedBstr name;
    base::win::ScopedVariant variant;
    if (SUCCEEDED(node->GetCachedPropertyValue(UIA_NamePropertyId,
                                               variant.Receive())) &&
        variant.type() == VT_BSTR) {
      return base::WideToUTF16(
          {variant.ptr()->bstrVal, SysStringLen(variant.ptr()->bstrVal)});
    }
  }
  return std::u16string();
}

void AXTreeFormatterUia::BuildCacheRequests() {
  // Create cache request for requesting children of a node.
  uia_->CreateCacheRequest(&children_cache_request_);
  CHECK(children_cache_request_.Get());
  children_cache_request_->put_TreeScope(TreeScope_Children);

  // Set filter to include all nodes in the raw view.
  Microsoft::WRL::ComPtr<IUIAutomationCondition> raw_view_condition;
  uia_->get_RawViewCondition(&raw_view_condition);
  CHECK(raw_view_condition.Get());
  children_cache_request_->put_TreeFilter(raw_view_condition.Get());

  // Create cache request for requesting information about a node.
  uia_->CreateCacheRequest(&element_cache_request_);
  CHECK(element_cache_request_.Get());
  element_cache_request_->put_TreeScope(TreeScope_Element);

  // Caching properties allows us to use GetCachedPropertyValue.
  // The non-cached version (GetCurrentPropertyValue) may cross
  // the process boundary for each call.

  // Cache all properties.
  for (long i : properties_) {
    element_cache_request_->AddProperty(i);
  }
  // Cache all patterns.
  for (long i : patterns_) {
    element_cache_request_->AddPattern(i);
  }
  // Cache pattern properties
  for (long i : pattern_properties_) {
    element_cache_request_->AddProperty(i);
  }
}

void AXTreeFormatterUia::BuildCustomPropertiesMap() {
  GetCustomPropertiesMap().insert(
      {UiaRegistrarWin::GetInstance().GetVirtualContentPropertyId(),
       "VirtualContent"});
}

std::string AXTreeFormatterUia::ProcessTreeForOutput(
    const base::Value::Dict& dict) const {
  std::string line;

  // Always show control type, and show it first.
  const std::string* control_type_value =
      dict.FindStringByDottedPath(GetPropertyName(UIA_ControlTypePropertyId));
  if (control_type_value) {
    WriteAttribute(true, *control_type_value, &line);
  }

  // Properties.
  for (long i : properties_) {
    ProcessPropertyForOutput(GetPropertyName(i), dict, line);
  }

  // Custom properties.
  for (const auto& i : GetCustomPropertiesMap()) {
    ProcessPropertyForOutput(GetPropertyName(i.first), dict, line);
  }

  // Patterns.
  const std::string pattern_property_names[] = {
      // UIA_AnnotationPatternId
      "Annotation.AnnotationTypeId", "Annotation.AnnotationTypeName",
      // UIA_ExpandCollapsePatternId
      "ExpandCollapse.ExpandCollapseState",
      // UIA_GridPatternId
      "Grid.ColumnCount", "Grid.RowCount",
      // UIA_GridItemPatternId
      "GridItem.Column", "GridItem.ColumnSpan", "GridItem.Row",
      "GridItem.RowSpan", "GridItem.ContainingGrid",
      // UIA_RangeValuePatternId
      "RangeValue.IsReadOnly", "RangeValue.LargeChange",
      "RangeValue.SmallChange", "RangeValue.Maximum", "RangeValue.Minimum",
      "RangeValue.Value",
      // UIA_ScrollPatternId
      "Scroll.HorizontalScrollPercent", "Scroll.HorizontalViewSize",
      "Scroll.HorizontallyScrollable", "Scroll.VerticalScrollPercent",
      "Scroll.VerticalViewSize", "Scroll.VerticallyScrollable",
      // UIA_SelectionPatternId
      "Selection.CanSelectMultiple", "Selection.IsSelectionRequired",
      // UIA_SelectionItemPatternId
      "SelectionItem.IsSelected", "SelectionItem.SelectionContainer",
      // UIA_TablePatternId
      "Table.RowOrColumnMajor",
      // UIA_TogglePatternId
      "Toggle.ToggleState",
      // UIA_ValuePatternId
      "Value.IsReadOnly", "Value.Value",
      // UIA_WindowPatternId
      "Window.IsModal"};

  for (const std::string& pattern_property_name : pattern_property_names) {
    ProcessPropertyForOutput(pattern_property_name, dict, line);
  }

  return line;
}

void AXTreeFormatterUia::ProcessPropertyForOutput(
    const std::string& property_name,
    const base::Value::Dict& dict,
    std::string& line) const {
  const base::Value* value = dict.FindByDottedPath(property_name);
  if (value) {
    ProcessValueForOutput(property_name, *value, line);
  }
}

void AXTreeFormatterUia::ProcessValueForOutput(const std::string& name,
                                               const base::Value& value,
                                               std::string& line) const {
  switch (value.type()) {
    case base::Value::Type::STRING: {
      WriteAttribute(false,
                     base::StringPrintf("%s='%s'", name.c_str(),
                                        value.GetString().c_str()),
                     &line);
      break;
    }
    case base::Value::Type::BOOLEAN: {
      WriteAttribute(false,
                     base::StringPrintf("%s=%s", name.c_str(),
                                        (value.GetBool() ? "true" : "false")),
                     &line);
      break;
    }
    case base::Value::Type::INTEGER: {
      WriteAttribute(false,
                     base::StringPrintf("%s=%d", name.c_str(), value.GetInt()),
                     &line);
      break;
    }
    case base::Value::Type::DOUBLE: {
      WriteAttribute(
          false, base::StringPrintf("%s=%.2f", name.c_str(), value.GetDouble()),
          &line);
      break;
    }
    case base::Value::Type::DICT: {
      if (name == "BoundingRectangle") {
        WriteAttribute(false,
                       FormatRectangle(value.GetDict(), "BoundingRectangle",
                                       "left", "top", "width", "height"),
                       &line);
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace ui
