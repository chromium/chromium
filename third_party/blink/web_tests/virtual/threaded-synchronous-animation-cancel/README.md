This directory is dedicated for testing animations on the compositor thread
with asynchonous cancellation of composited animations disabled.

The purpose is to ensure we do not regress the fallback position in the event
that we need to turn off the feature.
