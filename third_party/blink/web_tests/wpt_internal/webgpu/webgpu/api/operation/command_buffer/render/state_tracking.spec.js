/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Ensure state is set correctly. Tries to stress state caching (setting different states multiple
times in different orders) for setIndexBuffer and setVertexBuffer.
Equivalent tests for setBindGroup and setPipeline are in programmable/state_tracking.spec.ts.
Equivalent tests for viewport/scissor/blend/reference are in render/dynamic_state.spec.ts

TODO: plan and implement
- try setting states multiple times in different orders, check state is correct in a draw call.
    - setIndexBuffer: specifically test changing the format, offset, size, without changing the buffer
    - setVertexBuffer: specifically test changing the offset, size, without changing the buffer
- try changing the pipeline {before,after} the vertex/index buffers.
  (In D3D12, the vertex buffer stride is part of SetVertexBuffer instead of the pipeline.)
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
