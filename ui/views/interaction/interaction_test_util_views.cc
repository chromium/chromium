// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_views.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/menu/menu_host.h"
#include "ui/views/controls/menu/menu_host_root_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE) && \
    !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#define HANDLE_WAYLAND_FAILURE 1
#else
#define HANDLE_WAYLAND_FAILURE 0
#endif

#if HANDLE_WAYLAND_FAILURE
#include "ui/views/widget/widget_observer.h"
#endif

namespace views::test {

namespace {

#if HANDLE_WAYLAND_FAILURE

// On Wayland on Linux, window activation isn't guaranteed to work (based on
// what compositor extensions are installed). So instead, make a best effort to
// wait for the widget to activate, and if it fails, skip the test as it cannot
// possibly pass.
class WidgetActivationWaiterWayland final : public WidgetObserver {
 public:
  // A more than reasonable amount of time to wait for a window to activate.
  static constexpr base::TimeDelta kTimeout = base::Seconds(1);

  // Constructs an activation waiter for the given widget.
  explicit WidgetActivationWaiterWayland(Widget* widget)
      : active_(widget->IsActive()) {
    if (!active_) {
      widget_observation_.Observe(widget);
    }
  }
  ~WidgetActivationWaiterWayland() override = default;

  // Waits for the widget to become active or the operation to time out; returns
  // true on success. Returns immediately if the widget is already active.
  bool Wait() {
    if (!active_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&WidgetActivationWaiterWayland::OnTimeout,
                         weak_ptr_factory_.GetWeakPtr()),
          kTimeout);
      run_loop_.Run();
    }
    return active_;
  }

 private:
  // WidgetObserver:
  void OnWidgetDestroyed(Widget* widget) override {
    NOTREACHED() << "Widget destroyed before observation.";
  }
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (!active) {
      return;
    }
    active_ = true;
    widget_observation_.Reset();
    run_loop_.Quit();
  }

  void OnTimeout() {
    widget_observation_.Reset();
    run_loop_.Quit();
  }

  bool active_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
  base::WeakPtrFactory<WidgetActivationWaiterWayland> weak_ptr_factory_{this};
};

#endif  // HANDLE_WAYLAND_FAILURE

// Waits for the dropdown pop-up and selects the specified item from the list.
class DropdownItemSelector {
 public:
  // The owning `simulator` will be used to simulate a click on the
  // `item_index`-th drop-down menu item using `input_type`.
  DropdownItemSelector(InteractionTestUtilSimulatorViews* simulator,
                       ui::test::InteractionTestUtil::InputType input_type,
                       size_t item_index)
      : simulator_(simulator),
        input_type_(input_type),
        item_index_(item_index) {
    observer_.set_shown_callback(base::BindRepeating(
        &DropdownItemSelector::OnWidgetShown, weak_ptr_factory_.GetWeakPtr()));
    observer_.set_hidden_callback(base::BindRepeating(
        &DropdownItemSelector::OnWidgetHidden, weak_ptr_factory_.GetWeakPtr()));
  }
  DropdownItemSelector(const DropdownItemSelector&) = delete;
  void operator=(const DropdownItemSelector&) = delete;
  ~DropdownItemSelector() = default;

  // Synchronously waits for the drop-down to appear and selects the appropriate
  // item.
  void SelectItem() {
    CHECK(!run_loop_.running());
    CHECK(!result_.has_value());
    run_loop_.Run();
  }

  // Returns whether the operation succeeded or failed.
  ui::test::ActionResult result() const {
    return result_.value_or(ui::test::ActionResult::kFailed);
  }

 private:
  // Responds to a new widget being shown. The assumption is that this widget is
  // the combobox dropdown. If it is not, the follow-up call will fail.
  void OnWidgetShown(Widget* widget) {
    if (widget_ || result_.has_value()) {
      return;
    }

    widget_ = widget;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DropdownItemSelector::SelectItemImpl,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // Detects when a widget is hidden. Fails the operation if this was the drop-
  // down widget and the item has not yet been selected.
  void OnWidgetHidden(Widget* widget) {
    if (result_.has_value() || widget_ != widget) {
      return;
    }

    LOG(ERROR) << "Widget closed before selection took place.";
    SetResult(ui::test::ActionResult::kFailed);
  }

  // Actually finds and selects the item in the drop-down. If it is not present
  // or cannot be selected, fails the operation.
  void SelectItemImpl() {
    CHECK(widget_);
    CHECK(!result_.has_value());

    // Because this widget was just shown, it may not be laid out yet.
    widget_->LayoutRootViewIfNecessary();
    size_t index = item_index_;
    if (auto* const menu_item =
            FindMenuItem(widget_->GetContentsView(), index)) {
      // No longer tracking the widget. This will prevent synchronous widget
      // dismissed during SelectMenuItem() below from thinking it failed.
      widget_ = nullptr;

      // Try to select the item.
      const auto result = simulator_->SelectMenuItem(
          ElementTrackerViews::GetInstance()->GetElementForView(menu_item,
                                                                true),
          input_type_);
      SetResult(result);
      switch (result) {
        case ui::test::ActionResult::kFailed:
          LOG(ERROR) << "Unable to select dropdown menu item.";
          break;
        case ui::test::ActionResult::kNotAttempted:
          NOTREACHED();
        case ui::test::ActionResult::kKnownIncompatible:
          LOG(WARNING)
              << "Select dropdown item not available on this platform with "
                 "input type "
              << input_type_;
          break;
        case ui::test::ActionResult::kSucceeded:
          break;
      }
    } else {
      LOG(ERROR) << "Dropdown menu item not found.";
      SetResult(ui::test::ActionResult::kFailed);
    }
  }

  // Sets the result and aborts `run_loop_`. Should only ever be called once.
  void SetResult(ui::test::ActionResult result) {
    CHECK(!result_.has_value());
    result_ = result;
    widget_ = nullptr;
    weak_ptr_factory_.InvalidateWeakPtrs();
    run_loop_.Quit();
  }

  // Recursively search `from` for the `index`-th MenuItemView.
  //
  // Searches in-order, depth-first. It is assumed that menu items will appear
  // in search order in the same order they appear visually.
  static MenuItemView* FindMenuItem(View* from, size_t& index) {
    for (views::View* child : from->children()) {
      auto* const item = AsViewClass<MenuItemView>(child);
      if (item) {
        if (index == 0U)
          return item;
        --index;
      } else if (auto* result = FindMenuItem(child, index)) {
        return result;
      }
    }
    return nullptr;
  }

  const raw_ptr<InteractionTestUtilSimulatorViews> simulator_;
  const ui::test::InteractionTestUtil::InputType input_type_;
  const size_t item_index_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  AnyWidgetObserver observer_{views::test::AnyWidgetTestPasskey()};  // IN-TEST
  std::optional<ui::test::ActionResult> result_;
  raw_ptr<Widget> widget_ = nullptr;
  base::WeakPtrFactory<DropdownItemSelector> weak_ptr_factory_{this};
};

gfx::Point GetCenter(views::View* view) {
  return view->GetLocalBounds().CenterPoint();
}

bool SendDefaultAction(View* target) {
  ui::AXActionData action;
  action.action = ax::mojom::Action::kDoDefault;
  return target->HandleAccessibleAction(action);
}

// Sends a mouse click to the specified `target`.
// Views are EventHandlers but Widgets are not despite having the same API for
// event handling, so use a templated approach to support both cases.
template <class T>
void SendMouseClick(T* target, const gfx::Point& point) {
  ui::MouseEvent mouse_down(ui::EventType::kMousePressed, point, point,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseEvent(&mouse_down);
  ui::MouseEvent mouse_up(ui::EventType::kMouseReleased, point, point,
                          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseEvent(&mouse_up);
}

// Sends a tap gesture to the specified `target`.
// Views are EventHandlers but Widgets are not despite having the same API for
// event handling, so use a templated approach to support both cases.
template <class T>
void SendTapGesture(T* target, const gfx::Point& point) {
  ui::GestureEventDetails press_details(ui::EventType::kGestureTap);
  press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent press_event(point.x(), point.y(), ui::EF_NONE,
                               ui::EventTimeForNow(), press_details);
  target->OnGestureEvent(&press_event);

  ui::GestureEventDetails release_details(ui::EventType::kGestureEnd);
  release_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent release_event(point.x(), point.y(), ui::EF_NONE,
                                 ui::EventTimeForNow(), release_details);
  target->OnGestureEvent(&release_event);
}

// Sends a key press to the specified `target`. Returns true if the view is
// still valid after processing the keypress.
bool SendKeyPress(View* view, ui::KeyboardCode code, int flags = ui::EF_NONE) {
  ViewTracker tracker(view);
  view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, code, flags,
                                  ui::EventTimeForNow()));

  // Verify that the button is not destroyed after the key-down before trying
  // to send the key-up.
  if (!tracker.view())
    return false;

  tracker.view()->OnKeyReleased(ui::KeyEvent(ui::EventType::kKeyReleased, code,
                                             flags, ui::EventTimeForNow()));

  return tracker.view();
}

}  // namespace

InteractionTestUtilSimulatorViews::InteractionTestUtilSimulatorViews() =
    default;
InteractionTestUtilSimulatorViews::~InteractionTestUtilSimulatorViews() =
    default;

ui::test::ActionResult InteractionTestUtilSimulatorViews::PressButton(
    ui::TrackedElement* element,
    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;
  auto* const button =
      Button::AsButton(element->AsA<TrackedElementViews>()->view());
  if (!button)
    return ui::test::ActionResult::kNotAttempted;

  PressButton(button, input_type);
  return ui::test::ActionResult::kSucceeded;
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::SelectMenuItem(
    ui::TrackedElement* element,
    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;
  auto* const menu_item =
      AsViewClass<MenuItemView>(element->AsA<TrackedElementViews>()->view());
  if (!menu_item)
    return ui::test::ActionResult::kNotAttempted;

#if BUILDFLAG(IS_MAC)
  // Keyboard input isn't reliable on Mac for submenus, so unless the test
  // specifically calls for keyboard input, prefer mouse.
  if (input_type == ui::test::InteractionTestUtil::InputType::kDontCare)
    input_type = ui::test::InteractionTestUtil::InputType::kMouse;
#endif  // BUILDFLAG(IS_MAC)

  auto* const host = menu_item->GetWidget()->GetRootView();
  gfx::Point point = GetCenter(menu_item);
  View::ConvertPointToTarget(menu_item, host, &point);

  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(host->GetWidget(), point);
      break;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(host->GetWidget(), point);
      break;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
    case ui::test::InteractionTestUtil::InputType::kDontCare: {
#if BUILDFLAG(IS_MAC)
      constexpr ui::KeyboardCode kSelectMenuKeyboardCode = ui::VKEY_SPACE;
#else
      constexpr ui::KeyboardCode kSelectMenuKeyboardCode = ui::VKEY_RETURN;
#endif
      MenuController* const controller = menu_item->GetMenuController();
      controller->SelectItemAndOpenSubmenu(menu_item);
      ui::KeyEvent key_event(ui::EventType::kKeyPressed,
                             kSelectMenuKeyboardCode, ui::EF_NONE,
                             ui::EventTimeForNow());
      controller->OnWillDispatchKeyEvent(&key_event);
      break;
    }
  }
  return ui::test::ActionResult::kSucceeded;
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::DoDefaultAction(
    ui::TrackedElement* element,
    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;
  if (!DoDefaultAction(element->AsA<TrackedElementViews>()->view(),
                       input_type)) {
    LOG(ERROR) << "Failed to send default action to " << *element;
    return ui::test::ActionResult::kFailed;
  }
  return ui::test::ActionResult::kSucceeded;
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::SelectTab(
    ui::TrackedElement* tab_collection,
    size_t index,
    InputType input_type) {
  // Currently, only TabbedPane is supported, but other types of tab
  // collections (e.g. browsers and tabstrips) may be supported by a different
  // kind of simulator specific to browser code, so if this is not a supported
  // View type, just return false instead of sending an error.
  if (!tab_collection->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;
  auto* const pane = views::AsViewClass<TabbedPane>(
      tab_collection->AsA<TrackedElementViews>()->view());
  if (!pane)
    return ui::test::ActionResult::kNotAttempted;

  // Unlike with the element type, an out-of-bounds tab is always an error.
  auto* const tab = pane->GetTabAt(index);
  if (!tab) {
    LOG(ERROR) << "Tab index " << index << " out of range, there are "
               << pane->GetTabCount() << " tabs.";
    return ui::test::ActionResult::kFailed;
  }
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      if (!SendDefaultAction(tab)) {
        LOG(ERROR) << "Failed to send default action to tab.";
        return ui::test::ActionResult::kFailed;
      }
      break;
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(tab, GetCenter(tab));
      break;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(tab, GetCenter(tab));
      break;
    case ui::test::InteractionTestUtil::InputType::kKeyboard: {
      // Keyboard navigation is done by sending arrow keys to the currently-
      // selected tab. Scan through the tabs by using the right arrow until the
      // correct tab is selected; limit the number of times this is tried to
      // avoid in infinite loop if something goes wrong.
      const auto current_index = pane->GetSelectedTabIndex();
      if (current_index != index) {
        const auto code = ((current_index > index) ^ base::i18n::IsRTL())
                              ? ui::VKEY_LEFT
                              : ui::VKEY_RIGHT;
        const int count =
            std::abs(static_cast<int>(index) - static_cast<int>(current_index));
        LOG_IF(WARNING, count > 1)
            << "SelectTab via keyboard from " << current_index << " to "
            << index << " will pass through intermediate tabs.";
        for (int i = 0; i < count; ++i) {
          auto* const current_tab = pane->GetTabAt(pane->GetSelectedTabIndex());
          SendKeyPress(current_tab, code);
        }
        if (index != pane->GetSelectedTabIndex()) {
          LOG(ERROR) << "Unable to cycle through tabs to reach index " << index;
          return ui::test::ActionResult::kFailed;
        }
      }
      break;
    }
  }
  return ui::test::ActionResult::kSucceeded;
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::SelectDropdownItem(
    ui::TrackedElement* dropdown,
    size_t index,
    InputType input_type) {
  if (!dropdown->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;
  auto* const view = dropdown->AsA<TrackedElementViews>()->view();
  auto* const combobox = views::AsViewClass<Combobox>(view);
  auto* const editable_combobox = views::AsViewClass<EditableCombobox>(view);
  if (!combobox && !editable_combobox)
    return ui::test::ActionResult::kNotAttempted;
  auto* const model =
      combobox ? combobox->GetModel() : editable_combobox->GetComboboxModel();
  if (index >= model->GetItemCount()) {
    LOG(ERROR) << "Item index " << index << " is out of range, there are "
               << model->GetItemCount() << " items.";
    return ui::test::ActionResult::kFailed;
  }

  // InputType::kDontCare is implemented in a way that is safe across all
  // platforms and most test environments; it does not rely on popping up the
  // dropdown and selecting individual items.
  if (input_type == InputType::kDontCare) {
    if (combobox) {
      combobox->MenuSelectionAt(index);
    } else {
      editable_combobox->SetText(model->GetItemAt(index));
    }
    return ui::test::ActionResult::kSucceeded;
  }

  // For specific input types, the dropdown will be popped out. Because of
  // asynchronous and event-handling issues, this is not yet supported on Mac.
#if BUILDFLAG(IS_MAC)
  LOG(WARNING) << "SelectDropdownItem(): "
                  "only InputType::kDontCare is supported on Mac.";
  return ui::test::ActionResult::kKnownIncompatible;
#else

  // This is required in case we want to repeatedly test a combobox; otherwise
  // it will refuse to open the second time.
  if (combobox)
    combobox->closed_time_ = base::TimeTicks();

  // The highest-fidelity input simulation involves actually opening the
  // drop-down and selecting an item from the list.
  DropdownItemSelector selector(this, input_type, index);

  // Try to get the arrow. If it's present, the combobox will be opened by
  // activating the button. Note that while Combobox has the ability to hide its
  // arrow, the button is still present and visible, just transparent.
  auto* const arrow = combobox ? combobox->arrow_button_.get()
                               : editable_combobox->arrow_.get();
  if (arrow) {
    PressButton(arrow, input_type);
  } else {
    if (!editable_combobox) {
      LOG(ERROR) << "Only EditableCombobox should have the option to "
                    "completely remove its arrow.";
      return ui::test::ActionResult::kFailed;
    }
    // Editable comboboxes without visible arrows exist, but are weird.
    switch (input_type) {
      case InputType::kDontCare:
      case InputType::kKeyboard:
        // Have to resort to keyboard input; DoDefaultAction() doesn't work.
        SendKeyPress(editable_combobox->textfield_, ui::VKEY_DOWN);
        break;
      default:
        LOG(WARNING) << "Mouse and touch input are not supported for "
                        "comboboxes without visible arrows.";
        return ui::test::ActionResult::kNotAttempted;
    }
  }

  selector.SelectItem();
  return selector.result();
#endif
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::EnterText(
    ui::TrackedElement* element,
    std::u16string text,
    TextEntryMode mode) {
  if (!element->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;
  auto* const view = element->AsA<TrackedElementViews>()->view();

  // Currently, Textfields (and derived types like Textareas) are supported, as
  // well as EditableCombobox.
  Textfield* textfield = AsViewClass<Textfield>(view);
  if (!textfield && IsViewClass<EditableCombobox>(view))
    textfield = AsViewClass<EditableCombobox>(view)->textfield_;

  if (!textfield) {
    return ui::test::ActionResult::kNotAttempted;
  }

  if (textfield->GetReadOnly()) {
    LOG(ERROR) << "Cannot set text on read-only textfield.";
    return ui::test::ActionResult::kFailed;
  }

  // Textfield does not expose all of the power of RenderText so some care has
  // to be taken in positioning the input caret using the methods available to
  // this class.
  switch (mode) {
    case TextEntryMode::kAppend: {
      // Determine the start and end of the selectable range, and position the
      // caret immediately after the end of the range. This approach does not
      // make any assumptions about the indexing mode of the text,
      // multi-codepoint characters, etc.
      textfield->SelectAll(false);
      auto range = textfield->GetSelectedRange();
      range.set_start(range.end());
      textfield->SetSelectedRange(range);
      break;
    }
    case TextEntryMode::kInsertOrReplace:
      // No action needed; keep selection and cursor as they are.
      break;
    case TextEntryMode::kReplaceAll:
      textfield->SelectAll(false);
      break;
  }

  // This is an IME method that is the closest thing to inserting text from
  // the user rather than setting it programmatically.
  textfield->InsertText(
      text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  return ui::test::ActionResult::kSucceeded;
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::ActivateSurface(
    ui::TrackedElement* element) {
  if (!element->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;

  auto* const widget = element->AsA<TrackedElementViews>()->view()->GetWidget();
  if (!widget) {
    LOG(WARNING) << "View not assocaited with a widget.";
    return ui::test::ActionResult::kFailed;
  }

  return ActivateWidget(widget);
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::SendAccelerator(
    ui::TrackedElement* element,
    ui::Accelerator accelerator) {
  if (!element->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;

  element->AsA<TrackedElementViews>()
      ->view()
      ->GetFocusManager()
      ->ProcessAccelerator(accelerator);
  return ui::test::ActionResult::kSucceeded;
}

ui::test::ActionResult InteractionTestUtilSimulatorViews::Confirm(
    ui::TrackedElement* element) {
  if (!element->IsA<TrackedElementViews>())
    return ui::test::ActionResult::kNotAttempted;
  auto* const view = element->AsA<TrackedElementViews>()->view();

  // Currently, only dialogs can be confirmed. Fetch the delegate and call
  // Accept().
  DialogDelegate* delegate = nullptr;
  if (auto* const dialog = AsViewClass<DialogDelegateView>(view)) {
    delegate = dialog->AsDialogDelegate();
  } else if (auto* const bubble = AsViewClass<BubbleDialogDelegateView>(view)) {
    delegate = bubble->AsDialogDelegate();
  }

  if (!delegate)
    return ui::test::ActionResult::kNotAttempted;

  if (!delegate->GetOkButton()) {
    LOG(ERROR) << "Confirm(): cannot confirm dialog that has no OK button.";
    return ui::test::ActionResult::kFailed;
  }

  delegate->AcceptDialog();
  return ui::test::ActionResult::kSucceeded;
}

// static
bool InteractionTestUtilSimulatorViews::IsWayland() {
#if BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetPlatformNameForTest() == "wayland";
#else
  return false;
#endif
}

// static
ui::test::ActionResult InteractionTestUtilSimulatorViews::ActivateWidget(
    Widget* widget) {
#if HANDLE_WAYLAND_FAILURE
  if (IsWayland()) {
    WidgetActivationWaiterWayland waiter(widget);
    widget->Activate();
    if (!waiter.Wait()) {
      LOG(WARNING)
          << "Unable to activate widget due to lack of Wayland support for "
             "widget activation; test is not meaningful on this platform.";
      return ui::test::ActionResult::kKnownIncompatible;
    }
    return ui::test::ActionResult::kSucceeded;
  }
#endif  // HANDLE_WAYLAND_FAILURE

  widget->Activate();
  views::test::WaitForWidgetActive(widget, true);
  return ui::test::ActionResult::kSucceeded;
}

// static
bool InteractionTestUtilSimulatorViews::DoDefaultAction(View* view,
                                                        InputType input_type) {
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      return SendDefaultAction(view);
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(view, GetCenter(view));
      return true;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(view, GetCenter(view));
      return true;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
      SendKeyPress(view, ui::VKEY_SPACE);
      return true;
  }
}

// static
void InteractionTestUtilSimulatorViews::PressButton(Button* button,
                                                    InputType input_type) {
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(button, GetCenter(button));
      break;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(button, GetCenter(button));
      break;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      SendKeyPress(button, ui::VKEY_SPACE);
      break;
  }
}

}  // namespace views::test
