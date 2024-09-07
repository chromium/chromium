# Actions and ActionManager

For the purposes of this document, feature, command, and action will refer to a
single user- invoked (directly or indirectly) intent. This can be done via a
menu item, toolbar button, hotkey or accessible event.

## Intent of system

The ActionManager model as a centrally located, dynamically constructed registry
of available commands. These commands or ActionItems contain all the most common
information about the specific item. This includes the command title or name,
an optional icon, its current visibility and/or enabled state among others.
Additionally, this action item is the sole object by which the command is
executed.

This system is intended to use a late-bound construction mechanism. The
global ActionManager need not have any knowledge or understanding of what
ActionItems are available, only that it holds a hierarchical list of said
actions.

ActionItem instances are constructed and registered with the ActionManager at
startup or on-demand using an init callback or other equivalent mechanism. This
registration is usually done within the code related to a given feature or
entrypoint so each feature is physically located within the codebase next to its
implementation. This leads to better modularity and isolation.

The ActionManager may be used as the basis to allow the user to discover
available commands/features via a free-form text “search”. Since all the action
instances are centrally registered into the global ActionManager, they are
inherently searchable and discoverable.

## Use of system

ActionItems can then be attached to any number of separate UI elements which
will then self-configure based on the data and properties of that specific
instance. UI elements that traditionally surface feature “entry-points” are most
likely to be used. This includes menu items, toolbar buttons, and other kinds of
single-click elements.

**Note:** How the ActionItems are "attached" to the UI elements is wholly
dependent on how that particular UI framework interacts with ActionItems. See
[Integrating Actions and Views](/ui/views/actions/README.md) for an example how
ActionItems could be integrated into a UI framework.

## Defining ActionIds

Since this self-configuration will happen when a UI element references a given
ActionItem instance, a user-configurable UI can be built on top of this system.
Some mechanism needs to be able to persist this relationship across application
restarts.

ActionIds provide a way to persist a reference to a given ActionItem
into a user-specific profile. When the application is restarted, the ActionIds
are loaded from this profile and then used to lookup the specific ActionItem
instance in order to reconstitute the user-defined UI configuration.

In order to maintain as much modularity as possible, ActionIds can be defined
in several locations while also guarding against collisions. The only
requirement for ActionIds is that their name remain consistent. Their ordinal
value is irrelevant. This allows they to be arranged within the source code in
a logical and discoverable manner, such as by category or just alphabetically.

The [ActionId identifiers are converted to/from strings](https://source.chromium.org/chromium/chromium/src/+/main:ui/actions/actions.h;drc=d1dd0de451bff972745cdd0018abfcd0f9d7cd59;l=399) which can be persisted to
a user-profile. This allows the list of ActionIds to change physical order
within the source code without worrying about maintaining a fixed ordinal value.
As long as the actionid source-level identifier remains consistent, reading old
profile data will always work.

See [chrome_action_ids.h](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/actions/chrome_action_id.h)
for an example of how they are defined on a large scale.
