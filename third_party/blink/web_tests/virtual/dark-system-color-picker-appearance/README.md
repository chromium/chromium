This suite runs tests with:
--blink-settings=preferredColorScheme=0
--enable-features=SystemColorChooser

Currently the SystemColorChooser feature is only being used by macOS.
You can find the macOS baselines here:

../../platform/mac*/virtual/dark-system-color-picker-appearance/

The baselines found in this directory are for platforms that
don't enable SystemColorChooser but still use dark mode.
