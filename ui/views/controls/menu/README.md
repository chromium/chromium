# Views Menus

This document outlines how menus are implemented in Views. You should probably
read the [Views overview] if you haven't yet.

[TOC]

## Key Classes in //ui/views/controls/menu

  * [MenuController]
  * [SubmenuView]
  * [MenuRunner]
  * [MenuHost]
  * [MenuItemView]

## Key Classes Elsewhere

  * [ui::MenuModel]

## Creating & Showing A Menu

Conceptually, client code uses Views menus like so:

    MenuRunner runner(model, ...);
    runner.RunMenuAt(...);

The constructor of MenuRunner does little actual work; it is primarily concerned
with choosing an appropriate [MenuRunnerImplInterface] depending on the platform
and the requested type of menu. This document will mostly focus on
MenuRunnerImpl, which runs a Views menu; the only other MenuRunnerImplInterface
implementation, named [MenuRunnerImplCocoa], runs a native Mac menu instead
which has extremely different behavior.

The call to `MenuRunnerImpl::RunMenuAt` is responsible for determining which
MenuController will control this menu invocation, which may involve either
nesting the menu off an existing parent menu or cancelling an existing menu
first. In this context, "nesting" specifically means a fully separate menu that
runs while another menu is still running. Here's an example menu:

    Item 1
      Item 1.1
      Item 1.2
    Item 2
    Item 3

Mousing over Item 1 would open the submenu containing Items 1.1 and 1.2, but
that submenu is controlled by the same MenuController instance as the original
menu containing Item 1. However, if the user right-clicked on Item 2 to open its
context menu, that would create a nested MenuController to run the context menu.
At any given time, only one MenuController can be active, since menus are
conceptually modal.

Once that's done, `MenuController::Run` takes over.

That method has many responsibilities, but explaining them will require a brief
detour into the structure of MenuController itself.

## MenuController

MenuController is the "uber class" of the Views menu system. It has a large
amount of state and nearly all interesting logic is delegated to it, but the most important things it stores are:

  * The current and pending [MenuController::State]
  * A stack of prior states for parent MenuControllers, if the current
    MenuController is nested
  * The active mouse view, which is the view (if there is one) that the mouse is
    over; this is only used for dragging
  * The "hot tracked button" - this is a button which is a descendant view of
    the selected MenuItemView, used to deliver mouse events to both that subview
    and the MenuItemView itself, which can be useful for (eg) drawing hover
    effects
  * Various timers and locations used for animation & display
  * The "pre-target handler", which listens for any mouse click anywhere and
    closes the menu if they are outside the menu

The MenuController also contains the logic to allow dragging within menus, which
is used in bookmark menus to support reordering items.

## Menu Running & Selection

Back to `MenuController::Run`: after setting up some state, this method invokes
`MenuController::SetSelection`. That method is responsible for changing the
selection from the current selected MenuItemView (which may be nullptr) to a
provided new MenuItemView (which again may be nullptr), which involves closing
and opening submenus as needed and notifying accessibility events up and down
the menu tree. For example, if the current menu is:

    A [open]
      A1
      A2 [open]
          A2.1
          A2.2 [selected]
      A3
          A3.1

and the new selected node is A3.1, `MenuController::SetSelection` would be
responsible for:

  * Unselecting A2.2
  * Closing A2
  * Opening A3
  * Selecting A3.1

Note that, if the selection is not immediate, this fills the pending selection
but not the actual selection until later, to allow for menus to animate out and
in a bit; if this isn't done, menus flicker in and out as the mouse moves over
multiple items with submenus.

When the selection *is* immediate, as it is when invoked by
`MenuController::Run`, SetSelection will end up opening the root menu for this
MenuController (via `MenuController::CommitPendingSelection`). This method
handles actually closing and opening menus as needed to make the pending
selection visible. Since there is no existing open menu during the initial call,
practically this calls `MenuController::OpenMenu` on the first selectable item
in the menu.

That method, in turn, ultimately calls `SubmenuView::ShowAt` on the root
MenuItemView's submenu, which is the root submenu. That method constructs a
MenuHost, which is a special kind of [Widget] that contains a SubmenuView. The
MenuHost is responsible for:

  * Detecting touch events anywhere (not just in the menu) and forwarding them
    to the MenuController to maybe cancel the menu if they are out of bounds
  * Managing mouse capture if the menu has capture (some but not all menus do
    this)

Once `MenuHost::InitMenuHost` and `MenuHost::ShowMenuHost` are done, the menu is
on screen!

## Painting

Once a menu is on screen, its view tree looks like this:

    MenuHost (Widget)
      MenuHostRootView
        MenuItemView (root)
          MenuScrollViewContainer
            SubmenuView
              MenuItemView
              MenuItemView
              ...

None of these have any visuals except the MenuItemView, which contains:

  * A title, a secondary title, and an icon
  * A checkmark/radio marker
  * "Minor" text and icon
  * Arbitrary other child views

None of these things are separate Views - instead they are drawn directly by
`MenuItemView::OnPaint`, so the painting step for MenuItemView is also the
layout step.

## Event Handling

MenuController centralizes input event handling for menus. Key events enter
MenuController from multiple sources:

  * SubmenuView, via the normal Views event path
  * MenuHostRootView, which overrides Views hit-testing to ensure that mouse
    events within the MenuHost always go to the controller
  * Widget, which will dispatch incoming key events to a running MenuController
    if there is one - this allows the Widget to retain activation while the menu    handles events

MenuController::OnWillDispatchKeyEvent is one of the entry points to this logic,
but MenuController::OnKeyPressed handles all of the navigation and functional
keys, and MenuController::SelectByChar implements incremental searching and menu
accelerators.

Mouse events are mostly handled via the normal Views flow, except that
pre-target handlers can deliver mouse events to MenuController that did not
actually target the menu's widget, which MenuController uses to close itself in
response to those events.

Events can also be "reposted", where the menu decides to dismiss itself in
response to them and then propagate them up to the menu's parent widget. This
behavior is platform-specific and tricky, so it's best to read the code to
understand it.

## Positioning

TODO(ellyjones): How does this work?

## Drag & Drop

TODO(ellyjones): How does this work?

[Views overview]: ../../../../docs/ui/views/overview.md
[MenuController]: menu_controller.h
[MenuController::State]: https://source.chromium.org/chromium/chromium/src/+/main:ui/views/controls/menu/menu_controller.h;drc=ce8d17ff494cf684f35c8ff64cb6bd0947adcf46;bpv=0;bpt=1;l=289
[MenuHost]: menu_host.h
[MenuItemView]: menu_item_view.h
[MenuRunner]: menu_runner.h
[MenuRunnerImplCocoa]: menu_runner_impl_cocoa.h
[MenuRunnerImplInterface]: menu_runner_impl_interface.h
[SubmenuView]: submenu_view.h
[ui::MenuModel]: ../../base/models/menu_model.h
[Widget]: ../widget/widget.h

## Ownership

Unlike most other UI code, menus decouple their lifetimes from that of the
Widgets containing them as best they can by marking the members displayed
in Widgets by using `View::set_owned_by_client(true)`. The below diagram
gives an overview of the ownership relationships between the key menu classes.

```
                                                                                    ┌──────────────────────────┐
                                                                                    │ MenuHost : Widget        │
                                                                                    │                          │
                                                                                    │                          │
                                                                                    │                          │
                                                                                    └──┬───────────────────────┘
                                                                                       │
                                                                                       │ Owns 1
                                                                                       │
┌──────────────────────────┐Raw pointer   ┌──────────────────────────┐              ┌──▼───────────────────────┐
│ ui::MenuModel            ├──────────────► MenuModelAdapter         │              │ MenuHostRootView : View  │
│                          │              │                          │              │                          │
│                          │Raw pointer   │ Implements MenuDelegate, │              │                          │
│                          ◄──────────────┤ ui::MenuModelDelegate.   │              │                          │
└──────────────────────────┘              └───▲──────────────────────┘              └──┬───────────────────────┘
                                              │                                        │
                                              │ Raw pointer to                         │ Contains, but does not
                                              │ MenuDelegate.                          │ own
┌──────────────────────────┐                  │                                     ┌──▼───────────────────────┐
│ MenuRunner               │                  │                                     │ MenuScrollViewContainer :│
│                          │                  │                                     │ View                     │
│                          │                  │                                     │                          │
│                          │                  │                                     │ Is client-owned.         │
└──┬───────────────────────┘                  │                                     └──▲────────────┬──────────┘
   │ Owns (de facto)                          │                                        │            │
   │                                          │                                        │Owns 1      │ Contains
   │                                          │                                        │            │
┌──▼───────────────────────┐ Owns 1       ┌───┴──────────────────────┐ Owns 0 to 1  ┌──┴────────────▼──────────┐ Owns n     ┌──────────────────────────┐
│ MenuRunnerImpl           ├──────────────► MenuItemView : View      ├──────────────► SubMenuView : View       ├────────────► View (including          ├───► Continue
│                          │ Owns n to n  │                          │              │                          │            │ MenuItemView)            │     recursively
│                          ├───────────┐  │ The main menu.           │              │                          │            │                          │     (tree of
│                          │           │  │                          ├───────────┐  │ Is client-owned.         │            │                          │     submenus)
└──┬───────────────────────┘           │  └──┬───────────────────────┘  Owns n   │  └──────────────────────────┘            └──────────────────────────┘
   │ Creates and deletes        Weak ptr.    │                          (as views│
   │ (de facto)                ┌───────┬─────┘                          children)│
   │                           │       │                                         │
┌──▼───────────────────────┐   │       │  ┌──────────────────────────┐           │  ┌──────────────────────────┐
│ MenuController           ◄───┘       └──► MenuItemView : View      │           └──► View                     │
│                          │              │                          │              │                          │
│ Singleton, at most one   │              │ Sibling menus (relevant  ├───┐          │                          │
│ instance active globally.│              │ for drag & drop).        │   │          │                          │
└──────────────────────────┘              └──────────────────────────┘   │          └──────────────────────────┘
                                                                         │
                                                                       Same type of
                                                                       children as
                                                                       main menu
```
