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

NOTE (19 Dec 2023): cr_elements are currently being forked between Ash and
Desktop, see https://crbug.com/1512231. *Do not make new CrOS-only changes in
this folder*; instead make such changes in the Ash fork at
ash/webui/resources/common/cr_elements. If an element that is not yet forked
requires a CrOS-only change, comment on the bug to expedite forking it.
