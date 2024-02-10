# This suite runs tests with --disable-features=FormControlsVerticalWritingModeDirectionSupport, NonStandardAppearanceValueSliderVertical.

This test suite is to make sure that, when the feature FormControlsVerticalWritingModeDirectionSupport is disabled, vertical writing mode elements will have the same behavior for direction rtl and ltr. This test can be removed when the feature is shipped.

Further, with feature NonStandardAppearanceValueSliderVertical disabled, having CSS appearance value slider-vertical on a range should not make the element go vertical anymore.
