/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = '';
import { params, pbool, poptions } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('createQuerySet')
  .desc(
    `
Tests that creating query set shouldn't be valid without the required feature enabled.
- createQuerySet
  - type {occlusion, pipeline-statistics, timestamp}
  - x= {pipeline statistics, timestamp} query {enable, disable}
  `
  )
  .params(
    params()
      .combine(poptions('type', ['occlusion', 'pipeline-statistics', 'timestamp']))
      .combine(pbool('pipelineStatisticsQueryEnable'))
      .combine(pbool('timestampQueryEnable'))
  )
  .fn(async t => {
    const { type, pipelineStatisticsQueryEnable, timestampQueryEnable } = t.params;

    const extensions = [];
    if (pipelineStatisticsQueryEnable) {
      extensions.push('pipeline-statistics-query');
    }
    if (timestampQueryEnable) {
      extensions.push('timestamp-query');
    }

    await t.selectDeviceOrSkipTestCase({ extensions });

    const count = 1;
    const pipelineStatistics = type === 'pipeline-statistics' ? ['clipper-invocations'] : [];
    const shouldError =
      (type === 'pipeline-statistics' && !pipelineStatisticsQueryEnable) ||
      (type === 'timestamp' && !timestampQueryEnable);

    t.expectValidationError(() => {
      t.device.createQuerySet({ type, count, pipelineStatistics });
    }, shouldError);
  });
