/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for map-state of mappable buffers used in submitted command buffers.

- x= just before queue op, buffer in {BufferMapStatesToTest}
- x= in every possible place for mappable buffer:
  {writeBuffer, copyB2B {src,dst}, copyB2T, copyT2B, ..?}

TODO: implement
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);
