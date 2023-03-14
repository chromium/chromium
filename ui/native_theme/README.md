# Native Theme Rendering for UI

This directory contains source needed to draw UI elements in a native fashion,
either by actually calling native APIs or by emulating/reimplementing native
behavior. The primary class is [NativeTheme](native_theme.h), which provides a
cross-platform API for things like "is the system in dark mode" and "how do I
draw/color various bits of UI".
