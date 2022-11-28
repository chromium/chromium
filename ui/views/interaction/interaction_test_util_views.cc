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
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::test {

InteractionTestUtilSimulatorViews::InteractionTestUtilSimulatorViews() =
    default;
InteractionTestUtilSimulatorViews::~InteractionTestUtilSimulatorViews() =
    default;

namespace {

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
    CHECK(!success_.has_value());
    run_loop_.Run();
  }

  // Returns whether the operation succeeded or failed.
  bool success() const { return success_.value_or(false); }

 private:
  // Responds to a new widget being shown. The assumption is that this widget is
  // the combobox dropdown. If it is not, the follow-up call will fail.
  void OnWidgetShown(Widget* widget) {
    if (widget_ || success_.has_value())
      return;

    widget_ = widget;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DropdownItemSelector::SelectItemImpl,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // Detects when a widget is hidden. Fails the operation if this was the drop-
  // down widget and the item has not yet been selected.
  void OnWidgetHidden(Widget* widget) {
    if (success_.has_value() || widget_ != widget)
      return;

    LOG(ERROR) << "Widget closed before selection took place.";
    SetSuccess(false);
  }

  // Actually finds and selects the item in the drop-down. If it is not present
  // or cannot be selected, fails the operation.
  void SelectItemImpl() {
    CHECK(widget_);
    CHECK(!success_.has_value());

    // Because this widget was just shown, it may not be laid out yet.
    widget_->LayoutRootViewIfNecessary();
    size_t index = item_index_;
    if (auto* const menu_item =
            FindMenuItem(widget_->GetContentsView(), index)) {
      // No longer tracking the widget. This will prevent synchronous widget
      // dismissed during SelectMenuItem() below from thinking it failed.
      widget_ = nullptr;

      // Try to select the item.
      if (simulator_->SelectMenuItem(
              ElementTrackerViews::GetInstance()->GetElementForView(menu_item,
                                                                    true),
              input_type_)) {
        SetSuccess(true);
      } else {
        LOG(ERROR) << "Unable to select dropdown menu item.";
        SetSuccess(false);
      }
    } else {
      LOG(ERROR) << "Dropdown menu item not found.";
      SetSuccess(false);
    }
  }

  // Sets the success or failure state and aborts `run_loop_`. Should only ever
  // be called once.
  void SetSuccess(bool success) {
    CHECK(!success_.has_value());
    success_ = success;
    widget_ = nullptr;
    weak_ptr_factory_.InvalidateWeakPtrs();
    run_loop_.Quit();
  }

  // Recursively search `from` for the `index`-th MenuItemView.
  //
  // Searches in-order, depth-first. It is assumed that menu items will appear
  // in search order in the same order they appear visually.
  static MenuItemView* FindMenuItem(View* from, size_t& index) {
    for (auto* child : from->children()) {
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

  const base::raw_ptr<InteractionTestUtilSimulatorViews> simulator_;
  const ui::test::InteractionTestUtil::InputType input_type_;
  const size_t item_index_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  AnyWidgetObserver observer_{views::test::AnyWidgetTestPasskey()};  // IN-TEST
  absl::optional<bool> success_;
  base::raw_ptr<Widget> widget_ = nullptr;
  base::WeakPtrFactory<DropdownItemSelector> weak_ptr_factory_{this};
};

gfx::Point GetCenter(views::View* view) {
  return view->GetLocalBounds().CenterPoint();
}

void SendDefaultAction(View* target) {
  ui::AXActionData action;
  action.action = ax::mojom::Action::kDoDefault;
  CHECK(target->HandleAccessibleAction(action));
}

// Sends a mouse click to the specified `target`.
// Views are EventHandlers but Widgets are not despite having the same API for
// event handling, so use a templated approach to support both cases.
template <class T>
void SendMouseClick(T* target, const gfx::Point& point) {
  ui::MouseEvent mouse_down(ui::ET_MOUSE_PRESSED, point, point,
                            ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                            ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseEvent(&mouse_down);
  ui::MouseEvent mouse_up(ui::ET_MOUSE_RELEASED, point, point,
                          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseEvent(&mouse_up);
}

// Sends a tap gesture to the specified `target`.
// Views are EventHandlers but Widgets are not despite having the same API for
// event handling, so use a templated approach to support both cases.
template <class T>
void SendTapGesture(T* target, const gfx::Point& point) {
  ui::GestureEventDetails press_details(ui::ET_GESTURE_TAP);
  press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent press_event(point.x(), point.y(), ui::EF_NONE,
                               ui::EventTimeForNow(), press_details);
  target->OnGestureEvent(&press_event);

  ui::GestureEventDetails release_details(ui::ET_GESTURE_END);
  release_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent release_event(point.x(), point.y(), ui::EF_NONE,
                                 ui::EventTimeForNow(), release_details);
  target->OnGestureEvent(&release_event);
}

// Sends a key press to the specified `target`. Returns true if the view is
// still valid after processing the keypress.
bool SendKeyPress(View* view, ui::KeyboardCode code, int flags = ui::EF_NONE) {
  ViewTracker tracker(view);
  view->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, code, flags, ui::EventTimeForNow()));

  // Verify that the button is not destroyed after the key-down before trying
  // to send the key-up.
  if (!tracker.view())
    return false;

  tracker.view()->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, code, flags, ui::EventTimeForNow()));

  return tracker.view();
}

}  // namespace

bool InteractionTestUtilSimulatorViews::PressButton(ui::TrackedElement* element,
                                                    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return false;
  auto* const button =
      Button::AsButton(element->AsA<TrackedElementViews>()->view());
  if (!button)
    return false;

  PressButton(button, input_type);
  return true;
}

bool InteractionTestUtilSimulatorViews::SelectMenuItem(
    ui::TrackedElement* element,
    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return false;
  auto* const menu_item =
      AsViewClass<MenuItemView>(element->AsA<TrackedElementViews>()->view());
  if (!menu_item)
    return false;

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
      ui::KeyEvent key_event(ui::ET_KEY_PRESSED, kSelectMenuKeyboardCode,
                             ui::EF_NONE, ui::EventTimeForNow());
      controller->OnWillDispatchKeyEvent(&key_event);
      break;
    }
  }
  return true;
}

bool InteractionTestUtilSimulatorViews::DoDefaultAction(
    ui::TrackedElement* element,
    InputType input_type) {
  if (!element->IsA<TrackedElementViews>())
    return false;
  DoDefaultAction(element->AsA<TrackedElementViews>()->view(), input_type);
  return true;
}

bool InteractionTestUtilSimulatorViews::SelectTab(
    ui::TrackedElement* tab_collection,
    size_t index,
    InputType input_type) {
  // Currently, only TabbedPane is supported, but other types of tab
  // collections (e.g. browsers and tabstrips) may be supported by a different
  // kind of simulator specific to browser code, so if this is not a supported
  // View type, just return false instead of sending an error.
  if (!tab_collection->IsA<TrackedElementViews>())
    return false;
  auto* const pane = views::AsViewClass<TabbedPane>(
      tab_collection->AsA<TrackedElementViews>()->view());
  if (!pane)
    return false;

  // Unlike with the element type, an out-of-bounds tab is always an error.
  auto* const tab = pane->GetTabAt(index);
  CHECK(tab);
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      SendDefaultAction(tab);
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
        CHECK_EQ(index, pane->GetSelectedTabIndex());
      }
      break;
    }
  }
  return true;
}

bool InteractionTestUtilSimulatorViews::SelectDropdownItem(
    ui::TrackedElement* dropdown,
    size_t index,
    InputType input_type) {
  if (!dropdown->IsA<TrackedElementViews>())
    return false;
  auto* const view = dropdown->AsA<TrackedElementViews>()->view();
  auto* const combobox = views::AsViewClass<Combobox>(view);
  auto* const editable_combobox = views::AsViewClass<EditableCombobox>(view);
  if (!combobox && !editable_combobox)
    return false;
  auto* const model = combobox ? combobox->GetModel()
                               : editable_combobox->combobox_model_.get();
  CHECK_LT(index, model->GetItemCount());

  // InputType::kDontCare is implemented in a way that is safe across all
  // platforms and most test environments; it does not rely on popping up the
  // dropdown and selecting individual items.
  if (input_type == InputType::kDontCare) {
    if (combobox) {
      combobox->SetSelectedRow(index);
    } else {
      editable_combobox->SetText(model->GetItemAt(index));
    }
    return true;
  }

  // For specific input types, the dropdown will be popped out. Because of
  // asynchronous and event-handling issues, this is not yet supported on Mac.
#if BUILDFLAG(IS_MAC)
  LOG(ERROR) << "SelectDropdownItem(): "
                "only InputType::kDontCare is supported on Mac.";
  return false;
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
    CHECK(editable_combobox) << "Only EditableCombobox should have the option "
                                "to completely remove its arrow.";
    // Editable comboboxes without visible arrows exist, but are weird.
    switch (input_type) {
      case InputType::kDontCare:
      case InputType::kKeyboard:
        // Have to resort to keyboard input; DoDefaultAction() doesn't work.
        SendKeyPress(editable_combobox->textfield_, ui::VKEY_DOWN);
        break;
      default:
        LOG(ERROR) << "Mouse and touch input are not supported for "
                      "comboboxes without visible arrows.";
        return false;
    }
  }

  selector.SelectItem();
  return selector.success();
#endif
}

bool InteractionTestUtilSimulatorViews::EnterText(ui::TrackedElement* element,
                                                  const std::u16string& text,
                                                  TextEntryMode mode) {
  if (!element->IsA<TrackedElementViews>())
    return false;
  auto* const view = element->AsA<TrackedElementViews>()->view();

  // Currently, Textfields (and derived types like Textareas) are supported, as
  // well as EditableCombobox.
  Textfield* textfield = AsViewClass<Textfield>(view);
  if (!textfield && IsViewClass<EditableCombobox>(view))
    textfield = AsViewClass<EditableCombobox>(view)->textfield_;

  if (textfield) {
    if (textfield->GetReadOnly()) {
      LOG(ERROR) << "Cannot set text on read-only textfield.";
      return false;
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
    return true;
  }

  return false;
}

bool InteractionTestUtilSimulatorViews::ActivateSurface(
    ui::TrackedElement* element) {
  if (!element->IsA<TrackedElementViews>())
    return false;

  auto* const widget = element->AsA<TrackedElementViews>()->view()->GetWidget();
  views::test::WidgetActivationWaiter waiter(widget, true);
  widget->Activate();
  waiter.Wait();
  return true;
}

bool InteractionTestUtilSimulatorViews::SendAccelerator(
    ui::TrackedElement* element,
    const ui::Accelerator& accelerator) {
  if (!element->IsA<TrackedElementViews>())
    return false;

  element->AsA<TrackedElementViews>()
      ->view()
      ->GetFocusManager()
      ->ProcessAccelerator(accelerator);
  return true;
}

bool InteractionTestUtilSimulatorViews::Confirm(ui::TrackedElement* element) {
  if (!element->IsA<TrackedElementViews>())
    return false;
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
    return false;

  if (!delegate->GetOkButton()) {
    LOG(ERROR) << "Confirm(): cannot confirm dialog that has no OK button.";
    return false;
  }

  delegate->AcceptDialog();
  return true;
}

void InteractionTestUtilSimulatorViews::DoDefaultAction(View* view,
                                                        InputType input_type) {
  switch (input_type) {
    case ui::test::InteractionTestUtil::InputType::kDontCare:
      SendDefaultAction(view);
      break;
    case ui::test::InteractionTestUtil::InputType::kMouse:
      SendMouseClick(view, GetCenter(view));
      break;
    case ui::test::InteractionTestUtil::InputType::kTouch:
      SendTapGesture(view, GetCenter(view));
      break;
    case ui::test::InteractionTestUtil::InputType::kKeyboard:
      SendKeyPress(view, ui::VKEY_SPACE);
      break;
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

// static

}  // namespace views::test
