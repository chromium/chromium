Tests for the NewBaseUrlInheritanceBehavior feature.

This feature changes the manner in which fallback base urls are inherited,
snapshotting them from the frame's opener (at time of creation/navigation)
instead of from the frame's parent.

Some tests require the feature to be enabled in order to work, and hence
must be run in this virtual test suite to operate as intended.
