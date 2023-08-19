# //ui/base

This directory contains low-level code that is reused throughout the chromium UI
stack. Code in this directory can't depend on most of the rest of //ui (with a
few exceptions), and code throughout the rest of //ui can depend on it.

Toolkit-specific libraries in this directory include:

* [cocoa](cocoa): for working with Cocoa on macOS
* [wayland](wayland): for Linux systems with Wayland
* [webui](webui): for WebUI in Chromium
* [win](win): for Windows systems
* [x](x): for Linux systems with X11

Platform-independent libraries in this directory include:

* [l10n](l10n): localization APIs used throughout all of chromium, especially
  the widely-used `l10n_util::GetString*` functions
* [metadata](metadata): the implementation of the property/metadata system used
  throughout [views]
* [models](models): toolkit-agnostic types used to represent the data shown in
  UI controls and dialogs
* [resource](resource): resource bundle APIs, which are used for retrieving
  icons and raw localized strings from the app package

As with most "base" libraries this is somewhat of a dumping ground of code that
is used in a bunch of other places, and there's no hard and fast rule to tell
what should or shouldn't be in here. If in doubt, consult with the
[owners](OWNERS).

[views]: ../views
