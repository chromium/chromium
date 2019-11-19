// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_MACOSX)
#include "ui/views/controls/menu/menu_closure_animation_mac.h"
#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"
#endif

namespace ui {
class OSExchangeData;
}
namespace views {

class Button;
class MenuHostRootView;
class MenuItemView;
class MenuPreTargetHandler;
class MouseEvent;
class SubmenuView;
class View;
class ViewTracker;

namespace internal {
class MenuControllerDelegate;
class MenuRunnerImpl;
}

namespace test {
class MenuControllerTest;
class MenuControllerTestApi;
class MenuControllerUITest;
}

// MenuController -------------------------------------------------------------

// MenuController is used internally by the various menu classes to manage
// showing, selecting and drag/drop for menus. All relevant events are
// forwarded to the MenuController from SubmenuView and MenuHost.
class VIEWS_EXPORT MenuController
    : public base::SupportsWeakPtr<MenuController>,
      public gfx::AnimationDelegate,
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

  // If a menu is currently active, this returns the controller for it.
  static MenuController* GetActiveInstance();

  // Runs the menu at the specified location.
  void Run(Widget* parent,
           MenuButtonController* button_controller,
           MenuItemView* root,
           const gfx::Rect& bounds,
           MenuAnchorPosition position,
           bool context_menu,
           bool is_nested_drag);

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

  // Get the anchor position which is used to show this menu.
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
  int OnPerformDrop(SubmenuView* source, const ui::DropTargetEvent& event);

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

  // Only used for testing.
  bool IsCancelAllTimerRunningForTest();

  // Only used for testing. Clears |state_| and |pending_state_| without
  // notifying any menu items.
  void ClearStateForTest();

  // Only used for testing.
  static void TurnOffMenuSelectionHoldForTest();

  void set_use_touchable_layout(bool use_touchable_layout) {
    use_touchable_layout_ = use_touchable_layout;
  }
  bool use_touchable_layout() const { return use_touchable_layout_; }

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

 private:
  friend class internal::MenuRunnerImpl;
  friend class test::MenuControllerTest;
  friend class test::MenuControllerTestApi;
  friend class test::MenuControllerUITest;
  friend class MenuHostRootView;
  friend class MenuItemView;
  friend class SubmenuView;

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
    MenuItemView* item = nullptr;

    // Used to capture a hot tracked child button when a nested menu is opened
    // and to restore the hot tracked state when exiting a nested menu.
    Button* hot_button = nullptr;

    // If item has a submenu this indicates if the submenu is showing.
    bool submenu_open = false;

    // Bounds passed to the run menu. Used for positioning the first menu.
    gfx::Rect initial_bounds;

    // Position of the initial menu.
    MenuAnchorPosition anchor = MenuAnchorPosition::kTopLeft;

    // The direction child menus have opened in.
    std::list<bool> open_leading;

    // Bounds for the monitor we're showing on.
    gfx::Rect monitor_bounds;

    // Is the current menu a context menu.
    bool context_menu = false;
  };

  // Used by GetMenuPart to indicate the menu part at a particular location.
  struct MenuPart {
    // Type of part.
    enum Type {
      NONE,
      MENU_ITEM,
      SCROLL_UP,
      SCROLL_DOWN
    };

    // Convenience for testing type == SCROLL_DOWN or type == SCROLL_UP.
    bool is_scroll() const { return type == SCROLL_DOWN || type == SCROLL_UP; }

    // Type of part.
    Type type = NONE;

    // If type is MENU_ITEM, this is the menu item the mouse is over, otherwise
    // this is NULL.
    // NOTE: if type is MENU_ITEM and the mouse is not over a valid menu item
    //       but is over a menu (for example, the mouse is over a separator or
    //       empty menu), this is NULL and parent is the menu the mouse was
    //       clicked on.
    MenuItemView* menu = nullptr;

    // If type is MENU_ITEM but the mouse is not over a menu item this is the
    // parent of the menu item the user clicked on. Otherwise this is NULL.
    MenuItemView* parent = nullptr;

    // This is the submenu the mouse is over.
    SubmenuView* submenu = nullptr;

    // Whether the controller should apply SELECTION_OPEN_SUBMENU to this item.
    bool should_submenu_show = false;
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

  // Handles |key_code| as a keypress. Returns true if OnKeyPressed handled the
  // key code.
  bool OnKeyPressed(ui::KeyboardCode key_code);

  // Creates a MenuController. See |for_drop_| member for details on |for_drop|.
  MenuController(bool for_drop, internal::MenuControllerDelegate* delegate);

  ~MenuController() override;

  // Invokes AcceleratorPressed() on the hot tracked view if there is one.
  // Returns true if AcceleratorPressed() was invoked.
  bool SendAcceleratorToHotTrackedView();

  void UpdateInitialLocation(const gfx::Rect& bounds,
                             MenuAnchorPosition position,
                             bool context_menu);

  // Invoked when the user accepts the selected item. This is only used
  // when blocking. This schedules the loop to quit.
  void Accept(MenuItemView* item, int event_flags);
  void ReallyAccept(MenuItemView* item, int event_flags);

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
  MenuItemView* GetMenuItemAt(View* menu, int x, int y);

  // If there is an empty menu item at the specified location, it is returned.
  MenuItemView* GetEmptyMenuItemAt(View* source, int x, int y);

  // Returns true if the coordinate is over the scroll buttons of the
  // SubmenuView's MenuScrollViewContainer. If true is returned, part is set to
  // indicate which scroll button the coordinate is.
  bool IsScrollButtonAt(SubmenuView* source,
                        int x,
                        int y,
                        MenuPart::Type* part);

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

  // Converts the located event from |source|'s geometry to |dst|'s geometry,
  // iff the root view of source and dst differ.
  void ConvertLocatedEventForRootView(View* source,
                                      View* dst,
                                      ui::LocatedEvent* event);

  // Returns true if the SubmenuView contains the specified location. This does
  // NOT included the scroll buttons, only the submenu view.
  bool DoesSubmenuContainLocation(SubmenuView* submenu,
                                  const gfx::Point& screen_loc);

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

  // Calculates the bounds of the menu to show. is_leading is set to match the
  // direction the menu opened in.
  gfx::Rect CalculateMenuBounds(MenuItemView* item,
                                bool prefer_leading,
                                bool* is_leading);

  // Calculates the bubble bounds of the menu to show. is_leading is set to
  // match the direction the menu opened in.
  gfx::Rect CalculateBubbleMenuBounds(MenuItemView* item,
                                      bool prefer_leading,
                                      bool* is_leading);

  // Returns the depth of the menu.
  static int MenuDepth(MenuItemView* item);

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

  // Returns the next or previous selectable child menu item of |parent|
  // starting at |index| and incrementing or decrementing index by 1 depending
  // on |direction|. If there are no more selectable items NULL is returned.
  MenuItemView* FindNextSelectableMenuItem(
      MenuItemView* parent,
      int index,
      SelectionIncrementDirectionType direction,
      bool is_initial);

  // If the selected item has a submenu and it isn't currently open, the
  // the selection is changed such that the menu opens immediately.
  void OpenSubmenuChangeSelectionIfCan();

  // If possible, closes the submenu.
  void CloseSubmenu();

  // Returns details about which menu items match the mnemonic |key|.
  // |match_function| is used to determine which menus match.
  SelectByCharDetails FindChildForMnemonic(
      MenuItemView* parent,
      base::char16 key,
      bool (*match_function)(MenuItemView* menu, base::char16 mnemonic));

  // Selects or accepts the appropriate menu item based on |details|.
  void AcceptOrSelect(MenuItemView* parent, const SelectByCharDetails& details);

  // Selects by mnemonic, and if that doesn't work tries the first character of
  // the title.
  void SelectByChar(base::char16 key);

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
  void StopScrolling();

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
  void SetHotTrackedButton(Button* hot_button);

  // Returns whether typing a new character will continue the existing prefix
  // selection. If this returns false, typing a new character will start a new
  // prefix selection, and some characters (such as Space) will be treated as
  // commands instead of parts of the prefix.
  bool ShouldContinuePrefixSelection() const;

  // Manage alerted MenuItemViews that we are animating.
  void RegisterAlertedItem(MenuItemView* item);
  void UnregisterAlertedItem(MenuItemView* item);

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

  // If the user accepted the selection, this is the result.
  MenuItemView* result_ = nullptr;

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
  std::list<internal::MenuControllerDelegate*> delegate_stack_;

  // As the mouse moves around submenus are not opened immediately. Instead
  // they open after this timer fires.
  base::OneShotTimer show_timer_;

  // Used to invoke CancelAll(). This is used during drag and drop to hide the
  // menu after the mouse moves out of the of the menu. This is necessitated by
  // the lack of an ability to detect when the drag has completed from the drop
  // side.
  base::OneShotTimer cancel_all_timer_;

  // Drop target.
  MenuItemView* drop_target_ = nullptr;
  MenuDelegate::DropPosition drop_position_ =
      MenuDelegate::DropPosition::kUnknow;

  // Owner of child windows.
  // WARNING: this may be NULL.
  Widget* owner_ = nullptr;

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

  // Task for scrolling the menu. If non-null indicates a scroll is currently
  // underway.
  std::unique_ptr<MenuScrollTask> scroll_task_;

  // The lock to keep the menu button pressed while a menu is visible.
  std::unique_ptr<MenuButtonController::PressedLock> pressed_lock_;

  // ViewTracker used to store the View mouse drag events are forwarded to. See
  // UpdateActiveMouseView() for details.
  std::unique_ptr<ViewTracker> active_mouse_view_tracker_;

  // Current hot tracked child button if any.
  Button* hot_button_ = nullptr;

  internal::MenuControllerDelegate* delegate_;

  // The timestamp of the event which closed the menu - or 0 otherwise.
  base::TimeTicks closing_event_time_;

  // Time when the menu is first shown.
  base::TimeTicks menu_start_time_;

  // If a mouse press triggered this menu, this will have its location (in
  // screen coordinates). Otherwise this will be (0, 0).
  gfx::Point menu_start_mouse_press_loc_;

  // If the mouse was under the menu when the menu was run, this will have its
  // location. Otherwise it will be null. This is used to ignore mouse move
  // events triggered by the menu opening, to avoid selecting the menu item
  // over the mouse.
  base::Optional<gfx::Point> menu_open_mouse_loc_;

  // Controls behavior differences between a combobox and other types of menu
  // (like a context menu).
  ComboboxType combobox_type_ = ComboboxType::kNone;

  // Whether the menu |owner_| needs gesture events. When set to true, the menu
  // will preserve the gesture events of the |owner_| and MenuController will
  // forward the gesture events to |owner_| until no |ET_GESTURE_END| event is
  // captured.
  bool send_gesture_events_to_owner_ = false;

  // Set to true if the menu item was selected by touch.
  bool item_selected_by_touch_ = false;

  // Whether to use the touchable layout.
  bool use_touchable_layout_ = false;

  // During mouse event handling, this is the RootView to forward mouse events
  // to. We need this, because if we forward one event to it (e.g., mouse
  // pressed), subsequent events (like dragging) should also go to it, even if
  // the mouse is no longer over the view.
  MenuHostRootView* current_mouse_event_target_ = nullptr;

  // A mask of the EventFlags for the mouse buttons currently pressed.
  int current_mouse_pressed_state_ = 0;

#if defined(OS_MACOSX)
  std::unique_ptr<MenuClosureAnimationMac> menu_closure_animation_;
  std::unique_ptr<MenuCocoaWatcherMac> menu_cocoa_watcher_;
#endif

  std::unique_ptr<MenuPreTargetHandler> menu_pre_target_handler_;

  // Animation used for alerted MenuItemViews. Started on demand.
  gfx::ThrobAnimation alert_animation_;

  // Currently showing alerted menu items. Updated when submenus open and close.
  base::flat_set<MenuItemView*> alerted_items_;

  DISALLOW_COPY_AND_ASSIGN(MenuController);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
