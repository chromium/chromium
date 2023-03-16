This directory contains complex Polymer web components for Web UI. They may be
shared between Settings, login, stand alone dialogs, etc.

These components are allowed to use I18nBehavior. The Web UI hosting these
components is expected to provide loadTimeData with any necessary strings.
TODO(stevenjb/dschuyler): Add support for i18n{} substitution.

These components may also use chrome and extension APIs, e.g. chrome.send
(through a browser proxy) or chrome.settingsPrivate. The C++ code hosting the
component is expected to handle these calls.

For simpler components with no I18n or chrome dependencies, see cr_elements.

See [build_webui()](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/webui_build_configuration.md#build_webui)
as well as example from existing cr_components/ subfolders, for the recommended
way of building individual cr_components/.

cr_components/ that have a dedicated ts_library() or build_webui() target should
not use relative paths to other files in ui/webui/resources/, and users of these
components must add a dependency on the ts_library target to use the component.
