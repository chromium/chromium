/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests using a destroyed texture on a queue.

- used in {writeTexture,
  setBindGroup, copyT2T {src,dst}, copyB2T, copyT2B, copyImageBitmapToTexture,
  color attachment {0,>0}, {D,S,DS} attachment, ..?}
- x= if applicable, {in pass, in bundle}
- x= {destroyed, not destroyed (control case)}

TODO: implement. (Search for other places some of these cases may have already been tested.)
Consider whether these tests should be distributed throughout the suite, instead of centralized.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
