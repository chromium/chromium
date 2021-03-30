/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';
export const description = `
Test uninitialized buffers are initialized to zero when read
(or read-written, e.g. with depth write or atomics).

TODO
`;

export const g = makeTestGroup(GPUTest);
