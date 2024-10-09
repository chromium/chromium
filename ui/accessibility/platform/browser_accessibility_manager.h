// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "cc/base/rtree.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class AXNodeIdDelegate;
}

namespace content {
FORWARD_DECLARE_TEST(BrowserAccessibilityManagerTest,
                     TestShouldFireEventForNode);
FORWARD_DECLARE_TEST(BrowserAccessibilityManagerTest,
                     TestShouldFireEventForAlertEventWithEmptyName);
FORWARD_DECLARE_TEST(BrowserAccessibilityManagerTest,
                     TestShouldFireEventForAlertEventWithNonEmptyName);
}
namespace ui {

// Required by the several platform specific
// `BrowserAccessibilityManager::ToBrowserAccessibilityManager...()` methods
// declared below.
#if BUILDFLAG(IS_WIN)
class BrowserAccessibilityManagerWin;
#elif BUILDFLAG(USE_ATK)
class BrowserAccessibilityManagerAuraLinux;
#elif BUILDFLAG(IS_MAC)
class BrowserAccessibilityManagerMac;
#elif BUILDFLAG(IS_IOS)
class BrowserAccessibilityManagerIOS;
#endif

// To be called when a BrowserAccessibilityManager fires a generated event.
// Provides the host, the event fired, and which node id the event was for.
class BrowserAccessibilityManager;
using GeneratedEventCallbackForTesting = base::RepeatingCallback<
    void(BrowserAccessibilityManager*, AXEventGenerator::Event, AXNodeID)>;

COMPONENT_EXPORT(AX_PLATFORM)
AXTreeUpdate MakeAXTreeUpdateForTesting(
    const AXNodeData& node,
    const AXNodeData& node2 = AXNodeData(),
    const AXNodeData& node3 = AXNodeData(),
    const AXNodeData& node4 = AXNodeData(),
    const AXNodeData& node5 = AXNodeData(),
    const AXNodeData& node6 = AXNodeData(),
    const AXNodeData& node7 = AXNodeData(),
    const AXNodeData& node8 = AXNodeData(),
    const AXNodeData& node9 = AXNodeData(),
    const AXNodeData& node10 = AXNodeData(),
    const AXNodeData& node11 = AXNodeData(),
    const AXNodeData& node12 = AXNodeData(),
    const AXNodeData& node13 = AXNodeData(),
    const AXNodeData& node14 = AXNodeData());

// This is all of the information about the current find in page result,
// so we can activate it if requested.
struct BrowserAccessibilityFindInPageInfo {
  BrowserAccessibilityFindInPageInfo();

  // This data about find in text results is updated as the user types.
  int request_id;
  int match_index;
  int start_id;
  int start_offset;
  int end_id;
  int end_offset;

  // The active request id indicates that the user committed to a find query,
  // e.g. by pressing enter or pressing the next or previous buttons. If
  // |active_request_id| == |request_id|, we fire an accessibility event
  // to move screen reader focus to that event.
  int active_request_id;
};

// Manages a tree of BrowserAccessibility objects.
class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityManager
    : public AXPlatformTreeManager {
 public:
  // Creates the platform-specific BrowserAccessibilityManager.
  static BrowserAccessibilityManager* Create(
      const AXTreeUpdate& initial_tree,
      AXNodeIdDelegate& node_id_delegate,
      AXPlatformTreeManagerDelegate* delegate);
  static BrowserAccessibilityManager* Create(
      AXNodeIdDelegate& node_id_delegate,
      AXPlatformTreeManagerDelegate* delegate);

  static BrowserAccessibilityManager* FromID(AXTreeID ax_tree_id);

  BrowserAccessibilityManager(const BrowserAccessibilityManager&) = delete;
  BrowserAccessibilityManager& operator=(const BrowserAccessibilityManager&) =
      delete;

  ~BrowserAccessibilityManager() override;

  static AXTreeUpdate GetEmptyDocument();

  // Fires the notification event for an ARIA notification posted to the given
  // `node`. It should be overridden by each platform-specific implementation.
  virtual void FireAriaNotificationEvent(
      BrowserAccessibility* node,
      const std::string& announcement,
      const std::string& notification_id,
      ax::mojom::AriaNotificationInterrupt interrupt_property,
      ax::mojom::AriaNotificationPriority priority_property) {}

  virtual void FireBlinkEvent(ax::mojom::Event event_type,
                              BrowserAccessibility* node,
                              int action_request_id) {}

  // AXPlatformTreeManager overrides.
  void FireGeneratedEvent(AXEventGenerator::Event event_type,
                          const AXNode* node) override;

  // Checks whether focus has changed since the last time it was checked,
  // taking into account whether the window has focus and which frame within
  // the frame tree has focus. If focus has changed, calls FireFocusEvent.
  void FireFocusEventsIfNeeded();

  // Return whether or not we are currently able to fire events.
  bool CanFireEvents() const override;

  // Return a pointer to the root of the tree.
  BrowserAccessibility* GetBrowserAccessibilityRoot() const;

  // Returns a pointer to the BrowserAccessibility object for a given AXNode.
  BrowserAccessibility* GetFromAXNode(const AXNode* node) const;

  // Return a pointer to the object corresponding to the given id,
  // does not make a new reference.
  BrowserAccessibility* GetFromID(int32_t id) const;

  // If this tree has a parent tree, return the parent node in that tree.
  BrowserAccessibility* GetParentNodeFromParentTreeAsBrowserAccessibility()
      const;

  // In general, there is only a single node with the role of kRootWebArea,
  // but if a popup is opened, a second nested "root" is created in the same
  // tree as the "true" root. This will keep track of the nested root node.
  BrowserAccessibility* GetPopupRoot() const;

  // Called to notify the accessibility manager that its associated native
  // view got focused.
  virtual void OnWindowFocused();

  // Called to notify the accessibility manager that its associated native
  // view lost focus.
  virtual void OnWindowBlurred();

  // Notify the accessibility manager about page navigation.
  void UserIsNavigatingAway();
  virtual void UserIsReloading();
  void NavigationSucceeded();
  void NavigationFailed();
  void DidStopLoading();

  // For testing only, register a function to be called when
  // a generated event is fired from this BrowserAccessibilityManager.
  void SetGeneratedEventCallbackForTesting(
      const GeneratedEventCallbackForTesting& callback);

  // For testing only, register a function to be called when nodes
  // change location / bounding box in this BrowserAccessibilityManager.
  void SetLocationChangeCallbackForTesting(
      const base::RepeatingClosure& callback);

  // Normally we avoid firing accessibility focus events when the containing
  // native window isn't focused, and we also delay some other events like
  // live region events to improve screen reader compatibility.
  // However, this can lead to test flakiness, so for testing, simplify
  // this behavior and just fire all events with no delay as if the window
  // had focus.
  static void NeverSuppressOrDelayEventsForTesting();

  // Accessibility actions. All of these are implemented asynchronously
  // by sending a message to the renderer to perform the respective action
  // on the given node.  See the definition of |AXActionData| for more
  // information about each of these actions.
  void ClearAccessibilityFocus(const BrowserAccessibility& node);
  void Decrement(const BrowserAccessibility& node);
  void DoDefaultAction(const BrowserAccessibility& node);
  void GetImageData(const BrowserAccessibility& node,
                    const gfx::Size& max_size);
  void Expand(const BrowserAccessibility& node);
  void Collapse(const BrowserAccessibility& node);
  // Per third_party/blink/renderer/core/layout/hit_test_location.h, Blink
  // expects hit test points in page coordinates. However, WebAXObject::HitTest
  // applies the visual viewport offset, so we want to pass that function a
  // point in frame coordinates.
  void HitTest(const gfx::Point& frame_point, int request_id) const;
  void Increment(const BrowserAccessibility& node);
  void LoadInlineTextBoxes(const BrowserAccessibility& node);
  void Scroll(const BrowserAccessibility& node,
              ax::mojom::Action scroll_action);
  void ScrollToMakeVisible(
      const BrowserAccessibility& node,
      gfx::Rect subfocus,
      ax::mojom::ScrollAlignment horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollAlignment vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollBehavior scroll_behavior =
          ax::mojom::ScrollBehavior::kDoNotScrollIfVisible);
  void ScrollToPoint(const BrowserAccessibility& node, gfx::Point point);
  void SetAccessibilityFocus(const BrowserAccessibility& node);
  void Blur(const BrowserAccessibility& node);
  void SetFocus(const BrowserAccessibility& node);
  void SetSequentialFocusNavigationStartingPoint(
      const BrowserAccessibility& node);
  void SetScrollOffset(const BrowserAccessibility& node, gfx::Point offset);
  void SetValue(const BrowserAccessibility& node, const std::string& value);
  void SetSelection(const AXActionData& action_data);
  void SetSelection(const BrowserAccessibility::AXRange& range);
  void ShowContextMenu(const BrowserAccessibility& node);
  void SignalEndOfTest();
  void StitchChildTree(const BrowserAccessibility& node,
                       const AXTreeID& child_tree_id);

  // Retrieve the bounds of the parent View in screen coordinates.
  virtual gfx::Rect GetViewBoundsInScreenCoordinates() const;

  // Called when the renderer process has notified us of tree changes. Returns
  // false in fatal-error conditions, in which case the caller should destroy
  // the manager.
  virtual bool OnAccessibilityEvents(const AXUpdatesAndEvents& details);

  // Allows derived classes to do event pre-processing
  virtual void BeforeAccessibilityEvents();

  // Allows derived classes to do event post-processing.
  virtual void FinalizeAccessibilityEvents();

  // Called when the renderer process updates the location of accessibility
  // objects. Calls SendLocationChangeEvents(), which can be overridden.
  void OnLocationChanges(const AXLocationAndScrollUpdates& changes);

  // Called when a new find in page result is received. We hold on to this
  // information and don't activate it until the user requests it.
  virtual void OnFindInPageResult(int request_id,
                                  int match_index,
                                  int start_id,
                                  int start_offset,
                                  int end_id,
                                  int end_offset);

  // This is called when the user has committed to a find in page query,
  // e.g. by pressing enter or tapping on the next / previous result buttons.
  // If a match has already been received for this request id,
  // activate the result now by firing an accessibility event. If a match
  // has not been received, we hold onto this request id and update it
  // when OnFindInPageResult is called.
  void ActivateFindInPageResult(int request_id);

  // This is called when the user finishes a find in page query and all
  // highlighted matches are deactivated.
  virtual void OnFindInPageTermination() {}

#if BUILDFLAG(IS_WIN)
  BrowserAccessibilityManagerWin* ToBrowserAccessibilityManagerWin();
#endif

#if BUILDFLAG(USE_ATK)
  BrowserAccessibilityManagerAuraLinux*
  ToBrowserAccessibilityManagerAuraLinux();
#endif

#if BUILDFLAG(IS_MAC)
  BrowserAccessibilityManagerMac* ToBrowserAccessibilityManagerMac();
#endif

#if BUILDFLAG(IS_IOS)
  BrowserAccessibilityManagerIOS* ToBrowserAccessibilityManagerIOS();
#endif

  // Returns the object that has focus, starting at the top of the frame tree,
  // or returns nullptr if this manager doesn't have access to the top document.
  virtual BrowserAccessibility* GetFocus() const;

  // Return the object that has focus, only considering this frame and
  // descendants.
  BrowserAccessibility* GetFocusFromThisOrDescendantFrame() const;

  // Given a node, returns a descendant of that node if the node has an active
  // descendant, otherwise returns the node itself. The node does not need to be
  // focused.
  BrowserAccessibility* GetActiveDescendant(BrowserAccessibility* node) const;

  // Given a focused node |focus|, returns a list of nodes that the focused
  // node controls.
  std::vector<BrowserAccessibility*> GetAriaControls(
      const BrowserAccessibility* focus) const;

  // Returns true if native focus is anywhere in this WebContents or not.
  bool NativeViewHasFocus();

  // True by default, but some platforms want to treat the root
  // scroll offsets separately.
  bool UseRootScrollOffsetsWhenComputingBounds();
  void SetUseRootScrollOffsetsWhenComputingBoundsForTesting(bool use);

  // Walk the tree using depth-first pre-order traversal.
  static BrowserAccessibility* NextInTreeOrder(
      const BrowserAccessibility* object);
  static BrowserAccessibility* NextNonDescendantInTreeOrder(
      const BrowserAccessibility* object);
  static BrowserAccessibility* PreviousInTreeOrder(
      const BrowserAccessibility* object,
      bool can_wrap_to_last_element);
  static BrowserAccessibility* NextTextOnlyObject(
      const BrowserAccessibility* object);
  static BrowserAccessibility* PreviousTextOnlyObject(
      const BrowserAccessibility* object);

  // If the two objects provided have a common ancestor returns both the
  // common ancestor and the child indices of the two subtrees in which the
  // objects are located.
  // Returns false if a common ancestor cannot be found.
  static bool FindIndicesInCommonParent(const BrowserAccessibility& object1,
                                        const BrowserAccessibility& object2,
                                        BrowserAccessibility** common_parent,
                                        size_t* child_index1,
                                        size_t* child_index2);

  // Sets |out_is_before| to true if |object1| comes before |object2|
  // in tree order (pre-order traversal), and false if the objects are the
  // same or not in the same tree.
  static ax::mojom::TreeOrder CompareNodes(const BrowserAccessibility& object1,
                                           const BrowserAccessibility& object2);

  static std::vector<const BrowserAccessibility*> FindTextOnlyObjectsInRange(
      const BrowserAccessibility& start_object,
      const BrowserAccessibility& end_object);

  static std::u16string GetTextForRange(
      const BrowserAccessibility& start_object,
      const BrowserAccessibility& end_object);

  // If start and end offsets are greater than the text's length, returns all
  // the text.
  static std::u16string GetTextForRange(
      const BrowserAccessibility& start_object,
      int start_offset,
      const BrowserAccessibility& end_object,
      int end_offset);

  // DEPRECATED: Prefer using AXPlatformNodeDelegate bounds interfaces when
  // writing new code.
  static gfx::Rect GetRootFrameInnerTextRangeBoundsRect(
      const BrowserAccessibility& start_object,
      int start_offset,
      const BrowserAccessibility& end_object,
      int end_offset);

  // TODO(abrusher): Make this method non-virtual, or preferably remove
  // altogether. This method is temporarily virtual, because fuchsia has a
  // different path to retrieve the device scale factor. This is a temporary
  // measure while the flatland migration is in progress (fxbug.dev/90502).
  virtual void UpdateDeviceScaleFactor();

  float device_scale_factor() const;

  AXSerializableTree* ax_serializable_tree() const {
    return static_cast<AXSerializableTree*>(ax_tree());
  }

  // AXTreeObserver implementation.
  void OnNodeCreated(AXTree* tree, AXNode* node) override;
  void OnNodeReparented(AXTree* tree, AXNode* node) override;
  void OnAtomicUpdateStarting(
      AXTree* tree,
      const std::set<AXNodeID>& deleted_node_ids,
      const std::set<AXNodeID>& reparented_node_ids) override;
  void OnAtomicUpdateFinished(
      AXTree* tree,
      bool root_changed,
      const std::vector<AXTreeObserver::Change>& changes) override;

  // AXTreeManager overrides.
  AXNode* GetNode(const AXNodeID node_id) const override;
  void UpdateAttributesOnParent(AXNode* parent) override;

  // AXPlatformTreeManager overrides.
  AXPlatformNode* GetPlatformNodeFromTree(
      const AXNodeID node_id) const override;
  AXPlatformNode* GetPlatformNodeFromTree(const AXNode&) const override;
  AXPlatformNodeDelegate* RootDelegate() const override;

  AXPlatformTreeManagerDelegate* delegate() const { return delegate_; }

  // If this BrowserAccessibilityManager is a child frame or guest frame,
  // returns the BrowserAccessibilityManager from the root frame. The root frame
  // is the outermost frame, so this method will walk up to any parents (in the
  // case of subframes), any outer documents (e.g. fenced frame owners), and any
  // GuestViews. If the current frame is not connected to its parent frame yet,
  // or if it got disconnected after being reparented, return nullptr to
  // indicate that we don't have access to the manager of the root frame yet.
  BrowserAccessibilityManager* GetManagerForRootFrame() const;

  // Returns the `AXPlatformTreeManagerDelegate` from `GetRootManager`
  // above, or returns nullptr in case we don't have access to the root manager
  // yet.
  AXPlatformTreeManagerDelegate* GetDelegateFromRootManager() const;

  // Returns whether this is the root frame.
  bool IsRootFrameManager() const;

  // Get a snapshot of the current tree as an AXTreeUpdate.
  AXTreeUpdate SnapshotAXTreeForTesting();

  // Use a custom device scale factor for testing.
  void UseCustomDeviceScaleFactorForTesting(float device_scale_factor);

  // Given a point in physical pixel coordinates, trigger an asynchronous hit
  // test but return the best possible match instantly.
  BrowserAccessibility* CachingAsyncHitTest(
      const gfx::Point& physical_pixel_point) const;

  // Called in response to a hover event, caches the result for the next
  // call to CachingAsyncHitTest().
  void CacheHitTestResult(BrowserAccessibility* hit_test_result) const;

  // Updates the page scale factor for this frame.
  void SetPageScaleFactor(float page_scale_factor);

  // Returns the current page scale factor for this frame.
  float GetPageScaleFactor() const;

  // Builds a cache for hit testing an AXTree. Note that the structure is cache
  // from the last time this was called and must be updated if the underlying
  // AXTree is modified.
  void BuildAXTreeHitTestCache();

  // This is an approximate hit test that only uses the information in
  // the browser process to compute the correct result. It will not return
  // correct results in many cases of z-index, overflow, and absolute
  // positioning, so BrowserAccessibilityManager::CachingAsyncHitTest
  // should be used instead, which falls back on calling ApproximateHitTest
  // automatically. Note that if BuildAXTreeHitTestCache is called before this
  // method then BrowserAccessibilityManager::AXTreeHitTest will be used instead
  // of BrowserAccessibility::ApproximateHitTest.
  //
  // Note that unlike BrowserAccessibilityManager::CachingAsyncHitTest, this
  // method takes a parameter in Blink's definition of screen coordinates.
  // This is so that the scale factor is consistent with what we receive from
  // Blink and store in the AX tree.
  // Blink screen coordinates are 1:1 with physical pixels if use-zoom-for-dsf
  // is disabled; they're physical pixels divided by device scale factor if
  // use-zoom-for-dsf is disabled. For more information see:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  BrowserAccessibility* ApproximateHitTest(
      const gfx::Point& blink_screen_point) const;

  // Detaches this instance from its parent manager. Useful during
  // deconstruction.
  void DetachFromParentManager();

  // Wrapper for converting the AXNode* returned by RetargetForEvents
  // to a BrowserAccessibility*. This is often needed.
  BrowserAccessibility* RetargetBrowserAccessibilityForEvents(
      BrowserAccessibility* node,
      RetargetEventType type) const;

  // Returns the unique identifier for `node` for exposure to the native
  // platform.
  AXPlatformNodeId GetNodeUniqueId(const BrowserAccessibility* node);

 protected:
  FRIEND_TEST_ALL_PREFIXES(content::BrowserAccessibilityManagerTest,
                           TestShouldFireEventForNode);

  explicit BrowserAccessibilityManager(AXNodeIdDelegate& node_id_delegate,
                                       AXPlatformTreeManagerDelegate* delegate);

  // Send platform-specific notifications to each of these objects that
  // their location has changed. This is called by OnLocationChanges
  // after it's updated the internal data structure.
  virtual void SendLocationChangeEvents(
      const std::vector<AXLocationChange>& changes);

  // Given the data from an atomic update, collect the nodes that need updating
  // assuming that this platform is one where plain text node content is
  // directly included in parents' hypertext.
  void CollectChangedNodesAndParentsForAtomicUpdate(
      AXTree* tree,
      const std::vector<AXTreeObserver::Change>& changes,
      std::set<AXPlatformNode*>* nodes_needing_update);

  bool ShouldFireEventForNode(BrowserAccessibility* node) const;

  virtual std::unique_ptr<BrowserAccessibility> CreateBrowserAccessibility(
      AXNode* node);

  // An object that can retrieve information or perform actions on our behalf,
  // based on which layer this code is running on, Web vs. Views.
  raw_ptr<AXPlatformTreeManagerDelegate> delegate_;

  // A mapping from a node id to its wrapper of type BrowserAccessibility.
  // This is different from the map in AXTree, which does not contain extra mac
  // nodes from AXTableInfo.
  // TODO(accessibility) Find a way to have a single map for both, perhaps by
  // having BrowserAccessibility into a subclass of AXNode.
  std::map<AXNodeID, std::unique_ptr<BrowserAccessibility>> id_wrapper_map_;

  // True if the user has initiated a navigation to another page.
  bool user_is_navigating_away_;

  // If the load complete event is suppressed due to CanFireEvents() returning
  // false, this is set to true and the event will be fired later.
  bool defer_load_complete_event_ = false;

  // If the load complete has been received in a previous serialization, this
  // is set to true.
  bool is_post_load_ = false;

  BrowserAccessibilityFindInPageInfo find_in_page_info_;

  // These cache the AX tree ID, node ID, and global screen bounds of the
  // last object found by an asynchronous hit test. Subsequent hit test
  // requests that remain within this object's bounds will return the same
  // object, but will also trigger a new asynchronous hit test request.
  mutable AXTreeID last_hover_ax_tree_id_;
  mutable int last_hover_node_id_;
  mutable gfx::Rect last_hover_bounds_;

  // The device scale factor for the view associated with this frame,
  // cached each time there's any update to the accessibility tree.
  float device_scale_factor_;

  // The page scale factor for the view associated with this frame,
  // cached when we get an update via SetPageScaleFactor().
  float page_scale_factor_ = 1.0f;

  // For testing only: If true, the manually-set device scale factor will be
  // used and it won't be updated from the delegate.
  bool use_custom_device_scale_factor_for_testing_;

  // Whether we should include or exclude the scroll offsets on the root
  // scroller when computing bounding rectangles. Usually true, but on
  // some platforms the root scroll offsets are handled separately.
  bool use_root_scroll_offsets_when_computing_bounds_ = true;

  // For testing only: A function to call when a generated event is fired.
  GeneratedEventCallbackForTesting generated_event_callback_for_testing_;

  // For testing only; A function to call when locations change.
  base::RepeatingClosure location_change_callback_for_testing_;

  // Keeps track of the nested popup root's id, if it exists. See GetPopupRoot()
  // for details.
  std::set<int32_t> popup_root_ids_;

  // Fire all events regardless of focus and with no delay, to avoid test
  // flakiness. See NeverSuppressOrDelayEventsForTesting() for details.
  static bool never_suppress_or_delay_events_for_testing_;

  // For debug only: True when handling OnAccessibilityEvents.
#if DCHECK_IS_ON()
  bool in_on_accessibility_events_ = false;
#endif  // DCHECK_IS_ON()

 private:
  void BuildAXTreeHitTestCacheInternal(
      const BrowserAccessibility* node,
      std::vector<const BrowserAccessibility*>* storage);

  // This overrides `AXTreeManager::GetParentManager` only to add DCHECKs that
  // validate the following assumptions:
  // 1. If this BrowserAccessibilityManager is a child frame or guest frame,
  // returns the BrowserAccessibilityManager from the parent document in the
  // frame tree.
  // 2. If the current frame is not connected to its parent frame yet, or if it
  // got disconnected after being reparented, return nullptr to indicate that we
  // don't have access to the parent manager yet.
  AXTreeManager* GetParentManager() const override;

  // Performs hit testing on the AXTree using the cache from
  // BuildAXTreeHitTestCache. This requires BuildAXTreeHitTestCache to be
  // called first.
  BrowserAccessibility* AXTreeHitTest(
      const gfx::Point& blink_screen_point) const;

  // A delegate responsible for assigning window-unique identifiers for nodes.
  const raw_ref<AXNodeIdDelegate> node_id_delegate_;

  // Only used on the root node for AXTree hit testing as an alternative to
  // ApproximateHitTest when used without a renderer.
  std::unique_ptr<cc::RTree<AXNodeID>> cached_node_rtree_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_H_
