/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Validation for encoding begin/endable queries.

TODO:
- balance: {
    - begin 0, end 1
    - begin 1, end 0
    - begin 1, end 1
    - begin 2, end 2
    - }
    - x= {
        - render pass + occlusion
        - render pass + pipeline statistics
        - compute pass + pipeline statistics
        - }
- nesting: test whether it's allowed to nest various types of queries
  (including writeTimestamp inside begin/endable queries).
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
