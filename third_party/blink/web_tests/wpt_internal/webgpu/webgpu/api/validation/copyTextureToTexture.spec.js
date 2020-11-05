/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
copyTextureToTexture tests.

Test Plan: (TODO(jiawei.shao@intel.com): add tests on aspects and 1D/3D textures)
* the source and destination texture
  - the {source, destination} texture is {invalid, valid}.
  - mipLevel {>, =, <} the mipmap level count of the {source, destination} texture.
  - the source texture is created {with, without} GPUTextureUsage::CopySrc.
  - the destination texture is created {with, without} GPUTextureUsage::CopyDst.
* sample count
  - the sample count of the source texture {is, isn't} equal to the one of the destination texture
  - when the sample count is greater than 1:
    - it {is, isn't} a copy of the whole subresource of the source texture.
    - it {is, isn't} a copy of the whole subresource of the destination texture.
* texture format
  - the format of the source texture {is, isn't} equal to the one of the destination texture.
    - including: depth24plus-stencil8 to/from {depth24plus, stencil8}.
  - for each depth and/or stencil format: a copy between two textures with same format:
    - it {is, isn't} a copy of the whole subresource of the {source, destination} texture.
* copy ranges
  - if the texture dimension is 2D:
    - (srcOrigin.x + copyExtent.width) {>, =, <} the width of the subresource size of source
      textureCopyView.
    - (srcOrigin.y + copyExtent.height) {>, =, <} the height of the subresource size of source
      textureCopyView.
    - (srcOrigin.z + copyExtent.depth) {>, =, <} the depth of the subresource size of source
      textureCopyView.
    - (dstOrigin.x + copyExtent.width) {>, =, <} the width of the subresource size of destination
      textureCopyView.
    - (dstOrigin.y + copyExtent.height) {>, =, <} the height of the subresource size of destination
      textureCopyView.
    - (dstOrigin.z + copyExtent.depth) {>, =, <} the depth of the subresource size of destination
      textureCopyView.
* when the source and destination texture are the same one:
  - the set of source texture subresources {has, doesn't have} overlaps with the one of destination
    texture subresources.
`;
import { poptions, params } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert } from '../../../common/framework/util/util.js';
import {
  kAllTextureFormatInfo,
  kAllTextureFormats,
  kCompressedTextureFormats,
  kDepthStencilFormats,
  kTextureUsages,
} from '../../capability_info.js';
import { align } from '../../util/math.js';

import { ValidationTest } from './validation_test.js';

class F extends ValidationTest {
  TestCopyTextureToTexture(source, destination, copySize, isSuccess) {
    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.copyTextureToTexture(source, destination, copySize);

    this.expectValidationError(() => {
      commandEncoder.finish();
    }, !isSuccess);
  }

  GetPhysicalSubresourceSize(textureSize, format, mipLevel) {
    const virtualWidthAtLevel = Math.max(textureSize.width >> mipLevel, 1);
    const virtualHeightAtLevel = Math.max(textureSize.height >> mipLevel, 1);
    const physicalWidthAtLevel = align(
      virtualWidthAtLevel,
      kAllTextureFormatInfo[format].blockWidth
    );

    const physicalHeightAtLevel = align(
      virtualHeightAtLevel,
      kAllTextureFormatInfo[format].blockHeight
    );

    return { width: physicalWidthAtLevel, height: physicalHeightAtLevel, depth: textureSize.depth };
  }
}

export const g = makeTestGroup(F);

g.test('copy_with_invalid_texture').fn(async t => {
  const validTexture = t.device.createTexture({
    size: { width: 4, height: 4, depth: 1 },
    format: 'rgba8unorm',
    usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
  });

  const errorTexture = t.getErrorTexture();

  t.TestCopyTextureToTexture(
    { texture: errorTexture },
    { texture: validTexture },
    { width: 1, height: 1, depth: 1 },
    false
  );

  t.TestCopyTextureToTexture(
    { texture: validTexture },
    { texture: errorTexture },
    { width: 1, height: 1, depth: 1 },
    false
  );
});

g.test('mipmap_level')
  .params([
    { srcLevelCount: 1, dstLevelCount: 1, srcCopyLevel: 0, dstCopyLevel: 0 },
    { srcLevelCount: 1, dstLevelCount: 1, srcCopyLevel: 1, dstCopyLevel: 0 },
    { srcLevelCount: 1, dstLevelCount: 1, srcCopyLevel: 0, dstCopyLevel: 1 },
    { srcLevelCount: 3, dstLevelCount: 3, srcCopyLevel: 0, dstCopyLevel: 0 },
    { srcLevelCount: 3, dstLevelCount: 3, srcCopyLevel: 2, dstCopyLevel: 0 },
    { srcLevelCount: 3, dstLevelCount: 3, srcCopyLevel: 3, dstCopyLevel: 0 },
    { srcLevelCount: 3, dstLevelCount: 3, srcCopyLevel: 0, dstCopyLevel: 2 },
    { srcLevelCount: 3, dstLevelCount: 3, srcCopyLevel: 0, dstCopyLevel: 3 },
  ])
  .fn(async t => {
    const { srcLevelCount, dstLevelCount, srcCopyLevel, dstCopyLevel } = t.params;

    const srcTexture = t.device.createTexture({
      size: { width: 32, height: 32, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC,
      mipLevelCount: srcLevelCount,
    });

    const dstTexture = t.device.createTexture({
      size: { width: 32, height: 32, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_DST,
      mipLevelCount: dstLevelCount,
    });

    const isSuccess = srcCopyLevel < srcLevelCount && dstCopyLevel < dstLevelCount;
    t.TestCopyTextureToTexture(
      { texture: srcTexture, mipLevel: srcCopyLevel },
      { texture: dstTexture, mipLevel: dstCopyLevel },
      { width: 1, height: 1, depth: 1 },
      isSuccess
    );
  });

g.test('texture_usage')
  .params(
    params()
      .combine(poptions('srcUsage', kTextureUsages))
      .combine(poptions('dstUsage', kTextureUsages))
  )
  .fn(async t => {
    const { srcUsage, dstUsage } = t.params;

    const srcTexture = t.device.createTexture({
      size: { width: 4, height: 4, depth: 1 },
      format: 'rgba8unorm',
      usage: srcUsage,
    });

    const dstTexture = t.device.createTexture({
      size: { width: 4, height: 4, depth: 1 },
      format: 'rgba8unorm',
      usage: dstUsage,
    });

    const isSuccess =
      srcUsage === GPUTextureUsage.COPY_SRC && dstUsage === GPUTextureUsage.COPY_DST;

    t.TestCopyTextureToTexture(
      { texture: srcTexture },
      { texture: dstTexture },
      { width: 1, height: 1, depth: 1 },
      isSuccess
    );
  });

g.test('sample_count')
  .params(
    params()
      .combine(poptions('srcSampleCount', [1, 4]))
      .combine(poptions('dstSampleCount', [1, 4]))
  )
  .fn(async t => {
    const { srcSampleCount, dstSampleCount } = t.params;

    const srcTexture = t.device.createTexture({
      size: { width: 4, height: 4, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC,
      sampleCount: srcSampleCount,
    });

    const dstTexture = t.device.createTexture({
      size: { width: 4, height: 4, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_DST,
      sampleCount: dstSampleCount,
    });

    const isSuccess = srcSampleCount === dstSampleCount;
    t.TestCopyTextureToTexture(
      { texture: srcTexture },
      { texture: dstTexture },
      { width: 4, height: 4, depth: 1 },
      isSuccess
    );
  });

g.test('multisampled_copy_restrictions')
  .params(
    params()
      .combine(
        poptions('srcCopyOrigin', [
          { x: 0, y: 0, z: 0 },
          { x: 1, y: 0, z: 0 },
          { x: 0, y: 1, z: 0 },
          { x: 1, y: 1, z: 0 },
        ])
      )
      .combine(
        poptions('dstCopyOrigin', [
          { x: 0, y: 0, z: 0 },
          { x: 1, y: 0, z: 0 },
          { x: 0, y: 1, z: 0 },
          { x: 1, y: 1, z: 0 },
        ])
      )
      .expand(p => poptions('copyWidth', [32 - Math.max(p.srcCopyOrigin.x, p.dstCopyOrigin.x), 16]))
      .expand(p => poptions('copyHeight', [16 - Math.max(p.srcCopyOrigin.y, p.dstCopyOrigin.y), 8]))
  )
  .fn(async t => {
    const { srcCopyOrigin, dstCopyOrigin, copyWidth, copyHeight } = t.params;

    const kWidth = 32;
    const kHeight = 16;

    // Currently we don't support multisampled 2D array textures and the mipmap level count of the
    // multisampled textures must be 1.
    const srcTexture = t.device.createTexture({
      size: { width: kWidth, height: kHeight, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC,
      sampleCount: 4,
    });

    const dstTexture = t.device.createTexture({
      size: { width: kWidth, height: kHeight, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_DST,
      sampleCount: 4,
    });

    const isSuccess = copyWidth === kWidth && copyHeight === kHeight;
    t.TestCopyTextureToTexture(
      { texture: srcTexture, origin: srcCopyOrigin },
      { texture: dstTexture, origin: dstCopyOrigin },
      { width: copyWidth, height: copyHeight, depth: 1 },
      isSuccess
    );
  });

g.test('texture_format_equality')
  .params(
    params()
      .combine(poptions('srcFormat', kAllTextureFormats))
      .combine(poptions('dstFormat', kAllTextureFormats))
  )
  .fn(async t => {
    const { srcFormat, dstFormat } = t.params;
    const extensions = [];

    const srcFormatExtension = kAllTextureFormatInfo[srcFormat].extension;
    if (srcFormatExtension !== undefined) {
      extensions.push(srcFormatExtension);
    }
    const dstFormatExtension = kAllTextureFormatInfo[dstFormat].extension;
    if (dstFormatExtension !== undefined) {
      extensions.push(dstFormatExtension);
    }

    if (extensions.length) {
      await t.selectDeviceOrSkipTestCase({ extensions });
    }

    const kTextureSize = { width: 16, height: 16, depth: 1 };

    const srcTexture = t.device.createTexture({
      size: kTextureSize,
      format: srcFormat,
      usage: GPUTextureUsage.COPY_SRC,
    });

    const dstTexture = t.device.createTexture({
      size: kTextureSize,
      format: dstFormat,
      usage: GPUTextureUsage.COPY_DST,
    });

    const isSuccess = srcFormat === dstFormat;
    t.TestCopyTextureToTexture(
      { texture: srcTexture },
      { texture: dstTexture },
      kTextureSize,
      isSuccess
    );
  });

g.test('depth_stencil_copy_restrictions')
  .params(
    params()
      .combine(poptions('format', kDepthStencilFormats))
      .combine(
        poptions('copyBoxOffsets', [
          { x: 0, y: 0, width: 0, height: 0 },
          { x: 1, y: 0, width: 0, height: 0 },
          { x: 0, y: 1, width: 0, height: 0 },
          { x: 0, y: 0, width: -1, height: 0 },
          { x: 0, y: 0, width: 0, height: -1 },
        ])
      )
      .combine(
        poptions('srcTextureSize', [
          { width: 64, height: 64, depth: 1 },
          { width: 64, height: 32, depth: 1 },
          { width: 32, height: 32, depth: 1 },
        ])
      )
      .combine(
        poptions('dstTextureSize', [
          { width: 64, height: 64, depth: 1 },
          { width: 64, height: 32, depth: 1 },
          { width: 32, height: 32, depth: 1 },
        ])
      )
      .combine(poptions('srcCopyLevel', [1, 2]))
      .combine(poptions('dstCopyLevel', [0, 1]))
  )
  .fn(async t => {
    const {
      format,
      copyBoxOffsets,
      srcTextureSize,
      dstTextureSize,
      srcCopyLevel,
      dstCopyLevel,
    } = t.params;

    const kMipLevelCount = 3;
    const srcTexture = t.device.createTexture({
      size: { width: srcTextureSize.width, height: srcTextureSize.height, depth: 1 },
      format,
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_SRC,
    });

    const dstTexture = t.device.createTexture({
      size: { width: dstTextureSize.width, height: dstTextureSize.height, depth: 1 },
      format,
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_DST,
    });

    const srcSizeAtLevel = t.GetPhysicalSubresourceSize(srcTextureSize, format, srcCopyLevel);
    const dstSizeAtLevel = t.GetPhysicalSubresourceSize(dstTextureSize, format, dstCopyLevel);

    const copyOrigin = { x: copyBoxOffsets.x, y: copyBoxOffsets.y, z: 0 };

    const copyWidth =
      Math.min(srcSizeAtLevel.width, dstSizeAtLevel.width) + copyBoxOffsets.width - copyOrigin.x;
    const copyHeight =
      Math.min(srcSizeAtLevel.height, dstSizeAtLevel.height) + copyBoxOffsets.height - copyOrigin.y;

    // Depth/stencil copies must copy whole subresources.
    const isSuccess =
      copyOrigin.x === 0 &&
      copyOrigin.y === 0 &&
      copyWidth === srcSizeAtLevel.width &&
      copyHeight === srcSizeAtLevel.height &&
      copyWidth === dstSizeAtLevel.width &&
      copyHeight === dstSizeAtLevel.height;
    t.TestCopyTextureToTexture(
      { texture: srcTexture, origin: { x: 0, y: 0, z: 0 }, mipLevel: srcCopyLevel },
      { texture: dstTexture, origin: copyOrigin, mipLevel: dstCopyLevel },
      { width: copyWidth, height: copyHeight, depth: 1 },
      isSuccess
    );

    t.TestCopyTextureToTexture(
      { texture: srcTexture, origin: copyOrigin, mipLevel: srcCopyLevel },
      { texture: dstTexture, origin: { x: 0, y: 0, z: 0 }, mipLevel: dstCopyLevel },
      { width: copyWidth, height: copyHeight, depth: 1 },
      isSuccess
    );
  });

g.test('copy_ranges')
  .params(
    params()
      .combine(
        poptions('copyBoxOffsets', [
          { x: 0, y: 0, z: 0, width: 0, height: 0, depth: -2 },
          { x: 1, y: 0, z: 0, width: 0, height: 0, depth: -2 },
          { x: 1, y: 0, z: 0, width: -1, height: 0, depth: -2 },
          { x: 0, y: 1, z: 0, width: 0, height: 0, depth: -2 },
          { x: 0, y: 1, z: 0, width: 0, height: -1, depth: -2 },
          { x: 0, y: 0, z: 1, width: 0, height: 1, depth: -2 },
          { x: 0, y: 0, z: 2, width: 0, height: 1, depth: 0 },
          { x: 0, y: 0, z: 0, width: 1, height: 0, depth: -2 },
          { x: 0, y: 0, z: 0, width: 0, height: 1, depth: -2 },
          { x: 0, y: 0, z: 0, width: 0, height: 0, depth: 1 },
          { x: 0, y: 0, z: 0, width: 0, height: 0, depth: 0 },
          { x: 0, y: 0, z: 1, width: 0, height: 0, depth: -1 },
          { x: 0, y: 0, z: 2, width: 0, height: 0, depth: -1 },
        ])
      )
      .combine(poptions('srcCopyLevel', [0, 1, 3]))
      .combine(poptions('dstCopyLevel', [0, 1, 3]))
  )
  .fn(async t => {
    const { copyBoxOffsets, srcCopyLevel, dstCopyLevel } = t.params;

    const kTextureSize = { width: 16, height: 8, depth: 3 };
    const kMipLevelCount = 4;
    const kFormat = 'rgba8unorm';

    const srcTexture = t.device.createTexture({
      size: kTextureSize,
      format: kFormat,
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_SRC,
    });

    const dstTexture = t.device.createTexture({
      size: kTextureSize,
      format: kFormat,
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_DST,
    });

    const srcSizeAtLevel = t.GetPhysicalSubresourceSize(kTextureSize, kFormat, srcCopyLevel);
    const dstSizeAtLevel = t.GetPhysicalSubresourceSize(kTextureSize, kFormat, dstCopyLevel);

    const copyOrigin = { x: copyBoxOffsets.x, y: copyBoxOffsets.y, z: copyBoxOffsets.z };

    const copyWidth = Math.max(
      Math.min(srcSizeAtLevel.width, dstSizeAtLevel.width) + copyBoxOffsets.width - copyOrigin.x,
      0
    );

    const copyHeight = Math.max(
      Math.min(srcSizeAtLevel.height, dstSizeAtLevel.height) + copyBoxOffsets.height - copyOrigin.y,
      0
    );

    const copyDepth = kTextureSize.depth + copyBoxOffsets.depth - copyOrigin.z;

    {
      const isSuccess =
        copyWidth <= srcSizeAtLevel.width &&
        copyHeight <= srcSizeAtLevel.height &&
        copyOrigin.x + copyWidth <= dstSizeAtLevel.width &&
        copyOrigin.y + copyHeight <= dstSizeAtLevel.height &&
        copyOrigin.z + copyDepth <= kTextureSize.depth;

      t.TestCopyTextureToTexture(
        { texture: srcTexture, origin: { x: 0, y: 0, z: 0 }, mipLevel: srcCopyLevel },
        { texture: dstTexture, origin: copyOrigin, mipLevel: dstCopyLevel },
        { width: copyWidth, height: copyHeight, depth: copyDepth },
        isSuccess
      );
    }

    {
      const isSuccess =
        copyOrigin.x + copyWidth <= srcSizeAtLevel.width &&
        copyOrigin.y + copyHeight <= srcSizeAtLevel.height &&
        copyWidth <= dstSizeAtLevel.width &&
        copyHeight <= dstSizeAtLevel.height &&
        copyOrigin.z + copyDepth <= kTextureSize.depth;

      t.TestCopyTextureToTexture(
        { texture: srcTexture, origin: copyOrigin, mipLevel: srcCopyLevel },
        { texture: dstTexture, origin: { x: 0, y: 0, z: 0 }, mipLevel: dstCopyLevel },
        { width: copyWidth, height: copyHeight, depth: copyDepth },
        isSuccess
      );
    }
  });

g.test('copy_within_same_texture')
  .params(
    params()
      .combine(poptions('srcCopyOriginZ', [0, 2, 4]))
      .combine(poptions('dstCopyOriginZ', [0, 2, 4]))
      .combine(poptions('copyExtentDepth', [1, 2, 3]))
  )
  .fn(async t => {
    const { srcCopyOriginZ, dstCopyOriginZ, copyExtentDepth } = t.params;

    const kArrayLayerCount = 7;

    const testTexture = t.device.createTexture({
      size: { width: 16, height: 16, depth: kArrayLayerCount },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const isSuccess =
      Math.min(srcCopyOriginZ, dstCopyOriginZ) + copyExtentDepth <=
      Math.max(srcCopyOriginZ, dstCopyOriginZ);
    t.TestCopyTextureToTexture(
      { texture: testTexture, origin: { x: 0, y: 0, z: srcCopyOriginZ } },
      { texture: testTexture, origin: { x: 0, y: 0, z: dstCopyOriginZ } },
      { width: 16, height: 16, depth: copyExtentDepth },
      isSuccess
    );
  });

g.test('copy_ranges_with_compressed_texture_formats')
  .params(
    params()
      .combine(poptions('format', kCompressedTextureFormats))
      .combine(
        poptions('copyBoxOffsets', [
          { x: 0, y: 0, z: 0, width: 0, height: 0, depth: -2 },
          { x: 1, y: 0, z: 0, width: 0, height: 0, depth: -2 },
          { x: 4, y: 0, z: 0, width: 0, height: 0, depth: -2 },
          { x: 0, y: 0, z: 0, width: -1, height: 0, depth: -2 },
          { x: 0, y: 0, z: 0, width: -4, height: 0, depth: -2 },
          { x: 0, y: 1, z: 0, width: 0, height: 0, depth: -2 },
          { x: 0, y: 4, z: 0, width: 0, height: 0, depth: -2 },
          { x: 0, y: 0, z: 0, width: 0, height: -1, depth: -2 },
          { x: 0, y: 0, z: 0, width: 0, height: -4, depth: -2 },
          { x: 0, y: 0, z: 0, width: 0, height: 0, depth: 0 },
          { x: 0, y: 0, z: 1, width: 0, height: 0, depth: -1 },
        ])
      )
      .combine(poptions('srcCopyLevel', [0, 1, 2]))
      .combine(poptions('dstCopyLevel', [0, 1, 2]))
  )
  .fn(async t => {
    const { format, copyBoxOffsets, srcCopyLevel, dstCopyLevel } = t.params;

    const extension = kAllTextureFormatInfo[format].extension;
    assert(extension !== undefined);
    await t.selectDeviceOrSkipTestCase({ extensions: [extension] });

    const kTextureSize = { width: 60, height: 48, depth: 3 };
    const kMipLevelCount = 4;

    const srcTexture = t.device.createTexture({
      size: kTextureSize,
      format,
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_SRC,
    });

    const dstTexture = t.device.createTexture({
      size: kTextureSize,
      format,
      mipLevelCount: kMipLevelCount,
      usage: GPUTextureUsage.COPY_DST,
    });

    const srcSizeAtLevel = t.GetPhysicalSubresourceSize(kTextureSize, format, srcCopyLevel);
    const dstSizeAtLevel = t.GetPhysicalSubresourceSize(kTextureSize, format, dstCopyLevel);

    const copyOrigin = { x: copyBoxOffsets.x, y: copyBoxOffsets.y, z: copyBoxOffsets.z };

    const copyWidth = Math.max(
      Math.min(srcSizeAtLevel.width, dstSizeAtLevel.width) + copyBoxOffsets.width - copyOrigin.x,
      0
    );

    const copyHeight = Math.max(
      Math.min(srcSizeAtLevel.height, dstSizeAtLevel.height) + copyBoxOffsets.height - copyOrigin.y,
      0
    );

    const copyDepth = kTextureSize.depth + copyBoxOffsets.depth - copyOrigin.z;

    const texelBlockWidth = kAllTextureFormatInfo[format].blockWidth;
    const texelBlockHeight = kAllTextureFormatInfo[format].blockHeight;

    const isSuccessForCompressedFormats =
      copyOrigin.x % texelBlockWidth === 0 &&
      copyOrigin.y % texelBlockHeight === 0 &&
      copyWidth % texelBlockWidth === 0 &&
      copyHeight % texelBlockHeight === 0;

    {
      const isSuccess =
        isSuccessForCompressedFormats &&
        copyWidth <= srcSizeAtLevel.width &&
        copyHeight <= srcSizeAtLevel.height &&
        copyOrigin.x + copyWidth <= dstSizeAtLevel.width &&
        copyOrigin.y + copyHeight <= dstSizeAtLevel.height &&
        copyOrigin.z + copyDepth <= kTextureSize.depth;

      t.TestCopyTextureToTexture(
        { texture: srcTexture, origin: { x: 0, y: 0, z: 0 }, mipLevel: srcCopyLevel },
        { texture: dstTexture, origin: copyOrigin, mipLevel: dstCopyLevel },
        { width: copyWidth, height: copyHeight, depth: copyDepth },
        isSuccess
      );
    }

    {
      const isSuccess =
        isSuccessForCompressedFormats &&
        copyOrigin.x + copyWidth <= srcSizeAtLevel.width &&
        copyOrigin.y + copyHeight <= srcSizeAtLevel.height &&
        copyWidth <= dstSizeAtLevel.width &&
        copyHeight <= dstSizeAtLevel.height &&
        copyOrigin.z + copyDepth <= kTextureSize.depth;

      t.TestCopyTextureToTexture(
        { texture: srcTexture, origin: copyOrigin, mipLevel: srcCopyLevel },
        { texture: dstTexture, origin: { x: 0, y: 0, z: 0 }, mipLevel: dstCopyLevel },
        { width: copyWidth, height: copyHeight, depth: copyDepth },
        isSuccess
      );
    }
  });
