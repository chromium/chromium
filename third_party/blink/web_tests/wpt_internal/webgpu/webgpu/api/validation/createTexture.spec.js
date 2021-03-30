/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `createTexture validation tests.`;
import { poptions, params } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert } from '../../../common/framework/util/util.js';
import {
  kAllTextureFormats,
  kAllTextureFormatInfo,
  kCompressedTextureFormats,
  kCompressedTextureFormatInfo,
  kTextureDimensions,
  kTextureUsages,
  kUncompressedTextureFormats,
  kUncompressedTextureFormatInfo,
} from '../../capability_info.js';
import { DefaultLimits, GPUConst } from '../../constants.js';
import { maxMipLevelCount } from '../../util/texture/base.js';

import { ValidationTest } from './validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('zero_size')
  .desc(
    `Test texture creation with zero or nonzero size of
    width, height, depthOrArrayLayers and mipLevelCount for every dimension, and representative formats.
    TODO: add tests for depth/stencil format if depth/stencil format can support mipmaps.`
  )
  .cases(poptions('dimension', [undefined, ...kTextureDimensions]))
  .subcases(({ dimension }) =>
    params()
      .combine(
        poptions('zeroArgument', ['none', 'width', 'height', 'depthOrArrayLayers', 'mipLevelCount'])
      )
      .combine(poptions('format', ['rgba8unorm', 'rgb10a2unorm', 'bc1-rgba-unorm']))
      .unless(
        ({ format }) => format === 'bc1-rgba-unorm' && dimension !== '2d' && dimension !== undefined
      )
  )
  .fn(async t => {
    const { dimension, zeroArgument, format } = t.params;

    const info = kAllTextureFormatInfo[format];

    await t.selectDeviceOrSkipTestCase(info.extension);

    const size = [info.blockWidth, info.blockHeight, 1];
    let mipLevelCount = 1;

    switch (zeroArgument) {
      case 'width':
        size[0] = 0;
        break;
      case 'height':
        size[1] = 0;
        break;
      case 'depthOrArrayLayers':
        size[2] = 0;
        break;
      case 'mipLevelCount':
        mipLevelCount = 0;
        break;
      default:
        break;
    }

    const descriptor = {
      size,
      mipLevelCount,
      dimension,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success = zeroArgument === 'none';
    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('mipLevelCount,format')
  .desc(
    `Test texture creation with no mipmap chain, partial mipmap chain, full mipmap chain, out-of-bounds mipmap chain
    for every format with different texture dimension types.
    TODO: test 1D and 3D dimensions. Note that it is invalid for some formats with 1D/3D and/or mipmapping.`
  )
  .subcases(() =>
    params()
      .combine(poptions('format', kAllTextureFormats))
      .combine(poptions('mipLevelCount', [1, 3, 6, 7]))
  )
  .fn(async t => {
    const { format, mipLevelCount } = t.params;

    await t.selectDeviceOrSkipTestCase(kAllTextureFormatInfo[format].extension);

    const size = [32, 32, 1];
    const descriptor = {
      size,
      mipLevelCount,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success = mipLevelCount <= 6;
    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('mipLevelCount,bound_check')
  .desc(
    `Test mip level count bound check upon different texture size and different texture dimension types.
    The cases below test: 1) there must be no mip levels after a 1 level (1D texture), or 1x1 level (2D texture), or 1x1x1 level (3D texture), 2) array layers are not mip-mapped, 3) power-of-two, non-power-of-two, and non-square sizes.`
  )
  .subcases(() =>
    params()
      .combine(poptions('format', ['rgba8unorm', 'bc1-rgba-unorm']))
      .combine([
        { size: [32, 32] }, // Mip level sizes: 32x32, 16x16, 8x8, 4x4, 2x2, 1x1
        { size: [31, 32] }, // Mip level sizes: 31x32, 15x16, 7x8, 3x4, 1x2, 1x1
        { size: [28, 32] }, // Mip level sizes: 28x32, 14x16, 7x8, 3x4, 1x2, 1x1
        { size: [32, 31] }, // Mip level sizes: 32x31, 16x15, 8x7, 4x3, 2x1, 1x1
        { size: [32, 28] }, // Mip level sizes: 32x28, 16x14, 8x7, 4x3, 2x1, 1x1
        { size: [31, 31] }, // Mip level sizes: 31x31, 15x15, 7x7, 3x3, 1x1
        { size: [32], dimension: '1d' }, // Mip level sizes: 32, 16, 8, 4, 2, 1
        { size: [31], dimension: '1d' }, // Mip level sizes: 31, 15, 7, 3, 1
        { size: [32, 32, 32], dimension: '3d' }, // Mip level sizes: 32x32x32, 16x16x16, 8x8x8, 4x4x4, 2x2x2, 1x1x1
        { size: [32, 31, 31], dimension: '3d' }, // Mip level sizes: 32x31x31, 16x15x15, 8x7x7, 4x3x3, 2x1x1, 1x1x1
        { size: [31, 32, 31], dimension: '3d' }, // Mip level sizes: 31x32x31, 15x16x15, 7x8x7, 3x4x3, 1x2x1, 1x1x1
        { size: [31, 31, 32], dimension: '3d' }, // Mip level sizes: 31x31x32, 15x15x16, 7x7x8, 3x3x4, 1x1x2, 1x1x1
        { size: [31, 31, 31], dimension: '3d' }, // Mip level sizes: 31x31x31, 15x15x15, 7x7x7, 3x3x3, 1x1x1
        { size: [32, 8] }, // Mip levels: 32x8, 16x4, 8x2, 4x1, 2x1, 1x1
        { size: [32, 32, 64] }, // Mip levels: 32x32x64, 16x16x64, 8x8x64, 4x4x64, 2x2x64, 1x1x64
        { size: [32, 32, 64], dimension: '3d' }, // Mip levels: 32x32x64, 16x16x32, 8x8x16, 4x4x8, 2x2x4, 1x1x2, 1x1x1
      ])
      .unless(
        ({ format, size, dimension }) =>
          format === 'bc1-rgba-unorm' &&
          (dimension === '1d' ||
            dimension === '3d' ||
            size[0] % kAllTextureFormatInfo[format].blockWidth !== 0 ||
            size[1] % kAllTextureFormatInfo[format].blockHeight !== 0)
      )
  )
  .fn(async t => {
    const { format, size, dimension } = t.params;

    await t.selectDeviceOrSkipTestCase(kAllTextureFormatInfo[format].extension);

    const descriptor = {
      size,
      dimension,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const mipLevelCount = maxMipLevelCount(descriptor);
    descriptor.mipLevelCount = mipLevelCount;
    t.device.createTexture(descriptor);

    descriptor.mipLevelCount = mipLevelCount + 1;
    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    });
  });

g.test('mipLevelCount,bound_check,bigger_than_integer_bit_width')
  .desc(`Test mip level count bound check when mipLevelCount is bigger than integer bit width`)
  .fn(async t => {
    const descriptor = {
      size: [32, 32],
      mipLevelCount: 100,
      format: 'rgba8unorm',
      usage: GPUTextureUsage.SAMPLED,
    };

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    });
  });

g.test('sampleCount,various_sampleCount_with_all_formats')
  .desc(`Test texture creation with various (valid or invalid) sample count and all formats`)
  .subcases(() =>
    params()
      .combine(poptions('sampleCount', [0, 1, 2, 4, 8, 16, 32, 256]))
      .combine(poptions('format', kAllTextureFormats))
  )
  .fn(async t => {
    const { sampleCount, format } = t.params;

    await t.selectDeviceOrSkipTestCase(kAllTextureFormatInfo[format].extension);

    const descriptor = {
      size: [32, 32, 1],
      sampleCount,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success =
      sampleCount === 1 || (sampleCount === 4 && kAllTextureFormatInfo[format].multisample);
    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('sampleCount,valid_sampleCount_with_other_parameter_varies')
  .desc(
    `Test texture creation with valid sample count when dimensions, arrayLayerCount, mipLevelCount, format, and usage varies.
     Texture can be single sample (sampleCount is 1) or multi-sample (sampleCount is 4).
     Multisample texture requires that 1) its dimension is 2d or undefined, 2) its format is a uncompressed format, 3) its mipLevelCount and arrayLayerCount are 1, 4) its usage doesn't include STORAGE.`
  )
  .cases(poptions('dimension', [undefined, ...kTextureDimensions]))
  .subcases(({ dimension }) =>
    params()
      .combine(poptions('sampleCount', [1, 4]))
      .combine(poptions('arrayLayerCount', [1, 2]))
      .unless(
        ({ arrayLayerCount }) =>
          arrayLayerCount === 2 && dimension !== '2d' && dimension !== undefined
      )
      .combine(poptions('mipLevelCount', [1, 2]))
      .combine(poptions('format', kAllTextureFormats))
      .combine(poptions('usage', kTextureUsages))
      .unless(({ usage, format }) => {
        const info = kAllTextureFormatInfo[format];
        return (
          ((usage & GPUConst.TextureUsage.RENDER_ATTACHMENT) !== 0 && !info.renderable) ||
          ((usage & GPUConst.TextureUsage.STORAGE) !== 0 && !info.storage)
        );
      })
  )
  .fn(async t => {
    const { dimension, sampleCount, format, mipLevelCount, arrayLayerCount, usage } = t.params;

    await t.selectDeviceOrSkipTestCase(kAllTextureFormatInfo[format].extension);

    const size =
      dimension === '1d'
        ? [32, 1, 1]
        : dimension === '2d' || dimension === undefined
        ? [32, 32, arrayLayerCount]
        : [32, 32, 32];
    const descriptor = {
      size,
      mipLevelCount,
      sampleCount,
      dimension,
      format,
      usage,
    };

    const success =
      sampleCount === 1 ||
      (sampleCount === 4 &&
        (dimension === '2d' || dimension === undefined) &&
        kAllTextureFormatInfo[format].multisample &&
        mipLevelCount === 1 &&
        arrayLayerCount === 1 &&
        (usage & GPUConst.TextureUsage.STORAGE) === 0);

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('texture_size,default_value_and_smallest_size,uncompressed_format')
  .desc(
    `Test default values for height and depthOrArrayLayers for every dimension type and every uncompressed format.
	  It also tests smallest size (lower bound) for every dimension type and every uncompressed format, while other texture_size tests are testing the upper bound.`
  )
  .cases(poptions('dimension', [undefined, ...kTextureDimensions]))
  .subcases(() =>
    params()
      .combine(poptions('format', kUncompressedTextureFormats))
      .combine(poptions('size', [[1], [1, 1], [1, 1, 1]]))
  )
  .fn(async t => {
    const { dimension, format, size } = t.params;

    await t.selectDeviceOrSkipTestCase(kAllTextureFormatInfo[format].extension);

    const descriptor = {
      size,
      dimension,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    t.device.createTexture(descriptor);
  });

g.test('texture_size,default_value_and_smallest_size,compressed_format')
  .desc(
    `Test default values for height and depthOrArrayLayers for every dimension type and every compressed format.
	  It also tests smallest size (lower bound) for every dimension type and every compressed format, while other texture_size tests are testing the upper bound.`
  )
  .cases(poptions('dimension', [undefined, ...kTextureDimensions]))
  .subcases(() =>
    params()
      .combine(poptions('format', kCompressedTextureFormats))
      .expand(p => {
        const { blockWidth, blockHeight } = kAllTextureFormatInfo[p.format];
        return [
          { size: [1], _success: false },
          { size: [blockWidth], _success: false },
          { size: [1, 1], _success: false },
          { size: [blockWidth, blockHeight], _success: true },
          { size: [1, 1, 1], _success: false },
          { size: [blockWidth, blockHeight, 1], _success: true },
        ];
      })
  )
  .fn(async t => {
    const { dimension, format, size, _success } = t.params;

    const info = kCompressedTextureFormatInfo[format];

    await t.selectDeviceOrSkipTestCase(info.extension);

    const descriptor = {
      size,
      dimension,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !_success);
  });

g.test('texture_size,1d_texture')
  .desc(`Test texture size requirement for 1D texture`)
  .subcases(() =>
    params()
      .combine(poptions('format', kAllTextureFormats))
      .combine(
        poptions('width', [
          DefaultLimits.maxTextureDimension1D - 1,
          DefaultLimits.maxTextureDimension1D,
          DefaultLimits.maxTextureDimension1D + 1,
        ])
      )
      .combine(poptions('height', [1, 2]))
      .combine(poptions('depthOrArrayLayers', [1, 2]))
  )
  .fn(async t => {
    const { format, width, height, depthOrArrayLayers } = t.params;

    await t.selectDeviceOrSkipTestCase(kAllTextureFormatInfo[format].extension);

    const descriptor = {
      size: [width, height, depthOrArrayLayers],
      dimension: '1d',
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success =
      width <= DefaultLimits.maxTextureDimension1D && height === 1 && depthOrArrayLayers === 1;

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('texture_size,2d_texture,uncompressed_format')
  .desc(`Test texture size requirement for 2D texture with uncompressed format.`)
  .cases(poptions('dimension', [undefined, '2d']))
  .subcases(() =>
    params()
      .combine(poptions('format', kUncompressedTextureFormats))
      .combine(
        poptions('size', [
          // Test the bound of width
          [DefaultLimits.maxTextureDimension2D - 1, 1, 1],
          [DefaultLimits.maxTextureDimension2D, 1, 1],
          [DefaultLimits.maxTextureDimension2D + 1, 1, 1],
          // Test the bound of height
          [1, DefaultLimits.maxTextureDimension2D - 1, 1],
          [1, DefaultLimits.maxTextureDimension2D, 1],
          [1, DefaultLimits.maxTextureDimension2D + 1, 1],
          // Test the bound of array layers
          [1, 1, DefaultLimits.maxTextureArrayLayers - 1],
          [1, 1, DefaultLimits.maxTextureArrayLayers],
          [1, 1, DefaultLimits.maxTextureArrayLayers + 1],
        ])
      )
  )
  .fn(async t => {
    const { dimension, format, size } = t.params;

    await t.selectDeviceOrSkipTestCase(kUncompressedTextureFormatInfo[format].extension);

    const descriptor = {
      size,
      dimension,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success =
      size[0] <= DefaultLimits.maxTextureDimension2D &&
      size[1] <= DefaultLimits.maxTextureDimension2D &&
      size[2] <= DefaultLimits.maxTextureArrayLayers;

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('texture_size,2d_texture,compressed_format')
  .desc(`Test texture size requirement for 2D texture with compressed format.`)
  .cases(poptions('dimension', [undefined, '2d']))
  .subcases(() =>
    params()
      .combine(poptions('format', kCompressedTextureFormats))
      .expand(p => {
        const { blockWidth, blockHeight } = kAllTextureFormatInfo[p.format];
        return poptions('size', [
          // Test the bound of width
          [DefaultLimits.maxTextureDimension2D - 1, 1, 1],
          [DefaultLimits.maxTextureDimension2D - blockWidth, 1, 1],
          [DefaultLimits.maxTextureDimension2D - blockWidth, blockHeight, 1],
          [DefaultLimits.maxTextureDimension2D, 1, 1],
          [DefaultLimits.maxTextureDimension2D, blockHeight, 1],
          [DefaultLimits.maxTextureDimension2D + 1, 1, 1],
          [DefaultLimits.maxTextureDimension2D + blockWidth, 1, 1],
          [DefaultLimits.maxTextureDimension2D + blockWidth, blockHeight, 1],
          // Test the bound of height
          [1, DefaultLimits.maxTextureDimension2D - 1, 1],
          [1, DefaultLimits.maxTextureDimension2D - blockHeight, 1],
          [blockWidth, DefaultLimits.maxTextureDimension2D - blockHeight, 1],
          [1, DefaultLimits.maxTextureDimension2D, 1],
          [blockWidth, DefaultLimits.maxTextureDimension2D, 1],
          [1, DefaultLimits.maxTextureDimension2D + 1, 1],
          [1, DefaultLimits.maxTextureDimension2D + blockWidth, 1],
          [blockWidth, DefaultLimits.maxTextureDimension2D + blockHeight, 1],
          // Test the bound of array layers
          [1, 1, DefaultLimits.maxTextureArrayLayers - 1],
          [blockWidth, 1, DefaultLimits.maxTextureArrayLayers - 1],
          [1, blockHeight, DefaultLimits.maxTextureArrayLayers - 1],
          [blockWidth, blockHeight, DefaultLimits.maxTextureArrayLayers - 1],
          [1, 1, DefaultLimits.maxTextureArrayLayers],
          [blockWidth, 1, DefaultLimits.maxTextureArrayLayers],
          [1, blockHeight, DefaultLimits.maxTextureArrayLayers],
          [blockWidth, blockHeight, DefaultLimits.maxTextureArrayLayers],
          [1, 1, DefaultLimits.maxTextureArrayLayers + 1],
          [blockWidth, 1, DefaultLimits.maxTextureArrayLayers + 1],
          [1, blockHeight, DefaultLimits.maxTextureArrayLayers + 1],
          [blockWidth, blockHeight, DefaultLimits.maxTextureArrayLayers + 1],
        ]);
      })
  )
  .fn(async t => {
    const { dimension, format, size } = t.params;

    const info = kCompressedTextureFormatInfo[format];
    assert(
      DefaultLimits.maxTextureDimension2D % info.blockWidth === 0 &&
        DefaultLimits.maxTextureDimension2D % info.blockHeight === 0
    );

    await t.selectDeviceOrSkipTestCase(info.extension);

    const descriptor = {
      size,
      dimension,
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success =
      size[0] % info.blockWidth === 0 &&
      size[1] % info.blockHeight === 0 &&
      size[0] <= DefaultLimits.maxTextureDimension2D &&
      size[1] <= DefaultLimits.maxTextureDimension2D &&
      size[2] <= DefaultLimits.maxTextureArrayLayers;

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('texture_size,3d_texture,uncompressed_format')
  .desc(`Test texture size requirement for 3D texture with uncompressed format.`)
  .subcases(() =>
    params()
      .combine(poptions('format', kUncompressedTextureFormats))
      .combine(
        poptions('size', [
          // Test the bound of width
          [DefaultLimits.maxTextureDimension3D - 1, 1, 1],
          [DefaultLimits.maxTextureDimension3D, 1, 1],
          [DefaultLimits.maxTextureDimension3D + 1, 1, 1],
          // Test the bound of height
          [1, DefaultLimits.maxTextureDimension3D - 1, 1],
          [1, DefaultLimits.maxTextureDimension3D, 1],
          [1, DefaultLimits.maxTextureDimension3D + 1, 1],
          // Test the bound of depth
          [1, 1, DefaultLimits.maxTextureDimension3D - 1],
          [1, 1, DefaultLimits.maxTextureDimension3D],
          [1, 1, DefaultLimits.maxTextureDimension3D + 1],
        ])
      )
  )
  .fn(async t => {
    const { format, size } = t.params;

    await t.selectDeviceOrSkipTestCase(kUncompressedTextureFormatInfo[format].extension);

    const descriptor = {
      size,
      dimension: '3d',
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success =
      size[0] <= DefaultLimits.maxTextureDimension3D &&
      size[1] <= DefaultLimits.maxTextureDimension3D &&
      size[2] <= DefaultLimits.maxTextureDimension3D;

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('texture_size,3d_texture,compressed_format')
  .desc(`Test texture size requirement for 3D texture with compressed format.`)
  .subcases(() =>
    params()
      .combine(poptions('format', kCompressedTextureFormats))
      .expand(p => {
        const { blockWidth, blockHeight } = kAllTextureFormatInfo[p.format];
        return poptions('size', [
          // Test the bound of width
          [DefaultLimits.maxTextureDimension3D - 1, 1, 1],
          [DefaultLimits.maxTextureDimension3D - blockWidth, 1, 1],
          [DefaultLimits.maxTextureDimension3D - blockWidth, blockHeight, 1],
          [DefaultLimits.maxTextureDimension3D, 1, 1],
          [DefaultLimits.maxTextureDimension3D, blockHeight, 1],
          [DefaultLimits.maxTextureDimension3D + 1, 1, 1],
          [DefaultLimits.maxTextureDimension3D + blockWidth, 1, 1],
          [DefaultLimits.maxTextureDimension3D + blockWidth, blockHeight, 1],
          // Test the bound of height
          [1, DefaultLimits.maxTextureDimension3D - 1, 1],
          [1, DefaultLimits.maxTextureDimension3D - blockHeight, 1],
          [blockWidth, DefaultLimits.maxTextureDimension3D - blockHeight, 1],
          [1, DefaultLimits.maxTextureDimension3D, 1],
          [blockWidth, DefaultLimits.maxTextureDimension3D, 1],
          [1, DefaultLimits.maxTextureDimension3D + 1, 1],
          [1, DefaultLimits.maxTextureDimension3D + blockWidth, 1],
          [blockWidth, DefaultLimits.maxTextureDimension3D + blockHeight, 1],
          // Test the bound of depth
          [1, 1, DefaultLimits.maxTextureDimension3D - 1],
          [blockWidth, 1, DefaultLimits.maxTextureDimension3D - 1],
          [1, blockHeight, DefaultLimits.maxTextureDimension3D - 1],
          [blockWidth, blockHeight, DefaultLimits.maxTextureDimension3D - 1],
          [1, 1, DefaultLimits.maxTextureDimension3D],
          [blockWidth, 1, DefaultLimits.maxTextureDimension3D],
          [1, blockHeight, DefaultLimits.maxTextureDimension3D],
          [blockWidth, blockHeight, DefaultLimits.maxTextureDimension3D],
          [1, 1, DefaultLimits.maxTextureDimension3D + 1],
          [blockWidth, 1, DefaultLimits.maxTextureDimension3D + 1],
          [1, blockHeight, DefaultLimits.maxTextureDimension3D + 1],
          [blockWidth, blockHeight, DefaultLimits.maxTextureDimension3D + 1],
        ]);
      })
  )
  .fn(async t => {
    const { format, size } = t.params;

    t.skip('Compressed 3D texture is not supported');

    const info = kCompressedTextureFormatInfo[format];
    assert(
      DefaultLimits.maxTextureDimension3D % info.blockWidth === 0 &&
        DefaultLimits.maxTextureDimension3D % info.blockHeight === 0
    );

    await t.selectDeviceOrSkipTestCase(info.extension);

    const descriptor = {
      size,
      dimension: '3d',
      format,
      usage: GPUTextureUsage.SAMPLED,
    };

    const success =
      size[0] % info.blockWidth === 0 &&
      size[1] % info.blockHeight === 0 &&
      size[0] <= DefaultLimits.maxTextureDimension3D &&
      size[1] <= DefaultLimits.maxTextureDimension3D &&
      size[2] <= DefaultLimits.maxTextureDimension3D;

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });

g.test('texture_usage')
  .desc(
    `Test texture usage (single usage or combined usages) for every texture format and every dimension type`
  )
  .cases(poptions('dimension', [undefined, ...kTextureDimensions]))
  .subcases(() =>
    params()
      .combine(poptions('format', kAllTextureFormats))
      // If usage0 and usage1 are the same, then the usage being test is a single usage. Otherwise, it is a combined usage.
      .combine(poptions('usage0', kTextureUsages))
      .combine(poptions('usage1', kTextureUsages))
  )
  .fn(async t => {
    const { dimension, format, usage0, usage1 } = t.params;
    const info = kAllTextureFormatInfo[format];

    await t.selectDeviceOrSkipTestCase(info.extension);

    const size = [info.blockWidth, info.blockHeight, 1];
    const usage = usage0 | usage1;
    const descriptor = {
      size,
      dimension,
      format,
      usage,
    };

    let success = true;
    // Note that we unconditionally test copy usages for all formats. We don't check copySrc/copyDst in kAllTextureFormatInfo in capability_info.js
    // if (!info.copySrc && (usage & GPUTextureUsage.COPY_SRC) !== 0) success = false;
    // if (!info.copyDst && (usage & GPUTextureUsage.COPY_DST) !== 0) success = false;
    if (!info.storage && (usage & GPUTextureUsage.STORAGE) !== 0) success = false;
    if (!info.renderable && (usage & GPUTextureUsage.RENDER_ATTACHMENT) !== 0) success = false;

    t.expectValidationError(() => {
      t.device.createTexture(descriptor);
    }, !success);
  });
