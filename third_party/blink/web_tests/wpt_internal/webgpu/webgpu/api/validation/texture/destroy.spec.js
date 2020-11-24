/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Destroying a texture more than once is allowed.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('twice').fn(async t => {
  const tex = t.device.createTexture({
    size: [1, 1, 1],
    format: 'r8unorm',
    usage: GPUTextureUsage.SAMPLED,
  });

  tex.destroy();
  tex.destroy();
});
