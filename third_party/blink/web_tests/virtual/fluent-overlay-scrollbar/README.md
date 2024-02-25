The suit contains web tests for Fluent overlay scrollbar that can be
enabled via the following feature flag: `--enable-features=FluentOverlayScrollbar`.
Please see more details here: https://crbug.com/1479156.

If you are trying to rebase Win10 expectations in a Win11 machine, you can
follow these instructions to temporarily disable Win11 arrows:
1. Navigate to: `ui/native_theme/native_theme_constants_fluent.h`

2. Change `"Segoe Fluent Icons"` value to another name. For example,
`"Segoe Fluent Icons1"`.

3. Rebuild;
`$ autoninja blink_web_tests`
