# Input Protection System

This directory contains the input protection system for the Views framework. The
goal of this system is to prevent potentially unintended user interactions with
**sensitive UI elements**. These are elements that perform actions with
security, privacy, or system-state implications (e.g., "Allow" buttons on
prompts, "Install" buttons for extensions, or "Confirm" buttons for purchases).

By protecting these elements, the system mitigates various forms of clickjacking
and unintended interactions.

______________________________________________________________________

## Protection Levels: Default vs. Protected Widgets

The Views framework provides two tiers of input protection:

### Default Protection (All Widgets)

Even when a widget does not explicitly enable input activation protection, it
automatically receives baseline protection:

- **Located Events (Clicks / Taps):** Global occlusion protection for pointer
  interactions—blocked if the interaction coordinate is currently occluded (or
  was recently occluded within the double-click interval) by an always-on-top
  window.
- **Non-Located Events (Keyboard):** Inactive on unprotected widgets.

### Full Activation Protection

Widgets opt in by calling `widget->EnableInputEventActivationProtection()`,
which enables the full suite of safeguards:

- **Initial Show Cooldown:** Blocks interactions immediately after the widget is
  shown (`DefaultInputProtectionPolicy`).
- **Click-Spam Prevention:** Enforces a minimum delay between rapid successive
  interactions (`DefaultInputProtectionPolicy`).
- **Sudden Window Activation:** Blocks interactions if the widget activates
  after its parent was invisible (`WindowActivationInputProtectionPolicy`).
- **Occlusion Protection:** Evaluates non-located events (such as keyboard
  action keys like Space or Return) against always-on-top window occlusion
  (`OccludedWidgetInputProtector`).

| Safeguard                                                     | Enforcing Policy / Component            | Availability              |
| :------------------------------------------------------------ | :-------------------------------------- | :------------------------ |
| **Always-On-Top Occlusion** (Located events: Clicks / Taps)   | `OccludedWidgetInputProtector`          | **Default (All Widgets)** |
| **Always-On-Top Occlusion** (Non-located events: Key Actions) | `OccludedWidgetInputProtector`          | **Opt-In Only**           |
| **Initial Show Cooldown** (All events)                        | `DefaultInputProtectionPolicy`          | **Opt-In Only**           |
| **Click-Spam Prevention** (All events)                        | `DefaultInputProtectionPolicy`          | **Opt-In Only**           |
| **Sudden Window Activation** (All events)                     | `WindowActivationInputProtectionPolicy` | **Opt-In Only**           |

______________________________________________________________________

## How to Use

In the modern Views framework, input protection is managed at the **`Widget`
level**. Individual views do not need to intercept events or manage cooldowns
manually. Event interception and evaluation are handled automatically by
`InputProtectionEventHandler` at the `RootView` level.

### 1. Enable Protection on Your Widget (Recommended)

To protect sensitive UI elements (such as buttons on a dialog or permission
prompt), enable input protection on the containing `Widget` during setup (e.g.,
in `AddedToWidget()`):

```cpp
void MyView::AddedToWidget() {
  // Enables standard input protection (initial show cooldown, click-spam
  // protection, sudden activation protection, and occlusion protection for key events).
  GetWidget()->EnableInputEventActivationProtection();
}
```

### 2. Specify Custom Protected Bounds (Optional)

By default, occlusion protection covers the physical bounds of the targeted
view, requiring **complete occlusion** (full coverage) to block keyboard events.
To restrict protection to a specific sub-region (such as a confirmation button)
and enforce **partial intersection** checking, install an
`InputProtectionSpecification` on your view:

```cpp
// In your View subclass initialization:
InputProtectionSpecification::Install(
    *this, base::BindRepeating(&MyView::GetLocalProtectedBounds,
                               base::Unretained(this)));
```

Implement the callback to return the sensitive bounds in **local view
coordinates**:

```cpp
std::vector<gfx::Rect> MyView::GetLocalProtectedBounds() const {
  if (accept_button_) {
    return {accept_button_->bounds()};
  }
  return {};
}
```

Installing custom bounds restricts pointer occlusion checks to the declared
regions and causes keyboard interactions to be blocked if *any part* of the
custom bounds is occluded (see
[Event Occlusion Checking](#event-occlusion-checking)).

### 3. Custom Policy Configurations (Optional)

By default, `EnableInputEventActivationProtection()` configures an
`InputEventActivationProtector` with the standard policy suite
(`DefaultInputProtectionPolicy` and `WindowActivationInputProtectionPolicy`).

To customize which policies are active, pass a custom
`InputEventActivationProtector` to `EnableInputEventActivationProtection()`. The
constructor used for `InputEventActivationProtector` controls the initial policy
setup:

- **Default Constructor (`InputEventActivationProtector()`):** Instantiates with
  `DefaultInputProtectionPolicy` automatically installed.
- **Parameterized Constructor (`InputEventActivationProtector(policy)`):**
  Instantiates with **only** the provided policy (omitting
  `DefaultInputProtectionPolicy`).

If none of the existing policies meet your requirements and you need to create
your own policy, see [How to Create a New Policy](#how-to-create-a-new-policy).

```cpp
// Instantiates with only the window activation policy (bypassing default show cooldown):
auto custom_protector = std::make_unique<InputEventActivationProtector>(
    std::make_unique<WindowActivationInputProtectionPolicy>(widget));

// Add custom policies:
custom_protector->AddPolicy(std::make_unique<MyCustomSecurityPolicy>());

// Pass the custom protector to the widget (overriding default policies):
widget->EnableInputEventActivationProtection(std::move(custom_protector));
```

______________________________________________________________________

## Core Architecture

The core framework consists of a manager (`InputEventActivationProtector`) that
delegates interaction evaluations to one or more policy objects
(`InputProtectionPolicy`):

```
                       ┌─────────────────────────────┐
                       │InputEventActivationProtector│
                       └──────────────┬──────────────┘
                                      │
                                      │ Delegates to
                                      v
                        ┌─────────────┴─────────────┐
                        │                           │
                        v                           v
             ┌──────────────────────┐   ┌──────────────────────┐
             │DefaultInputProtection│   │WindowActivationInput │
             │Policy                │   │ProtectionPolicy      │
             └──────────────────────┘   └──────────────────────┘
```

### `InputEventActivationProtector`

This is the main manager class. It is typically owned by a `View` that requires
protection (for example, `DialogClientView` or `BubbleFrameView`).

- It exposes `IsPossiblyUnintendedInteraction(event, target_view)` to check if
  an input event should be blocked.
- It maintains a list of `InputProtectionPolicy` objects and delegates the check
  to them. If *any* policy recommends blocking, the event is blocked.
- It forwards lifecycle events (like the view being shown or hidden) to the
  registered policies.

### `InputProtectionPolicy`

The abstract base class for all protection rules. Subclasses must implement:

- `IsPossiblyUnintendedInteraction(event, target_view, protector)`: Evaluates if
  the event should be blocked.

Subclasses can optionally override lifecycle methods to manage their internal
state:

- `OnProtectionStarted()`: Called when the protected target becomes visible.
- `OnProtectionStopped()`: Called when the protected target is hidden or
  destroyed.
- `OnProtectionReset()`: Called when a UI change (e.g., layout or window
  stationarity change) requires restarting the cooldown.

### View-Defined Protected Bounds (InputProtectionSpecification)

To declare specific regions within a view that require input protection, you can
install an `InputProtectionSpecification` on the view (e.g., a Dialog).

This is done using the `InputProtectionSpecification::Install` helper:

```cpp
InputProtectionSpecification::Install(
    *view, base::BindRepeating(&MyView::GetLocalProtectedBounds));
```

The callback must return a vector of `gfx::Rect` in the **local coordinates** of
the view on which it is installed.

This is implemented using the property `kInputProtectionKey`. When an event is
processed, the system walks up the parent chain starting from the target view to
gather and **accumulate** the bounds from all `InputProtectionSpecification`s it
finds. This ensures that parent-level protection is additive and cannot be
bypassed by descendant views.

The framework automatically handles:

- **Clipping**: The returned bounds are clipped to the boundaries of the view
  that holds the specification (preventing views from declaring bounds outside
  themselves).
- **Coordinate Conversion**: The valid bounds are automatically converted from
  the local coordinate space of the owner view to screen coordinates.

The `InputProtectionSpecification` is used by `OccludedWidgetInputProtector`
during occlusion evaluation; protection policies (such as default cooldown or
window activation) do not query these bounds.

> **Note:** Installing this specification defines custom bounds to protect. To
> enforce occlusion checks against these bounds for non-located events (e.g.
> keyboard action keys), the containing widget (or its primary window widget)
> must enable protection by calling
> `Widget::EnableInputEventActivationProtection()`. For details, see
> [How to Use](#how-to-use).

______________________________________________________________________

## Existing Policies

### Default Input Protection Policy

Implemented by `DefaultInputProtectionPolicy`:

- **Show Cooldown**: Blocks all input events for a short period (cooldown)
  immediately after the view becomes visible. It can automatically observe the
  protected `View`'s visibility when initialized with the view, or rely on
  manual visibility forwarding from the protector otherwise.
- **Click-Spam Protection**: Blocks rapid successive clicks (key repeats or
  click-spam) by enforcing a minimum delay between interactions.

### Window Activation Input Protection Policy

Implemented by `WindowActivationInputProtectionPolicy`:

- **Sudden Activation**: Triggered when a widget becomes active, but its parent
  window was previously invisible. This protects against cases where a dialog
  suddenly appears and steals focus just as the user is clicking.

______________________________________________________________________

## How to Create a New Policy

To create a new protection policy:

1. Create a class that inherits from `InputProtectionPolicy`.
2. Implement `IsPossiblyUnintendedInteraction` to define your blocking logic.
3. If your policy depends on timing or lifecycle events, override
   `OnProtectionStarted`, `OnProtectionStopped`, and/or `OnProtectionReset` to
   manage your state (e.g., updating timestamps).
4. (Optional) If your policy needs to observe external events (like widget
   activation), implement the appropriate observer interface (e.g.,
   `views::WidgetObserver`) and manage the observation lifecycle.
5. Register your new policy with the `InputEventActivationProtector` using
   `AddPolicy`.

Example template:

```cpp
class MyCustomPolicy : public InputProtectionPolicy {
 public:
  MyCustomPolicy() = default;
  ~MyCustomPolicy() override = default;

  // InputProtectionPolicy:
  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      const InputEventActivationProtector& protector) override {
    // Return true if the event should be blocked based on your custom logic.
    return SomeConditionIsMet(event);
  }

  void OnProtectionStarted() override {
    // Initialize state when target becomes visible.
  }
};
```

______________________________________________________________________

## Always-On-Top Occlusion Tracking (OccludedWidgetInputProtector)

`OccludedWidgetInputProtector` is a singleton that tracks always-on-top widgets
to prevent occlusion-based attacks. It is queried by
`InputProtectionEventHandler` to evaluate point occlusion for located events
(such as clicks or taps) across all widgets, and view-bounds occlusion for
non-located events (such as keyboard action keys) on widgets with input
activation protection enabled.

### Tracking State

The protector maintains two types of tracking data:

1. **Live Always-On-Top Widgets**: Currently visible always-on-top widgets and
   their screen bounds.
2. **Historical Occlusions**: Cooldown records of recently hidden or moved
   always-on-top widgets. These records are kept for the duration of the
   **system double-click interval (typically 500ms)** to prevent "pop-away"
   attacks (where an always-on-top window is suddenly dismissed to trigger a
   click on the window underneath).

### Evaluation Logic (`ShouldBlockEvent`)

When a client calls the `ShouldBlockEvent(event, target_view)` method, the
protector checks the target against both live and historical tracking state.

#### Protected Bounds Gathering

The protector determines the bounds that need to be protected by gathering
specifications from the target view itself and all its ancestors, falling back
to the target view's own physical bounds if no specifications are found:

1. It walks up the view hierarchy starting from the `target_view` itself.
2. For each view in the chain, it queries `kInputProtectionKey` to find any
   installed `InputProtectionSpecification`.
3. For each specification found, it calls `GetProtectedBoundsInScreen()`, which
   runs the callback, clips the bounds to the owner view's boundaries, converts
   them to screen coordinates, and returns them.
4. It **accumulates** all of these screen bounds into a single list of protected
   regions.
5. If the accumulated list is empty (no specifications were found in the chain),
   the protector falls back to using the physical screen bounds of the
   `target_view` itself (`{target_view.GetBoundsInScreen()}`).

```
   ┌────────────────────────────┐
   │OccludedWidgetInputProtector│
   └─────────────┬──────────────┘
                 │
                 │ Walks parent chain and accumulates bounds
                 ▼
     (Walks up parent chain)
                 │
                 ├─► [Target View] (If spec found) ──► GetProtectedBoundsInScreen() ──┐
                 │                                                                    │
                 ├─► [Parent View] (If spec found) ──► GetProtectedBoundsInScreen() ──┼─► [Accumulated Screen Bounds]
                 │                                                                    │
                 └─► [Ancestor View] (If spec found) ─► GetProtectedBoundsInScreen() ─┘
```

#### Event Occlusion Checking

The gathered bounds are then checked for occlusion depending on the event type:

- **Located Events (Mouse/Touch/Clicks)**:
  - The event is **only** blocked if the event coordinate falls *inside* the
    gathered protected bounds (either view-defined bounds or default view
    bounds) **and** that coordinate is occluded by an always-on-top window (live
    or historical).
  - Events landing outside the gathered protected bounds (even if inside the
    view's default bounds) are never blocked.
- **Non-Located Events (Keyboard Interactions)**:
  - **Opt-In Required**: Only evaluated if the target widget (or its primary
    window widget) has explicitly enabled protection via
    `Widget::EnableInputEventActivationProtection()`.
  - **Occlusion Check Mode**:
    - **Strict Check (For View-Defined Bounds)**: If the target view or its
      ancestors have view-defined bounds, the event is blocked if *any part* of
      those bounds is occluded (i.e., if an always-on-top window intersects with
      any of the view-defined bounds).
    - **Lenient Check (For Default Bounds)**: If no view-defined bounds exist
      and we fell back to the default view bounds, the event is only blocked if
      the default view bounds are *fully occluded* (i.e., if an always-on-top
      window completely covers the view's screen bounds).

______________________________________________________________________

## Protection Models

The Views framework supports two models for applying input protection:

- **Modern Model (Widget-Level)**: Enforced centrally on the `Widget` via
  `InputProtectionEventHandler`.
- **Legacy Model (View-Level)**: Managed and queried directly by individual
  `View`s.

### Modern Model: Widget-Level Protection (Recommended)

In the modern architecture, input protection is coordinated at the `Widget`
level and enforced automatically by `InputProtectionEventHandler` on `RootView`.
Individual views do not need to own a protector or manually intercept events.

#### Architecture

```
                          ┌──────────────┐
                          │    Widget    │
                          └──────┬───────┘
                                 │ Owns
                                 ├──────────────────────────────────────────────────┐
                                 │                                                  │
                                 v                                                  v
  ┌────────────────────────────────────────────────────────┐ Installed as  ┌─────────────────┐
  │              InputProtectionEventHandler               ├──pre-target──►│    RootView     │
  └──────────────┬──────────────────────────────┬──────────┘  handler on   └─────────────────┘
                 │                              │
  Queries global │                              │ Evaluates widget
  occlusion via  │                              │ policies via
                 v                              v
  ┌────────────────────────────┐ ┌─────────────────────────────┐
  │OccludedWidgetInputProtector│ │InputEventActivationProtector│
  └────────────────────────────┘ └──────────────┬──────────────┘
                                                │ Delegates to
                                ┌───────────────┴───────────────┐
                                │                               │
                                v                               v
                     ┌──────────────────────┐        ┌──────────────────────┐
                     │DefaultInputProtection│        │WindowActivationInput │
                     │Policy                │        │ProtectionPolicy      │
                     └──────────────────────┘        └──────────────────────┘
```

#### `InputProtectionEventHandler`

`InputProtectionEventHandler` is registered as a pre-target handler on the
`Widget`'s `RootView` during widget initialization when the
`features::kEnableInputProtection` feature flag is enabled. It intercepts
incoming user interactions before they are dispatched down the view hierarchy.

##### Event Interception and Filtering

The handler listens for user interaction entry-points across multiple input
modalities:

- **Pointer/Touch Events**: Mouse presses (`kMousePressed`), touch presses
  (`kTouchPressed`), and gesture tap sequences (`kGestureTap`,
  `kGestureTapDown`, `kGestureDoubleTap`, `kGestureLongPress`,
  `kGestureLongTap`).
- **Key Events**: Key presses (`kKeyPressed`, e.g., Space or Return on a focused
  view).

To prevent redundant processing on re-dispatched or re-routed events, the
handler stamps every evaluated event with the `kPropertyInputProtected`
property. If an event is already tagged with this property, subsequent
evaluations are skipped.

###### Focus Traversal Key Bypass

A **focus traversal key** is a key event used by `FocusManager` to move focus
between focusable views in the UI hierarchy.

Focus traversal keys include:

- **Tab Traversal**: Unmodified Tab (forward) and Shift+Tab (reverse) keys
  (without Ctrl or Alt).
- **Arrow Key Traversal**: Unmodified arrow keys (Up, Down, Left, Right) when
  directed to views that do not consume them internally. If the focused view
  overrides `View::SkipDefaultKeyEventProcessing` (such as a `Textfield` that
  handles arrow keys for text caret movement), arrow keys are treated as action
  keys rather than traversal keys and are subject to input protection.

Any other key events (such as Space, Return, or keys with modifiers like
`Ctrl+Arrow`) are treated as action keys and are evaluated by input protection
policies.

###### Accessibility Mode Bypass

When any accessibility mode is active
(`!ui::AXPlatform::GetInstance().GetMode().is_mode_off()`), input protection is
completely bypassed without querying active policies to avoid interfering with
assistive technologies.

##### Evaluation Order

When an untagged interaction event is received, `InputProtectionEventHandler`
performs two levels of evaluation:

1. **Global Occlusion Protection**: It queries the
   `OccludedWidgetInputProtector` singleton
   (`ShouldBlockEvent(event, target_view)`).
   - For located events (clicks, taps), all widgets are protected if the event
     coordinate is occluded by an always-on-top window.
   - For non-located events (action keys), widgets with input activation
     protection enabled are evaluated per
     [Event Occlusion Checking](#event-occlusion-checking) (partial occlusion
     for view-defined specifications, or full occlusion for fallback view
     bounds).
2. **Widget-Level Activation Protection**: If global occlusion does not block
   the event and input activation protection is enabled on the widget
   (`Widget::IsInputEventActivationProtectionEnabled()`), the handler forwards
   the event to the widget's `InputEventActivationProtector` to evaluate against
   installed policies (such as `DefaultInputProtectionPolicy` or
   `WindowActivationInputProtectionPolicy`).

##### Event Consumption and State Reset

If any active policy flags the interaction as unintended, the handler calls
`event->StopPropagation()` to consume the event and prevent it from reaching the
target view. In addition, it calls `RootView::ResetEventHandlers()` to clear any
active gesture or pointer tracking state (such as `mouse_pressed_handler_`),
ensuring that follow-up events (like mouse or touch releases) are safely dropped
rather than triggering unintended activations.

### Legacy Model: View-Level Protection (For Reference)

In older code, individual sensitive views owned their own protector instances
and manually checked interactions.

#### Architecture

```
                               ┌──────────────┐
                               │     View     │
                               └──────┬───────┘
                                      │
                                      │ Owns
                                      v
                       ┌─────────────────────────────┐
                       │InputEventActivationProtector│
                       └──────────────┬──────────────┘
                                      │
                                      │ Delegates to
                                      v
                        ┌─────────────┴─────────────┐
                        │                           │
                        v                           v
             ┌──────────────────────┐   ┌──────────────────────┐
             │DefaultInputProtection│   │WindowActivationInput │
             │Policy                │   │ProtectionPolicy      │
             └──────────────────────┘   └──────────────────────┘
```

#### How to Use

To protect a view in the legacy model, add an `InputEventActivationProtector`
member to your view class.

##### Step 1: Configure the Protector

The constructor you use determines whether the protector automatically installs
the default policy or uses a custom configuration.

###### Default Configuration (Constructor without arguments)

Instantiates with `DefaultInputProtectionPolicy` installed automatically:

```cpp
// Instantiates with DefaultInputProtectionPolicy installed automatically.
input_protector_ = std::make_unique<InputEventActivationProtector>();

// Add additional policies if needed (all registered policies will be active).
input_protector_->AddPolicy(
    std::make_unique<WindowActivationInputProtectionPolicy>(widget));
```

Then, because the automatically installed default policy does not observe the
view, you must manually forward visibility events when the protected view
visibility changes:

```cpp
input_protector_->VisibilityChanged(is_visible);
```

###### Custom Configuration (Constructor with arguments)

If you use the parameterized constructor, the protector installs **only** the
passed policy. Use this to configure custom policies (e.g., in tests to bypass
the default cooldown, or for specialized UIs).

```cpp
// Instantiates with only the window activation policy.
input_protector_ = std::make_unique<InputEventActivationProtector>(
    std::make_unique<WindowActivationInputProtectionPolicy>(widget));

// You can still add more policies later if needed:
input_protector_->AddPolicy(std::make_unique<MyCustomPolicy>());
```

##### Step 2: Query the Protector

Before handling a sensitive event (e.g., a button click), query the protector
manually:

```cpp
void MyView::OnButtonPressed(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event, this)) {
    return; // Block the event
  }
  // Handle the event...
}
```

##### Step 3: Specify View-Defined Protected Bounds (Optional)

If a view requires localized input protection (e.g., only protecting a specific
button rather than the entire view), install an `InputProtectionSpecification`
on the view. This is queried by `OccludedWidgetInputProtector`.

To do this, call `InputProtectionSpecification::Install` during view
initialization:

```cpp
// In your View subclass initialization:
InputProtectionSpecification::Install(
    *this, base::BindRepeating(&MyView::GetLocalProtectedBounds,
                               base::Unretained(this)));
```

And implement the callback method to return the bounds in **local coordinates**
of the view:

```cpp
std::vector<gfx::Rect> MyView::GetLocalProtectedBounds() const {
  // If the protected button exists, protect only that button region.
  if (protected_button_) {
    return {protected_button_->bounds()};
  }
  return {};
}
```

> **Note:** New code should prefer the **Widget-level modern model**
> (`Widget::EnableInputEventActivationProtection()`) instead of manually
> managing protectors inside individual views.
