# Input Protection System

This directory contains the input protection system for the Views framework. The
goal of this system is to prevent potentially unintended user interactions with
**sensitive UI elements**. These are elements that perform actions with
security, privacy, or system-state implications (e.g., "Allow" buttons on
prompts, "Install" buttons for extensions, or "Confirm" buttons for purchases).

By protecting these elements, the system mitigates risks like clickjacking,
rapid-fire clicking, and sudden UI appearances (e.g., a dialog popping up right
under the user's cursor).

## Architecture

The system consists of a manager and one or more policies:

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
           ┌──────────────────────────┼──────────────────────────┐
           │                          │                          │
           v                          v                          v
┌──────────────────────┐   ┌──────────────────────┐   ┌──────────────────────┐
│DefaultInputProtection│   │WindowActivationInput │   │OcclusionAwareInput   │
│Policy                │   │ProtectionPolicy      │   │ProtectionPolicy      │
└──────────────────────┘   └──────────────────────┘   └──────────────────────┘
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

The `InputProtectionSpecification` is currently only used by the
[Occlusion-Aware Input Protection Policy](#occlusion-aware-input-protection-policy)
during event evaluation. Other policies (such as default cooldown or window
activation policies) do not query these bounds.

> [!NOTE] Installing this specification only defines the bounds to protect. It
> does not automatically enable input protection. To enforce occlusion
> protection, the containing widget (or its primary window widget) must enable
> protection by calling `Widget::EnableInputEventActivationProtection()`. For
> details, see [How to Use](#how-to-use).

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

### Occlusion-Aware Input Protection Policy

Implemented by `OcclusionAwareInputProtectionPolicy`:

- **Current/Recent Occlusion**: Blocks inputs if the target area is currently
  covered, or was recently covered, by an always-on-top window (managed by
  `OccludedWidgetInputProtector`).

______________________________________________________________________

## Always-On-Top Occlusion Tracking (OccludedWidgetInputProtector)

`OccludedWidgetInputProtector` is a singleton that tracks always-on-top widgets
to prevent occlusion-based attacks.

While it operates as a tracker for occlusion by always-on-top windows, it is
primarily queried by the `OcclusionAwareInputProtectionPolicy` to check if a
sensitive interaction is occluded.

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

## How to Use

To protect a view, add an `InputEventActivationProtector` member to your view
class.

### Step 1: Configure the Protector

The constructor you use determines whether the protector automatically installs
the default policy or uses a custom configuration.

#### Default Configuration (Constructor without arguments)

If you instantiate the protector using the default constructor, it automatically
installs a `DefaultInputProtectionPolicy` initialized without a view.

First, initialize the protector and add any additional policies (typically in
your view's constructor):

```cpp
// Instantiates with DefaultInputProtectionPolicy installed automatically.
input_protector_ = std::make_unique<InputEventActivationProtector>();

// Add additional policies if needed (all registered policies will be active).
input_protector_->AddPolicy(
    std::make_unique<WindowActivationInputProtectionPolicy>(widget));
```

Then, because the automatically installed default policy does not observe the
view, you must manually forward visibility events when the protected view's
visibility changes:

```cpp
input_protector_->VisibilityChanged(is_visible);
```

#### Custom Configuration (Constructor with arguments)

If you use the parameterized constructor, the protector installs **only** the
passed policy. Use this to configure custom policies (e.g., in tests to bypass
the default cooldown, or for specialized UIs).

```cpp
// Instantiates with ONLY the window activation policy.
input_protector_ = std::make_unique<InputEventActivationProtector>(
    std::make_unique<WindowActivationInputProtectionPolicy>(widget));

// You can still add more policies later if needed:
input_protector_->AddPolicy(std::make_unique<MyCustomPolicy>());
```

### Step 2: Query the Protector

Before handling a sensitive event (e.g., a button click), query the protector:

```cpp
void MyView::OnButtonPressed(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event, this)) {
    return; // Block the event
  }
  // Handle the event...
}
```

### Step 3: Specify View-Defined Protected Bounds (Optional)

If a view requires localized input protection (e.g., only protecting a specific
button rather than the entire view), you can install an
`InputProtectionSpecification` on the view. This is currently only queried by
the
[Occlusion-Aware Input Protection Policy](#occlusion-aware-input-protection-policy).

To do this, call `InputProtectionSpecification::Install` during your view's
initialization:

```cpp
// In your View subclass initialization:
InputProtectionSpecification::Install(
    *this, base::BindRepeating(&MyView::GetLocalProtectedBounds));
```

And implement the callback method to return the bounds in **local coordinates**
of the view:

```cpp
std::vector<gfx::Rect> MyView::GetLocalProtectedBounds() const {
  // If the protected button exists, protect only that button's region.
  // Note: returned bounds must be local to `MyView` (e.g., relative to 0,0 of MyView).
  if (protected_button_) {
    return {protected_button_->bounds()};
  }
  return {};
}
```

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
