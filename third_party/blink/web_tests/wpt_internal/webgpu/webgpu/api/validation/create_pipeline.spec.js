/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `

TODO:
For {createRenderPipeline, createComputePipeline}, start with a valid descriptor (control case),
then for each stage {{vertex, fragment}, compute}, make exactly one of the following errors:
- one stage's module is an invalid object
- one stage's entryPoint doesn't exist
  - {different name, empty string, name that's almost the same but differs in some subtle unicode way}
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';

import { ValidationTest } from './validation_test.js';

export const g = makeTestGroup(ValidationTest);
