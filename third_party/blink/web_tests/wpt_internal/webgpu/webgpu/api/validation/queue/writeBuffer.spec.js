/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests writeBuffer validation.

- buffer missing usage flag
- bufferOffset {ok, too large for buffer}
- dataOffset {ok, too large for data}
- size {ok, too large for buffer}
- size {ok, too large for data}
- size unspecified; default {ok, too large for buffer}

Note: destroyed buffer is tested in destroyed/.

TODO: implement.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);
