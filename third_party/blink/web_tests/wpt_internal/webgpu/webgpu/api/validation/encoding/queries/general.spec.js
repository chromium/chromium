/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:

- For each way to start a query (all possible types in all possible encoders):
    - queryIndex {in, out of} range for GPUQuerySet
    - GPUQuerySet {valid, invalid}
        - or {undefined}, for occlusionQuerySet
    - x = {occlusion, pipeline statistics, timestamp} query
`;
import { params, poptions } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { kQueryTypes } from '../../../../capability_info.js';
import { ValidationTest } from '../../validation_test.js';

class F extends ValidationTest {
  async selectDeviceForQuerySetOrSkipTestCase(type) {
    return this.selectDeviceOrSkipTestCase(
      type === 'pipeline-statistics'
        ? 'pipeline-statistics-query'
        : type === 'timestamp'
        ? 'timestamp-query'
        : undefined
    );
  }
}

export const g = makeTestGroup(F);

g.test('writeTimestamp,query_type_and_index')
  .desc(
    `
Tests that write timestamp to all types of query set on all possible encoders:
- type {occlusion, pipeline statistics, timestamp}
- queryIndex {in, out of} range for GPUQuerySet
- x= {non-pass, compute, render} encoder
  `
  )
  .cases(
    params()
      .combine(poptions('encoderType', ['non-pass', 'compute pass', 'render pass']))
      .combine(poptions('type', kQueryTypes))
  )
  .subcases(({ type }) => poptions('queryIndex', type === 'timestamp' ? [0, 2] : [0]))
  .fn(async t => {
    const { encoderType, type, queryIndex } = t.params;

    await t.selectDeviceForQuerySetOrSkipTestCase(type);

    const count = 2;
    const pipelineStatistics = type === 'pipeline-statistics' ? ['clipper-invocations'] : [];
    const querySet = t.device.createQuerySet({ type, count, pipelineStatistics });

    const encoder = t.createEncoder(encoderType);
    encoder.encoder.writeTimestamp(querySet, queryIndex);

    t.expectValidationError(() => {
      encoder.finish();
    }, type !== 'timestamp' || queryIndex >= count);
  });

g.test('writeTimestamp,invalid_queryset')
  .desc(
    `
Tests that write timestamp to a invalid queryset that failed during creation:
- x= {non-pass, compute, render} enconder
  `
  )
  .subcases(() => poptions('encoderType', ['non-pass', 'compute pass', 'render pass']))
  .fn(async t => {
    const querySet = t.createQuerySetWithState('invalid');

    const encoder = t.createEncoder(t.params.encoderType);
    encoder.encoder.writeTimestamp(querySet, 0);

    t.expectValidationError(() => {
      encoder.finish();
    });
  });
