/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- for sampleCount = 4, alphaToCoverageEnabled = true and various combinations of:
    - rasterization masks
    - increasing alpha values of the first color output including { < 0, = 0, = 1/16, = 2/16, ..., = 15/16, = 1, > 1 }
    - alpha values of the second color output = { 0, 0.5, 1.0 }.
- test that for a single pixel in { first, second } { color, depth, stencil } output the final sample mask is applied to it, moreover:
    - if alpha is 0.0 or less then alpha to coverage mask is 0x0,
    - if alpha is 1.0 or greater then alpha to coverage mask is 0xFFFFFFFF,
    - that the number of bits in the alpha to coverage mask is non-decreasing,
    - that the computation of alpha to coverage mask doesn't depend on any other color output than first,
    - (not included in the spec): that once a sample is included in the alpha to coverage sample mask
      it will be included for any alpha greater than or equal to the current value.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
