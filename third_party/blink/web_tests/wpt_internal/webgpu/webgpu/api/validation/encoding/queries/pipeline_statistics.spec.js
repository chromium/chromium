/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Validation for encoding pipeline statistics queries.
Excludes query begin/end balance and nesting (begin_end.spec.ts)
and querySet/queryIndex (general.spec.ts).

TODO:
- Test with an invalid querySet.
- Test an pipeline statistics query with no draw/dispatch calls.
  (If that's valid, move the test to api/operation/.)
- ?
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
