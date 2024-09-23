// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/controls/menu/menu_closure_animation_mac.h"
#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"
#endif

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace ui {
class MouseEvent;
class OSExchangeData;
struct OwnedWindowAnchor;
}  // namespace ui

namespace views {

class Button;
class MenuControllerTest;
class MenuHostRootView;
class MenuItemView;
class MenuPreTargetHandler;
class SubmenuView;
class View;
class ViewTracker;

namespace internal {
class MenuControllerDelegate;
class MenuRunnerImpl;
}  // namespace internal

namespace test {
class MenuControllerTestApi;
class MenuControllerUITest;
}  // namespace test

// MenuController -------------------------------------------------------------

// MenuController is used internally by the various menu classes to manage
// showing, selecting and drag/drop for menus. All relevant events are
// forwarded to the MenuController from SubmenuView and MenuHost.
class VIEWS_EXPORT MenuController final : public gfx::AnimationDelegate,
                                          public WidgetObserver {
 public:
  // Enumeration of how the menu should exit.
  enum class ExitType {
    // Don't exit.
    kNone,

    // All menus, including nested, should be exited.
    kAll,

    // Only the outermost menu should be exited.
    kOutermost,

    // the menu is being closed as the result of one of the menus being
    // destroyed.
    kDestroyed
  };

  // Types of comboboxes.
  enum class ComboboxType {
    kNone,
    kEditable,
    kReadonly,
  };

  // The direction in which a menu opens relative to its anchor.
  enum class MenuOpenDirection {
    kLeading,
    kTrailing,
  };

  // Callback that is used to pass events to an "annotation" bubble or widget,
  // such as a help bubble, that floats alongside the menu and acts as part of
  // the menu for event-handling purposes. These require special handling
  // because menus never actually become active, and activating any other widget
  // causes the menu to close, so this handler should in most cases not activate
  // the annotation widget or otherwise close the menu.
  //
  // Not all events will be forwarded, but mouse clicks, taps, and hover/mouse
  // move events will be. `event.root_location()` will be the screen coordinates
  // of the event; `event.location()` is relative to the menu and can safely be
  // ignored.
  //
  // Returns true if `event` is handled by the annotation and should not be
  // processed by the menu (except for purposes of e.g. hot-tracking).
  using AnnotationCallback =
      base::RepeatingCallback<bool(const ui::LocatedEvent& event)>;

  // If a menu is currently active, this returns the controller for it.
  static MenuController* GetActiveInstance();

  MenuController(const MenuController&) = delete;
  MenuController& operator=(const MenuController&) = delete;

  // Runs the menu at the specified location.
  void Run(Widget* parent,
           MenuButtonController* button_controller,
           MenuItemView* root,
           const gfx::Rect& anchor_bounds,
           MenuAnchorPosition position,
           bool context_menu,
           bool is_nested_drag,
           gfx::NativeView native_view_for_gestures = gfx::NativeView());

  bool for_drop() const { return for_drop_; }

  bool in_nested_run() const { return !menu_stack_.empty(); }

  // Whether or not drag operation is in progress.
  bool drag_in_progress() const { return drag_in_progress_; }

  // Whether the MenuController initiated the drag in progress. False if there
  // is no drag in progress.
  bool did_initiate_drag() const { return did_initiate_drag_; }

  bool send_gesture_events_to_owner() const {
    return send_gesture_events_to_owner_;
  }

  void set_send_gesture_events_to_owner(bool send_gesture_events_to_owner) {
    send_gesture_events_to_owner_ = send_gesture_events_to_owner;
  }

  // Returns the owner of child windows.
  // WARNING: this may be NULL.
  Widget* owner() { return owner_; }

  // Gets the most-current selected menu item, if any, including if the
  // selection has not been committed yet.
  views::MenuItemView* GetSelectedMenuItem() { return pending_state_.item; }

  // Selects a menu-item and opens its sub-menu (if one exists) if not already
  // so. Clears any selections within the submenu if it is already open.
  void SelectItemAndOpenSubmenu(MenuItemView* item);

  // Gets the anchor position which is used to show this menu.
  MenuAnchorPosition GetAnchorPosition() { return state_.anchor; }

  // Cancels the current Run. See ExitType for a description of what happens
  // with the various parameters.
  void Cancel(ExitType type);

  // When is_nested_run() this will add a delegate to the stack. The most recent
  // delegate will be notified. It will be removed upon the exiting of the
  // nested menu. Ownership is not taken.
  void AddNestedDelegate(internal::MenuControllerDelegate* delegate);

  // Returns the current exit type. This returns a value other than
  // ExitType::kNone if the menu is being canceled.
  ExitType exit_type() const { return exit_type_; }

  // Returns the time from the event which closed the menu - or 0.
  base::TimeTicks closing_event_time() const { return closing_event_time_; }

  // Set/Get combobox type.
  void set_combobox_type(ComboboxType combobox_type) {
    combobox_type_ = combobox_type;
  }
  bool IsCombobox() const;
  bool IsEditableCombobox() const;
  bool IsReadonlyCombobox() const;

  bool IsContextMenu() const;

  // Various events, forwarded from the submenu.
  //
  // NOTE: the coordinates of the events are in that of the
  // MenuScrollViewContainer.
  bool OnMousePressed(SubmenuView* source, const ui::MouseEvent& event);
  bool OnMouseDragged(SubmenuView* source, const ui::MouseEvent& event);
  void OnMouseReleased(SubmenuView* source, const ui::MouseEvent& event);
  void OnMouseMoved(SubmenuView* source, const ui::MouseEvent& event);
  void OnMouseEntered(SubmenuView* source, const ui::MouseEvent& event);
  bool OnMouseWheel(SubmenuView* source, const ui::MouseWheelEvent& event);
  void OnGestureEvent(SubmenuView* source, ui::GestureEvent* event);
  void OnTouchEvent(SubmenuView* source, ui::TouchEvent* event);
  View* GetTooltipHandlerForPoint(SubmenuView* source, const gfx::Point& point);
  void ViewHierarchyChanged(SubmenuView* source,
                            const ViewHierarchyChangedDetails& details);

  bool GetDropFormats(SubmenuView* source,
                      int* formats,
                      std::set<ui::ClipboardFormatType>* format_types);
  bool AreDropTypesRequired(SubmenuView* source);
  bool CanDrop(SubmenuView* source, const ui::OSExchangeData& data);
  void OnDragEntered(SubmenuView* source, const ui::DropTargetEvent& event);
  int OnDragUpdated(SubmenuView* source, const ui::DropTargetEvent& event);
  void OnDragExited(SubmenuView* source);
  views::View::DropCallback GetDropCallback(SubmenuView* source,
                                            const ui::DropTargetEvent& event);

  // Invoked from the scroll buttons of the MenuScrollViewContainer.
  void OnDragEnteredScrollButton(SubmenuView* source, bool is_up);
  void OnDragExitedScrollButton(SubmenuView* source);

  // Called by the MenuHost when a drag is about to start on a child view.
  // This could be initiated by one of our MenuItemViews, or could be through
  // another child View.
  void OnDragWillStart();

  // Called by the MenuHost when the drag has completed. |should_close|
  // corresponds to whether or not the menu should close.
  void OnDragComplete(bool should_close);

  // Called while dispatching messages to intercept key events.
  // Returns ui::POST_DISPATCH_NONE if the event was swallowed by the menu.
  ui::PostDispatchAction OnWillDispatchKeyEvent(ui::KeyEvent* event);

  // Update the submenu's selection based on the current mouse location
  void UpdateSubmenuSelection(SubmenuView* source);

  // WidgetObserver overrides:
  void OnWidgetDestroying(Widget* widget) override;
  void OnWidgetShowStateChanged(Widget* widget) override;

  // Only used for testing.
  bool IsCancelAllTimerRunningForTest();

  // Only used for testing. Clears |state_| and |pending_state_| without
  // notifying any menu items.
  void ClearStateForTest();

  // Only used for testing.
  static void TurnOffMenuSelectionHoldForTest();

  void set_use_ash_system_ui_layout(bool value) {
    use_ash_system_ui_layout_ = value;
  }
  bool use_ash_system_ui_layout() const { return use_ash_system_ui_layout_; }

  // The rounded corners of the context menu.
  std::optional<gfx::RoundedCornersF> rounded_corners() const {
    return rounded_corners_;
  }

  // Returns the separator color ID according to the menu layout type.
  ui::ColorId GetSeparatorColorId() const;

  // Notifies |this| that |menu_item| is being destroyed.
  void OnMenuItemDestroying(MenuItemView* menu_item);

  // Returns whether this menu can handle input events right now. This method
  // can return false while running animations.
  bool CanProcessInputEvents() const;

  // Gets the animation used for menu item alerts. The returned pointer lives as
  // long as the MenuController.
  const gfx::Animation* GetAlertAnimation() const { return &alert_animation_; }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Sets the customized rounded corners of the context menu.
  void SetMenuRoundedCorners(std::optional<gfx::RoundedCornersF> corners);

  // Adds an annotation event handler. The subscription should be discarded when
  // the calling code no longer wants to intercept events for the annotation. It
  // is safe to discard the handle after the menu controller has been destroyed.
  base::CallbackListSubscription AddAnnotationCallback(
      AnnotationCallback callback);

  void SetShowMenuHostDurationHistogram(std::optional<std::string> histogram) {
    show_menu_host_duration_histogram_ = std::move(histogram);
  }

  std::optional<std::string> TakeShowMenuHostDurationHistogram() {
    std::optional<std::string> value =
        std::move(show_menu_host_duration_histogram_);
    show_menu_host_duration_histogram_.reset();
    return value;
  }

  base::WeakPtr<MenuController> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class internal::MenuRunnerImpl;
  friend class MenuControllerTest;
  friend class MenuHostRootView;
  friend class MenuItemView;
  friend class SubmenuView;
  friend class test::MenuControllerTestApi;
  friend class test::MenuControllerUITest;

  struct MenuPart;

  class MenuScrollTask;

  struct SelectByCharDetails;

  // Values supplied to SetSelection.
  enum SetSelectionTypes {
    SELECTION_DEFAULT = 0,

    // If set submenus are opened immediately, otherwise submenus are only
    // opened after a timer fires.
    SELECTION_UPDATE_IMMEDIATELY = 1 << 0,

    // If set and the menu_item has a submenu, the submenu is shown.
    SELECTION_OPEN_SUBMENU = 1 << 1,

    // SetSelection is being invoked as the result exiting or cancelling the
    // menu. This is used for debugging.
    SELECTION_EXIT = 1 << 2,
  };

  // Direction for IncrementSelection and FindInitialSelectableMenuItem.
  enum SelectionIncrementDirectionType {
    // Navigate the menu up.
    INCREMENT_SELECTION_UP,

    // Navigate the menu down.
    INCREMENT_SELECTION_DOWN,
  };

  // Tracks selection information.
  struct State {
    State();
    State(const State& other);
    ~State();

    // The selected menu item.
    raw_ptr<MenuItemView, DanglingUntriaged> item = nullptr;

    // Used to capture a hot tracked child button when a nested menu is opened
    // and to restore the hot tracked state when exiting a nested menu.
    raw_ptr<Button> hot_button = nullptr;

    // If item has a submenu this indicates if the submenu is showing.
    bool submenu_open = false;

    // Bounds passed to the run menu. Used for positioning the first menu.
    gfx::Rect initial_bounds;

    // Position of the initial menu.
    MenuAnchorPosition anchor = MenuAnchorPosition::kTopLeft;

    // Bounds for the monitor we're showing on.
    gfx::Rect monitor_bounds;

    // Is the current menu a context menu.
    bool context_menu = false;
  };

  // Sets the selection to |menu_item|. A value of NULL unselects
  // everything. |types| is a bitmask of |SetSelectionTypes|.
  //
  // Internally this updates pending_state_ immediately. state_ is only updated
  // immediately if SELECTION_UPDATE_IMMEDIATELY is set. If
  // SELECTION_UPDATE_IMMEDIATELY is not set CommitPendingSelection is invoked
  // to show/hide submenus and update state_.
  void SetSelection(MenuItemView* menu_item, int types);

  void SetSelectionOnPointerDown(SubmenuView* source,
                                 const ui::LocatedEvent* event);
  void StartDrag(SubmenuView* source, const gfx::Point& location);

  // Returns true if OnKeyPressed handled the key |event|.
  bool OnKeyPressed(const ui::KeyEvent& event);

  // Creates a MenuController. See |for_drop_| member for details on |for_drop|.
  MenuController(bool for_drop, internal::MenuControllerDelegate* delegate);

  ~MenuController() override;

  // Invokes AcceleratorPressed() on the hot tracked view if there is one.
  // Returns true if AcceleratorPressed() was invoked. |event_flags| is the
  // flags of the received key event.
  bool SendAcceleratorToHotTrackedView(int event_flags);

  void UpdateInitialLocation(const gfx::Rect& anchor_bounds,
                             MenuAnchorPosition position,
                             bool context_menu);

  // Returns the anchor position adjusted for RTL languages. For example,
  // in RTL MenuAnchorPosition::kBubbleLeft is mapped to kBubbleRight.
  static MenuAnchorPosition AdjustAnchorPositionForRtl(
      MenuAnchorPosition position);

  // Invoked when the user accepts the selected item. This is only used
  // when blocking. This schedules the loop to quit.
  void Accept(MenuItemView* item, int event_flags);
  void ReallyAccept();

  bool ShowSiblingMenu(SubmenuView* source, const gfx::Point& mouse_location);

  // Shows a context menu for |menu_item| as a result of an event if
  // appropriate, using the given |screen_location|. This is invoked on long
  // press, releasing the right mouse button, and pressing the "app" key.
  // Returns whether a context menu was shown.
  bool ShowContextMenu(MenuItemView* menu_item,
                       const gfx::Point& screen_location,
                       ui::MenuSourceType source_type);

  // Closes all menus, including any menus of nested invocations of Run.
  void CloseAllNestedMenus();

  // Gets the enabled menu item at the specified location.
  // If over_any_menu is non-null it is set to indicate whether the location
  // is over any menu. It is possible for this to return NULL, but
  // over_any_menu to be true. For example, the user clicked on a separator.
  MenuItemView* GetMenuItemAt(View* menu, const gfx::Point& location);

  // Returns the target for the mouse event. The coordinates are in terms of
  // source's scroll view container.
  MenuPart GetMenuPart(SubmenuView* source, const gfx::Point& source_loc);

  // Returns the target for mouse events. The search is done through |item| and
  // all its parents.
  MenuPart GetMenuPartByScreenCoordinateUsingMenu(MenuItemView* item,
                                                  const gfx::Point& screen_loc);

  // Implementation of GetMenuPartByScreenCoordinate for a single menu. Returns
  // true if the supplied SubmenuView contains the location in terms of the
  // screen. If it does, part is set appropriately and true is returned.
  bool GetMenuPartByScreenCoordinateImpl(SubmenuView* menu,
                                         const gfx::Point& screen_loc,
                                         MenuPart* part);

  // Returns the RootView of the target for the mouse event, if there is a
  // target at |source_loc|.
  MenuHostRootView* GetRootView(SubmenuView* source,
                                const gfx::Point& source_loc);

  // Returns whether the location is over the ACTIONABLE_SUBMENU's submenu area.
  bool IsLocationOverSubmenuAreaOfActionableSubmenu(
      MenuItemView* item,
      const gfx::Point& screen_loc) const;

  // Opens/Closes the necessary menus such that state_ matches that of
  // pending_state_. This is invoked if submenus are not opened immediately,
  // but after a delay.
  void CommitPendingSelection();

  // If item has a submenu, it is closed. This does NOT update the selection
  // in anyway.
  void CloseMenu(MenuItemView* item);

  // If item has a submenu, it is opened. This does NOT update the selection
  // in anyway.
  void OpenMenu(MenuItemView* item);

  // Implementation of OpenMenu. If |show| is true, this invokes show on the
  // menu, otherwise Reposition is invoked.
  void OpenMenuImpl(MenuItemView* item, bool show);

  // Invoked when the children of a menu change and the menu is showing.
  // This closes any submenus and resizes the submenu.
  void MenuChildrenChanged(MenuItemView* item);

  // Builds the paths of the two menu items into the two paths, and
  // sets first_diff_at to the location of the first difference between the
  // two paths.
  void BuildPathsAndCalculateDiff(MenuItemView* old_item,
                                  MenuItemView* new_item,
                                  std::vector<MenuItemView*>* old_path,
                                  std::vector<MenuItemView*>* new_path,
                                  size_t* first_diff_at);

  // Builds the path for the specified item.
  void BuildMenuItemPath(MenuItemView* item, std::vector<MenuItemView*>* path);

  // Starts/stops the timer that commits the pending state to state
  // (opens/closes submenus).
  void StartShowTimer();
  void StopShowTimer();

  // Starts/stops the timer cancel the menu. This is used during drag and
  // drop when the drop enters/exits the menu.
  void StartCancelAllTimer();
  void StopCancelAllTimer();

  // Calculates the bounds of the menu to show. `resulting_direction` is set to
  // match the direction the menu opened in. Also calculates anchor that system
  // compositor can use to position the menu. Resulting menu bounds, and bounds
  // set for the `anchor` are in screen coordinates.
  gfx::Rect CalculateMenuBounds(MenuItemView* item,
                                MenuOpenDirection preferred_open_direction,
                                MenuOpenDirection* resulting_direction,
                                ui::OwnedWindowAnchor* anchor);

  // Calculates the bubble bounds of the menu to show. `resulting_direction` is
  // set to match the direction the menu opened in. Also calculates anchor that
  // system compositor can use to position the menu.
  // TODO(msisov): anchor.anchor_rect equals to returned rect at the moment as
  // bubble menu bounds are used only by ash, as its backend uses menu bounds
  // instead of anchor for positioning.
  gfx::Rect CalculateBubbleMenuBounds(
      MenuItemView* item,
      MenuOpenDirection preferred_open_direction,
      MenuOpenDirection* resulting_direction,
      ui::OwnedWindowAnchor* anchor);

  // Returns the depth of the menu.
  static size_t MenuDepth(MenuItemView* item);

  // Selects the next or previous (depending on |direction|) menu item.
  void IncrementSelection(SelectionIncrementDirectionType direction);

  // Sets up accessible indices for menu items based on up/down arrow selection
  // logic, to be used by screen readers to give accurate "item X of Y"
  // information (and to be consistent with accessible keyboard use).
  //
  // This only sets one level of menu, so it must be called when submenus are
  // opened as well.
  void SetSelectionIndices(MenuItemView* parent);

  // Selects the first or last (depending on |direction|) menu item.
  void MoveSelectionToFirstOrLastItem(
      SelectionIncrementDirectionType direction);

  // Returns the first (|direction| == NAVIGATE_SELECTION_DOWN) or the last
  // (|direction| == INCREMENT_SELECTION_UP) selectable child menu item of
  // |parent|. If there are no selectable items returns NULL.
  MenuItemView* FindInitialSelectableMenuItem(
      MenuItemView* parent,
      SelectionIncrementDirectionType direction);

  // If the selected item has a submenu and it isn't currently open, the
  // the selection is changed such that the menu opens immediately.
  void OpenSubmenuChangeSelectionIfCan();

  // If possible, closes the submenu.
  void CloseSubmenu();

  // Returns details about which menu items match the mnemonic |key|.
  // |match_function| is used to determine which menus match.
  SelectByCharDetails FindChildForMnemonic(
      MenuItemView* parent,
      char16_t key,
      bool (*match_function)(MenuItemView* menu, char16_t mnemonic));

  // Selects or accepts the appropriate menu item based on |details|.
  void AcceptOrSelect(MenuItemView* parent, const SelectByCharDetails& details);

  // Selects by mnemonic, and if that doesn't work tries the first character of
  // the title.
  void SelectByChar(char16_t key);

  // For Windows and Aura we repost an event which dismisses the |source| menu.
  // The menu may also be canceled depending on the target of the event. |event|
  // is then processed without the menu present. On non-aura Windows, a new
  // mouse event is generated and posted to the window (if there is one) at the
  // location of the event. On aura, the event is reposted on the RootWindow.
  void RepostEventAndCancel(SubmenuView* source, const ui::LocatedEvent* event);

  // Sets the drop target to new_item.
  void SetDropMenuItem(MenuItemView* new_item,
                       MenuDelegate::DropPosition position);

  // Starts/stops scrolling as appropriate. part gives the part the mouse is
  // over.
  void UpdateScrolling(const MenuPart& part);

  // Stops scrolling.
  void StopScrollingViaButton();

  // Updates active mouse view from the location of the event and sends it
  // the appropriate events. This is used to send mouse events to child views so
  // that they react to click-drag-release as if the user clicked on the view
  // itself.
  void UpdateActiveMouseView(SubmenuView* event_source,
                             const ui::MouseEvent& event,
                             View* target_menu);

  // Sends a mouse release event to the current active mouse view and sets
  // it to null.
  void SendMouseReleaseToActiveView(SubmenuView* event_source,
                                    const ui::MouseEvent& event);

  // Sends a mouse capture lost event to the current active mouse view and sets
  // it to null.
  void SendMouseCaptureLostToActiveView();

  // Sets exit type. Calling this can terminate the active nested message-loop.
  void SetExitType(ExitType type);

  // Performs the teardown of menus. This will notify the |delegate_|. If
  // |exit_type_| is ExitType::kAll all nested runs will be exited.
  void ExitMenu();

  // Performs the teardown of the menu launched by Run(). The selected item is
  // returned.
  MenuItemView* ExitTopMostMenu();

  // Handles the mouse location event on the submenu |source|.
  void HandleMouseLocation(SubmenuView* source,
                           const gfx::Point& mouse_location);

  // Sets hot-tracked state to the first focusable descendant view of |item|.
  void SetInitialHotTrackedView(MenuItemView* item,
                                SelectionIncrementDirectionType direction);

  // Sets hot-tracked state to the next focusable element after |item| in
  // |direction|.
  void SetNextHotTrackedView(MenuItemView* item,
                             SelectionIncrementDirectionType direction);

  // Updates the current |hot_button_| and its hot tracked state.
  void SetHotTrackedButton(Button* new_hot_button);

  // Returns whether typing a new character will continue the existing prefix
  // selection. If this returns false, typing a new character will start a new
  // prefix selection, and some characters (such as Space) will be treated as
  // commands instead of parts of the prefix.
  bool ShouldContinuePrefixSelection() const;

  // Manage alerted MenuItemViews that we are animating.
  void RegisterAlertedItem(MenuItemView* item);
  void UnregisterAlertedItem(MenuItemView* item);

  // Sets anchor position, gravity and constraints for the |item|.
  void SetAnchorParametersForItem(MenuItemView* item,
                                  const gfx::Point& item_loc,
                                  ui::OwnedWindowAnchor* anchor);

  // Possibly forwards the specified `event` to an annotation callback, if one
  // is present, and returns the result (default false if no callback is set).
  bool MaybeForwardToAnnotation(SubmenuView* source,
                                const ui::LocatedEvent& event);

  // Returns the direction in which the most recent child menu opened for a menu
  // at the given `depth`.
  MenuOpenDirection GetChildMenuOpenDirectionAtDepth(size_t depth) const;

  // Updates the direction that a child menu opened in for a menu at `depth`.
  void SetChildMenuOpenDirectionAtDepth(size_t depth,
                                        MenuOpenDirection direction);

  // The active instance.
  static MenuController* active_instance_;

  // If true the menu is shown for a drag and drop. Note that the semantics for
  // drag and drop are slightly different: cancel timer is kicked off any time
  // the drag moves outside the menu, mouse events do nothing...
  const bool for_drop_;

  // If true, we're showing.
  bool showing_ = false;

  // Indicates what to exit.
  ExitType exit_type_ = ExitType::kNone;

  // Whether we did a capture. We do a capture only if we're blocking and
  // the mouse was down when Run.
  bool did_capture_ = false;

  // As the user drags the mouse around pending_state_ changes immediately.
  // When the user stops moving/dragging the mouse (or clicks the mouse)
  // pending_state_ is committed to state_, potentially resulting in
  // opening or closing submenus. This gives a slight delayed effect to
  // submenus as the user moves the mouse around. This is done so that as the
  // user moves the mouse all submenus don't immediately pop.
  State pending_state_;
  State state_;

  // The direction in which the most recent child menu opened for a menu at a
  // given depth. The direction for a menu at depth N is stored at position N-1.
  // This information is used as a hint when computing child menu bounds. The
  // intention is to have child menus at a given depth open in the same
  // direction if possible.
  std::vector<MenuOpenDirection> child_menu_open_direction_;

  // If the user accepted the selection, this is the result.
  raw_ptr<MenuItemView> result_ = nullptr;

  // The event flags when the user selected the menu.
  int accept_event_flags_ = 0;

  // If not empty, it means we're nested. When Run is invoked from within
  // Run, the current state (state_) is pushed onto menu_stack_. This allows
  // MenuController to restore the state when the nested run returns.
  using NestedState =
      std::pair<State, std::unique_ptr<MenuButtonController::PressedLock>>;
  std::list<NestedState> menu_stack_;

  // When Run is invoked during an active Run, it may be called from a separate
  // MenuControllerDelegate. If not empty it means we are nested, and the
  // stacked delegates should be notified instead of |delegate_|.
  std::list<raw_ptr<internal::MenuControllerDelegate, CtnExperimental>>
      delegate_stack_;

  // As the mouse moves around submenus are not opened immediately. Instead
  // they open after this timer fires.
  base::OneShotTimer show_timer_;

  // Used to invoke CancelAll(). This is used during drag and drop to hide the
  // menu after the mouse moves out of the of the menu. This is necessitated by
  // the lack of an ability to detect when the drag has completed from the drop
  // side.
  base::OneShotTimer cancel_all_timer_;

  // Drop target.
  raw_ptr<MenuItemView> drop_target_ = nullptr;
  MenuDelegate::DropPosition drop_position_ =
      MenuDelegate::DropPosition::kUnknow;

  // Owner of child windows.
  // WARNING: this may be NULL.
  raw_ptr<Widget> owner_ = nullptr;

  // An optional NativeView to which gestures will be forwarded to if
  // RunType::SEND_GESTURE_EVENTS_TO_OWNER is set.
  gfx::NativeView native_view_for_gestures_ = gfx::NativeView();

  // Indicates a possible drag operation.
  bool possible_drag_ = false;

  // True when drag operation is in progress.
  bool drag_in_progress_ = false;

  // True when the drag operation in progress was initiated by the
  // MenuController for a child MenuItemView (as opposed to initiated separately
  // by a child View).
  bool did_initiate_drag_ = false;

  // Location the mouse was pressed at. Used to detect d&d.
  gfx::Point press_pt_;

  // We get a slew of drag updated messages as the mouse is over us. To avoid
  // continually processing whether we can drop, we cache the coordinates.
  bool valid_drop_coordinates_ = false;
  gfx::Point drop_pt_;
  int last_drop_operation_ = ui::DragDropTypes::DRAG_NONE;

  // If true, we're in the middle of invoking ShowAt on a submenu.
  bool showing_submenu_ = false;

  // Task for scrolling the menu via scroll button. If non-null, indicates a
  // scroll is currently underway.
  std::unique_ptr<MenuScrollTask> scroll_task_;

  // The lock to keep the menu button pressed while a menu is visible.
  std::unique_ptr<MenuButtonController::PressedLock> pressed_lock_;

  // ViewTracker used to store the View mouse drag events are forwarded to. See
  // UpdateActiveMouseView() for details.
  std::unique_ptr<ViewTracker> active_mouse_view_tracker_;

  // Current hot tracked child button if any.
  raw_ptr<Button> hot_button_ = nullptr;

  raw_ptr<internal::MenuControllerDelegate> delegate_;

  // The timestamp of the event which closed the menu - or 0 otherwise.
  base::TimeTicks closing_event_time_;

  // Time when the menu is first shown.
  base::TimeTicks menu_start_time_;

  // If a mouse press triggered this menu, this will have its location (in
  // screen coordinates). Otherwise this will be (0, 0).
  gfx::Point menu_start_mouse_press_loc_;

  // Set to the location, relative to the root menu item's widget, of the mouse
  // cursor if any submenu is opened while the cursor is over that menu. This is
  // used to ignore mouse move events triggered by the menu opening, to avoid
  // auto-selecting the menu item under the mouse.
  std::optional<gfx::Point> menu_open_mouse_loc_;

  // Controls behavior differences between a combobox and other types of menu
  // (like a context menu).
  ComboboxType combobox_type_ = ComboboxType::kNone;

  // Whether the menu |owner_| needs gesture events. When set to true, the menu
  // will preserve the gesture events of the |owner_| and MenuController will
  // forward the gesture events to |owner_| until no |EventType::kGestureEnd|
  // event is captured.
  bool send_gesture_events_to_owner_ = false;

  // Set to true if the menu item was selected by touch.
  bool item_selected_by_touch_ = false;

  // Whether to use the ash system UI specific layout.
  bool use_ash_system_ui_layout_ = false;

  // During mouse event handling, this is the RootView to forward mouse events
  // to. We need this, because if we forward one event to it (e.g., mouse
  // pressed), subsequent events (like dragging) should also go to it, even if
  // the mouse is no longer over the view.
  raw_ptr<MenuHostRootView> current_mouse_event_target_ = nullptr;

  // A mask of the EventFlags for the mouse buttons currently pressed.
  int current_mouse_pressed_state_ = 0;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<MenuClosureAnimationMac> menu_closure_animation_;
  std::unique_ptr<MenuCocoaWatcherMac> menu_cocoa_watcher_;
#endif

  std::unique_ptr<MenuPreTargetHandler> menu_pre_target_handler_;

  // Animation used for alerted MenuItemViews. Started on demand.
  gfx::ThrobAnimation alert_animation_;

  // Currently showing alerted menu items. Updated when submenus open and close.
  base::flat_set<raw_ptr<MenuItemView, CtnExperimental>> alerted_items_;

  // The rounded corners of the context menu.
  std::optional<gfx::RoundedCornersF> rounded_corners_ = std::nullopt;

  // The current annotation callbacks. Callbacks will be wrapped in such a way
  // that a callback list can be used, with the return value as an out
  // parameter. See `AnnotationCallback` for more information.
  base::RepeatingCallbackList<void(bool&, const ui::LocatedEvent& event)>
      annotation_callbacks_;

  // A histogram name for recording the time from menu host initialization to
  // its successful presentation
  std::optional<std::string> show_menu_host_duration_histogram_;

  base::WeakPtrFactory<MenuController> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
