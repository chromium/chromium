/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `Test culling and rasterizaion state.

Test coverage:
Test all culling combinations of GPUFrontFace and GPUCullMode show the correct output.

Use 2 triangles with different winding orders:

- Test that the counter-clock wise triangle has correct output for:
  - All FrontFaces (ccw, cw)
  - All CullModes (none, front, back)
  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)
  - Some primitive topologies (triangle-list, TODO: triangle-strip)

- Test that the clock wise triangle has correct output for:
  - All FrontFaces (ccw, cw)
  - All CullModes (none, front, back)
  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)
  - Some primitive topologies (triangle-list, TODO: triangle-strip)
`;
import { poptions, params } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

function faceIsCulled(face, frontFace, cullMode) {
  return cullMode !== 'none' && (frontFace === face) === (cullMode === 'front');
}

function faceColor(face, frontFace, cullMode) {
  // front facing color is green, non front facing is red, background is blue
  const isCulled = faceIsCulled(face, frontFace, cullMode);
  if (!isCulled && face === frontFace) {
    return new Uint8Array([0x00, 0xff, 0x00, 0xff]);
  } else if (isCulled) {
    return new Uint8Array([0x00, 0x00, 0xff, 0xff]);
  } else {
    return new Uint8Array([0xff, 0x00, 0x00, 0xff]);
  }
}

export const g = makeTestGroup(GPUTest);

g.test('culling')
  .params(
    params()
      .combine(poptions('frontFace', ['ccw', 'cw']))
      .combine(poptions('cullMode', ['none', 'front', 'back']))
      .combine(
        poptions('depthStencilFormat', [
          null,
          'depth24plus',
          'depth32float',
          'depth24plus-stencil8',
        ])
      )

      // TODO: test triangle-strip as well
      .combine(poptions('primitiveTopology', ['triangle-list']))
  )
  .fn(t => {
    const size = 4;
    const format = 'rgba8unorm';

    const texture = t.device.createTexture({
      size: { width: size, height: size, depth: 1 },
      format,
      usage: GPUTextureUsage.OUTPUT_ATTACHMENT | GPUTextureUsage.COPY_SRC,
    });

    const depthTexture = t.params.depthStencilFormat
      ? t.device.createTexture({
          size: { width: size, height: size, depth: 1 },
          format: t.params.depthStencilFormat,
          usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
        })
      : null;

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: texture.createView(),
          loadValue: { r: 0.0, g: 0.0, b: 1.0, a: 1.0 },
        },
      ],

      depthStencilAttachment: depthTexture
        ? {
            attachment: depthTexture.createView(),
            depthLoadValue: 1.0,
            depthStoreOp: 'store',
            stencilLoadValue: 0,
            stencilStoreOp: 'store',
          }
        : undefined,
    });

    // Draw two triangles with different winding orders:
    // 1. The top-left one is counterclockwise (CCW)
    // 2. The bottom-right one is clockwise (CW)
    pass.setPipeline(
      t.device.createRenderPipeline({
        vertexStage: {
          module: t.device.createShaderModule({
            code: `
              [[builtin(position)]] var<out> Position : vec4<f32>;
              [[builtin(vertex_idx)]] var<in> VertexIndex : i32;

              [[stage(vertex)]] fn main() -> void {
                const pos : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
                    vec2<f32>(-1.0,  1.0),
                    vec2<f32>(-1.0,  0.0),
                    vec2<f32>( 0.0,  1.0),
                    vec2<f32>( 0.0, -1.0),
                    vec2<f32>( 1.0,  0.0),
                    vec2<f32>( 1.0, -1.0));
                Position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
                return;
              }`,
          }),

          entryPoint: 'main',
        },

        fragmentStage: {
          module: t.device.createShaderModule({
            code: `
              [[location(0)]] var<out> fragColor : vec4<f32>;
              [[builtin(front_facing)]] var<in> FrontFacing : bool;

              [[stage(fragment)]] fn main() -> void {
                if (FrontFacing) {
                  fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
                } else {
                  fragColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
                }
                return;
              }`,
          }),

          entryPoint: 'main',
        },

        primitiveTopology: t.params.primitiveTopology,
        rasterizationState: {
          frontFace: t.params.frontFace,
          cullMode: t.params.cullMode,
        },

        colorStates: [{ format }],
        depthStencilState: depthTexture ? { format: t.params.depthStencilFormat } : undefined,
      })
    );

    pass.draw(6, 1, 0, 0);
    pass.endPass();

    t.device.defaultQueue.submit([encoder.finish()]);

    // front facing color is green, non front facing is red, background is blue
    const kCCWTriangleTopLeftColor = faceColor('ccw', t.params.frontFace, t.params.cullMode);
    t.expectSinglePixelIn2DTexture(
      texture,
      format,
      { x: 0, y: 0 },
      { exp: kCCWTriangleTopLeftColor }
    );

    const kCWTriangleBottomRightColor = faceColor('cw', t.params.frontFace, t.params.cullMode);
    t.expectSinglePixelIn2DTexture(
      texture,
      format,
      { x: size - 1, y: size - 1 },
      { exp: kCWTriangleBottomRightColor }
    );

    // TODO: check the contents of the depth and stencil outputs
  });
