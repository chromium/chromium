# This suite runs tests with --disable-features=FormControlsVerticalWritingModeDirectionSupport --enable-features=NonStandardAppearanceValueSliderVertical.

This test suite is to make sure that, when the feature FormControlsVerticalWritingModeDirectionSupport is disabled, vertical writing mode elements will have the same behavior for direction rtl and ltr. This test can be removed when the feature is shipped.

Further, with feature NonStandardAppearanceValueSliderVertical enabled, having CSS appearance value slider-vertical on a range should make the element go vertical with value bottom-to-top.
