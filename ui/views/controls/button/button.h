// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/button/button_controller_delegate.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/painter.h"

namespace views {
namespace test {
class ButtonTestApi;
}

class Button;
class ButtonController;
class Event;

// A View representing a button. A Button is focusable by default and will
// be part of the focus chain.
class VIEWS_EXPORT Button : public View, public AnimationDelegateViews {
 public:
  // Button states for various button sub-types.
  enum ButtonState {
    STATE_NORMAL = 0,
    STATE_HOVERED,
    STATE_PRESSED,
    STATE_DISABLED,
    STATE_COUNT,
  };

  // An enum describing the events on which a button should be clicked for a
  // given key event.
  enum class KeyClickAction {
    kOnKeyPress,
    kOnKeyRelease,
    kNone,
  };

  // TODO(cyan): Consider having Button implement ButtonControllerDelegate.
  class VIEWS_EXPORT DefaultButtonControllerDelegate
      : public ButtonControllerDelegate {
   public:
    explicit DefaultButtonControllerDelegate(Button* button);

    DefaultButtonControllerDelegate(const DefaultButtonControllerDelegate&) =
        delete;
    DefaultButtonControllerDelegate& operator=(
        const DefaultButtonControllerDelegate&) = delete;

    ~DefaultButtonControllerDelegate() override;

    // views::ButtonControllerDelegate:
    void RequestFocusFromEvent() override;
    void NotifyClick(const ui::Event& event) override;
    void OnClickCanceled(const ui::Event& event) override;
    bool IsTriggerableEvent(const ui::Event& event) override;
    bool ShouldEnterPushedState(const ui::Event& event) override;
    bool ShouldEnterHoveredState() override;
    InkDrop* GetInkDrop() override;
    int GetDragOperations(const gfx::Point& press_pt) override;
    bool InDrag() override;
  };

  // PressedCallback wraps a one-arg callback type with multiple constructors to
  // allow callers to specify a RepeatingClosure if they don't care about the
  // callback arg.
  // TODO(crbug.com/772945): Re-evaluate if this class can/should be converted
  // to a type alias + various helpers or overloads to support the
  // RepeatingClosure case.
  class VIEWS_EXPORT PressedCallback {
   public:
    using Callback = base::RepeatingCallback<void(const ui::Event& event)>;

    // Allow providing callbacks that expect either zero or one args, since many
    // callers don't care about the argument and can avoid adapter functions
    // this way.
    PressedCallback(Callback callback = Callback());  // NOLINT
    PressedCallback(base::RepeatingClosure closure);  // NOLINT
    PressedCallback(const PressedCallback&);
    PressedCallback(PressedCallback&&);
    PressedCallback& operator=(const PressedCallback&);
    PressedCallback& operator=(PressedCallback&&);
    ~PressedCallback();

    explicit operator bool() const { return !!callback_; }

    void Run(const ui::Event& event) { callback_.Run(event); }

   private:
    Callback callback_;
  };

  static constexpr ButtonState kButtonStates[STATE_COUNT] = {
      ButtonState::STATE_NORMAL, ButtonState::STATE_HOVERED,
      ButtonState::STATE_PRESSED, ButtonState::STATE_DISABLED};

  METADATA_HEADER(Button);

  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  ~Button() override;

  static const Button* AsButton(const View* view);
  static Button* AsButton(View* view);

  static ButtonState GetButtonStateFrom(ui::NativeTheme::State state);

  void SetTooltipText(const std::u16string& tooltip_text);
  std::u16string GetTooltipText() const;

  // Tag is now a property. These accessors are deprecated. Use GetTag() and
  // SetTag() below or even better, use SetID()/GetID() from the ancestor.
  int tag() const { return tag_; }
  void set_tag(int tag) { tag_ = tag; }

  virtual void SetCallback(PressedCallback callback);

  const std::u16string& GetAccessibleName() const override;

  // Get/sets the current display state of the button.
  ButtonState GetState() const;
  // Clients passing in STATE_DISABLED should consider calling
  // SetEnabled(false) instead because the enabled flag can affect other things
  // like event dispatching, focus traversals, etc. Calling SetEnabled(false)
  // will also set the state of |this| to STATE_DISABLED.
  void SetState(ButtonState state);

  // Starts throbbing. See HoverAnimation for a description of cycles_til_stop.
  // This method does nothing if |animate_on_state_change_| is false.
  void StartThrobbing(int cycles_til_stop);

  // Stops throbbing immediately.
  void StopThrobbing();

  // Set how long the hover animation will last for.
  void SetAnimationDuration(base::TimeDelta duration);

  void SetTriggerableEventFlags(int triggerable_event_flags);
  int GetTriggerableEventFlags() const;

  // Sets whether |RequestFocus| should be invoked on a mouse press. The default
  // is false.
  void SetRequestFocusOnPress(bool value);
  bool GetRequestFocusOnPress() const;

  // See description above field.
  void SetAnimateOnStateChange(bool value);
  bool GetAnimateOnStateChange() const;

  void SetHideInkDropWhenShowingContextMenu(bool value);
  bool GetHideInkDropWhenShowingContextMenu() const;

  void SetShowInkDropWhenHotTracked(bool value);
  bool GetShowInkDropWhenHotTracked() const;

  void SetHasInkDropActionOnClick(bool value);
  bool GetHasInkDropActionOnClick() const;

  void SetInstallFocusRingOnFocus(bool install_focus_ring_on_focus);
  bool GetInstallFocusRingOnFocus() const;

  void SetHotTracked(bool is_hot_tracked);
  bool IsHotTracked() const;

  // TODO(crbug/1266066): These property accessors and tag_ field should be
  // removed and use SetID()/GetID from the ancestor View class.
  void SetTag(int value);
  int GetTag() const;

  void SetFocusPainter(std::unique_ptr<Painter> focus_painter);

  // Highlights the ink drop for the button.
  void SetHighlighted(bool highlighted);

  base::CallbackListSubscription AddStateChangedCallback(
      PropertyChangedCallback callback);

  // Overridden from View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  void OnDragDone() override;
  // Instead of overriding this, subclasses that want custom painting should use
  // PaintButtonContents.
  void OnPaint(gfx::Canvas* canvas) final;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnFocus() override;
  void OnBlur() override;

  // Overridden from views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Returns the click action for the given key event.
  // Subclasses may override this method to support default actions for key
  // events.
  // TODO(cyan): Move this into the ButtonController.
  virtual KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event);

  ButtonController* button_controller() const {
    return button_controller_.get();
  }

  void SetButtonController(std::unique_ptr<ButtonController> button_controller);

  gfx::Point GetMenuPosition() const;

 protected:
  explicit Button(PressedCallback callback = PressedCallback());

  // Called when the button has been clicked or tapped and should request focus
  // if necessary.
  virtual void RequestFocusFromEvent();

  // Cause the button to notify the listener that a click occurred.
  virtual void NotifyClick(const ui::Event& event);

  // Called when a button gets released without triggering an action.
  // Note: This is only wired up for mouse button events and not gesture
  // events.
  virtual void OnClickCanceled(const ui::Event& event);

  // Called when the tooltip is set.
  virtual void OnSetTooltipText(const std::u16string& tooltip_text);

  // Invoked from SetState() when SetState() is passed a value that differs from
  // the current node_data. Button's implementation of StateChanged() does
  // nothing; this method is provided for subclasses that wish to do something
  // on state changes.
  virtual void StateChanged(ButtonState old_state);

  // Returns true if the event is one that can trigger notifying the listener.
  // This implementation returns true if the left mouse button is down.
  // TODO(cyan): Remove this method and move the implementation into
  // ButtonController.
  virtual bool IsTriggerableEvent(const ui::Event& event);

  // Returns true if the ink drop should be updated by Button when
  // OnClickCanceled() is called. This method is provided for subclasses.
  // If the method is overriden and returns false, the subclass is responsible
  // will be responsible for updating the ink drop.
  virtual bool ShouldUpdateInkDropOnClickCanceled() const;

  // Returns true if the button should become pressed when the user
  // holds the mouse down over the button. For this implementation,
  // we simply return IsTriggerableEvent(event).
  virtual bool ShouldEnterPushedState(const ui::Event& event);

  // Override to paint custom button contents. Any background or border set on
  // the view will be painted before this is called and |focus_painter_| will be
  // painted afterwards.
  virtual void PaintButtonContents(gfx::Canvas* canvas);

  // Returns true if the button should enter hovered state; that is, if the
  // mouse is over the button, and no other window has capture (which would
  // prevent the button from receiving MouseExited events and updating its
  // node_data). This does not take into account enabled node_data.
  bool ShouldEnterHoveredState();

  const gfx::ThrobAnimation& hover_animation() const {
    return hover_animation_;
  }

  // Getter used by metadata only.
  const PressedCallback& GetCallback() const { return callback_; }

 private:
  friend class test::ButtonTestApi;
  FRIEND_TEST_ALL_PREFIXES(BlueButtonTest, Border);

  void OnEnabledChanged();

  // The text shown in a tooltip.
  std::u16string tooltip_text_;

  // The button's listener. Notified when clicked.
  PressedCallback callback_;

  // The id tag associated with this button. Used to disambiguate buttons.
  // TODO(pbos): See if this can be removed, e.g. by replacing with SetID().
  int tag_ = -1;

  ButtonState state_ = STATE_NORMAL;

  gfx::ThrobAnimation hover_animation_{this};

  // Should we animate when the state changes?
  bool animate_on_state_change_ = false;

  // Is the hover animation running because StartThrob was invoked?
  bool is_throbbing_ = false;

  // Mouse event flags which can trigger button actions.
  int triggerable_event_flags_ = ui::EF_LEFT_MOUSE_BUTTON;

  // See description above setter.
  bool request_focus_on_press_ = false;

  // True when a button click should trigger an animation action on
  // ink_drop_delegate().
  bool has_ink_drop_action_on_click_ = false;

  // When true, the ink drop ripple and hover will be hidden prior to showing
  // the context menu.
  bool hide_ink_drop_when_showing_context_menu_ = true;

  // When true, the ink drop ripple will be shown when setting state to hot
  // tracked with SetHotTracked().
  bool show_ink_drop_when_hot_tracked_ = false;

  std::unique_ptr<Painter> focus_painter_;

  // ButtonController is responsible for handling events sent to the Button and
  // related state changes from the events.
  // TODO(cyan): Make sure all state changes are handled within
  // ButtonController.
  std::unique_ptr<ButtonController> button_controller_;

  base::CallbackListSubscription enabled_changed_subscription_{
      AddEnabledChangedCallback(base::BindRepeating(&Button::OnEnabledChanged,
                                                    base::Unretained(this)))};
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, Button, View)
VIEW_BUILDER_PROPERTY(std::u16string, AccessibleName)
VIEW_BUILDER_PROPERTY(Button::PressedCallback, Callback)
VIEW_BUILDER_PROPERTY(base::TimeDelta, AnimationDuration)
VIEW_BUILDER_PROPERTY(bool, AnimateOnStateChange)
VIEW_BUILDER_PROPERTY(bool, HasInkDropActionOnClick)
VIEW_BUILDER_PROPERTY(bool, HideInkDropWhenShowingContextMenu)
VIEW_BUILDER_PROPERTY(bool, InstallFocusRingOnFocus)
VIEW_BUILDER_PROPERTY(bool, RequestFocusOnPress)
VIEW_BUILDER_PROPERTY(Button::ButtonState, State)
VIEW_BUILDER_PROPERTY(int, Tag)
VIEW_BUILDER_PROPERTY(std::u16string, TooltipText)
VIEW_BUILDER_PROPERTY(int, TriggerableEventFlags)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, Button)

#endif  // UI_VIEWS_CONTROLS_BUTTON_BUTTON_H_
