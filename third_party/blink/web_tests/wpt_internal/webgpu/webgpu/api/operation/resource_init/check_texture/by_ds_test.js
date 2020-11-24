/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../../../../common/framework/util/util.js';
import { mipSize } from '../../../../util/texture/subresource.js';

function makeFullscreenVertexModule(device) {
  return device.createShaderModule({
    code: `
    [[builtin(position)]] var<out> Position : vec4<f32>;
    [[builtin(vertex_idx)]] var<in> VertexIndex : i32;

    [[stage(vertex)]]
    fn main() -> void {
      const pos : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -3.0),
        vec2<f32>( 3.0,  1.0),
        vec2<f32>(-1.0,  1.0));
      Position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
      return;
    }
    `,
  });
}

function getDepthTestEqualPipeline(t, format, sampleCount, expected) {
  return t.device.createRenderPipeline({
    vertexStage: {
      entryPoint: 'main',
      module: makeFullscreenVertexModule(t.device),
    },

    fragmentStage: {
      entryPoint: 'main',
      module: t.device.createShaderModule({
        code: `
        [[builtin(frag_depth)]] var<out> FragDepth : f32;
        [[location(0)]] var<out> outSuccess : f32;

        [[stage(fragment)]]
        fn main() -> void {
          FragDepth = f32(${expected});
          outSuccess = 1.0;
          return;
        }
        `,
      }),
    },

    colorStates: [{ format: 'r8unorm' }],
    depthStencilState: {
      format,
      depthCompare: 'equal',
    },

    primitiveTopology: 'triangle-list',
    sampleCount,
  });
}

function getStencilTestEqualPipeline(t, format, sampleCount) {
  return t.device.createRenderPipeline({
    vertexStage: {
      entryPoint: 'main',
      module: makeFullscreenVertexModule(t.device),
    },

    fragmentStage: {
      entryPoint: 'main',
      module: t.device.createShaderModule({
        code: `
        [[location(0)]] var<out> outSuccess : f32;

        [[stage(fragment)]]
        fn main() -> void {
          outSuccess = 1.0;
          return;
        }
        `,
      }),
    },

    colorStates: [
      {
        format: 'r8unorm',
      },
    ],

    depthStencilState: {
      format,
      stencilFront: { compare: 'equal' },
      stencilBack: { compare: 'equal' },
    },

    primitiveTopology: 'triangle-list',
    sampleCount,
  });
}

const checkContents = (type, t, params, texture, state, subresourceRange) => {
  for (const viewDescriptor of t.generateTextureViewDescriptorsForRendering(
    params.aspect,
    subresourceRange
  )) {
    assert(viewDescriptor.baseMipLevel !== undefined);
    const [width, height] = mipSize([t.textureWidth, t.textureHeight], viewDescriptor.baseMipLevel);

    const renderTexture = t.device.createTexture({
      size: [width, height, 1],
      format: 'r8unorm',
      usage: GPUTextureUsage.OUTPUT_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      sampleCount: params.sampleCount,
    });

    let resolveTexture = undefined;
    let resolveTarget = undefined;
    if (params.sampleCount > 1) {
      resolveTexture = t.device.createTexture({
        size: [width, height, 1],
        format: 'r8unorm',
        usage: GPUTextureUsage.OUTPUT_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      });

      resolveTarget = resolveTexture.createView();
    }

    const commandEncoder = t.device.createCommandEncoder();
    const pass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: renderTexture.createView(),
          resolveTarget,
          loadValue: [0, 0, 0, 0],
          storeOp: 'store',
        },
      ],

      depthStencilAttachment: {
        attachment: texture.createView(viewDescriptor),
        depthStoreOp: 'store',
        depthLoadValue: 'load',
        stencilStoreOp: 'store',
        stencilLoadValue: 'load',
      },
    });

    switch (type) {
      case 'depth': {
        const expectedDepth = t.stateToTexelComponents[state].Depth;
        assert(expectedDepth !== undefined);

        pass.setPipeline(
          getDepthTestEqualPipeline(t, params.format, params.sampleCount, expectedDepth)
        );

        break;
      }

      case 'stencil': {
        const expectedStencil = t.stateToTexelComponents[state].Stencil;
        assert(expectedStencil !== undefined);

        pass.setPipeline(getStencilTestEqualPipeline(t, params.format, params.sampleCount));
        pass.setStencilReference(expectedStencil);
        break;
      }
    }

    pass.draw(3);
    pass.endPass();

    t.queue.submit([commandEncoder.finish()]);

    t.expectSingleColor(resolveTexture || renderTexture, 'r8unorm', {
      size: [width, height, 1],
      exp: { R: 1 },
    });
  }
};

export const checkContentsByDepthTest = (...args) => checkContents('depth', ...args);

export const checkContentsByStencilTest = (...args) => checkContents('stencil', ...args);
