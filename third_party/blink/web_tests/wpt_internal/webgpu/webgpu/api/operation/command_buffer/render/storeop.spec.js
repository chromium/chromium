/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
renderPass store op test that drawn quad is either stored or cleared based on storeop`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('storeOp_controls_whether_1x1_drawn_quad_is_stored')
  .params([
    { storeOp: 'store', _expected: 1 }, //
    { storeOp: 'clear', _expected: 0 },
  ])
  .fn(async t => {
    const renderTexture = t.device.createTexture({
      size: { width: 1, height: 1, depth: 1 },
      format: 'r8unorm',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.OUTPUT_ATTACHMENT,
    });

    // create render pipeline
    const renderPipeline = t.device.createRenderPipeline({
      vertexStage: {
        module: t.device.createShaderModule({
          code: `
            [[builtin(position)]] var<out> Position : vec4<f32>;
            [[builtin(vertex_idx)]] var<in> VertexIndex : i32;

            [[stage(vertex)]] fn main() -> void {
              const pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                  vec2<f32>( 1.0, -1.0),
                  vec2<f32>( 1.0,  1.0),
                  vec2<f32>(-1.0,  1.0));
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
              fragColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
              return;
            }
            `,
        }),

        entryPoint: 'main',
      },

      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'r8unorm' }],
    });

    // encode pass and submit
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: renderTexture.createView(),
          storeOp: t.params.storeOp,
          loadValue: { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
        },
      ],
    });

    pass.setPipeline(renderPipeline);
    pass.draw(3);
    pass.endPass();
    t.device.defaultQueue.submit([encoder.finish()]);

    // expect the buffer to be clear
    t.expectSingleColor(renderTexture, 'r8unorm', {
      size: [1, 1, 1],
      exp: { R: t.params._expected },
    });
  });
