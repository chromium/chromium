/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- createCommandEncoder
- non-pass command, or beginPass, during {render, compute} pass
- {before (control case), after} finish()
    - x= {finish(), ... all non-pass commands}
- {before (control case), after} endPass()
    - x= {render, compute} pass
    - x= {finish(), ... all relevant pass commands}
    - x= {
        - before endPass (control case)
        - after endPass (no pass open)
        - after endPass+beginPass (a new pass of the same type is open)
        - }
    - should make whole encoder invalid
- ?
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);
