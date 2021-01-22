/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for the general aspects of draw/drawIndexed/drawIndirect/drawIndexedIndirect.

TODO: plan and implement
- draws (note bind group state is not tested here):
    - various zero-sized draws
    - draws with vertexCount not aligned to primitive topology (line-list or triangle-list) (should not error)
    - index buffer is {unset, set}
    - vertex buffers are {unset, set} (some that the pipeline uses, some it doesn't)
      (note: to test this, the shader in the pipeline doesn't have to actually use inputs)
    - x= {draw, drawIndexed, drawIndirect, drawIndexedIndirect}
- ?
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
