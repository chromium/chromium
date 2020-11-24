/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Test validation conditions for requestDevice.
`;
import { Fixture } from '../../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';

export const g = makeTestGroup(Fixture);

g.test('features,nonexistent')
  .desc('requestDevice with a made-up feature name. Should resolve to null.')
  .unimplemented();

g.test('features,known_but_unavailable')
  .desc(
    `requestDevice with a valid feature that's unavailable on the adapter. Should resolve to null.
(Skipped if such a feature can't be found. But most browsers should support both BC and ETC
while most hardware should only support one.)`
  )
  .unimplemented();

g.test('limits')
  .desc(
    `For each limit, request with various values. Some should resolve to null. (TODO: which?)

- value = {
    - less than default
    - default
    - default + 1
    - best available
    - better than what's available
    - }
  `
  )
  .unimplemented();
