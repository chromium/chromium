/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
- Baseline tests checking vertex/instance IDs, with:
    - No vertexState at all (i.e. no vertex buffers)
    - One vertex buffer with no attributes
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
