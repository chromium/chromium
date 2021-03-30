/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for the indirect-specific aspects of drawIndirect/drawIndexedIndirect.

TODO:
* parameter_packing - Test that the indirect draw parameters are tightly packed.
  - offset= {0, 4, k * sizeof(args struct), k * sizeof(args struct) + 4}
  - mode= {drawIndirect, drawIndexedIndirect}
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
