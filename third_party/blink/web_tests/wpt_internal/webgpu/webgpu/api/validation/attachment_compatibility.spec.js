/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Validation for attachment compatibility between render passes, bundles, and pipelines

TODO: Add sparse color attachment compatibility test when defined by specification
`;
import { poptions, params } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { range } from '../../../common/framework/util/util.js';
import {
  kRegularTextureFormatInfo,
  kRegularTextureFormats,
  kSizedDepthStencilFormats,
  kUnsizedDepthStencilFormats,
  kTextureSampleCounts,
  kMaxColorAttachments,
} from '../../capability_info.js';

import { ValidationTest } from './validation_test.js';

const kColorAttachmentCounts = range(kMaxColorAttachments, i => i + 1);
const kDepthStencilAttachmentFormats = [
  undefined,
  ...kSizedDepthStencilFormats,
  ...kUnsizedDepthStencilFormats,
];

class F extends ValidationTest {
  createAttachmentTextureView(format, sampleCount) {
    return this.device
      .createTexture({
        size: [1, 1, 1],
        format,
        usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
        sampleCount,
      })
      .createView();
  }

  createColorAttachment(format, sampleCount) {
    return {
      attachment: this.createAttachmentTextureView(format, sampleCount),
      loadValue: [0, 0, 0, 0],
    };
  }

  createDepthAttachment(format, sampleCount) {
    return {
      attachment: this.createAttachmentTextureView(format, sampleCount),
      depthLoadValue: 0,
      depthStoreOp: 'clear',
      stencilLoadValue: 1,
      stencilStoreOp: 'clear',
    };
  }

  createPassOrBundleEncoder(encoderType, colorFormats, depthStencilFormat, sampleCount) {
    const encoder = this.device.createCommandEncoder();
    const passDesc = {
      colorAttachments: Array.from(colorFormats, desc =>
        this.createColorAttachment(desc, sampleCount)
      ),

      depthStencilAttachment:
        depthStencilFormat !== undefined
          ? this.createDepthAttachment(depthStencilFormat, sampleCount)
          : undefined,
    };

    const pass = encoder.beginRenderPass(passDesc);
    switch (encoderType) {
      case 'render bundle': {
        const bundleEncoder = this.device.createRenderBundleEncoder({
          colorFormats,
          depthStencilFormat,
          sampleCount,
        });

        return {
          encoder: bundleEncoder,
          finish() {
            const bundle = bundleEncoder.finish();
            pass.executeBundles([bundle]);
            pass.endPass();
            return encoder.finish();
          },
        };
      }
      case 'render pass':
        return {
          encoder: pass,
          finish() {
            pass.endPass();
            return encoder.finish();
          },
        };
    }
  }

  createRenderPipeline(colorStates, depthStencilState, sampleCount) {
    return this.device.createRenderPipeline({
      vertexStage: {
        module: this.device.createShaderModule({
          code: `
            [[builtin(position)]] var<out> position : vec4<f32>;

            [[stage(vertex)]] fn main() -> void {
              position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
            }`,
        }),

        entryPoint: 'main',
      },

      fragmentStage: {
        module: this.device.createShaderModule({
          code: '[[stage(fragment)]] fn main() -> void {}',
        }),

        entryPoint: 'main',
      },

      primitiveTopology: 'triangle-list',
      colorStates,
      depthStencilState,
      sampleCount,
    });
  }
}

export const g = makeTestGroup(F);

const kColorAttachmentFormats = kRegularTextureFormats.filter(format => {
  const info = kRegularTextureFormatInfo[format];
  return info.color && info.renderable;
});

g.test('render_pass_and_bundle,color_format')
  .desc('Test that color attachment formats in render passes and bundles must match.')
  .params(
    params()
      .combine(poptions('passFormat', kColorAttachmentFormats))
      .combine(poptions('bundleFormat', kColorAttachmentFormats))
  )
  .fn(t => {
    const { passFormat, bundleFormat } = t.params;
    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: [bundleFormat],
    });

    const bundle = bundleEncoder.finish();
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [t.createColorAttachment(passFormat)],
    });

    pass.executeBundles([bundle]);
    pass.endPass();
    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, passFormat !== bundleFormat);
  });

g.test('render_pass_and_bundle,color_count')
  .desc(
    `
  Test that the number of color attachments in render passes and bundles must match.

  TODO: Add sparse color attachment compatibility test when defined by specification
  `
  )
  .params(
    params()
      .combine(poptions('passCount', kColorAttachmentCounts))
      .combine(poptions('bundleCount', kColorAttachmentCounts))
  )
  .fn(t => {
    const { passCount, bundleCount } = t.params;
    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: range(bundleCount, () => 'rgba8unorm'),
    });

    const bundle = bundleEncoder.finish();

    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: range(passCount, () => t.createColorAttachment('rgba8unorm')),
    });

    pass.executeBundles([bundle]);
    pass.endPass();
    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, passCount !== bundleCount);
  });

g.test('render_pass_and_bundle,depth_format')
  .desc('Test that the depth attachment format in render passes and bundles must match.')
  .params(
    params()
      .combine(poptions('passFormat', kDepthStencilAttachmentFormats))
      .combine(poptions('bundleFormat', kDepthStencilAttachmentFormats))
  )
  .fn(t => {
    const { passFormat, bundleFormat } = t.params;
    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: ['rgba8unorm'],
      depthStencilFormat: bundleFormat,
    });

    const bundle = bundleEncoder.finish();
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [t.createColorAttachment('rgba8unorm')],
      depthStencilAttachment:
        passFormat !== undefined ? t.createDepthAttachment(passFormat) : undefined,
    });

    pass.executeBundles([bundle]);
    pass.endPass();
    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, passFormat !== bundleFormat);
  });

g.test('render_pass_and_bundle,sample_count')
  .desc('Test that the sample count in render passes and bundles must match.')
  .params(
    params()
      .combine(poptions('renderSampleCount', kTextureSampleCounts))
      .combine(poptions('bundleSampleCount', kTextureSampleCounts))
  )
  .fn(t => {
    const { renderSampleCount, bundleSampleCount } = t.params;
    const bundleEncoder = t.device.createRenderBundleEncoder({
      colorFormats: ['rgba8unorm'],
      sampleCount: bundleSampleCount,
    });

    const bundle = bundleEncoder.finish();
    const encoder = t.device.createCommandEncoder();
    const pass = encoder.beginRenderPass({
      colorAttachments: [t.createColorAttachment('rgba8unorm', renderSampleCount)],
    });

    pass.executeBundles([bundle]);
    pass.endPass();
    t.expectValidationError(() => {
      t.queue.submit([encoder.finish()]);
    }, renderSampleCount !== bundleSampleCount);
  });

g.test('render_pass_or_bundle_and_pipeline,color_format')
  .desc(
    `
Test that color attachment formats in render passes or bundles match the pipeline color format.
`
  )
  .params(
    params()
      .combine(poptions('encoderType', ['render pass', 'render bundle']))
      .combine(poptions('encoderFormat', kColorAttachmentFormats))
      .combine(poptions('pipelineFormat', kColorAttachmentFormats))
  )
  .fn(t => {
    const { encoderType, encoderFormat, pipelineFormat } = t.params;
    const pipeline = t.createRenderPipeline([{ format: pipelineFormat }]);

    const { encoder, finish } = t.createPassOrBundleEncoder(encoderType, [encoderFormat]);
    encoder.setPipeline(pipeline);

    t.expectValidationError(() => {
      t.queue.submit([finish()]);
    }, encoderFormat !== pipelineFormat);
  });

g.test('render_pass_or_bundle_and_pipeline,color_count')
  .desc(
    `
Test that the number of color attachments in render passes or bundles match the pipeline color
count.

TODO: Add sparse color attachment compatibility test when defined by specification
`
  )
  .params(
    params()
      .combine(poptions('encoderType', ['render pass', 'render bundle']))
      .combine(poptions('encoderCount', kColorAttachmentCounts))
      .combine(poptions('pipelineCount', kColorAttachmentCounts))
  )
  .fn(t => {
    const { encoderType, encoderCount, pipelineCount } = t.params;
    const pipeline = t.createRenderPipeline(range(pipelineCount, () => ({ format: 'rgba8unorm' })));

    const { encoder, finish } = t.createPassOrBundleEncoder(
      encoderType,
      range(encoderCount, () => 'rgba8unorm')
    );

    encoder.setPipeline(pipeline);

    t.expectValidationError(() => {
      t.queue.submit([finish()]);
    }, encoderCount !== pipelineCount);
  });

g.test('render_pass_or_bundle_and_pipeline,depth_format')
  .desc(
    `
Test that the depth attachment format in render passes or bundles match the pipeline depth format.
`
  )
  .params(
    params()
      .combine(poptions('encoderType', ['render pass', 'render bundle']))
      .combine(poptions('encoderFormat', kDepthStencilAttachmentFormats))
      .combine(poptions('pipelineFormat', kDepthStencilAttachmentFormats))
  )
  .fn(t => {
    const { encoderType, encoderFormat, pipelineFormat } = t.params;
    const pipeline = t.createRenderPipeline(
      [{ format: 'rgba8unorm' }],
      pipelineFormat !== undefined ? { format: pipelineFormat } : undefined
    );

    const { encoder, finish } = t.createPassOrBundleEncoder(
      encoderType,
      ['rgba8unorm'],
      encoderFormat
    );

    encoder.setPipeline(pipeline);

    t.expectValidationError(() => {
      t.queue.submit([finish()]);
    }, encoderFormat !== pipelineFormat);
  });

g.test('render_pass_or_bundle_and_pipeline,sample_count')
  .desc(
    `
Test that the sample count in render passes or bundles match the pipeline sample count.
`
  )
  .params(
    params()
      .combine(poptions('encoderType', ['render pass', 'render bundle']))
      .combine(poptions('encoderSampleCount', kTextureSampleCounts))
      .combine(poptions('pipelineSampleCount', kTextureSampleCounts))
  )
  .fn(t => {
    const { encoderType, encoderSampleCount, pipelineSampleCount } = t.params;
    const pipeline = t.createRenderPipeline(
      [{ format: 'rgba8unorm' }],
      undefined,
      pipelineSampleCount
    );

    const { encoder, finish } = t.createPassOrBundleEncoder(
      encoderType,
      ['rgba8unorm'],
      undefined,
      encoderSampleCount
    );

    encoder.setPipeline(pipeline);

    t.expectValidationError(() => {
      t.queue.submit([finish()]);
    }, encoderSampleCount !== pipelineSampleCount);
  });
