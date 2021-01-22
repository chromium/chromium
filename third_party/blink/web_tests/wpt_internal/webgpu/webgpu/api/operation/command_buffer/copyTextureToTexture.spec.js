/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `copyTexturetoTexture operation tests

  TODO(jiawei.shao@intel.com): support all WebGPU texture formats.
  `;
import { poptions, params } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/framework/util/util.js';
import {
  kSizedTextureFormatInfo,
  kRegularTextureFormats,
  kCompressedTextureFormatInfo,
  kCompressedTextureFormats,
} from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';
import { align } from '../../../util/math.js';
import { physicalMipSize } from '../../../util/texture/subresource.js';

class F extends GPUTest {
  GetInitialDataPerMipLevel(textureSize, format, mipLevel) {
    // TODO(jiawei.shao@intel.com): support 3D textures
    const textureSizeAtLevel = physicalMipSize(textureSize, format, '2d', mipLevel);
    const bytesPerBlock = kSizedTextureFormatInfo[format].bytesPerBlock;
    const blockWidthInTexel = kSizedTextureFormatInfo[format].blockWidth;
    const blockHeightInTexel = kSizedTextureFormatInfo[format].blockHeight;
    const blocksPerSubresource =
      (textureSizeAtLevel.width / blockWidthInTexel) *
      (textureSizeAtLevel.height / blockHeightInTexel);

    const byteSize = bytesPerBlock * blocksPerSubresource * textureSizeAtLevel.depth;
    const initialData = new Uint8Array(new ArrayBuffer(byteSize));

    for (let i = 0; i < byteSize; ++i) {
      initialData[i] = (i ** 3 + i) % 251;
    }
    return initialData;
  }

  DoCopyTextureToTextureTest(
    srcTextureSize,
    dstTextureSize,
    format,
    copyBoxOffsets,

    srcCopyLevel,
    dstCopyLevel
  ) {
    const kMipLevelCount = 4;

    // Create srcTexture and dstTexture
    const srcTextureDesc = {
      size: srcTextureSize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
      mipLevelCount: kMipLevelCount,
    };

    const srcTexture = this.device.createTexture(srcTextureDesc);
    const dstTextureDesc = {
      size: dstTextureSize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
      mipLevelCount: kMipLevelCount,
    };

    const dstTexture = this.device.createTexture(dstTextureDesc);

    // Fill the whole subresource of srcTexture at srcCopyLevel with initialSrcData.
    const initialSrcData = this.GetInitialDataPerMipLevel(srcTextureSize, format, srcCopyLevel);
    const srcTextureSizeAtLevel = physicalMipSize(srcTextureSize, format, '2d', srcCopyLevel);
    const bytesPerBlock = kSizedTextureFormatInfo[format].bytesPerBlock;
    const blockWidth = kSizedTextureFormatInfo[format].blockWidth;
    const blockHeight = kSizedTextureFormatInfo[format].blockHeight;
    const srcBlocksPerRow = srcTextureSizeAtLevel.width / blockWidth;
    const srcBlockRowsPerImage = srcTextureSizeAtLevel.height / blockHeight;
    this.device.defaultQueue.writeTexture(
      { texture: srcTexture, mipLevel: srcCopyLevel },
      initialSrcData,
      {
        bytesPerRow: srcBlocksPerRow * bytesPerBlock,
        rowsPerImage: srcBlockRowsPerImage,
      },

      srcTextureSizeAtLevel
    );

    // Copy the region specified by copyBoxOffsets from srcTexture to dstTexture.
    const dstTextureSizeAtLevel = physicalMipSize(dstTextureSize, format, '2d', dstCopyLevel);
    const minWidth = Math.min(srcTextureSizeAtLevel.width, dstTextureSizeAtLevel.width);
    const minHeight = Math.min(srcTextureSizeAtLevel.height, dstTextureSizeAtLevel.height);

    const appliedSrcOffset = {
      x: Math.min(copyBoxOffsets.srcOffset.x * blockWidth, minWidth),
      y: Math.min(copyBoxOffsets.srcOffset.y * blockHeight, minHeight),
      z: copyBoxOffsets.srcOffset.z,
    };

    const appliedDstOffset = {
      x: Math.min(copyBoxOffsets.dstOffset.x * blockWidth, minWidth),
      y: Math.min(copyBoxOffsets.dstOffset.y * blockHeight, minHeight),
      z: copyBoxOffsets.dstOffset.z,
    };

    const appliedCopyWidth = Math.max(
      minWidth +
        copyBoxOffsets.copyExtent.width * blockWidth -
        Math.max(appliedSrcOffset.x, appliedDstOffset.x),
      0
    );

    const appliedCopyHeight = Math.max(
      minHeight +
        copyBoxOffsets.copyExtent.height * blockHeight -
        Math.max(appliedSrcOffset.y, appliedDstOffset.y),
      0
    );

    assert(appliedCopyWidth % blockWidth === 0 && appliedCopyHeight % blockHeight === 0);

    const appliedCopyDepth =
      srcTextureSize.depth +
      copyBoxOffsets.copyExtent.depth -
      Math.max(appliedSrcOffset.z, appliedDstOffset.z);
    assert(appliedCopyDepth >= 0);

    const encoder = this.device.createCommandEncoder();
    encoder.copyTextureToTexture(
      { texture: srcTexture, mipLevel: srcCopyLevel, origin: appliedSrcOffset },
      { texture: dstTexture, mipLevel: dstCopyLevel, origin: appliedDstOffset },
      { width: appliedCopyWidth, height: appliedCopyHeight, depth: appliedCopyDepth }
    );

    // Copy the whole content of dstTexture at dstCopyLevel to dstBuffer.
    const dstBlocksPerRow = dstTextureSizeAtLevel.width / blockWidth;
    const dstBlockRowsPerImage = dstTextureSizeAtLevel.height / blockHeight;
    const bytesPerDstAlignedBlockRow = align(dstBlocksPerRow * bytesPerBlock, 256);
    const dstBufferSize =
      (dstBlockRowsPerImage * dstTextureSizeAtLevel.depth - 1) * bytesPerDstAlignedBlockRow +
      align(dstBlocksPerRow * bytesPerBlock, 4);
    const dstBufferDesc = {
      size: dstBufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    };

    const dstBuffer = this.device.createBuffer(dstBufferDesc);

    encoder.copyTextureToBuffer(
      { texture: dstTexture, mipLevel: dstCopyLevel },
      {
        buffer: dstBuffer,
        bytesPerRow: bytesPerDstAlignedBlockRow,
        rowsPerImage: dstBlockRowsPerImage,
      },

      dstTextureSizeAtLevel
    );

    this.device.defaultQueue.submit([encoder.finish()]);

    // Fill expectedDataWithPadding with the expected data of dstTexture. The other values in
    // expectedDataWithPadding are kept 0 to check if the texels untouched by the copy are 0
    // (their previous values).
    const expectedDataWithPadding = new ArrayBuffer(dstBufferSize);
    const expectedUint8DataWithPadding = new Uint8Array(expectedDataWithPadding);
    const expectedUint8Data = new Uint8Array(initialSrcData);

    const appliedCopyBlocksPerRow = appliedCopyWidth / blockWidth;
    const appliedCopyBlockRowsPerImage = appliedCopyHeight / blockHeight;
    const srcCopyOffsetInBlocks = {
      x: appliedSrcOffset.x / blockWidth,
      y: appliedSrcOffset.y / blockHeight,
      z: appliedSrcOffset.z,
    };

    const dstCopyOffsetInBlocks = {
      x: appliedDstOffset.x / blockWidth,
      y: appliedDstOffset.y / blockHeight,
      z: appliedDstOffset.z,
    };

    for (let z = 0; z < appliedCopyDepth; ++z) {
      const srcOffsetZ = srcCopyOffsetInBlocks.z + z;
      const dstOffsetZ = dstCopyOffsetInBlocks.z + z;
      for (let y = 0; y < appliedCopyBlockRowsPerImage; ++y) {
        const dstOffsetYInBlocks = dstCopyOffsetInBlocks.y + y;
        const expectedDataWithPaddingOffset =
          bytesPerDstAlignedBlockRow * (dstBlockRowsPerImage * dstOffsetZ + dstOffsetYInBlocks) +
          dstCopyOffsetInBlocks.x * bytesPerBlock;

        const srcOffsetYInBlocks = srcCopyOffsetInBlocks.y + y;
        const expectedDataOffset =
          bytesPerBlock *
            srcBlocksPerRow *
            (srcBlockRowsPerImage * srcOffsetZ + srcOffsetYInBlocks) +
          srcCopyOffsetInBlocks.x * bytesPerBlock;

        expectedUint8DataWithPadding.set(
          expectedUint8Data.slice(
            expectedDataOffset,
            expectedDataOffset + appliedCopyBlocksPerRow * bytesPerBlock
          ),

          expectedDataWithPaddingOffset
        );
      }
    }

    // Verify the content of the whole subresouce of dstTexture at dstCopyLevel (in dstBuffer) is expected.
    this.expectContents(dstBuffer, expectedUint8DataWithPadding);
  }

  static kCopyBoxOffsetsForWholeDepth = [
    // From (0, 0) of src to (0, 0) of dst.
    {
      srcOffset: { x: 0, y: 0, z: 0 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: 0, height: 0, depth: 0 },
    },

    // From (0, 0) of src to (blockWidth, 0) of dst.
    {
      srcOffset: { x: 0, y: 0, z: 0 },
      dstOffset: { x: 1, y: 0, z: 0 },
      copyExtent: { width: 0, height: 0, depth: 0 },
    },

    // From (0, 0) of src to (0, blockHeight) of dst.
    {
      srcOffset: { x: 0, y: 0, z: 0 },
      dstOffset: { x: 0, y: 1, z: 0 },
      copyExtent: { width: 0, height: 0, depth: 0 },
    },

    // From (blockWidth, 0) of src to (0, 0) of dst.
    {
      srcOffset: { x: 1, y: 0, z: 0 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: 0, height: 0, depth: 0 },
    },

    // From (0, blockHeight) of src to (0, 0) of dst.
    {
      srcOffset: { x: 0, y: 1, z: 0 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: 0, height: 0, depth: 0 },
    },

    // From (blockWidth, 0) of src to (0, 0) of dst, and the copy extent will not cover the last
    // texel block column of both source and destination texture.
    {
      srcOffset: { x: 1, y: 0, z: 0 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: -1, height: 0, depth: 0 },
    },

    // From (0, blockHeight) of src to (0, 0) of dst, and the copy extent will not cover the last
    // texel block row of both source and destination texture.
    {
      srcOffset: { x: 0, y: 1, z: 0 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: 0, height: -1, depth: 0 },
    },
  ];

  static kCopyBoxOffsetsFor2DArrayTextures = [
    // Copy the whole array slices from the source texture to the destination texture.
    // The copy extent will cover the whole subresource of either source or the
    // destination texture
    ...F.kCopyBoxOffsetsForWholeDepth,

    // Copy 1 texture slice from the 1st slice of the source texture to the 1st slice of the
    // destination texture.
    {
      srcOffset: { x: 0, y: 0, z: 0 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: 0, height: 0, depth: -2 },
    },

    // Copy 1 texture slice from the 2nd slice of the source texture to the 2nd slice of the
    // destination texture.
    {
      srcOffset: { x: 0, y: 0, z: 1 },
      dstOffset: { x: 0, y: 0, z: 1 },
      copyExtent: { width: 0, height: 0, depth: -3 },
    },

    // Copy 1 texture slice from the 1st slice of the source texture to the 2nd slice of the
    // destination texture.
    {
      srcOffset: { x: 0, y: 0, z: 0 },
      dstOffset: { x: 0, y: 0, z: 1 },
      copyExtent: { width: 0, height: 0, depth: -1 },
    },

    // Copy 1 texture slice from the 2nd slice of the source texture to the 1st slice of the
    // destination texture.
    {
      srcOffset: { x: 0, y: 0, z: 1 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: 0, height: 0, depth: -1 },
    },

    // Copy 2 texture slices from the 1st slice of the source texture to the 1st slice of the
    // destination texture.
    {
      srcOffset: { x: 0, y: 0, z: 0 },
      dstOffset: { x: 0, y: 0, z: 0 },
      copyExtent: { width: 0, height: 0, depth: -3 },
    },

    // Copy 3 texture slices from the 2nd slice of the source texture to the 2nd slice of the
    // destination texture.
    {
      srcOffset: { x: 0, y: 0, z: 1 },
      dstOffset: { x: 0, y: 0, z: 1 },
      copyExtent: { width: 0, height: 0, depth: -1 },
    },
  ];
}

export const g = makeTestGroup(F);

g.test('color_textures,non_compressed,non_array')
  .desc(
    `
  Validate the correctness of the copy by filling the srcTexture with testable data and any
  non-compressed color format supported by WebGPU, doing CopyTextureToTexture() copy, and verifying
  the content of the whole dstTexture.

  Copy {1 texel block, part of, the whole} srcTexture to the dstTexture {with, without} a non-zero
  valid srcOffset that
  - covers the whole dstTexture subresource
  - covers the corners of the dstTexture
  - doesn't cover any texels that are on the edge of the dstTexture
  - covers the mipmap level > 0
  `
  )
  .params(
    params()
      .combine(poptions('format', kRegularTextureFormats))
      .combine(
        poptions('textureSize', [
          {
            srcTextureSize: { width: 32, height: 32, depth: 1 },
            dstTextureSize: { width: 32, height: 32, depth: 1 },
          },

          {
            srcTextureSize: { width: 31, height: 33, depth: 1 },
            dstTextureSize: { width: 31, height: 33, depth: 1 },
          },

          {
            srcTextureSize: { width: 32, height: 32, depth: 1 },
            dstTextureSize: { width: 64, height: 64, depth: 1 },
          },

          {
            srcTextureSize: { width: 32, height: 32, depth: 1 },
            dstTextureSize: { width: 63, height: 61, depth: 1 },
          },
        ])
      )
      .combine(poptions('copyBoxOffsets', F.kCopyBoxOffsetsForWholeDepth))
      .combine(poptions('srcCopyLevel', [0, 3]))
      .combine(poptions('dstCopyLevel', [0, 3]))
  )
  .fn(async t => {
    const { textureSize, format, copyBoxOffsets, srcCopyLevel, dstCopyLevel } = t.params;

    t.DoCopyTextureToTextureTest(
      textureSize.srcTextureSize,
      textureSize.dstTextureSize,
      format,
      copyBoxOffsets,
      srcCopyLevel,
      dstCopyLevel
    );
  });

g.test('color_textures,compressed,non_array')
  .desc(
    `
  Validate the correctness of the copy by filling the srcTexture with testable data and any
  compressed color format supported by WebGPU, doing CopyTextureToTexture() copy, and verifying
  the content of the whole dstTexture.
  `
  )
  .params(
    params()
      .combine(poptions('format', kCompressedTextureFormats))
      .combine(
        poptions('textureSize', [
          // The heights and widths are all power of 2
          {
            srcTextureSize: { width: 64, height: 32, depth: 1 },
            dstTextureSize: { width: 64, height: 32, depth: 1 },
          },

          // The virtual width of the source texture at mipmap level 2 (15) is not a multiple of 4
          {
            srcTextureSize: { width: 60, height: 32, depth: 1 },
            dstTextureSize: { width: 64, height: 32, depth: 1 },
          },

          // The virtual width of the destination texture at mipmap level 2 (15) is not a multiple
          // of 4
          {
            srcTextureSize: { width: 64, height: 32, depth: 1 },
            dstTextureSize: { width: 60, height: 32, depth: 1 },
          },

          // The virtual height of the source texture at mipmap level 2 (13) is not a multiple of 4
          {
            srcTextureSize: { width: 64, height: 52, depth: 1 },
            dstTextureSize: { width: 64, height: 32, depth: 1 },
          },

          // The virtual height of the destination texture at mipmap level 2 (13) is not a
          // multiple of 4
          {
            srcTextureSize: { width: 64, height: 32, depth: 1 },
            dstTextureSize: { width: 64, height: 52, depth: 1 },
          },

          // None of the widths or heights are power of 2
          {
            srcTextureSize: { width: 60, height: 52, depth: 1 },
            dstTextureSize: { width: 60, height: 52, depth: 1 },
          },
        ])
      )
      .combine(poptions('copyBoxOffsets', F.kCopyBoxOffsetsForWholeDepth))
      .combine(poptions('srcCopyLevel', [0, 2]))
      .combine(poptions('dstCopyLevel', [0, 2]))
  )
  .fn(async t => {
    const { textureSize, format, copyBoxOffsets, srcCopyLevel, dstCopyLevel } = t.params;

    const extension = kCompressedTextureFormatInfo[format].extension;
    await t.selectDeviceOrSkipTestCase({ extensions: [extension] });

    t.DoCopyTextureToTextureTest(
      textureSize.srcTextureSize,
      textureSize.dstTextureSize,
      format,
      copyBoxOffsets,
      srcCopyLevel,
      dstCopyLevel
    );
  });

g.test('color_textures,non_compressed,array')
  .desc(
    `
  Validate the correctness of the texture-to-texture copy on 2D array textures by filling the
  srcTexture with testable data and any non-compressed color format supported by WebGPU, doing
  CopyTextureToTexture() copy, and verifying the content of the whole dstTexture.
  `
  )
  .params(
    params()
      .combine(poptions('format', kRegularTextureFormats))
      .combine(
        poptions('textureSize', [
          {
            srcTextureSize: { width: 64, height: 32, depth: 5 },
            dstTextureSize: { width: 64, height: 32, depth: 5 },
          },

          {
            srcTextureSize: { width: 31, height: 33, depth: 5 },
            dstTextureSize: { width: 31, height: 33, depth: 5 },
          },
        ])
      )
      .combine(poptions('copyBoxOffsets', F.kCopyBoxOffsetsFor2DArrayTextures))
      .combine(poptions('srcCopyLevel', [0, 3]))
      .combine(poptions('dstCopyLevel', [0, 3]))
  )
  .fn(async t => {
    const { textureSize, format, copyBoxOffsets, srcCopyLevel, dstCopyLevel } = t.params;

    t.DoCopyTextureToTextureTest(
      textureSize.srcTextureSize,
      textureSize.dstTextureSize,
      format,
      copyBoxOffsets,
      srcCopyLevel,
      dstCopyLevel
    );
  });

g.test('color_textures,compressed,array')
  .desc(
    `
  Validate the correctness of the texture-to-texture copy on 2D array textures by filling the
  srcTexture with testable data and any compressed color format supported by WebGPU, doing
  CopyTextureToTexture() copy, and verifying the content of the whole dstTexture.
  `
  )
  .params(
    params()
      .combine(poptions('format', kCompressedTextureFormats))
      .combine(
        poptions('textureSize', [
          // The heights and widths are all power of 2
          {
            srcTextureSize: { width: 8, height: 8, depth: 5 },
            dstTextureSize: { width: 8, height: 8, depth: 5 },
          },

          // None of the widths or heights are power of 2
          {
            srcTextureSize: { width: 60, height: 52, depth: 5 },
            dstTextureSize: { width: 60, height: 52, depth: 5 },
          },
        ])
      )
      .combine(poptions('copyBoxOffsets', F.kCopyBoxOffsetsFor2DArrayTextures))
      .combine(poptions('srcCopyLevel', [0, 2]))
      .combine(poptions('dstCopyLevel', [0, 2]))
  )
  .fn(async t => {
    const { textureSize, format, copyBoxOffsets, srcCopyLevel, dstCopyLevel } = t.params;

    const extension = kCompressedTextureFormatInfo[format].extension;
    await t.selectDeviceOrSkipTestCase({ extensions: [extension] });

    t.DoCopyTextureToTextureTest(
      textureSize.srcTextureSize,
      textureSize.dstTextureSize,
      format,
      copyBoxOffsets,
      srcCopyLevel,
      dstCopyLevel
    );
  });

g.test('zero_sized')
  .desc(
    `
  Validate the correctness of zero-sized copies (should be no-ops).

  - Copies that are zero-sized in only one dimension {x, y, z}, each touching the {lower, upper} end
  of that dimension.
  `
  )
  .params(
    params()
      .combine(
        poptions('copyBoxOffset', [
          // copyExtent.width === 0
          {
            srcOffset: { x: 0, y: 0, z: 0 },
            dstOffset: { x: 0, y: 0, z: 0 },
            copyExtent: { width: -64, height: 0, depth: 0 },
          },

          // copyExtent.width === 0 && srcOffset.x === textureWidth
          {
            srcOffset: { x: 64, y: 0, z: 0 },
            dstOffset: { x: 0, y: 0, z: 0 },
            copyExtent: { width: -64, height: 0, depth: 0 },
          },

          // copyExtent.width === 0 && dstOffset.x === textureWidth
          {
            srcOffset: { x: 0, y: 0, z: 0 },
            dstOffset: { x: 64, y: 0, z: 0 },
            copyExtent: { width: -64, height: 0, depth: 0 },
          },

          // copyExtent.height === 0
          {
            srcOffset: { x: 0, y: 0, z: 0 },
            dstOffset: { x: 0, y: 0, z: 0 },
            copyExtent: { width: 0, height: -32, depth: 0 },
          },

          // copyExtent.height === 0 && srcOffset.y === textureHeight
          {
            srcOffset: { x: 0, y: 32, z: 0 },
            dstOffset: { x: 0, y: 0, z: 0 },
            copyExtent: { width: 0, height: -32, depth: 0 },
          },

          // copyExtent.height === 0 && dstOffset.y === textureHeight
          {
            srcOffset: { x: 0, y: 0, z: 0 },
            dstOffset: { x: 0, y: 32, z: 0 },
            copyExtent: { width: 0, height: -32, depth: 0 },
          },

          // copyExtent.depth === 0
          {
            srcOffset: { x: 0, y: 0, z: 0 },
            dstOffset: { x: 0, y: 0, z: 0 },
            copyExtent: { width: 0, height: 0, depth: -5 },
          },

          // copyExtent.depth === 0 && srcOffset.z === textureDepth
          {
            srcOffset: { x: 0, y: 0, z: 5 },
            dstOffset: { x: 0, y: 0, z: 0 },
            copyExtent: { width: 0, height: 0, depth: 0 },
          },

          // copyExtent.depth === 0 && dstOffset.z === textureDepth
          {
            srcOffset: { x: 0, y: 0, z: 0 },
            dstOffset: { x: 0, y: 0, z: 5 },
            copyExtent: { width: 0, height: 0, depth: 0 },
          },
        ])
      )
      .combine(poptions('srcCopyLevel', [0, 3]))
      .combine(poptions('dstCopyLevel', [0, 3]))
  )
  .fn(async t => {
    const { copyBoxOffset, srcCopyLevel, dstCopyLevel } = t.params;

    const format = 'rgba8unorm';
    const textureSize = { width: 64, height: 32, depth: 5 };

    t.DoCopyTextureToTextureTest(
      textureSize,
      textureSize,
      format,
      copyBoxOffset,
      srcCopyLevel,
      dstCopyLevel
    );
  });
