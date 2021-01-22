/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests the behavior of anisotropic filtering.

TODO:
Note that anisotropic filtering is never guaranteed to occur, but we might be able to test some
things. If there are no guarantees we can issue warnings instead of failures. Ideas:
  - No *more* than the provided maxAnisotropy samples are used, by testing how many unique
    sample values come out of the sample operation.
  - Result with and without anisotropic filtering is different (if the hardware supports it).
  - Check anisotropy is done in the correct direciton (by having a 2D gradient and checking we get
    more of the color in the correct direction).
More generally:
  - Test very large and very small values (even if tests are very weak).
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
