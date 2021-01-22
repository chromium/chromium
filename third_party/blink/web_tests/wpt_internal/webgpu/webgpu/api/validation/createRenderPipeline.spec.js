/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
createRenderPipeline validation tests.

TODO: review existing tests, write descriptions, and make sure tests are complete.
      Make sure the following is covered. Consider splitting the file if too large/disjointed.
> - various attachment problems
>
> - interface matching between vertex and fragment shader
>     - superset, subset, etc.
>
> - vertexStage {valid, invalid}
> - fragmentStage {valid, invalid}
> - primitiveTopology all possible values
> - rasterizationState various values
> - sampleCount {0, 1, 3, 4, 8, 16, 1024}
> - sampleMask {0, 0xFFFFFFFF}
> - alphaToCoverage:
>     - alphaToCoverageEnabled is { true, false } and sampleCount { = 1, = 4 }.
>       The only failing case is (true, 1).
>     - output SV_Coverage semantics is statically used by fragmentStage and
>       alphaToCoverageEnabled is { true (fails), false (passes) }.
>     - sampleMask is being used and alphaToCoverageEnabled is { true (fails), false (passes) }.

`;
import { poptions } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { kAllTextureFormats, kAllTextureFormatInfo } from '../../capability_info.js';

import { ValidationTest } from './validation_test.js';

class F extends ValidationTest {
  getDescriptor(options = {}) {
    const defaultColorStates = [{ format: 'rgba8unorm' }];
    const {
      primitiveTopology = 'triangle-list',
      colorStates = defaultColorStates,
      sampleCount = 1,
      depthStencilState,
    } = options;

    const format = colorStates.length ? colorStates[0].format : 'rgba8unorm';

    let fragColorType;
    let suffix;
    if (format.endsWith('sint')) {
      fragColorType = 'i32';
      suffix = '';
    } else if (format.endsWith('uint')) {
      fragColorType = 'u32';
      suffix = 'u';
    } else {
      fragColorType = 'f32';
      suffix = '.0';
    }

    return {
      vertexStage: {
        module: this.device.createShaderModule({
          code: `
            [[builtin(position)]] var<out> Position : vec4<f32>;

            [[stage(vertex)]] fn main() -> void {
              Position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
            }`,
        }),

        entryPoint: 'main',
      },

      fragmentStage: {
        module: this.device.createShaderModule({
          code: `
            [[location(0)]] var<out> fragColor : vec4<${fragColorType}>;
            [[stage(fragment)]] fn main() -> void {
              fragColor = vec4<${fragColorType}>(0${suffix}, 1${suffix}, 0${suffix}, 1${suffix});
            }`,
        }),

        entryPoint: 'main',
      },

      layout: this.getPipelineLayout(),
      primitiveTopology,
      colorStates,
      sampleCount,
      depthStencilState,
    };
  }

  getPipelineLayout() {
    return this.device.createPipelineLayout({ bindGroupLayouts: [] });
  }

  createTexture(params) {
    const { format, sampleCount } = params;

    return this.device.createTexture({
      size: { width: 4, height: 4, depth: 1 },
      usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
      format,
      sampleCount,
    });
  }
}

export const g = makeTestGroup(F);

g.test('basic_use_of_createRenderPipeline').fn(t => {
  const descriptor = t.getDescriptor();

  t.device.createRenderPipeline(descriptor);
});

g.test('at_least_one_color_state_is_required').fn(async t => {
  const goodDescriptor = t.getDescriptor({
    colorStates: [{ format: 'rgba8unorm' }],
  });

  // Control case
  t.device.createRenderPipeline(goodDescriptor);

  // Fail because lack of color states
  const badDescriptor = t.getDescriptor({
    colorStates: [],
  });

  t.expectValidationError(() => {
    t.device.createRenderPipeline(badDescriptor);
  });
});

g.test('color_formats_must_be_renderable')
  .params(poptions('format', kAllTextureFormats))
  .fn(async t => {
    const format = t.params.format;
    const info = kAllTextureFormatInfo[format];

    const descriptor = t.getDescriptor({ colorStates: [{ format }] });

    if (info.renderable && info.color) {
      // Succeeds when color format is renderable
      t.device.createRenderPipeline(descriptor);
    } else {
      // Fails because when format is non-renderable
      t.expectValidationError(() => {
        t.device.createRenderPipeline(descriptor);
      });
    }
  });

g.test('sample_count_must_be_valid')
  .params([
    { sampleCount: 0, _success: false },
    { sampleCount: 1, _success: true },
    { sampleCount: 2, _success: false },
    { sampleCount: 3, _success: false },
    { sampleCount: 4, _success: true },
    { sampleCount: 8, _success: false },
    { sampleCount: 16, _success: false },
  ])
  .fn(async t => {
    const { sampleCount, _success } = t.params;

    const descriptor = t.getDescriptor({ sampleCount });

    if (_success) {
      // Succeeds when sample count is valid
      t.device.createRenderPipeline(descriptor);
    } else {
      // Fails when sample count is not 4 or 1
      t.expectValidationError(() => {
        t.device.createRenderPipeline(descriptor);
      });
    }
  });

g.test('sample_count_must_be_equal_to_the_one_of_every_attachment_in_the_render_pass')
  .params([
    { attachmentSamples: 4, pipelineSamples: 4, _success: true }, // It is allowed to use multisampled render pass and multisampled render pipeline.
    { attachmentSamples: 4, pipelineSamples: 1, _success: false }, // It is not allowed to use multisampled render pass and non-multisampled render pipeline.
    { attachmentSamples: 1, pipelineSamples: 4, _success: false }, // It is not allowed to use non-multisampled render pass and multisampled render pipeline.
  ])
  .fn(async t => {
    const { attachmentSamples, pipelineSamples, _success } = t.params;

    const colorTexture = t.createTexture({
      format: 'rgba8unorm',
      sampleCount: attachmentSamples,
    });

    const depthStencilTexture = t.createTexture({
      format: 'depth24plus-stencil8',
      sampleCount: attachmentSamples,
    });

    const renderPassDescriptorWithoutDepthStencil = {
      colorAttachments: [
        {
          attachment: colorTexture.createView(),
          loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        },
      ],
    };

    const renderPassDescriptorWithDepthStencilOnly = {
      colorAttachments: [],
      depthStencilAttachment: {
        attachment: depthStencilTexture.createView(),
        depthLoadValue: 1.0,
        depthStoreOp: 'store',
        stencilLoadValue: 0,
        stencilStoreOp: 'store',
      },
    };

    const pipelineWithoutDepthStencil = t.device.createRenderPipeline(
      t.getDescriptor({
        sampleCount: pipelineSamples,
      })
    );

    const pipelineWithDepthStencilOnly = t.device.createRenderPipeline(
      t.getDescriptor({
        colorStates: [],
        depthStencilState: { format: 'depth24plus-stencil8' },
        sampleCount: pipelineSamples,
      })
    );

    for (const { renderPassDescriptor, pipeline } of [
      {
        renderPassDescriptor: renderPassDescriptorWithoutDepthStencil,
        pipeline: pipelineWithoutDepthStencil,
      },

      {
        renderPassDescriptor: renderPassDescriptorWithDepthStencilOnly,
        pipeline: pipelineWithDepthStencilOnly,
      },
    ]) {
      const commandEncoder = t.device.createCommandEncoder();
      const renderPass = commandEncoder.beginRenderPass(renderPassDescriptor);
      renderPass.setPipeline(pipeline);
      renderPass.endPass();

      t.expectValidationError(() => {
        commandEncoder.finish();
      }, !_success);
    }
  });
