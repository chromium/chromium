This directory contains complex Lit web components for Web UI (as well as some
legacy Polymer components; no new Polymer components may be added). They may be
shared between Settings, login, stand alone dialogs, etc.

These components are allowed to use I18nMixinLit. The Web UI hosting these
components is expected to provide loadTimeData with any necessary strings.

These components may also use Mojo APIs (through a browser proxy) or extension
APIs (e.g. chrome.settingsPrivate). The C++ code hosting the component is
expected to handle these calls.

For simpler components with no I18n, Mojo, or chrome dependencies, see
cr_elements.

See [build_webui()](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/webui/webui_build_configuration.md#build_webui)
as well as example from existing cr_components/ subfolders, for the recommended
way of building individual cr_components/.

cr_components/ that have a dedicated ts_library() or build_webui() target should
not use relative paths to other files in ui/webui/resources/, and users of these
components must add a dependency on the ts_library target to use the component.
