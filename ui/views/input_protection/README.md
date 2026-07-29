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

______________________________________________________________________

## Existing Policies

1. **`DefaultInputProtectionPolicy`**:
   - **Show Cooldown**: Blocks all input events for a short period (cooldown)
     immediately after the view becomes visible. It can automatically observe
     the protected `View`'s visibility when initialized with the view, or rely
     on manual visibility forwarding from the protector otherwise.
   - **Click-Spam Protection**: Blocks rapid successive clicks (key repeats or
     click-spam) by enforcing a minimum delay between interactions.
2. **`WindowActivationInputProtectionPolicy`**:
   - **Sudden Activation**: Triggered when a widget becomes active, but its
     parent window was previously invisible. This protects against cases where a
     dialog suddenly appears and steals focus just as the user is clicking.
3. **`OcclusionAwareInputProtectionPolicy`**:
   - **Current/Recent Occlusion**: Blocks inputs if the target area is currently
     covered, or was recently covered, by an always-on-top window (managed by
     `OccludedWidgetInputProtector`).

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
