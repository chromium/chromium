/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- Copy GPUBuffer to another thread while {pending, mapped mappedAtCreation} on {same,diff} thread
- Destroy on one thread while {pending, mapped, mappedAtCreation, mappedAtCreation+unmap+mapped}
  on another thread.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
