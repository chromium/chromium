# Elements and Interaction Sequences

This folder contains primitives for locating named elements in different
application windows as well as following specific sequences of user interactions
with the UI. It is designed to support things like User Education Tutorials and
interactive UI testing, and to work with multiple UI presentation frameworks.

See the [Supported Frameworks](#Supported%20Frameworks) section for a list of
currently-supported  frameworks.

[TOC]

## Named elements

Named elements are the fundamental building blocks of these systems. Each
supported framework must define how a UI element can be assigned an
[ElementIdentifier](/ui/base/interaction/element_identifier.h) which its
[framework implementation](#Framework%20implementation) must be able to read.
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
[the relevant section](#2.%20Define%20how%20`ElementContext`%20works%20in%20your%20framework)
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
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kClassMemberIdentifier);
};

// This goes in the .cc file:
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MyClass, kClassMemberIdentifier);
```
And finally, if you want to create an identifier that's local to a .cc file or
class method, there is a single-line declaration available, as shown in these
examples:
``` cpp
// This declares a module-local identifier. The anonymous namespace is optional
// (though recommended) as kModuleLocalIdentifier will also be marked 'static'.
namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kModuleLocalIdentifier);
}

// This declares a method-local identifier.
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

`ElementTrackerElement`, a polymorphic class defined in
[`element_tracker.h`](/ui/base/interaction/element_tracker.h), represents a
platform-agnostic UI element with an identifier and context. There must be a 1:1
correspondence between a visible named UI element and an
`ElementTrackerElement`; the `ElementTrackerElement` is what is passed to
callbacks when the UI element is shown, hidden, or activated.

Each framework has its own derived version of `ElementTrackerElement` that may
provide additional information about the element. If you know what platform the
element is from you may use the `AsA()` template method to dynamically downcast
to the platform-specific element type. If you are working in an environment with
multiple presentation frameworks, you can use the `IsA()` method to determine if
the element is of the expected type.

Here is an example that shows some of the functionality of `ElementTracker` and
`ElementTrackerElement`. Note that you must specify the `ElementContext` in
which you are listening:
``` cpp
void ListenForShowEvent(ui::ElementIdentifier id, ui::ElementContext context) {
  auto callback =
      base::BindRepeating(&MyClass::OnElementShown, base::Unretained(this)));
  subscription_ = ui::ElementTracker::GetElementTracker()
      ->AddElementShownCallback(id, context, callback);
}

void OnElementShown(ui::ElementTrackerElement* element) {
// Technically you don't need the IsA() call here, since AsA() returns null if
// the object is the wrong type.
if (element->IsA<views::ElementTrackerElementViews>()) {
  views::View* const view =
      element->AsA<views::ElementTrackerElementViews>()->view();
  // Do something with the view that was shown here.
}
```
## Defining and following user interaction sequences

*(Add documentation for `InteractionSequence` here)*

...

## Supporting additional UI frameworks

If you want to use ElementTracker with a framework that isn't supported yet, you
must at minimum do the following:
1. Derive a class from `ElementTrackerElement` representing visual elements in
   your framework.
2. Determine how `ElementContext`s are defined in your framework.
3. Implement code to create and register your derived element objects with
   `ElementTracker` when UI elements become visible to the user, send events
   when they are activated by the user (however you choose to define
   "activation"), and to unregister them when the element is no longer visible.

See [`ElementTrackerViews`](/ui/views/interaction/element_tracker_views.h) for
an example implementation.

When you are done, please add the folder containing the implementation code to
the [Supported Frameworks](#Supported%20Frameworks) section below.

### 1. Derive a class from `ElementTrackerElement`

When you derive a class from `ElementTrackerElement` to use for your UI
framework, you are obliged to declare specific metadata in order to support
`IsA()` and `AsA()`. To do this, add the following to the class definition:
``` cpp
class ElementTrackerElementMyPlatform {
 public:
  // This provides the required ElementTrackerElement metadata support.
  DECLARE_ELEMENT_TRACKER_METADATA();
}

// In the corresponding .cc file:
DEFINE_ELEMENT_TRACKER_METADATA(ElementTrackerElementMyPlatform)
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
could create an `ElementTrackerElement` whenever a named UI element in your
framework becomes visible to the user, or you could have every UI element
with an associated `ElementIdentifier` hold a permanent `ElementTrackerElement`.

The one requirement is that a single `ElementTrackerElement` must be associated
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
[`ElementTrackerViews`](ui/views/interaction/element_tracker_views.h) - which
first maps the `Button`, `MenuItem`, etc. to an `ElementTrackerElementViews`
before passing that object to the appropriate delegate method.

## Supported Frameworks

The following UI frameworks support `ElementTracker` and `InteractionSequence`.
Please add additional frameworks to this list as they become supported.

* Views:
  * [`ElementTrackerViews`](/ui/views/interaction/element_tracker_views.h)
