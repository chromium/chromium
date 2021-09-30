Fenced Frames + ShadowDOM

This directory contains <fencedframe> tests that exercise the `blink::features::kFencedFrames`
feature. Specifically, they exercise the default implementation mode of fenced frames, which is
`blink::features::FencedFramesImplementationType::kShadowDOM`.

The test are also run exercising the MPArch implementation path via the virtual test suite (see
the `VirtualTestSuites` file).