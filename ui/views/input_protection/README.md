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
     immediately after the view becomes visible.
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

### Step 1: Configure the Protector (Choose one alternative)

#### Alternative A: Default Configuration (With Default Policy)

If you instantiate the protector using the default constructor, it automatically
installs the `DefaultInputProtectionPolicy` (which handles show cooldown and
click-spam protection).

You can then add additional policies if needed:

```cpp
// Instantiates with DefaultInputProtectionPolicy installed automatically.
input_protector_ = std::make_unique<InputEventActivationProtector>();

// Add an additional policy (both will be active).
input_protector_->AddPolicy(
    std::make_unique<WindowActivationInputProtectionPolicy>(widget));
```

#### Alternative B: Custom Configuration (Without Default Policy)

If you want to use a custom policy *instead* of the default one (e.g., in tests
or for specialized UIs), use the parameterized constructor. This constructor
installs **only** the passed policy and does not install the default policy.

```cpp
// Instantiates with ONLY the window activation policy.
// DefaultInputProtectionPolicy is NOT installed.
input_protector_ = std::make_unique<InputEventActivationProtector>(
    std::make_unique<WindowActivationInputProtectionPolicy>(widget));

// You can still add more policies later if needed:
input_protector_->AddPolicy(std::make_unique<MyCustomPolicy>());
```

### Step 2: Query the Protector

Before handling a sensitive event (e.g., a button click), query the protector:

```cpp
void MyView::OnButtonPressed(const ui::Event& event) {
  if (input_protector_.IsPossiblyUnintendedInteraction(event, this)) {
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
