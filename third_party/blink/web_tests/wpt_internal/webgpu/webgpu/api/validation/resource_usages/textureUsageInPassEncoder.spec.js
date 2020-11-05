/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Texture Usages Validation Tests in Render Pass and Compute Pass.

Test Coverage:
  - For each combination of two texture usages:
    - For various subresource ranges (different mip levels or array layers) that overlap a given
      subresources or not for color formats:
      - For various places that resources are used, for example, used in bundle or used in render
        pass directly.
        - Check that an error is generated when read-write or write-write usages are binding to the
          same texture subresource. Otherwise, no error should be generated. One exception is race
          condition upon two writeonly-storage-texture usages, which is valid.

  - For each combination of two texture usages:
    - For various aspects (all, depth-only, stencil-only) that overlap a given subresources or not
      for depth/stencil formats:
      - Check that an error is generated when read-write or write-write usages are binding to the
        same aspect. Otherwise, no error should be generated.

  - Test combinations of two shader stages:
    - Texture usages in bindings with invisible shader stages should be validated. Invisible shader
      stages include shader stage with visibility none, compute shader stage in render pass, and
      vertex/fragment shader stage in compute pass.

  - Tests replaced bindings:
    - Texture usages via bindings replaced by another setBindGroup() upon the same bindGroup index
      in render pass should be validated. However, replaced bindings should not be validated in
      compute pass.

  - Test texture usages in bundle:
    - Texture usages in bundle should be validated if that bundle is executed in the current scope.

  - Test texture usages with unused bindings:
    - Texture usages should be validated even its bindings is not used in pipeline.

  - Test texture usages validation scope:
    - Texture usages should be validated per each render pass. And they should be validated per each
      dispatch call in compute.
`;
import { pbool, poptions, params } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/framework/util/util.js';
import {
  kDepthStencilFormats,
  kDepthStencilFormatInfo,
  kTextureBindingTypes,
  kTextureBindingTypeInfo,
  kShaderStages,
} from '../../../capability_info.js';
import { GPUConst } from '../../../constants.js';
import { ValidationTest } from '../validation_test.js';

const SIZE = 32;
class TextureUsageTracking extends ValidationTest {
  createTexture(options = {}) {
    const {
      width = SIZE,
      height = SIZE,
      arrayLayerCount = 1,
      mipLevelCount = 1,
      sampleCount = 1,
      format = 'rgba8unorm',
      usage = GPUTextureUsage.OUTPUT_ATTACHMENT | GPUTextureUsage.SAMPLED,
    } = options;

    return this.device.createTexture({
      size: { width, height, depth: arrayLayerCount },
      mipLevelCount,
      sampleCount,
      dimension: '2d',
      format,
      usage,
    });
  }

  createBindGroup(index, view, bindingType, dimension, bindingTexFormat) {
    return this.device.createBindGroup({
      entries: [{ binding: index, resource: view }],
      layout: this.device.createBindGroupLayout({
        entries: [
          {
            binding: index,
            visibility: GPUShaderStage.COMPUTE | GPUShaderStage.FRAGMENT,
            type: bindingType,
            viewDimension: dimension,
            storageTextureFormat: bindingTexFormat,
          },
        ],
      }),
    });
  }

  createAndExecuteBundle(index, bindGroup, pass) {
    const bundleEncoder = this.device.createRenderBundleEncoder({
      colorFormats: ['rgba8unorm'],
    });

    bundleEncoder.setBindGroup(index, bindGroup);
    const bundle = bundleEncoder.finish();
    pass.executeBundles([bundle]);
  }

  beginSimpleRenderPass(encoder, view) {
    return encoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: view,
          loadValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
          storeOp: 'store',
        },
      ],
    });
  }

  testValidationScope(compute) {
    // Create two bind groups. Resource usages conflict between these two bind groups. But resource
    // usage inside each bind group doesn't conflict.
    const view = this.createTexture({
      usage: GPUTextureUsage.STORAGE | GPUTextureUsage.SAMPLED,
    }).createView();
    const bindGroup0 = this.createBindGroup(0, view, 'sampled-texture', '2d', undefined);
    const bindGroup1 = this.createBindGroup(
      0,
      view,
      'writeonly-storage-texture',
      '2d',
      'rgba8unorm'
    );

    const encoder = this.device.createCommandEncoder();
    const pass = compute
      ? encoder.beginComputePass()
      : this.beginSimpleRenderPass(encoder, this.createTexture().createView());

    // Create pipeline. Note that bindings unused in pipeline should be validated too.
    const pipeline = compute ? this.createNoOpComputePipeline() : this.createNoOpRenderPipeline();
    return {
      bindGroup0,
      bindGroup1,
      encoder,
      pass,
      pipeline,
    };
  }

  setPipeline(pass, pipeline, compute) {
    if (compute) {
      pass.setPipeline(pipeline);
    } else {
      pass.setPipeline(pipeline);
    }
  }

  issueDrawOrDispatch(pass, compute) {
    if (compute) {
      pass.dispatch(1);
    } else {
      pass.draw(3, 1, 0, 0);
    }
  }

  setComputePipelineAndCallDispatch(pass) {
    const pipeline = this.createNoOpComputePipeline();
    pass.setPipeline(pipeline);
    pass.dispatch(1);
  }
}

export const g = makeTestGroup(TextureUsageTracking);

const BASE_LEVEL = 1;
const TOTAL_LEVELS = 6;
const BASE_LAYER = 1;
const TOTAL_LAYERS = 6;
const SLICE_COUNT = 2;

// For all tests below, we test compute pass if 'compute' is true, and test render pass otherwise.
g.test('subresources_and_binding_types_combination_for_color')
  .params(
    params()
      .combine(pbool('compute'))
      .combine(pbool('binding0InBundle'))
      .combine(pbool('binding1InBundle'))
      .combine([
        // Two texture usages are binding to the same texture subresource.
        {
          levelCount0: 1,
          layerCount0: 1,
          baseLevel1: BASE_LEVEL,
          levelCount1: 1,
          baseLayer1: BASE_LAYER,
          layerCount1: 1,
          _resourceSuccess: false,
        },

        // Two texture usages are binding to different mip levels of the same texture.
        {
          levelCount0: 1,
          layerCount0: 1,
          baseLevel1: BASE_LEVEL + 1,
          levelCount1: 1,
          baseLayer1: BASE_LAYER,
          layerCount1: 1,
          _resourceSuccess: true,
        },

        // Two texture usages are binding to different array layers of the same texture.
        {
          levelCount0: 1,
          layerCount0: 1,
          baseLevel1: BASE_LEVEL,
          levelCount1: 1,
          baseLayer1: BASE_LAYER + 1,
          layerCount1: 1,
          _resourceSuccess: true,
        },

        // The second texture usage contains the whole mip chain where the first texture usage is using.
        {
          levelCount0: 1,
          layerCount0: 1,
          baseLevel1: 0,
          levelCount1: TOTAL_LEVELS,
          baseLayer1: BASE_LAYER,
          layerCount1: 1,
          _resourceSuccess: false,
        },

        // The second texture usage contains all layers where the first texture usage is using.
        {
          levelCount0: 1,
          layerCount0: 1,
          baseLevel1: BASE_LEVEL,
          levelCount1: 1,
          baseLayer1: 0,
          layerCount1: TOTAL_LAYERS,
          _resourceSuccess: false,
        },

        // The second texture usage contains all subresources where the first texture usage is using.
        {
          levelCount0: 1,
          layerCount0: 1,
          baseLevel1: 0,
          levelCount1: TOTAL_LEVELS,
          baseLayer1: 0,
          layerCount1: TOTAL_LAYERS,
          _resourceSuccess: false,
        },

        // Both of the two usages access a few mip levels on the same layer but they don't overlap.
        {
          levelCount0: SLICE_COUNT,
          layerCount0: 1,
          baseLevel1: BASE_LEVEL + SLICE_COUNT,
          levelCount1: 3,
          baseLayer1: BASE_LAYER,
          layerCount1: 1,
          _resourceSuccess: true,
        },

        // Both of the two usages access a few mip levels on the same layer and they overlap.
        {
          levelCount0: SLICE_COUNT,
          layerCount0: 1,
          baseLevel1: BASE_LEVEL + SLICE_COUNT - 1,
          levelCount1: 3,
          baseLayer1: BASE_LAYER,
          layerCount1: 1,
          _resourceSuccess: false,
        },

        // Both of the two usages access a few array layers on the same level but they don't overlap.
        {
          levelCount0: 1,
          layerCount0: SLICE_COUNT,
          baseLevel1: BASE_LEVEL,
          levelCount1: 1,
          baseLayer1: BASE_LAYER + SLICE_COUNT,
          layerCount1: 3,
          _resourceSuccess: true,
        },

        // Both of the two usages access a few array layers on the same level and they overlap.
        {
          levelCount0: 1,
          layerCount0: SLICE_COUNT,
          baseLevel1: BASE_LEVEL,
          levelCount1: 1,
          baseLayer1: BASE_LAYER + SLICE_COUNT - 1,
          layerCount1: 3,
          _resourceSuccess: false,
        },

        // Both of the two usages access a few array layers and mip levels but they don't overlap.
        {
          levelCount0: SLICE_COUNT,
          layerCount0: SLICE_COUNT,
          baseLevel1: BASE_LEVEL + SLICE_COUNT,
          levelCount1: 3,
          baseLayer1: BASE_LAYER + SLICE_COUNT,
          layerCount1: 3,
          _resourceSuccess: true,
        },

        // Both of the two usages access a few array layers and mip levels and they overlap.
        {
          levelCount0: SLICE_COUNT,
          layerCount0: SLICE_COUNT,
          baseLevel1: BASE_LEVEL + SLICE_COUNT - 1,
          levelCount1: 3,
          baseLayer1: BASE_LAYER + SLICE_COUNT - 1,
          layerCount1: 3,
          _resourceSuccess: false,
        },
      ])
      .combine([
        {
          type0: 'sampled-texture',
          type1: 'sampled-texture',
          _usageSuccess: true,
        },

        {
          type0: 'sampled-texture',
          type1: 'readonly-storage-texture',
          _usageSuccess: true,
        },

        {
          type0: 'sampled-texture',
          type1: 'writeonly-storage-texture',
          _usageSuccess: false,
        },

        {
          type0: 'sampled-texture',
          type1: 'render-target',
          _usageSuccess: false,
        },

        {
          type0: 'readonly-storage-texture',
          type1: 'readonly-storage-texture',
          _usageSuccess: true,
        },

        {
          type0: 'readonly-storage-texture',
          type1: 'writeonly-storage-texture',
          _usageSuccess: false,
        },

        {
          type0: 'readonly-storage-texture',
          type1: 'render-target',
          _usageSuccess: false,
        },

        // Race condition upon multiple writable storage texture is valid.
        {
          type0: 'writeonly-storage-texture',
          type1: 'writeonly-storage-texture',
          _usageSuccess: true,
        },

        {
          type0: 'writeonly-storage-texture',
          type1: 'render-target',
          _usageSuccess: false,
        },

        {
          type0: 'render-target',
          type1: 'render-target',
          _usageSuccess: false,
        },
      ])

      // Every color attachment can use only one single subresource.
      .unless(
        p =>
          (p.type0 === 'render-target' && (p.levelCount0 !== 1 || p.layerCount0 !== 1)) ||
          (p.type1 === 'render-target' && (p.levelCount1 !== 1 || p.layerCount1 !== 1))
      )

      // All color attachments' size should be the same.
      .unless(
        p =>
          p.type0 === 'render-target' && p.type1 === 'render-target' && p.baseLevel1 !== BASE_LEVEL
      )
      .unless(
        p =>
          // We can't set 'render-target' in bundle, so we need to exclude it from bundle.
          (p.binding0InBundle && p.type0 === 'render-target') ||
          (p.binding1InBundle && p.type1 === 'render-target')
      )
      .unless(
        p =>
          // We can't set 'render-target' or bundle in compute.
          p.compute &&
          (p.binding0InBundle ||
            p.binding1InBundle ||
            p.type0 === 'render-target' ||
            p.type1 === 'render-target')
      )
  )
  .fn(async t => {
    const {
      compute,
      binding0InBundle,
      binding1InBundle,
      levelCount0,
      layerCount0,
      baseLevel1,
      baseLayer1,
      levelCount1,
      layerCount1,
      type0,
      type1,
      _usageSuccess,
      _resourceSuccess,
    } = t.params;

    const texture = t.createTexture({
      arrayLayerCount: TOTAL_LAYERS,
      mipLevelCount: TOTAL_LEVELS,
      usage: GPUTextureUsage.SAMPLED | GPUTextureUsage.STORAGE | GPUTextureUsage.OUTPUT_ATTACHMENT,
    });

    const dimension0 = layerCount0 !== 1 ? '2d-array' : '2d';
    const view0 = texture.createView({
      dimension: dimension0,
      baseMipLevel: BASE_LEVEL,
      mipLevelCount: levelCount0,
      baseArrayLayer: BASE_LAYER,
      arrayLayerCount: layerCount0,
    });

    const dimension1 = layerCount1 !== 1 ? '2d-array' : '2d';
    const view1 = texture.createView({
      dimension: dimension1,
      baseMipLevel: baseLevel1,
      mipLevelCount: levelCount1,
      baseArrayLayer: baseLayer1,
      arrayLayerCount: layerCount1,
    });

    const encoder = t.device.createCommandEncoder();
    if (type0 === 'render-target') {
      // Note that type1 is 'render-target' too. So we don't need to create bindings.
      assert(type1 === 'render-target');
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            attachment: view0,
            loadValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
            storeOp: 'store',
          },

          {
            attachment: view1,
            loadValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
            storeOp: 'store',
          },
        ],
      });

      pass.endPass();
    } else {
      const pass = compute
        ? encoder.beginComputePass()
        : t.beginSimpleRenderPass(
            encoder,
            type1 === 'render-target' ? view1 : t.createTexture().createView()
          );

      // Create bind groups. Set bind groups in pass directly or set bind groups in bundle.
      const storageTextureFormat0 = type0 === 'sampled-texture' ? undefined : 'rgba8unorm';
      const bindGroup0 = t.createBindGroup(0, view0, type0, dimension0, storageTextureFormat0);
      if (binding0InBundle) {
        assert(pass instanceof GPURenderPassEncoder);
        t.createAndExecuteBundle(0, bindGroup0, pass);
      } else {
        pass.setBindGroup(0, bindGroup0);
      }
      if (type1 !== 'render-target') {
        const storageTextureFormat1 = type1 === 'sampled-texture' ? undefined : 'rgba8unorm';
        const bindGroup1 = t.createBindGroup(1, view1, type1, dimension1, storageTextureFormat1);
        if (binding1InBundle) {
          assert(pass instanceof GPURenderPassEncoder);
          t.createAndExecuteBundle(1, bindGroup1, pass);
        } else {
          pass.setBindGroup(1, bindGroup1);
        }
      }
      if (compute) t.setComputePipelineAndCallDispatch(pass);
      pass.endPass();
    }

    const success = _resourceSuccess || _usageSuccess;
    t.expectValidationError(() => {
      encoder.finish();
    }, !success);
  });

g.test('subresources_and_binding_types_combination_for_aspect')
  .params(
    params()
      .combine(pbool('compute'))
      .combine(pbool('binding0InBundle'))
      .combine(pbool('binding1InBundle'))
      .combine(poptions('format', kDepthStencilFormats))
      .combine([
        {
          baseLevel: BASE_LEVEL,
          baseLayer: BASE_LAYER,
          _resourceSuccess: false,
        },

        {
          baseLevel: BASE_LEVEL + 1,
          baseLayer: BASE_LAYER,
          _resourceSuccess: true,
        },

        {
          baseLevel: BASE_LEVEL,
          baseLayer: BASE_LAYER + 1,
          _resourceSuccess: true,
        },
      ])
      .combine(poptions('aspect0', ['all', 'depth-only', 'stencil-only']))
      .combine(poptions('aspect1', ['all', 'depth-only', 'stencil-only']))
      .unless(
        p =>
          (p.aspect0 === 'stencil-only' && !kDepthStencilFormatInfo[p.format].stencil) ||
          (p.aspect1 === 'stencil-only' && !kDepthStencilFormatInfo[p.format].stencil)
      )
      .unless(
        p =>
          (p.aspect0 === 'depth-only' && !kDepthStencilFormatInfo[p.format].depth) ||
          (p.aspect1 === 'depth-only' && !kDepthStencilFormatInfo[p.format].depth)
      )
      .combine([
        {
          type0: 'sampled-texture',
          type1: 'sampled-texture',
          _usageSuccess: true,
        },

        {
          type0: 'sampled-texture',
          type1: 'render-target',
          _usageSuccess: false,
        },
      ])
      .unless(
        p =>
          // We can't set 'render-target' in bundle, so we need to exclude it from bundle.
          p.binding1InBundle && p.type1 === 'render-target'
      )
      .unless(
        p =>
          // We can't set 'render-target' or bundle in compute. Note that type0 is definitely not
          // 'render-target'
          p.compute && (p.binding0InBundle || p.binding1InBundle || p.type1 === 'render-target')
      )
  )
  .fn(async t => {
    const {
      compute,
      binding0InBundle,
      binding1InBundle,
      format,
      baseLevel,
      baseLayer,
      aspect0,
      aspect1,
      type0,
      type1,
      _resourceSuccess,
      _usageSuccess,
    } = t.params;

    const texture = t.createTexture({
      arrayLayerCount: TOTAL_LAYERS,
      mipLevelCount: TOTAL_LEVELS,
      format,
    });

    const view0 = texture.createView({
      baseMipLevel: BASE_LEVEL,
      mipLevelCount: 1,
      baseArrayLayer: BASE_LAYER,
      arrayLayerCount: 1,
      aspect: aspect0,
    });

    const view1 = texture.createView({
      baseMipLevel: baseLevel,
      mipLevelCount: 1,
      baseArrayLayer: baseLayer,
      arrayLayerCount: 1,
      aspect: aspect1,
    });

    const encoder = t.device.createCommandEncoder();
    // Color attachment's size should match depth/stencil attachment's size. Note that if
    // type1 !== 'render-target' then there's no depthStencilAttachment to match anyway.
    const size = SIZE >> baseLevel;
    const pass = compute
      ? encoder.beginComputePass()
      : encoder.beginRenderPass({
          colorAttachments: [
            {
              attachment: t.createTexture({ width: size, height: size }).createView(),
              loadValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
              storeOp: 'store',
            },
          ],

          depthStencilAttachment:
            type1 !== 'render-target'
              ? undefined
              : {
                  attachment: view1,
                  depthStoreOp: 'clear',
                  depthLoadValue: 'load',
                  stencilStoreOp: 'clear',
                  stencilLoadValue: 'load',
                },
        });

    // Create bind groups. Set bind groups in pass directly or set bind groups in bundle.
    const bindGroup0 = t.createBindGroup(0, view0, type0, '2d', undefined);
    if (binding0InBundle) {
      assert(pass instanceof GPURenderPassEncoder);
      t.createAndExecuteBundle(0, bindGroup0, pass);
    } else {
      pass.setBindGroup(0, bindGroup0);
    }
    if (type1 !== 'render-target') {
      const bindGroup1 = t.createBindGroup(1, view1, type1, '2d', undefined);
      if (binding1InBundle) {
        assert(pass instanceof GPURenderPassEncoder);
        t.createAndExecuteBundle(1, bindGroup1, pass);
      } else {
        pass.setBindGroup(1, bindGroup1);
      }
    }
    if (compute) t.setComputePipelineAndCallDispatch(pass);
    pass.endPass();

    const disjointAspects =
      (aspect0 === 'depth-only' && aspect1 === 'stencil-only') ||
      (aspect0 === 'stencil-only' && aspect1 === 'depth-only');

    // If subresources' mip/array slices has no overlap, or their binding types don't conflict,
    // it will definitely success no matter what aspects they are binding to.
    const success = disjointAspects || _resourceSuccess || _usageSuccess;

    t.expectValidationError(() => {
      encoder.finish();
    }, !success);
  });

g.test('shader_stages_and_visibility')
  .params(
    params()
      .combine(pbool('compute'))
      .combine(poptions('readVisibility', [0, ...kShaderStages]))
      .combine(poptions('writeVisibility', [0, ...kShaderStages]))
      .unless(
        p =>
          // Writeonly-storage-texture binding type is not supported in vertex stage. But it is the
          // only way to write into texture in compute. So there is no means to successfully create
          // a binding which attempt to write into stage(s) with vertex stage in compute pass.
          p.compute && Boolean(p.writeVisibility & GPUConst.ShaderStage.VERTEX)
      )
  )
  .fn(async t => {
    const { compute, readVisibility, writeVisibility } = t.params;

    // writeonly-storage-texture binding type is not supported in vertex stage. So, this test
    // uses writeonly-storage-texture binding as writable binding upon the same subresource if
    // vertex stage is not included. Otherwise, it uses output attachment instead.
    const writeHasVertexStage = Boolean(writeVisibility & GPUShaderStage.VERTEX);
    const texUsage = writeHasVertexStage
      ? GPUTextureUsage.SAMPLED | GPUTextureUsage.OUTPUT_ATTACHMENT
      : GPUTextureUsage.SAMPLED | GPUTextureUsage.STORAGE;

    const texture = t.createTexture({ usage: texUsage });
    const view = texture.createView();
    const bglEntries = [{ binding: 0, visibility: readVisibility, type: 'sampled-texture' }];

    const bgEntries = [{ binding: 0, resource: view }];
    if (!writeHasVertexStage) {
      bglEntries.push({
        binding: 1,
        visibility: writeVisibility,
        type: 'writeonly-storage-texture',
        storageTextureFormat: 'rgba8unorm',
      });

      bgEntries.push({ binding: 1, resource: view });
    }
    const bindGroup = t.device.createBindGroup({
      entries: bgEntries,
      layout: t.device.createBindGroupLayout({ entries: bglEntries }),
    });

    const encoder = t.device.createCommandEncoder();
    const pass = compute
      ? encoder.beginComputePass()
      : t.beginSimpleRenderPass(
          encoder,
          writeHasVertexStage ? view : t.createTexture().createView()
        );

    pass.setBindGroup(0, bindGroup);
    if (compute) t.setComputePipelineAndCallDispatch(pass);
    pass.endPass();

    // Texture usages in bindings with invisible shader stages should be validated. Invisible shader
    // stages include shader stage with visibility none, compute shader stage in render pass, and
    // vertex/fragment shader stage in compute pass.
    t.expectValidationError(() => {
      encoder.finish();
    });
  });

// We should validate the texture usages in bindings which are replaced by another setBindGroup()
// call site upon the same index in the same render pass. However, replaced bindings in compute
// should not be validated.
g.test('replaced_binding')
  .params(
    params()
      .combine(pbool('compute'))
      .combine(pbool('callDrawOrDispatch'))
      .combine(poptions('bindingType', kTextureBindingTypes))
  )
  .fn(async t => {
    const { compute, callDrawOrDispatch, bindingType } = t.params;
    const info = kTextureBindingTypeInfo[bindingType];
    const bindingTexFormat = info.resource === 'storageTex' ? 'rgba8unorm' : undefined;

    const sampledView = t.createTexture().createView();
    const sampledStorageView = t
      .createTexture({ usage: GPUTextureUsage.STORAGE | GPUTextureUsage.SAMPLED })
      .createView();

    // Create bindGroup0. It has two bindings. These two bindings use different views/subresources.
    const bglEntries0 = [
      { binding: 0, visibility: GPUShaderStage.FRAGMENT, type: 'sampled-texture' },
      {
        binding: 1,
        visibility: GPUShaderStage.FRAGMENT,
        type: bindingType,
        storageTextureFormat: bindingTexFormat,
      },
    ];

    const bgEntries0 = [
      { binding: 0, resource: sampledView },
      { binding: 1, resource: sampledStorageView },
    ];

    const bindGroup0 = t.device.createBindGroup({
      entries: bgEntries0,
      layout: t.device.createBindGroupLayout({ entries: bglEntries0 }),
    });

    // Create bindGroup1. It has one binding, which use the same view/subresoure of a binding in
    // bindGroup0. So it may or may not conflicts with that binding in bindGroup0.
    const bindGroup1 = t.createBindGroup(0, sampledStorageView, 'sampled-texture', '2d', undefined);

    const encoder = t.device.createCommandEncoder();
    const pass = compute
      ? encoder.beginComputePass()
      : t.beginSimpleRenderPass(encoder, t.createTexture().createView());

    // Set bindGroup0 and bindGroup1. bindGroup0 is replaced by bindGroup1 in the current pass.
    // But bindings in bindGroup0 should be validated too.
    pass.setBindGroup(0, bindGroup0);
    if (callDrawOrDispatch) {
      const pipeline = compute ? t.createNoOpComputePipeline() : t.createNoOpRenderPipeline();
      t.setPipeline(pass, pipeline, compute);
      t.issueDrawOrDispatch(pass, compute);
    }
    pass.setBindGroup(0, bindGroup1);
    pass.endPass();

    // TODO: If the Compatible Usage List (https://gpuweb.github.io/gpuweb/#compatible-usage-list)
    // gets programmatically defined in capability_info, use it here, instead of this logic, for clarity.
    let success = bindingType !== 'writeonly-storage-texture';
    // Replaced bindings should not be validated in compute pass, because validation only occurs
    // inside dispatch() which only looks at the current resource usages.
    success || (success = compute);

    t.expectValidationError(() => {
      encoder.finish();
    }, !success);
  });

g.test('bindings_in_bundle')
  .params(
    params()
      .combine(pbool('binding0InBundle'))
      .combine(pbool('binding1InBundle'))
      .combine(poptions('type0', ['render-target', ...kTextureBindingTypes]))
      .combine(poptions('type1', ['render-target', ...kTextureBindingTypes]))
      .unless(
        p =>
          // We can't set 'render-target' in bundle, so we need to exclude it from bundle.
          // In addition, if both bindings are non-bundle, there is no need to test it because
          // we have far more comprehensive test cases for that situation in this file.
          (p.binding0InBundle && p.type0 === 'render-target') ||
          (p.binding1InBundle && p.type1 === 'render-target') ||
          (!p.binding0InBundle && !p.binding1InBundle)
      )
  )
  .fn(async t => {
    const { binding0InBundle, binding1InBundle, type0, type1 } = t.params;

    // Two bindings are attached to the same texture view.
    const view = t
      .createTexture({
        usage:
          GPUTextureUsage.OUTPUT_ATTACHMENT | GPUTextureUsage.STORAGE | GPUTextureUsage.SAMPLED,
      })
      .createView();

    const bindGroups = [];
    if (type0 !== 'render-target') {
      const binding0TexFormat = type0 === 'sampled-texture' ? undefined : 'rgba8unorm';
      bindGroups[0] = t.createBindGroup(0, view, type0, '2d', binding0TexFormat);
    }
    if (type1 !== 'render-target') {
      const binding1TexFormat = type1 === 'sampled-texture' ? undefined : 'rgba8unorm';
      bindGroups[1] = t.createBindGroup(1, view, type1, '2d', binding1TexFormat);
    }

    const encoder = t.device.createCommandEncoder();
    // At least one binding is in bundle, which means that its type is not 'render-target'.
    // As a result, only one binding's type is 'render-target' at most.
    const pass = t.beginSimpleRenderPass(
      encoder,
      type0 === 'render-target' || type1 === 'render-target' ? view : t.createTexture().createView()
    );

    const bindingsInBundle = [binding0InBundle, binding1InBundle];
    for (let i = 0; i < 2; i++) {
      // Create a bundle for each bind group if its bindings is required to be in bundle on purpose.
      // Otherwise, call setBindGroup directly in pass if needed (when its binding is not
      // 'render-target').
      if (bindingsInBundle[i]) {
        const bundleEncoder = t.device.createRenderBundleEncoder({
          colorFormats: ['rgba8unorm'],
        });

        bundleEncoder.setBindGroup(i, bindGroups[i]);
        const bundleInPass = bundleEncoder.finish();
        pass.executeBundles([bundleInPass]);
      } else if (bindGroups[i] !== undefined) {
        pass.setBindGroup(i, bindGroups[i]);
      }
    }

    pass.endPass();

    let success = false;
    if (
      (type0 === 'sampled-texture' || type0 === 'readonly-storage-texture') &&
      (type1 === 'sampled-texture' || type1 === 'readonly-storage-texture')
    ) {
      success = true;
    }

    if (type0 === 'writeonly-storage-texture' && type1 === 'writeonly-storage-texture') {
      success = true;
    }

    // Resource usages in bundle should be validated.
    t.expectValidationError(() => {
      encoder.finish();
    }, !success);
  });

g.test('unused_bindings_in_pipeline')
  .params(
    params()
      .combine(pbool('compute'))
      .combine(pbool('useBindGroup0'))
      .combine(pbool('useBindGroup1'))
      .combine(poptions('setBindGroupsOrder', ['common', 'reversed']))
      .combine(poptions('setPipeline', ['before', 'middle', 'after', 'none']))
      .combine(pbool('callDrawOrDispatch'))
  )
  .fn(async t => {
    const {
      compute,
      useBindGroup0,
      useBindGroup1,
      setBindGroupsOrder,
      setPipeline,
      callDrawOrDispatch,
    } = t.params;
    const view = t.createTexture({ usage: GPUTextureUsage.STORAGE }).createView();
    const bindGroup0 = t.createBindGroup(0, view, 'readonly-storage-texture', '2d', 'rgba8unorm');
    const bindGroup1 = t.createBindGroup(0, view, 'writeonly-storage-texture', '2d', 'rgba8unorm');

    const wgslVertex = `
      fn main() -> void {
        return;
      }

      entry_point vertex = main;
    `;
    // TODO: revisit the shader code once 'image' can be supported in wgsl.
    const wgslFragment = `
      ${useBindGroup0 ? '[[set 0, binding 0]] var<image> image0;' : ''}
      ${useBindGroup1 ? '[[set 1, binding 0]] var<image> image1;' : ''}
      fn main() -> void {
        return;
      }

      entry_point fragment = main;
    `;

    // TODO: revisit the shader code once 'image' can be supported in wgsl.
    const wgslCompute = `
      ${useBindGroup0 ? '[[set 0, binding 0]] var<image> image0;' : ''}
      ${useBindGroup1 ? '[[set 1, binding 0]] var<image> image1;' : ''}
      fn main() -> void {
        return;
      }

      entry_point compute = main;
    `;

    const pipeline = compute
      ? t.device.createComputePipeline({
          computeStage: {
            module: t.device.createShaderModule({
              code: wgslCompute,
            }),

            entryPoint: 'main',
          },
        })
      : t.device.createRenderPipeline({
          vertexStage: {
            module: t.device.createShaderModule({
              code: wgslVertex,
            }),

            entryPoint: 'main',
          },

          fragmentStage: {
            module: t.device.createShaderModule({
              code: wgslFragment,
            }),

            entryPoint: 'main',
          },

          primitiveTopology: 'triangle-list',
          colorStates: [{ format: 'rgba8unorm' }],
        });

    const encoder = t.device.createCommandEncoder();
    const pass = compute
      ? encoder.beginComputePass()
      : encoder.beginRenderPass({
          colorAttachments: [
            {
              attachment: t.createTexture().createView(),
              loadValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
              storeOp: 'store',
            },
          ],
        });

    const index0 = setBindGroupsOrder === 'common' ? 0 : 1;
    const index1 = setBindGroupsOrder === 'common' ? 1 : 0;
    if (setPipeline === 'before') t.setPipeline(pass, pipeline, compute);
    pass.setBindGroup(index0, bindGroup0);
    if (setPipeline === 'middle') t.setPipeline(pass, pipeline, compute);
    pass.setBindGroup(index1, bindGroup1);
    if (setPipeline === 'after') t.setPipeline(pass, pipeline, compute);
    if (callDrawOrDispatch) t.issueDrawOrDispatch(pass, compute);
    pass.endPass();

    // Resource usage validation scope is defined by dispatch calls. If dispatch is not called,
    // we don't need to do resource usage validation and no validation error to be reported.
    const success = compute && !callDrawOrDispatch;

    t.expectValidationError(() => {
      encoder.finish();
    }, !success);
  });

g.test('validation_scope,no_draw_or_dispatch')
  .params(pbool('compute'))
  .fn(async t => {
    const { compute } = t.params;

    const { bindGroup0, bindGroup1, encoder, pass, pipeline } = t.testValidationScope(compute);
    t.setPipeline(pass, pipeline, compute);
    pass.setBindGroup(0, bindGroup0);
    pass.setBindGroup(1, bindGroup1);
    pass.endPass();

    // Resource usage validation scope is defined by dispatch calls. If dispatch is not called,
    // we don't need to do resource usage validation and no validation error to be reported.
    t.expectValidationError(() => {
      encoder.finish();
    }, !compute);
  });

g.test('validation_scope,same_draw_or_dispatch')
  .params(pbool('compute'))
  .fn(async t => {
    const { compute } = t.params;

    const { bindGroup0, bindGroup1, encoder, pass, pipeline } = t.testValidationScope(compute);
    t.setPipeline(pass, pipeline, compute);
    pass.setBindGroup(0, bindGroup0);
    pass.setBindGroup(1, bindGroup1);
    t.issueDrawOrDispatch(pass, compute);
    pass.endPass();

    t.expectValidationError(() => {
      encoder.finish();
    });
  });

g.test('validation_scope,different_draws_or_dispatches')
  .params(pbool('compute'))
  .fn(async t => {
    const { compute } = t.params;
    const { bindGroup0, bindGroup1, encoder, pass, pipeline } = t.testValidationScope(compute);
    t.setPipeline(pass, pipeline, compute);

    pass.setBindGroup(0, bindGroup0);
    t.issueDrawOrDispatch(pass, compute);

    pass.setBindGroup(1, bindGroup1);
    t.issueDrawOrDispatch(pass, compute);

    pass.endPass();

    // Note that bindGroup0 will be inherited in the second draw/dispatch.
    t.expectValidationError(() => {
      encoder.finish();
    });
  });

g.test('validation_scope,different_passes')
  .params(pbool('compute'))
  .fn(async t => {
    const { compute } = t.params;
    const { bindGroup0, bindGroup1, encoder, pass, pipeline } = t.testValidationScope(compute);
    t.setPipeline(pass, pipeline, compute);
    pass.setBindGroup(0, bindGroup0);
    if (compute) t.setComputePipelineAndCallDispatch(pass);
    pass.endPass();

    const pass1 = compute
      ? encoder.beginComputePass()
      : t.beginSimpleRenderPass(encoder, t.createTexture().createView());
    t.setPipeline(pass1, pipeline, compute);
    pass1.setBindGroup(1, bindGroup1);
    if (compute) t.setComputePipelineAndCallDispatch(pass);
    pass1.endPass();

    // No validation error.
    encoder.finish();
  });
