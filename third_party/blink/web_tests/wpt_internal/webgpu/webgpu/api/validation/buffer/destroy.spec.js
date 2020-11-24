/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Destroying a buffer more than once is allowed.
`;
import { params, pbool } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUConst } from '../../../constants.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('twice')
  .desc('Tests various mapping-related descripton options that could affect how state is tracked.')
  .params(
    params()
      .combine(pbool('mappedAtCreation'))
      .combine([
        { size: 4, usage: GPUConst.BufferUsage.COPY_SRC },
        { size: 4, usage: GPUConst.BufferUsage.MAP_WRITE | GPUConst.BufferUsage.COPY_SRC },
        { size: 4, usage: GPUConst.BufferUsage.COPY_DST | GPUConst.BufferUsage.MAP_READ },
      ])
  )
  .fn(async t => {
    const buf = t.device.createBuffer(t.params);

    buf.destroy();
    buf.destroy();
  });
