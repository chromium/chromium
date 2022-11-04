# Chromium Interaction Library

Note: for **Interactive Testing**, see the
[Interactive Test Documentation](/chrome/test/interaction/README.md)

This folder contains primitives for locating named elements in different
application windows as well as following specific sequences of user interactions
with the UI. It is designed to support things like User Education Tutorials and
interactive UI testing, and to work with multiple UI presentation frameworks.

See the [Supported Frameworks](#Supported-Frameworks) section for a list of
currently-supported  frameworks.

[TOC]

## Named elements

Named elements are the fundamental building blocks of these systems. Each
supported framework must define how a UI element can be assigned an
[ElementIdentifier](/ui/base/interaction/element_identifier.h) which its
[framework implementation](#Supporting-additional-UI-frameworks) must be able to read.
A UI element with a non-null `ElementIdentifier` assigned to it is known as a
*named element*.

Each `ElementIdentifier` contains a globally unique opaque handle, with its
underlying value based on the address of a block of memory allocated at compile
time.

Two named elements with the same identifier are not necessarily equivalent. In
an application with multiple primary windows (such as a browser with several
windows and a PWA open), each primary window and all of its secondary UI - menus,
dialogs, bubbles, etc. - are represented by a single `ElementContext`. Like
identifiers, contexts are opaque handles that are unique to each primary window.
To get the context associated with a UI element or window, you will need to call
a method specific to your framework; see
[the relevant section](#2_Define-how-works-in-your-framework)
below.

`ElementIdentifier` and `ElementContext` both support the methods one might
expect from a handle or opaque pointer:
* Assignment (`=`)
* Equality (`==`, `!=`)
* Ordering (`<`) - for use in `std::set` and `std::map`
* Boolean value (`!` and ```explicit operator bool```)
* Conversion to and from an integer type (`intptr_t`) - for platforms written in
  languages that don't support pointers

The only falsy/null/zero value is the default-constructed value. No guarantees
are made about the ordering produced by the `<` operator; only that there is
one.

### Creating `ElementIdentifier` values for your application

You will want to create named constants representing each identifier you want to
use in your code. These constants are defined using macros found in
[element_identifier.h](/ui/base/interaction/element_identifier.h).

Each declaration creates a compile-time constant that can be copied and used
anywhere in your application - they are valid from the time your application
starts up until it exits.

Furthermore, every `ElementIdentifier` you declare in code will either be
default-constructed (and therefore null), or be assigned a copy of one of these
compile-time constants. You should _never_ construct an `ElementIdentifier` or
assign a value from anything other than another `ElementIdentifier`.

To create a public, unique `ElementIdentifier` value in a global scope:
``` cpp
// This goes in the .h file:
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMyIdentifierName);

// This goes in the .cc file:
DEFINE_ELEMENT_IDENTIFIER_VALUE(kMyIdentifierName);
```
To create a class member that is a unique identifier, use:
``` cpp
// This declares a unique identifier, MyClass::kClassMemberIdentifier. You could
// also make the identifier protected or private if you wanted.
class MyClass {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(MyClass, kClassMemberIdentifier);
};

// This goes in the .cc file:
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MyClass, kClassMemberIdentifier);
```
And finally, if you want to create an identifier that's local to a .cc file or
class method, there is a single-line declaration available, as shown in these
examples:
``` cpp
// This declares a module-local identifier. The anonymous namespace is optional
// (though recommended) as kModuleLocalIdentifier will also be marked 'static'
// and the identifier's name will contain the file and line.
namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kModuleLocalIdentifier);
}

// This declares a method-local identifier. Again, the use of the local
// identifier will cause the generated name to include file and line number.
/* static */ ui::ElementIdentifier MyClass::GetIdentifier() {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMethodLocalIdentifier);
  return kMethodLocalIdentifier;
}
```
Please note that since all `ElementIdentifier`s created using these macros are
constexpr/compile-time constants, copies of these values can be used outside of
their originating classes or modules - the value of the `ElementIdentifier` is
valid anywhere in the application.

## UI Elements and element events

[`ElementTracker`](/ui/base/interaction/element_tracker.h) is a global singleton
object that allows you to:
* Retrieve a visible element or elements from a particular context by identifier
* Register for callbacks when elements with a given identifier and context
  become visible
* Register for callbacks when elements with a given identifier and context
  become hidden or are destroyed
* Register for callbacks when the user interacts with an element with a given
  identifier and context (known as _activation_)

`TrackedElement`, a polymorphic class defined in
[`element_tracker.h`](/ui/base/interaction/element_tracker.h), represents a
platform-agnostic UI element with an identifier and context. There must be a 1:1
correspondence between a visible named UI element and an
`TrackedElement`; the `TrackedElement` is what is passed to
callbacks when the UI element is shown, hidden, or activated.

Each framework has its own derived version of `TrackedElement` that may
provide additional information about the element. If you know what platform the
element is from you may use the `AsA()` template method to dynamically downcast
to the platform-specific element type. If you are working in an environment with
multiple presentation frameworks, you can use the `IsA()` method to determine if
the element is of the expected type.

Here is an example that shows some of the functionality of `ElementTracker` and
`TrackedElement`. Note that you must specify the `ElementContext` in
which you are listening:
``` cpp
void ListenForShowEvent(ui::ElementIdentifier id, ui::ElementContext context) {
  auto callback =
      base::BindRepeating(&MyClass::OnElementShown, base::Unretained(this)));
  subscription_ = ui::ElementTracker::GetElementTracker()
      ->AddElementShownCallback(id, context, callback);
}

void OnElementShown(ui::TrackedElement* element) {
// Technically you don't need the IsA() call here, since AsA() returns null if
// the object is the wrong type.
if (element->IsA<views::TrackedElementViews>()) {
  views::View* const view =
      element->AsA<views::TrackedElementViews>()->view();
  // Do something with the view that was shown here.
}
```

Then, in your production code, assign an element identifier to the element you
want to track:
``` cpp
// Note: matching DECLARE macro must go in header file.
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MyView, kMyElementIdentifier);

MyView::MyView() {
  auto* const child_view = AddChildView(std::make_unique<ChildViewType>());

  // This child view will now generate events with the given identifier.
  // The context will be derived from the widget the parent view is added to.
  child_view->SetProperty(views::kElementIdentifierKey, kMyElementIdentifier);
}
```
## Defining and following user interaction sequences

The `InteractionSequence` class provides a way to describe a sequence of
interactions between the application and the user (real or simulated). Sequences
are useful for e.g. creating user education tutorials guiding the user through
the steps of using a new feature, or for simulating user input during
interaction testing.

Each sequence consists of a series of steps of the following types:
* **Shown** - a UI element is or becomes visible to the user
* **Activated** - the element is visible and user clicks on or otherwise interacts with the UI element
* **Hidden** - a UI element is not visible or stops being visible to the user

These are respective to the corresponding events provided by
`ElementTracker`, but it is an important distinction that these signify
both events *and* the current element state.

Once a step runs, interaction sequence will watch for the event/state of
the next step to occur. For example, the next kShown/kHidden step starts if:

1. the element is visible/not visible at start of step
2. the element becomes visible/not visible during or after step transition

If SetTransitionOnlyOnEvent() is set to true, (1) does not apply.

All of the steps must be followed in order, or the sequence is
_aborted_. Callbacks may be registered at the start and end of each
step, and for when the sequence completes or aborts.

Events not in the sequence (other UI elements appearing, focus changes, mouse
hover, scroll) are ignored unless they result in a required UI element being
dismissed (such as if a dialog or menu is closed).

To create an interaction sequence, use a `InteractionSequence::Builder`. To add
steps to the builder, use an `InteractionSequence::StepBuilder`, or call a
convenience method like `WithInitialElement()`. Here is an example that expects
the user to interact with a feature entry point and then displays a help bubble
on the resulting dialog:

``` cpp
initial_element =
    ElementTracker::GetFirstMatchingElement(kFeatureEntryPointID, context());
sequence_ = InteractionSequence::Builder()
    .SetCompletedCallback(base::BindOnce(
        &MyClass::OnSequenceComplete,
        base::Unretained(this)))
    .AddStep(InteractionSequence::WithInitialElement(initial_element))
    .AddStep(InteractionSequence::StepBuilder()
        .SetElementID(initial_element->identifier())
        .SetType(StepType::kActivated)
        .Build())
    .AddStep(InteractionSequence::StepBuilder()
        .SetElementID(kFeatureDialogID)
        .SetType(StepType::kShown)
        .SetStartCallback(&MyClass::ShowHelpBubble, base::Unretained(this))
        .Build())
    .Build();
sequence_->Start();
```

### Interaction steps

Each `Step` has properties that can be set via its `StepBuilder`:
* **Type** - whether this is a show, activate, or hide step
* **Element ID** - the identifier of the element involved in the step
* **Must be visible at start** - the element must be visible at the start of the
  step or else the sequence is aborted
* **Must remain visible** - the element must remain visible until the next step
  begins; not compatible with **hidden** steps
* **Start callback** - called as soon as the step is started
* **End callback** - called as soon as the next step is started, or the sequence
  aborts or completes

Of these, only **Type** and **Element ID** are required. If you do not specify
whether the element must be visible at start or remain visible, default values
will be assigned according to the type of step. All callbacks are optional.

Instead of using `StepBuilder`, for the initial step you can call
`InteractionSequence::WithInitialElement()`. This creates a default **shown**
step for an element that is already visible; it expects the element to be
visible when `Start()` is called or the sequence will abort. You may pass
optional step start and end callbacks to `WithInitialElement()`; these are
useful for displaying an initial prompt to the user (in the case of a
tutorial).

There is an additional method on `StepBuilder`, `SetContext()`, but it is only
used by helper methods and for testing. You should instead use
`Builder::SetContext()` or `InteractionSequence::WithInitialElement()`. There
is currently no support for cross-context sequences and setting conflicting
contexts in a sequence is an error and will crash if DCHECK is enabled.

### Step callbacks

Each step callback (start or end) has three parameters:
* **Element** - the element involved in the step; null if the element is not
  available (i.e. was hidden before the callback could be called)
* **Identifier** - the `ElementIdentifier` associated with the step, which is
  always valid even if the element is null
* **Type** - the step type; **shown**, **activated**, or **hidden**

The **element** can be used to retrieve the UI element in your framework by
downcasting via `AsA()` - see
[UI Elements and element events](#UI-Elements-and-element-events) above.

Typically, when using a sequence to run a tutorial, this will be the code that
shows or hides a tutorial dialog or prompt. When using the sequence for
interaction testing, the callback will contain the code to simulate the next
input to the UI.

### Best practices

In general, it will be pretty obvious how to construct your sequence, because
you know the steps you need to perform in the UI to get where you want to go.
However, keep the following in mind:
* Try to start the sequence with a step generated by `WithInitialElement()`,
  keyed to a UI element you know will be visible when the sequence starts.
* Do not assume the order in which elements will become visible when a surface
  is shown.
* Do not assume that interacting with a button or menu item will bring up a
  resulting surface (another menu, a dialog) _before_ the initial button or
  menu item disappears.

To elaborate on the third point: it is better to have the following steps in the
case where a menu item brings up a dialog:
1. Menu item shown
2. Menu item activated (does not need to remain visible; default)
3. Dialog element shown (does not need to be visible at start; default)

If you specify that the menu item must stay visible or that the dialog element
must be visible at step start, the sequence could fail depending on the order in
which the presentation framework dismisses the menu and displays the dialog.

However, in the case where you want the user to navigate a series of submenus,
if the platform supports menu-open-via-hover you may not receive the
**activated** signal and a sequence like the following might work better:
1. Menu item shown
2. Submenu item shown (triggers as soon as the submenu is opened, regardless of
   how)
3. Submenu item activated
4. ...

### Known limitations

* Cannot nest sequences (might be able to in some cases via callbacks)
* Cannot provide alternate sets of steps in the same sequence
* Cannot skip ahead (e.g. if the user uses a shortcut key to bypass a menu)
* Cannot restart steps (e.g. if the user hovers a submenu containing the next
  element, then un-hovers it, then hovers it again)

All of these can be addressed if a relevant, concrete need is found.

## Supporting additional UI frameworks

If you want to use ElementTracker with a framework that isn't supported yet, you
must at minimum do the following:
1. Derive a class from `TrackedElement` representing visual elements in
   your framework.
2. Determine how `ElementContext`s are defined in your framework.
3. Implement code to create and register your derived element objects with
   `ElementTracker` when UI elements become visible to the user, send events
   when they are activated by the user (however you choose to define
   "activation"), and to unregister them when the element is no longer visible.

See [`ElementTrackerViews`](/ui/views/interaction/element_tracker_views.h) for
an example implementation.

When you are done, please add the folder containing the implementation code to
the [Supported Frameworks](#Supported-Frameworks) section below.

### 1. Derive a class from `TrackedElement`

When you derive a class from `TrackedElement` to use for your UI
framework, you are obliged to declare specific metadata in order to support
`IsA()` and `AsA()`. To do this, add the following to the class definition:
``` cpp
class TrackedElementMyPlatform {
 public:
  // This provides the required TrackedElement metadata support.
  DECLARE_ELEMENT_TRACKER_METADATA();
}

// In the corresponding .cc file:
DEFINE_ELEMENT_TRACKER_METADATA(TrackedElementMyPlatform)
```
You will also be expected to pass an immutable identifier and context into the
constructor, and if the element object stores a pointer or handle to the
associated UI element in your framework, that reference should not change for
the life of the element (and the element should not outlive the corresponding
framework object).

### 2. Define how `ElementContext` works in your framework

You will need a method to generate an `ElementContext` from a window or UI
element. The context identifies the primary window associated with the UI, such
as a browser or PWA window, the taskbar of an operating system's GUI, a file
browser window, etc. Good candidates are the handle of the primary window or the
address of the framework object that represents it. The value of the handle must
not change over the lifetime of that window.

If you do not already have one, you will probably want a helper method that
finds the primary window given a UI element that might be in secondary UI (such
as a dialog or menu).

In Views, we use the address of the primary window's `Widget` to construct the
`ElementContext` and we provide the `ElementTrackerViews::GetContextForView()`
method to fetch it. We also added the `Widget::GetPrimaryWindowWidget()` helper
method for finding the primary window.

### 3. Managing the lifetime of your elements and sending events

How your platform manages the lifetime of elements is entirely up to you. You
could create an `TrackedElement` whenever a named UI element in your
framework becomes visible to the user, or you could have every UI element
with an associated `ElementIdentifier` hold a permanent `TrackedElement`.

The one requirement is that a single `TrackedElement` must be associated
with any named UI element, and must remain associated with that implementation
as long as the element remains visible.

To register or unregister an element or send activation events, get the
`ElementTrackerFrameworkDelegate` from the `ElementTracker` class and call the
appropriate method:
``` cpp
auto* const delegate = ui::ElementTracker::GetFrameworkDelegate();

// Register a visible element:
delegate->NotifyElementShown(my_element);

// Notify that the element was activated by the user:
delegate->NotifyElementActivated(my_element);

// Unregister the element on hide:
delegate->NotifyElementHidden(my_element);
```
"Activation" requires some special discussion here, as what it means to be
activated is extremely context-specific. For example:
* A button is activated when the user clicks it
* A menu item is activated when the user performs the item's action
* A browser tab is activated when the user selects the tab with the mouse or
  keyboard

Activation should be the **default** action that occurs when the user directly
interacts with that UI element. So for example, the _Back_ button in a browser
can be clicked to return to the previous page, or long-pressed/dragged to open a
menu containing a list of previous pages. The single-click is the default
action, and therefore should be the action that results in
`NotifyElementActivated()` being called.

How you proxy events from UI elements to calls to
`ElementTrackerFrameworkDelegate` is entirely up to you. In Views we go through
a mediator object -
[`ElementTrackerViews`](/ui/views/interaction/element_tracker_views.h) - which
first maps the `Button`, `MenuItem`, etc. to an `TrackedElementViews`
before passing that object to the appropriate delegate method.

## Supported Frameworks

The following UI frameworks support `ElementTracker` and `InteractionSequence`.
Please add additional frameworks to this list as they become supported.

* Views:
  * [`ElementTrackerViews`](/ui/views/interaction/element_tracker_views.h)
  * [`InteractionSequenceViews`](/ui/views/interaction/interaction_sequence_views.h)
