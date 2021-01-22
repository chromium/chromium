/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- 2 views:
    - x= {upon the same subresource, or different subresources {mip level, array layer, aspect} of the same texture}
    - x= possible binding types on each view: read = {sampled texture, readonly storage texture}, write = {storage texture, render target}
    - x= different shader stages: {0, ..., 7}
        - maybe first view vis = {1, 2, 4}, second view vis = {0, ..., 7}
    - x= bindings are in {
        - same draw call
        - same pass, different draw call
        - different pass
        - }
(It's probably not necessary to test EVERY possible combination of options in this whole
block, so we could break it down into a few smaller ones (one for different types of
subresources, one for same draw/same pass/different pass, one for visibilities).)
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
