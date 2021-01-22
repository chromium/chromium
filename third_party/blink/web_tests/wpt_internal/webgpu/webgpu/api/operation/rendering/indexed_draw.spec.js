/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for the indexing-specific aspects of drawIndexed/drawIndexedIndirect.

TODO: plan and implement
- Test indexed draws with the combinations:
  - Renderable cases:
    - indexCount {=, >} the required points of primitive topology and
      {<, =} the size of index buffer
    - instanceCount is {1, largeish}
    - {firstIndex, baseVertex, firstInstance} = 0
    - firstIndex  {<, =} the size of index buffer
  - Not renderable cases:
    - indexCount = 0
    - indexCount < the required points of primitive topology
    - instanceCount = {undefined, 0}
    - firstIndex out of buffer range
    - firstIndex largeish
  - x = {drawIndexed, drawIndexedIndirect}
  - x = index formats
- ?
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
