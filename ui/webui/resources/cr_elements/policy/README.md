# Policy indicators

Settings that can't be controlled by the current user often show an icon and a
tooltip explaining why. This happens when a setting is:

* enforced by user policy, or different from a policy's "recommended" value
* overridden by an extension
* or (on Chrome OS):
    * enforced/recommended by device policy (for enrolled devices)
    * set by the device owner (for non-enrolled devices)
    * controlled by the primary user (for multiple profile sessions)

## Indicator UI

The badge icons are sourced from [cr_elements/icons.html] by default.

Indicators show a tooltip with explanatory text on hover if `CrPolicyStrings`
is set; see [settings_ui.ts] for an example from MD Settings.

## Using an indicator

`<cr-policy-indicator>` is provided to be reused in WebUI pages:
<cr-policy-indicator indicator-type="userPolicy"></cr-policy-indicator>

[cr_elements/icons.html]: ../icons.html
[settings_ui.ts]: /chrome/browser/resources/settings/settings_ui/settings_ui.ts
