/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- 2 views: upon the same subresource, or different subresources of the same texture
    - texture usages in copies and in render pass
    - consecutively set bind groups on the same index (@Richard-Yunchao: Maybe I can combine this one with the above tests. The two bind groups can either have the same index or different indices.)
    - unused bind groups
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
