This directory contains simple Polymer web components for Web UI. These
components may be shared across any WebUI and should be compatible across
all platforms (including ios).

These web components may not contain any i18n dependencies and may not use
I18nBehavior. Instead, any text (labels, tooltips, etc) should be passed as
properties.

These web components should avoid the use of chrome.send and should generally
avoid dependencies on extension APIs as well.

TODO(dpapad): Audit elements currently using chrome.settingsPrivate and decide whether to move these or update the
guidelines.

For more complex components, see cr_components.
