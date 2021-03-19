/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = '';
import { params, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/framework/util/util.js';
import { kSizedTextureFormats, kSizedTextureFormatInfo } from '../../../capability_info.js';
import { GPUConst } from '../../../constants.js';
import { align } from '../../../util/math.js';
import { kImageCopyTypes } from '../../../util/texture/image_copy.js';

import {
  ImageCopyTest,
  texelBlockAlignmentTestExpanderForValueToCoordinate,
  formatCopyableWithMethod,
} from './image_copy.js';

export const g = makeTestGroup(ImageCopyTest);

g.test('valid')
  .desc(`The texture must be valid and not destroyed.`)
  .cases(
    params()
      .combine(poptions('method', kImageCopyTypes))
      .combine(poptions('textureState', ['valid', 'destroyed', 'error']))
  )
  .fn(async t => {
    const { method, textureState } = t.params;

    // A valid texture.
    let texture = t.device.createTexture({
      size: { width: 4, height: 4, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    switch (textureState) {
      case 'destroyed': {
        texture.destroy();
        break;
      }
      case 'error': {
        texture = t.getErrorTexture();
        break;
      }
    }

    const success = textureState === 'valid';
    const submit = textureState === 'destroyed';

    t.testRun(
      { texture },
      { bytesPerRow: 0 },
      { width: 0, height: 0, depthOrArrayLayers: 0 },
      { dataSize: 1, method, success, submit }
    );
  });

g.test('usage')
  .desc(`The texture must have the appropriate COPY_SRC/COPY_DST usage.`)
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() =>
    poptions('usage', [
      GPUConst.TextureUsage.COPY_SRC | GPUConst.TextureUsage.SAMPLED,
      GPUConst.TextureUsage.COPY_DST | GPUConst.TextureUsage.SAMPLED,
      GPUConst.TextureUsage.COPY_SRC | GPUConst.TextureUsage.COPY_DST,
    ])
  )
  .fn(async t => {
    const { usage, method } = t.params;

    const texture = t.device.createTexture({
      size: { width: 4, height: 4, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage,
    });

    const success =
      method === 'CopyT2B'
        ? (usage & GPUTextureUsage.COPY_SRC) !== 0
        : (usage & GPUTextureUsage.COPY_DST) !== 0;

    t.testRun(
      { texture },
      { bytesPerRow: 0 },
      { width: 0, height: 0, depthOrArrayLayers: 0 },
      { dataSize: 1, method, success }
    );
  });

g.test('sample_count')
  .desc(`Multisampled textures cannot be copied.`)
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() => poptions('sampleCount', [1, 4]))
  .fn(async t => {
    const { sampleCount, method } = t.params;

    const texture = t.device.createTexture({
      size: { width: 4, height: 4, depthOrArrayLayers: 1 },
      sampleCount,
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST | GPUTextureUsage.SAMPLED,
    });

    const success = sampleCount === 1;

    t.testRun(
      { texture },
      { bytesPerRow: 0 },
      { width: 0, height: 0, depthOrArrayLayers: 0 },
      { dataSize: 1, method, success }
    );
  });

g.test('mip_level')
  .desc(`The mipLevel of the copy must be in range of the texture.`)
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() =>
    params()
      .combine(poptions('mipLevelCount', [3, 5]))
      .combine(poptions('mipLevel', [3, 4]))
  )
  .fn(async t => {
    const { mipLevelCount, mipLevel, method } = t.params;

    const texture = t.device.createTexture({
      size: { width: 32, height: 32, depthOrArrayLayers: 1 },
      mipLevelCount,
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const success = mipLevel < mipLevelCount;

    t.testRun(
      { texture, mipLevel },
      { bytesPerRow: 0 },
      { width: 0, height: 0, depthOrArrayLayers: 0 },
      { dataSize: 1, method, success }
    );
  });

g.test('origin_alignment')
  .desc(`Copy origin must be aligned to block size.`)
  .cases(
    params()
      .combine(poptions('method', kImageCopyTypes))
      .combine(poptions('format', kSizedTextureFormats))
      .filter(formatCopyableWithMethod)
  )
  .subcases(p =>
    params()
      .combine(poptions('coordinateToTest', ['x', 'y', 'z']))
      .expand(({ coordinateToTest }) =>
        texelBlockAlignmentTestExpanderForValueToCoordinate({ format: p.format, coordinateToTest })
      )
  )
  .fn(async t => {
    const { valueToCoordinate, coordinateToTest, format, method } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    const origin = { x: 0, y: 0, z: 0 };
    const size = { width: 0, height: 0, depthOrArrayLayers: 0 };
    let success = true;

    origin[coordinateToTest] = valueToCoordinate;
    switch (coordinateToTest) {
      case 'x': {
        success = origin.x % info.blockWidth === 0;
        break;
      }
      case 'y': {
        success = origin.y % info.blockHeight === 0;
        break;
      }
    }

    const texture = t.createAlignedTexture(format, size, origin);

    t.testRun({ texture, origin }, { bytesPerRow: 0 }, size, {
      dataSize: 1,
      method,
      success,
    });
  });

g.test('1d')
  .desc(`1d texture copies must have height=depth=1.`)
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() =>
    params()
      .combine(poptions('width', [0, 1]))
      .combine([
        { height: 1, depthOrArrayLayers: 1 },
        { height: 1, depthOrArrayLayers: 0 },
        { height: 1, depthOrArrayLayers: 2 },
        { height: 0, depthOrArrayLayers: 1 },
        { height: 2, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { method, width, height, depthOrArrayLayers } = t.params;
    const size = { width, height, depthOrArrayLayers };

    const texture = t.device.createTexture({
      size: { width: 2, height: 1, depthOrArrayLayers: 1 },
      dimension: '1d',
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    // For 1d textures we require copyHeight and copyDepth to be 1,
    // copyHeight or copyDepth being 0 should cause a validation error.
    const success = size.height === 1 && size.depthOrArrayLayers === 1;

    t.testRun({ texture }, { bytesPerRow: 256, rowsPerImage: 4 }, size, {
      dataSize: 16,
      method,
      success,
    });
  });

g.test('size_alignment')
  .desc(`Copy size must be aligned to block size.`)
  .cases(
    params()
      .combine(poptions('method', kImageCopyTypes))
      .combine(poptions('format', kSizedTextureFormats))
      .filter(formatCopyableWithMethod)
  )
  .subcases(p =>
    params()
      .combine(poptions('coordinateToTest', ['width', 'height', 'depthOrArrayLayers']))
      .expand(({ coordinateToTest }) =>
        texelBlockAlignmentTestExpanderForValueToCoordinate({ format: p.format, coordinateToTest })
      )
  )
  .fn(async t => {
    const { valueToCoordinate, coordinateToTest, format, method } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    const origin = { x: 0, y: 0, z: 0 };
    const size = { width: 0, height: 0, depthOrArrayLayers: 0 };
    let success = true;

    size[coordinateToTest] = valueToCoordinate;
    switch (coordinateToTest) {
      case 'width': {
        success = size.width % info.blockWidth === 0;
        break;
      }
      case 'height': {
        success = size.height % info.blockHeight === 0;
        break;
      }
    }

    const texture = t.createAlignedTexture(format, size, origin);

    assert(size.width % info.blockWidth === 0);
    const bytesPerRow = align(size.width / info.blockWidth, 256);
    assert(size.height % info.blockHeight === 0);
    const rowsPerImage = size.height / info.blockHeight;
    t.testRun({ texture, origin }, { bytesPerRow, rowsPerImage }, size, {
      dataSize: 1,
      method,
      success,
    });
  });

g.test('copy_rectangle')
  .desc(`The max corner of the copy rectangle (origin+copySize) must be inside the texture.`)
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() =>
    params()
      .combine(poptions('originValue', [7, 8]))
      .combine(poptions('copySizeValue', [7, 8]))
      .combine(poptions('textureSizeValue', [14, 15]))
      .combine(poptions('mipLevel', [0, 2]))
      .combine(poptions('coordinateToTest', [0, 1, 2]))
  )
  .fn(async t => {
    const {
      originValue,
      copySizeValue,
      textureSizeValue,
      mipLevel,
      coordinateToTest,
      method,
    } = t.params;
    const format = 'rgba8unorm';
    const info = kSizedTextureFormatInfo[format];

    const origin = [0, 0, 0];
    const copySize = [0, 0, 0];
    const textureSize = { width: 16 << mipLevel, height: 16 << mipLevel, depthOrArrayLayers: 16 };
    const success = originValue + copySizeValue <= textureSizeValue;

    origin[coordinateToTest] = originValue;
    copySize[coordinateToTest] = copySizeValue;
    switch (coordinateToTest) {
      case 0: {
        textureSize.width = textureSizeValue << mipLevel;
        break;
      }
      case 1: {
        textureSize.height = textureSizeValue << mipLevel;
        break;
      }
      case 2: {
        textureSize.depthOrArrayLayers = textureSizeValue;
        break;
      }
    }

    const texture = t.device.createTexture({
      size: textureSize,
      mipLevelCount: 3,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    assert(copySize[0] % info.blockWidth === 0);
    const bytesPerRow = align(copySize[0] / info.blockWidth, 256);
    assert(copySize[1] % info.blockHeight === 0);
    const rowsPerImage = copySize[1] / info.blockHeight;
    t.testRun({ texture, origin, mipLevel }, { bytesPerRow, rowsPerImage }, copySize, {
      dataSize: 1,
      method,
      success,
    });
  });
