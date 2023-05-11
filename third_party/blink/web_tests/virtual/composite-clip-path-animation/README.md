This test suite contains ALL tests that have composited clip path animations,
even incidentally. This suite ensures that composited clip path animations
function properly.

To facilitate this, this suite needs the impl thread to actually exist, as
paint worklet based animations are not defined for when there is no impl thread
task runner.

As such, this suite runs with the arguments:

--enable-blink-features=CompositeClipPathAnimation
--enable-threaded-compositing

Because composited clip path aniamtions do not work without threaded
compositing, all tests in this suite must be virtual only if this feature is
enabled-by-default. A corresponding suite for main thread composited animations
exists to ensure main thread coverage is not lost

If the Composite Clip Path animation is ever removed due to this feature
maturing, this suite will need to be moved to the threaded compositing virtual
suite. In that case, every directory/file currently tested for this suite
would be need to be added, as well as being added to the list of exclusive
directories for that suite.
