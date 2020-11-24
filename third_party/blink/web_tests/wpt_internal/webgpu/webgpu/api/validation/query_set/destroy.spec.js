/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Destroying a query set more than once is allowed.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('twice').fn(async t => {
  const qset = t.device.createQuerySet({ type: 'occlusion', count: 1 });

  qset.destroy();
  qset.destroy();
});
