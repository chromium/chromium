/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for the indirect-specific aspects of drawIndirect/drawIndexedIndirect.

TODO: plan and implement
- indirect draws:
    - indirectBuffer is {valid, invalid, destroyed, doesn't have usage)
    - indirectOffset is {
        - 0, 1, 4
        - b.size - sizeof(args struct)
        - b.size - sizeof(args struct) + min alignment (1 or 2 or 4)
        - }
    - x= {drawIndirect, drawIndexedIndirect}
- ?
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
