/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests using a destroyed query set on a queue.

- used in {resolveQuerySet, timestamp {compute, render, non-pass},
    pipeline statistics {compute, render}, occlusion}
- x= {destroyed, not destroyed (control case)}

TODO: implement. (Search for other places some of these cases may have already been tested.)
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
