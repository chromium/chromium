This test suite contains ALL tests that have composited clip path animations,
even incidentally. This suite ensures that composited clip path animations
function properly.

To facilitate this, this suite needs the impl thread to actually exists, as
paint worklet based animations are not defined for when there is no impl thread
task runner.

As such, this suite runs with the arguments:

--enable-blink-features=CompositeClipPathAnimation
--enable-threaded-compositing

Because composited clip path aniamtions do not work without threaded
compositing, all tests in this suite must be virtual only if this feature is
enabled-by-default. If this feature is ever disabled from a previously-enabled
state, these tests should all be re-enabled to allow main threaded clip path
animations to have coverage.
