// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_COM_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_COM_WIN_H_

#include <oleacc.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/win/atl.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_win.h"
#include "base/component_export.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "third_party/isimpledom/ISimpleDOMDocument.h"
#include "third_party/isimpledom/ISimpleDOMNode.h"
#include "third_party/isimpledom/ISimpleDOMText.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

// This nonstandard GUID is taken directly from the Mozilla sources
// (https://searchfox.org/mozilla-central/source/accessible/windows/msaa/ServiceProvider.cpp#110).
const GUID GUID_ISimpleDOM = {0x0c539790,
                              0x12e4,
                              0x11cf,
                              {0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8}};

namespace ui {
class BrowserAccessibilityWin;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibilityComWin
//
// Class implementing the windows accessible interface used by screen readers
// and other assistive technology (AT). It typically is created and owned by
// a BrowserAccessibilityWin delegate. When this owner goes away, the
// BrowserAccessibilityComWin objects may continue to exists being held onto by
// MSCOM (due to reference counting). However, such objects are invalid and
// should gracefully fail by returning E_FAIL from all MSCOM methods.
//
////////////////////////////////////////////////////////////////////////////////
class __declspec(uuid("562072fe-3390-43b1-9e2c-dd4118f5ac79"))
BrowserAccessibilityComWin : public AXPlatformNodeWin,
                             public IAccessibleApplication,
                             public IAccessibleHyperlink,
                             public IAccessibleImage,
                             public ISimpleDOMDocument,
                             public ISimpleDOMNode,
                             public ISimpleDOMText {
 public:
  BEGIN_COM_MAP(BrowserAccessibilityComWin)
  COM_INTERFACE_ENTRY(IAccessibleAction)
  COM_INTERFACE_ENTRY(IAccessibleApplication)
  COM_INTERFACE_ENTRY(IAccessibleHyperlink)
  COM_INTERFACE_ENTRY(IAccessibleImage)
  COM_INTERFACE_ENTRY(ISimpleDOMDocument)
  COM_INTERFACE_ENTRY(ISimpleDOMNode)
  COM_INTERFACE_ENTRY(ISimpleDOMText)
  COM_INTERFACE_ENTRY_CHAIN(AXPlatformNodeWin)
  END_COM_MAP()

  // Mappings from roles and states to human readable strings. Initialize
  // with |InitializeStringMaps|.
  static std::map<int32_t, std::u16string> role_string_map;
  static std::map<int32_t, std::u16string> state_string_map;

  COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityComWin();

  BrowserAccessibilityComWin(const BrowserAccessibilityComWin&) = delete;
  BrowserAccessibilityComWin& operator=(const BrowserAccessibilityComWin&) =
      delete;

  COMPONENT_EXPORT(AX_PLATFORM) ~BrowserAccessibilityComWin() override;

  // AXPlatformNodeWin methods.
  COMPONENT_EXPORT(AX_PLATFORM) void OnReferenced() override;
  COMPONENT_EXPORT(AX_PLATFORM) void OnDereferenced() override;

  // Called after an atomic tree update completes. See
  // BrowserAccessibilityManagerWin::OnAtomicUpdateFinished for more
  // details on what these do.
  COMPONENT_EXPORT(AX_PLATFORM) void UpdateStep1ComputeWinAttributes();
  COMPONENT_EXPORT(AX_PLATFORM) void UpdateStep2ComputeHypertext();
  COMPONENT_EXPORT(AX_PLATFORM) void UpdateStep3FireEvents();

  //
  // IAccessible2 methods.
  //
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_attributes(BSTR* attributes) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  scrollTo(enum IA2ScrollType scroll_type) override;

  //
  // IAccessibleApplication methods.
  //
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_appName(BSTR* app_name) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_appVersion(BSTR* app_version) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_toolkitName(BSTR* toolkit_name) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_toolkitVersion(BSTR* toolkit_version) override;

  //
  // IAccessibleImage methods.
  //
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_description(BSTR* description) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_imagePosition(enum IA2CoordinateType coordinate_type,
                    LONG* x,
                    LONG* y) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_imageSize(LONG* height,
                                              LONG* width) override;

  //
  // IAccessibleText methods.
  //

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_characterExtents(LONG offset,
                       enum IA2CoordinateType coord_type,
                       LONG* out_x,
                       LONG* out_y,
                       LONG* out_width,
                       LONG* out_height) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_nSelections(LONG* n_selections) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_selection(LONG selection_index,
                                              LONG* start_offset,
                                              LONG* end_offset) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_text(LONG start_offset,
                                         LONG end_offset,
                                         BSTR* text) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_newText(IA2TextSegment* new_text) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_oldText(IA2TextSegment* old_text) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  scrollSubstringTo(LONG start_index,
                    LONG end_index,
                    enum IA2ScrollType scroll_type) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  scrollSubstringToPoint(LONG start_index,
                         LONG end_index,
                         enum IA2CoordinateType coordinate_type,
                         LONG x,
                         LONG y) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP setCaretOffset(LONG offset) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP setSelection(LONG selection_index,
                                             LONG start_offset,
                                             LONG end_offset) override;

  // IAccessibleText methods not implemented.
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_attributes(LONG offset,
                                               LONG* start_offset,
                                               LONG* end_offset,
                                               BSTR* text_attributes) override;

  //
  // IAccessibleHypertext methods.
  //

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_nHyperlinks(LONG* hyperlink_count) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  // A hyperlink represents an embedded object character (leading to a subtree).
  get_hyperlink(LONG index, IAccessibleHyperlink** hyperlink) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_hyperlinkIndex(LONG char_index, LONG* hyperlink_index) override;

  // IAccessibleHyperlink methods.
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_anchor(LONG index,
                                           VARIANT* anchor) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_anchorTarget(LONG index, VARIANT* anchor_target) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_startIndex(LONG* index) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_endIndex(LONG* index) override;
  // This method is deprecated in the IA2 Spec and so we don't implement it.
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_valid(boolean* valid) override;

  // IAccessibleAction mostly not implemented.
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP nActions(LONG* n_actions) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP doAction(LONG action_index) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_description(LONG action_index,
                                                BSTR* description) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_keyBinding(LONG action_index,
                                               LONG n_max_bindings,
                                               BSTR** key_bindings,
                                               LONG* n_bindings) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_name(LONG action_index,
                                         BSTR* name) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_localizedName(LONG action_index, BSTR* localized_name) override;

  //
  // ISimpleDOMDocument methods.
  //

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_URL(BSTR* url) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_title(BSTR* title) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_mimeType(BSTR* mime_type) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_docType(BSTR* doc_type) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_nameSpaceURIForID(SHORT name_space_id, BSTR* name_space_uri) override;
  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  put_alternateViewMediaTypes(BSTR* comma_separated_media_types) override;

  //
  // ISimpleDOMNode methods.
  //

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_nodeInfo(BSTR* node_name,
                                             SHORT* name_space_id,
                                             BSTR* node_value,
                                             unsigned int* num_children,
                                             unsigned int* unique_id,
                                             USHORT* node_type) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_attributes(USHORT max_attribs,
                                               BSTR* attrib_names,
                                               SHORT* name_space_id,
                                               BSTR* attrib_values,
                                               USHORT* num_attribs) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_attributesForNames(USHORT num_attribs,
                         BSTR* attrib_names,
                         SHORT* name_space_id,
                         BSTR* attrib_values) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_computedStyle(USHORT max_style_properties,
                    boolean use_alternate_view,
                    BSTR* style_properties,
                    BSTR* style_values,
                    USHORT* num_style_properties) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_computedStyleForProperties(USHORT num_style_properties,
                                 boolean use_alternate_view,
                                 BSTR* style_properties,
                                 BSTR* style_values) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP scrollTo(boolean placeTopLeft) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_parentNode(ISimpleDOMNode** node) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_firstChild(ISimpleDOMNode** node) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_lastChild(ISimpleDOMNode** node) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_previousSibling(ISimpleDOMNode** node) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_nextSibling(ISimpleDOMNode** node) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_childAt(unsigned int child_index,
                                            ISimpleDOMNode** node) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_innerHTML(BSTR* innerHTML) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_localInterface(void** local_interface) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_language(BSTR* language) override;

  //
  // ISimpleDOMText methods.
  //

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_domText(BSTR* dom_text) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_clippedSubstringBounds(unsigned int start_index,
                             unsigned int end_index,
                             int* out_x,
                             int* out_y,
                             int* out_width,
                             int* out_height) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  get_unclippedSubstringBounds(unsigned int start_index,
                               unsigned int end_index,
                               int* out_x,
                               int* out_y,
                               int* out_width,
                               int* out_height) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP
  scrollToSubstring(unsigned int start_index, unsigned int end_index) override;

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP get_fontFamily(BSTR* font_family) override;

  //
  // IServiceProvider methods.
  //

  COMPONENT_EXPORT(AX_PLATFORM) IFACEMETHODIMP QueryService(REFGUID guidService,
                                             REFIID riid,
                                             void** object) override;

  //
  // CComObjectRootEx methods.
  //

  // Called by BEGIN_COM_MAP() / END_COM_MAP().
  static COMPONENT_EXPORT(AX_PLATFORM) STDMETHODIMP
  InternalQueryInterface(void* this_ptr,
                         const _ATL_INTMAP_ENTRY* entries,
                         REFIID iid,
                         void** object);

  // Computes and caches the IA2 text style attributes for the text and other
  // embedded child objects.
  COMPONENT_EXPORT(AX_PLATFORM) void ComputeStylesIfNeeded();

  // Public accessors (these do not have COM accessible accessors)
  const TextAttributeMap& offset_to_text_attributes() const {
    return win_attributes_->offset_to_text_attributes;
  }

 private:
  // Private accessors.
  const std::vector<std::wstring>& ia2_attributes() const {
    return win_attributes_->ia2_attributes;
  }
  std::wstring name() const { return win_attributes_->name; }
  std::wstring description() const { return win_attributes_->description; }
  std::wstring value() const { return win_attributes_->value; }

  BrowserAccessibilityWin* GetOwner() const;
  BrowserAccessibilityManager* Manager() const;

  // Returns the IA2 text attributes for this object.
  TextAttributeList ComputeTextAttributes() const;

  // Add one to the reference count and return the same object. Always
  // use this method when returning a BrowserAccessibilityComWin object as
  // an output parameter to a COM interface, never use it otherwise.
  BrowserAccessibilityComWin* NewReference();

  // Many MSAA methods take a var_id parameter indicating that the operation
  // should be performed on a particular child ID, rather than this object.
  // This method tries to figure out the target object from |var_id| and
  // returns a pointer to the target object if it exists, otherwise NULL.
  // Does not return a new reference.
  BrowserAccessibilityComWin* GetTargetFromChildID(const VARIANT& var_id);

  // Retrieve the value of an attribute from the string attribute map and
  // if found and nonempty, allocate a new BSTR (with SysAllocString)
  // and return S_OK. If not found or empty, return S_FALSE.
  HRESULT GetStringAttributeAsBstr(ax::mojom::StringAttribute attribute,
                                   BSTR* value_bstr);

  // Retrieves the name, allocates a new BSTR if non-empty and returns S_OK. If
  // name is empty, returns S_FALSE.
  HRESULT GetNameAsBstr(BSTR* value_bstr);

  // Sets the selection given a start and end offset in IA2 Hypertext.
  void SetIA2HypertextSelection(LONG start_offset, LONG end_offset);

  // Searches forward from the given offset until the start of the next style
  // is found, or searches backward from the given offset until the start of the
  // current style is found.
  LONG FindStartOfStyle(LONG start_offset, ax::mojom::MoveDirection direction);

  // ID refers to the node ID in the current tree, not the globally unique ID.
  // TODO(nektar): Could we use globally unique IDs everywhere?
  // TODO(nektar): Rename this function to GetFromNodeID.
  BrowserAccessibilityComWin* GetFromID(int32_t id) const;

  // Fire a Windows-specific accessibility event notification on this node.
  void FireNativeEvent(LONG win_event_type) const;
  struct WinAttributes {
    WinAttributes();
    ~WinAttributes();

    // Ignored state
    bool ignored;

    // IAccessible role and state.
    int32_t ia_role;
    int32_t ia_state;

    // IAccessible name, description, help, value.
    std::wstring name;
    std::wstring description;
    std::wstring value;

    // IAccessible2 role and state.
    int32_t ia2_role;
    int32_t ia2_state;

    // IAccessible2 attributes.
    std::vector<std::wstring> ia2_attributes;

    // Maps each style span to its start offset in hypertext.
    TextAttributeMap offset_to_text_attributes;
  };

  std::unique_ptr<WinAttributes> win_attributes_;

  // Holds transient state needed only while processing a tree update.
  struct UpdateState {
    UpdateState(std::unique_ptr<WinAttributes> old_win_attributes,
                AXLegacyHypertext old_hypertext);
    UpdateState(const UpdateState&) = delete;
    UpdateState& operator=(const UpdateState&) = delete;
    ~UpdateState();

    std::unique_ptr<WinAttributes> old_win_attributes;
    AXLegacyHypertext old_hypertext;
  };

  // Only valid during the scope of a IA2_EVENT_TEXT_REMOVED or
  // IA2_EVENT_TEXT_INSERTED event.
  std::unique_ptr<UpdateState> update_state_;

  // The previous scroll position, so we can tell if this object scrolled.
  int previous_scroll_x_;
  int previous_scroll_y_;

  // Give BrowserAccessibility::Create access to our constructor.
  friend class BrowserAccessibility;
  friend class BrowserAccessibilityWin;
};

COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityComWin* ToBrowserAccessibilityComWin(
    BrowserAccessibility* obj);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_COM_WIN_H_
