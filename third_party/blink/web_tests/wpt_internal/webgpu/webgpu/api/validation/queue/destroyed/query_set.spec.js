/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests using a destroyed query set on a queue.

- used in {resolveQuerySet, timestamp {compute, render, non-pass},
    pipeline statistics {compute, render}, occlusion}
- x= {destroyed, not destroyed (control case)}

TODO: implement. (Search for other places some of these cases may have already been tested.)
Consider whether these tests should be distributed throughout the suite, instead of centralized.
`;
import { poptions } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export let EncoderType;
(function (EncoderType) {
  EncoderType['CommandEncoder'] = 'CommandEncoder';
  EncoderType['ComputeEncoder'] = 'ComputeEncoder';
  EncoderType['RenderEncoder'] = 'RenderEncoder';
})(EncoderType || (EncoderType = {}));

export const g = makeTestGroup(ValidationTest);

g.test('writeTimestamp')
  .desc(
    `
Tests that use a destroyed query set in writeTimestamp on {non-pass, compute, render} encoder.
- x= {destroyed, not destroyed (control case)}
  `
  )
  .cases(poptions('encoderType', ['non-pass', 'compute pass', 'render pass']))
  .subcases(() => poptions('querySetState', ['valid', 'destroyed']))
  .fn(async t => {
    await t.selectDeviceOrSkipTestCase('timestamp-query');

    const querySet = t.createQuerySetWithState(t.params.querySetState, {
      type: 'timestamp',
      count: 2,
    });

    const encoder = t.createEncoder(t.params.encoderType);
    encoder.encoder.writeTimestamp(querySet, 0);

    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, t.params.querySetState === 'destroyed');
  });

g.test('resolveQuerySet')
  .desc(
    `
Tests that use a destroyed query set in resolveQuerySet.
- x= {destroyed, not destroyed (control case)}
  `
  )
  .subcases(() => poptions('querySetState', ['valid', 'destroyed']))
  .fn(async t => {
    const querySet = t.createQuerySetWithState(t.params.querySetState);

    const buffer = t.device.createBuffer({ size: 8, usage: GPUBufferUsage.QUERY_RESOLVE });

    const encoder = t.device.createCommandEncoder();
    encoder.resolveQuerySet(querySet, 0, 1, buffer, 0);

    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, t.params.querySetState === 'destroyed');
  });
