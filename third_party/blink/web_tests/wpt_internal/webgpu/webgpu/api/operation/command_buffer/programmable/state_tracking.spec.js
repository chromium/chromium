/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Ensure state is set correctly. Tries to stress state caching (setting different states multiple
times in different orders) for setBindGroup and setPipeline.

TODO: for each programmable pass encoder {compute pass, render pass, render bundle encoder}
- try setting states multiple times in different orders, check state is correct in draw/dispatch.
    - Changing from pipeline A to B where both have the same layout except for {first,mid,last}
      bind group index.
    - Try with a pipeline that e.g. only uses bind group 1, or bind groups 0 and 2.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
