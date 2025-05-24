# virtual/align-surface-layer

## Summary

At present, the feature AlignSurfaceLayerImplToPixelGrid is off by
default. In order that the tests written specifically for this feature
are also run with the feature enabled, this virtual test suite turns
the feature on for the required tests.

The associated tests are marked as FAIL in TestExpectations, but should
not crash.
