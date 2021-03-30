/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
indexed draws validation tests.

TODO: review and make sure these notes are covered:
> - indexed draws:
>     - index access out of bounds (make sure this doesn't overlap with robust access)
>         - bound index buffer **range** is {exact size, just under exact size} needed for draws with:
>             - indexCount largeish
>             - firstIndex {=, >} 0
>     - x= {drawIndexed, drawIndexedIndirect}

TODO: Since there are no errors here, these should be "robustness" operation tests (with multiple
valid results).
`;
import { params, poptions, pbool } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

class F extends ValidationTest {
  createIndexBuffer(indexData) {
    const indexArray = new Uint32Array(indexData);

    const indexBuffer = this.device.createBuffer({
      mappedAtCreation: true,
      size: indexArray.byteLength,
      usage: GPUBufferUsage.INDEX,
    });

    new Uint32Array(indexBuffer.getMappedRange()).set(indexArray);
    indexBuffer.unmap();

    return indexBuffer;
  }

  createRenderPipeline() {
    return this.device.createRenderPipeline({
      vertex: {
        module: this.device.createShaderModule({
          code: `
            [[builtin(position)]] var<out> Position : vec4<f32>;

            [[stage(vertex)]] fn main() -> void {
              Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
              return;
            }`,
        }),

        entryPoint: 'main',
      },

      fragment: {
        module: this.device.createShaderModule({
          code: `
            [[location(0)]] var<out> fragColor : vec4<f32>;
            [[stage(fragment)]] fn main() -> void {
              fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
              return;
            }`,
        }),

        entryPoint: 'main',
        targets: [{ format: 'rgba8unorm' }],
      },

      primitive: {
        topology: 'triangle-strip',
        stripIndexFormat: 'uint32',
      },
    });
  }

  beginRenderPass(encoder) {
    const colorAttachment = this.device.createTexture({
      format: 'rgba8unorm',
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });

    return encoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: colorAttachment.createView(),
          loadValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
          storeOp: 'store',
        },
      ],
    });
  }

  drawIndexed(indexBuffer, indexCount, instanceCount, firstIndex, baseVertex, firstInstance) {
    const pipeline = this.createRenderPipeline();

    const encoder = this.device.createCommandEncoder();
    const pass = this.beginRenderPass(encoder);
    pass.setPipeline(pipeline);
    pass.setIndexBuffer(indexBuffer, 'uint32');
    pass.drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    pass.endPass();

    this.device.queue.submit([encoder.finish()]);
  }

  drawIndexedIndirect(indexBuffer, bufferArray, indirectOffset) {
    const indirectBuffer = this.device.createBuffer({
      mappedAtCreation: true,
      size: bufferArray.byteLength,
      usage: GPUBufferUsage.INDIRECT,
    });

    new Uint32Array(indirectBuffer.getMappedRange()).set(bufferArray);
    indirectBuffer.unmap();

    const pipeline = this.createRenderPipeline();

    const encoder = this.device.createCommandEncoder();
    const pass = this.beginRenderPass(encoder);
    pass.setPipeline(pipeline);
    pass.setIndexBuffer(indexBuffer, 'uint32');
    pass.drawIndexedIndirect(indirectBuffer, indirectOffset);
    pass.endPass();

    this.device.queue.submit([encoder.finish()]);
  }
}

export const g = makeTestGroup(F);

g.test('out_of_bounds')
  .desc(
    `Test drawing with out of bound index access to make sure the implementation is robust
    with the following indexCount and firstIndex conditions
    - valid draw
    - either is within bound but indexCount + firstIndex is out of bound
    - only firstIndex is out of bound
    - only indexCount is out of bound
    - firstIndex much larger than indexCount
    - indexCount much larger than firstIndex
    - max uint32 value for both to make sure the sum doesn't overflow
    - max uint32 indexCount and small firstIndex
    - max uint32 firstIndex and small indexCount
    Together with normal and large instanceCount`
  )
  .cases(pbool('indirect'))
  .subcases(
    () =>
      params()
        .combine([
          { indexCount: 6, firstIndex: 1 }, // indexCount + firstIndex out of bound
          { indexCount: 0, firstIndex: 6 }, // indexCount is 0 but firstIndex out of bound
          { indexCount: 6, firstIndex: 6 }, // only firstIndex out of bound
          { indexCount: 6, firstIndex: 10000 }, // firstIndex much larger than the bound
          { indexCount: 7, firstIndex: 0 }, // only indexCount out of bound
          { indexCount: 10000, firstIndex: 0 }, // indexCount much larger than the bound
          { indexCount: 0xffffffff, firstIndex: 0xffffffff }, // max uint32 value
          { indexCount: 0xffffffff, firstIndex: 2 }, // max uint32 indexCount and small firstIndex
          { indexCount: 2, firstIndex: 0xffffffff }, // small indexCount and max uint32 firstIndex
        ])
        .combine(poptions('instanceCount', [1, 10000])) // normal and large instanceCount
  )
  .fn(t => {
    const { indirect, indexCount, firstIndex, instanceCount } = t.params;

    const indexBuffer = t.createIndexBuffer([0, 1, 2, 3, 1, 2]);

    if (indirect) {
      t.drawIndexedIndirect(
        indexBuffer,
        new Uint32Array([indexCount, instanceCount, firstIndex, 0, 0]),
        0
      );
    } else {
      t.drawIndexed(indexBuffer, indexCount, instanceCount, firstIndex, 0, 0);
    }
  });

g.test('out_of_bounds_zero_sized_index_buffer')
  .desc(
    `Test drawing with an empty index buffer to make sure the implementation is robust
    with the following indexCount and firstIndex conditions
    - indexCount + firstIndex is out of bound
    - indexCount is 0 but firstIndex is out of bound
    - only indexCount is out of bound
    - both are 0s (not out of bound) but index buffer size is 0
    Together with normal and large instanceCount`
  )
  .cases(pbool('indirect'))
  .subcases(
    () =>
      params()
        .combine([
          { indexCount: 3, firstIndex: 1 }, // indexCount + firstIndex out of bound
          { indexCount: 0, firstIndex: 1 }, // indexCount is 0 but firstIndex out of bound
          { indexCount: 3, firstIndex: 0 }, // only indexCount out of bound
          { indexCount: 0, firstIndex: 0 }, // just zeros
        ])
        .combine(poptions('instanceCount', [1, 10000])) // normal and large instanceCount
  )
  .fn(t => {
    const { indirect, indexCount, firstIndex, instanceCount } = t.params;

    const indexBuffer = t.createIndexBuffer([]);

    if (indirect) {
      t.drawIndexedIndirect(
        indexBuffer,
        new Uint32Array([indexCount, instanceCount, firstIndex, 0, 0]),
        0
      );
    } else {
      t.drawIndexed(indexBuffer, indexCount, instanceCount, firstIndex, 0, 0);
    }
  });
