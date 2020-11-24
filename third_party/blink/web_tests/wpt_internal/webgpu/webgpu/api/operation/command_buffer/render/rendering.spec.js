/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = '';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('fullscreen_quad').fn(async t => {
  const dst = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
  });

  const colorAttachment = t.device.createTexture({
    format: 'rgba8unorm',
    size: { width: 1, height: 1, depth: 1 },
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.OUTPUT_ATTACHMENT,
  });

  const colorAttachmentView = colorAttachment.createView();

  const pipeline = t.device.createRenderPipeline({
    vertexStage: {
      module: t.device.createShaderModule({
        code: `
          [[builtin(position)]] var<out> Position : vec4<f32>;
          [[builtin(vertex_idx)]] var<in> VertexIndex : i32;

          [[stage(vertex)]] fn main() -> void {
            const pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -3.0),
                vec2<f32>(3.0, 1.0),
                vec2<f32>(-1.0, 1.0));
            Position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
            return;
          }
          `,
      }),

      entryPoint: 'main',
    },

    fragmentStage: {
      module: t.device.createShaderModule({
        code: `
          [[location(0)]] var<out> fragColor : vec4<f32>;
          [[stage(fragment)]] fn main() -> void {
            fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
            return;
          }
          `,
      }),

      entryPoint: 'main',
    },

    primitiveTopology: 'triangle-list',
    colorStates: [{ format: 'rgba8unorm' }],
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [
      {
        attachment: colorAttachmentView,
        storeOp: 'store',
        loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
      },
    ],
  });

  pass.setPipeline(pipeline);
  pass.draw(3);
  pass.endPass();
  encoder.copyTextureToBuffer(
    { texture: colorAttachment, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
    { buffer: dst, bytesPerRow: 256 },
    { width: 1, height: 1, depth: 1 }
  );

  t.device.defaultQueue.submit([encoder.finish()]);

  t.expectContents(dst, new Uint8Array([0x00, 0xff, 0x00, 0xff]));
});
