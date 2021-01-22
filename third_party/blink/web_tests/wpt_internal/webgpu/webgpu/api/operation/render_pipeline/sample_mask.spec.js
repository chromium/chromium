/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- for sampleCount = { 1, 4 } and various combinations of:
    - rasterization mask = { 0, 1, 2, 3, 15 }
    - sample mask = { 0, 1, 2, 3, 15, 30 }
    - fragment shader output mask (SV_Coverage) = { 0, 1, 2, 3, 15, 30 }
- test that final sample mask is the logical AND of all the
  relevant masks -- meaning that the samples not included in the final mask are discarded
  for all the { color outputs, depth tests, stencil operations } on any attachments.
- [choosing 30 = 2 + 4 + 8 + 16 because the 5th bit should be ignored]
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
