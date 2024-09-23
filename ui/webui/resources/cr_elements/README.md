This directory contains simple web components for Web UI. These components may
be shared across any WebUI. Non-Polymer/Lit components should be compatible
across all platforms (including ios). Polymer/Lit components currently are not
supported on iOS or Android.

These web components must be used in at least 3 different WebUI surfaces. In
some cases, an exception may be made for a component used in 2 surfaces, if
there is a strong justification for why a more limited sharing model (see
chrome/browser/resources/settings_shared/ for an example) is not more
appropriate.

These web components must have corresponding unit tests in
chrome/test/data/webui/cr_elements.

These web components may use loadTimeData and i18nMixin, but may not use
i18n{...} replacements.

These web components should avoid the use of chrome.send/Mojo APIs and
should avoid dependencies on extension APIs as well.

For more complex components, see cr_components.
