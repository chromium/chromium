/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for validation in createQuerySet.
`;
import { params, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kQueryTypes, kMaxQueryCount } from '../../../capability_info.js';
import { ValidationTest } from '../validation_test.js';

async function selectDeviceForQueryType(t, type) {
  const extensions = [];
  if (type === 'pipeline-statistics') {
    extensions.push('pipeline-statistics-query');
  } else if (type === 'timestamp') {
    extensions.push('timestamp-query');
  }

  await t.selectDeviceOrSkipTestCase({ extensions });
}

export const g = makeTestGroup(ValidationTest);

g.test('count')
  .desc(
    `
Tests that create query set with the count for all query types:
- count {<, =, >} kMaxQueryCount
- x= {occlusion, pipeline-statistics, timestamp} query
  `
  )
  .params(
    params()
      .combine(poptions('type', kQueryTypes))
      .combine(poptions('count', [0, kMaxQueryCount, kMaxQueryCount + 1]))
  )
  .fn(async t => {
    const { type, count } = t.params;

    await selectDeviceForQueryType(t, type);

    const pipelineStatistics = type === 'pipeline-statistics' ? ['clipper-invocations'] : [];

    t.expectValidationError(() => {
      t.device.createQuerySet({ type, count, pipelineStatistics });
    }, count > kMaxQueryCount);
  });

g.test('pipelineStatistics,all_types')
  .desc(
    `
Tests that create query set with the GPUPipelineStatisticName for all query types:
- pipelineStatistics is undefined or empty
- pipelineStatistics is a sequence of valid values
- x= {occlusion, pipeline-statistics, timestamp} query
  `
  )
  .params(
    params()
      .combine(poptions('type', kQueryTypes))
      .combine(poptions('pipelineStatistics', [undefined, [], ['clipper-invocations']]))
  )
  .fn(async t => {
    const { type, pipelineStatistics } = t.params;

    await selectDeviceForQueryType(t, type);

    const count = 1;
    const shouldError =
      (type !== 'pipeline-statistics' &&
        pipelineStatistics !== undefined &&
        pipelineStatistics.length > 0) ||
      (type === 'pipeline-statistics' &&
        (pipelineStatistics === undefined || pipelineStatistics.length === 0));

    t.expectValidationError(() => {
      t.device.createQuerySet({ type, count, pipelineStatistics });
    }, shouldError);
  });

g.test('pipelineStatistics,duplicates_and_all')
  .desc(
    `
Tests that create query set with the duplicate values and all values of GPUPipelineStatisticName for pipeline-statistics query.
  `
  )
  .params(
    poptions('pipelineStatistics', [
      ['clipper-invocations', 'clipper-invocations'],
      [
        'clipper-invocations',
        'clipper-primitives-out',
        'compute-shader-invocations',
        'fragment-shader-invocations',
        'vertex-shader-invocations',
      ],
    ])
  )
  .fn(async t => {
    const type = 'pipeline-statistics';

    await selectDeviceForQueryType(t, type);

    const count = 1;
    const pipelineStatistics = t.params.pipelineStatistics;

    t.expectValidationError(() => {
      t.device.createQuerySet({ type, count, pipelineStatistics });
    }, pipelineStatistics.length !== Array.from(new Set(pipelineStatistics)).length);
  });
