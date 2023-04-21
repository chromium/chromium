This directory contains simple web components for Web UI. These components may
be shared across any WebUI. Non-Polymer components should be compatible across
all platforms (including ios). Polymer components currently are not supported
on iOS or Android.

These web components may use loadTimeData and i18nMixin, but may not use
i18n{...} replacements.

These web components should avoid the use of chrome.send and should generally
avoid dependencies on extension APIs as well.

TODO(dpapad): Audit elements currently using chrome.settingsPrivate and decide whether to move these or update the
guidelines.

For more complex components, see cr_components.
