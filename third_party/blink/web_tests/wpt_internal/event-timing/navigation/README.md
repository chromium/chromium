The tests in this folder expect to be run in two ways:
- With a flag (NavigationEventTiming)
- Without that flag

Both cases have expectations:
- With the flag: that specific event timings fire with interactionId.
- Without the flag: that these events do NOT fire, and no other visible effects
  happen, i.e. interactionCount doesn't increment.

For this, we set up a VirtualTest suite.

The tests themselves fork expectations based on the flag setting.
