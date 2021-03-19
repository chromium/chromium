/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- test compatibility between bind groups and pipelines
    - bind groups required by the pipeline layout are required.
    - bind groups unused by the pipeline layout can be set or not.
        (Even if e.g. bind groups 0 and 2 are used, but 1 is unused.)
    - bindGroups[i].layout is "group-equivalent" (value-equal) to pipelineLayout.bgls[i].
    - in the test fn, test once without the dispatch/draw (should always be valid) and once with
      the dispatch/draw, to make sure the validation happens in dispatch/draw.
    - x= {dispatch, all draws} (dispatch/draw should be size 0 to make sure validation still happens if no-op)
    - x= all relevant stages

TODO: subsume existing test, rewrite fixture as needed.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

class F extends ValidationTest {
  getUniformBuffer() {
    return this.device.createBuffer({
      size: 8 * Float32Array.BYTES_PER_ELEMENT,
      usage: GPUBufferUsage.UNIFORM,
    });
  }

  createRenderPipeline() {
    const pipeline = this.device.createRenderPipeline({
      vertexStage: {
        module: this.device.createShaderModule({
          code: `
            [[block]] struct VertexUniforms {
              [[offset(0)]] transform : mat2x2<f32> ;
            };
            [[group(0), binding(0)]] var<uniform> uniforms : VertexUniforms;

            [[builtin(position)]] var<out> Position : vec4<f32>;
            [[builtin(vertex_index)]] var<in> VertexIndex : i32;
            [[stage(vertex)]] fn main() -> void {
              var pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 1.0, -1.0),
                vec2<f32>(-1.0,  1.0)
              );
              Position = vec4<f32>(uniforms.transform * pos[VertexIndex], 0.0, 1.0);
              return;
            }`,
        }),

        entryPoint: 'main',
      },

      fragmentStage: {
        module: this.device.createShaderModule({
          code: `
            [[block]] struct FragmentUniforms {
              [[offset(0)]] color : vec4<f32>;
            };
            [[group(1), binding(0)]] var<uniform> uniforms : FragmentUniforms;

            [[location(0)]] var<out> fragColor : vec4<f32>;
            [[stage(fragment)]] fn main() -> void {
              fragColor = uniforms.color;
              return;
            }`,
        }),

        entryPoint: 'main',
      },

      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'rgba8unorm' }],
    });

    return pipeline;
  }

  beginRenderPass(commandEncoder) {
    const attachmentTexture = this.device.createTexture({
      format: 'rgba8unorm',
      size: { width: 16, height: 16, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
    });

    return commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: attachmentTexture.createView(),
          loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        },
      ],
    });
  }
}

export const g = makeTestGroup(F);

g.test('it_is_invalid_to_draw_in_a_render_pass_with_missing_bind_groups')
  .params([
    { setBindGroup1: true, setBindGroup2: true, _success: true },
    { setBindGroup1: true, setBindGroup2: false, _success: false },
    { setBindGroup1: false, setBindGroup2: true, _success: false },
    { setBindGroup1: false, setBindGroup2: false, _success: false },
  ])
  .fn(async t => {
    const { setBindGroup1, setBindGroup2, _success } = t.params;

    const pipeline = t.createRenderPipeline();

    const uniformBuffer = t.getUniformBuffer();

    const bindGroup0 = t.device.createBindGroup({
      entries: [
        {
          binding: 0,
          resource: {
            buffer: uniformBuffer,
          },
        },
      ],

      layout: pipeline.getBindGroupLayout(0),
    });

    const bindGroup1 = t.device.createBindGroup({
      entries: [
        {
          binding: 0,
          resource: {
            buffer: uniformBuffer,
          },
        },
      ],

      layout: pipeline.getBindGroupLayout(1),
    });

    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = t.beginRenderPass(commandEncoder);
    renderPass.setPipeline(pipeline);
    if (setBindGroup1) {
      renderPass.setBindGroup(0, bindGroup0);
    }
    if (setBindGroup2) {
      renderPass.setBindGroup(1, bindGroup1);
    }
    renderPass.draw(3);
    renderPass.endPass();
    t.expectValidationError(() => {
      commandEncoder.finish();
    }, !_success);
  });
