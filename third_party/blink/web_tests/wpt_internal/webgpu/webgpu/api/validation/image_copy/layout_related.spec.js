/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = '';
import { params, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import {
  kUncompressedTextureFormatInfo,
  kSizedTextureFormats,
  kSizedTextureFormatInfo,
} from '../../../capability_info.js';
import { align } from '../../../util/math.js';
import {
  bytesInACompleteRow,
  dataBytesForCopyOrOverestimate,
  dataBytesForCopyOrFail,
  kImageCopyTypes,
} from '../../../util/texture/image_copy.js';

import {
  ImageCopyTest,
  texelBlockAlignmentTestExpanderForOffset,
  texelBlockAlignmentTestExpanderForRowsPerImage,
  formatCopyableWithMethod,
} from './image_copy.js';

export const g = makeTestGroup(ImageCopyTest);

g.test('bound_on_rows_per_image')
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() =>
    params()
      .combine(poptions('rowsPerImage', [undefined, 0, 1, 2, 1024]))
      .combine(poptions('copyHeightInBlocks', [0, 1, 2]))
      .combine(poptions('copyDepth', [1, 3]))
  )
  .fn(async t => {
    const { rowsPerImage, copyHeightInBlocks, copyDepth, method } = t.params;

    const format = 'rgba8unorm';
    const copyHeight = copyHeightInBlocks * kUncompressedTextureFormatInfo[format].blockHeight;

    const texture = t.device.createTexture({
      size: { width: 4, height: 4, depthOrArrayLayers: 3 },
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const layout = { bytesPerRow: 1024, rowsPerImage };
    const copySize = { width: 0, height: copyHeight, depthOrArrayLayers: copyDepth };
    const { minDataSizeOrOverestimate, copyValid } = dataBytesForCopyOrOverestimate({
      layout,
      format,
      copySize,
      method,
    });

    t.testRun({ texture }, layout, copySize, {
      dataSize: minDataSizeOrOverestimate,
      method,
      success: copyValid,
    });
  });

g.test('copy_end_overflows_u64')
  .desc(`Test what happens when offset+requiredBytesInCopy overflows GPUSize64.`)
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() => [
    { bytesPerRow: 2 ** 31, rowsPerImage: 2 ** 31, depthOrArrayLayers: 1, _success: true }, // success case
    { bytesPerRow: 2 ** 31, rowsPerImage: 2 ** 31, depthOrArrayLayers: 16, _success: false }, // bytesPerRow * rowsPerImage * (depthOrArrayLayers - 1) overflows.
  ])
  .fn(async t => {
    const { method, bytesPerRow, rowsPerImage, depthOrArrayLayers, _success } = t.params;

    const texture = t.device.createTexture({
      size: [1, 1, depthOrArrayLayers],
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    t.testRun(
      { texture },
      { bytesPerRow, rowsPerImage },
      { width: 1, height: 1, depthOrArrayLayers },
      {
        dataSize: 10000,
        method,
        success: _success,
      }
    );
  });

g.test('required_bytes_in_copy')
  .desc(
    `Test that the min data size condition (requiredBytesInCopy) is checked correctly.

  - Exact requiredBytesInCopy should succeed.
  - requiredBytesInCopy - 1 should fail.
  `
  )
  .cases(
    params()
      .combine(poptions('method', kImageCopyTypes))
      .combine(poptions('format', kSizedTextureFormats))
      .filter(formatCopyableWithMethod)
  )
  .subcases(() =>
    params()
      .combine([
        { bytesPerRowPadding: 0, rowsPerImagePaddingInBlocks: 0 }, // no padding
        { bytesPerRowPadding: 0, rowsPerImagePaddingInBlocks: 6 }, // rowsPerImage padding
        { bytesPerRowPadding: 6, rowsPerImagePaddingInBlocks: 0 }, // bytesPerRow padding
        { bytesPerRowPadding: 15, rowsPerImagePaddingInBlocks: 17 }, // both paddings
      ])
      .combine([
        { copyWidthInBlocks: 3, copyHeightInBlocks: 4, copyDepth: 5, offsetInBlocks: 0 }, // standard copy
        { copyWidthInBlocks: 5, copyHeightInBlocks: 4, copyDepth: 3, offsetInBlocks: 11 }, // standard copy, offset > 0
        { copyWidthInBlocks: 256, copyHeightInBlocks: 3, copyDepth: 2, offsetInBlocks: 0 }, // copyWidth is 256-aligned
        { copyWidthInBlocks: 0, copyHeightInBlocks: 4, copyDepth: 5, offsetInBlocks: 0 }, // empty copy because of width
        { copyWidthInBlocks: 3, copyHeightInBlocks: 0, copyDepth: 5, offsetInBlocks: 0 }, // empty copy because of height
        { copyWidthInBlocks: 3, copyHeightInBlocks: 4, copyDepth: 0, offsetInBlocks: 13 }, // empty copy because of depth, offset > 0
        { copyWidthInBlocks: 1, copyHeightInBlocks: 4, copyDepth: 5, offsetInBlocks: 0 }, // copyWidth = 1
        { copyWidthInBlocks: 3, copyHeightInBlocks: 1, copyDepth: 5, offsetInBlocks: 15 }, // copyHeight = 1, offset > 0
        { copyWidthInBlocks: 5, copyHeightInBlocks: 4, copyDepth: 1, offsetInBlocks: 0 }, // copyDepth = 1
        { copyWidthInBlocks: 7, copyHeightInBlocks: 1, copyDepth: 1, offsetInBlocks: 0 }, // copyHeight = 1 and copyDepth = 1
      ])
  )
  .fn(async t => {
    const {
      offsetInBlocks,
      bytesPerRowPadding,
      rowsPerImagePaddingInBlocks,
      copyWidthInBlocks,
      copyHeightInBlocks,
      copyDepth,
      format,
      method,
    } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    // In the CopyB2T and CopyT2B cases we need to have bytesPerRow 256-aligned,
    // to make this happen we align the bytesInACompleteRow value and multiply
    // bytesPerRowPadding by 256.
    const bytesPerRowAlignment = method === 'WriteTexture' ? 1 : 256;

    const copyWidth = copyWidthInBlocks * info.blockWidth;
    const copyHeight = copyHeightInBlocks * info.blockHeight;
    const offset = offsetInBlocks * info.bytesPerBlock;
    const rowsPerImage = copyHeight + rowsPerImagePaddingInBlocks * info.blockHeight;
    const bytesPerRow =
      align(bytesInACompleteRow(copyWidth, format), bytesPerRowAlignment) +
      bytesPerRowPadding * bytesPerRowAlignment;
    const copySize = { width: copyWidth, height: copyHeight, depthOrArrayLayers: copyDepth };

    const layout = { offset, bytesPerRow, rowsPerImage };
    const minDataSize = dataBytesForCopyOrFail({ layout, format, copySize, method });

    const texture = t.createAlignedTexture(format, copySize);

    t.testRun({ texture }, { offset, bytesPerRow, rowsPerImage }, copySize, {
      dataSize: minDataSize,
      method,
      success: true,
    });

    if (minDataSize > 0) {
      t.testRun({ texture }, { offset, bytesPerRow, rowsPerImage }, copySize, {
        dataSize: minDataSize - 1,
        method,
        success: false,
      });
    }
  });

g.test('rows_per_image_alignment')
  .desc(`rowsPerImage is measured in multiples of block height, so has no alignment constraints.`)
  .cases(
    params()
      .combine(poptions('method', kImageCopyTypes))
      .combine(poptions('format', kSizedTextureFormats))
      .filter(formatCopyableWithMethod)
  )
  .subcases(texelBlockAlignmentTestExpanderForRowsPerImage)
  .fn(async t => {
    const { rowsPerImage, format, method } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    const size = { width: 0, height: 0, depthOrArrayLayers: 0 };

    const texture = t.createAlignedTexture(format, size);

    t.testRun({ texture }, { bytesPerRow: 0, rowsPerImage }, size, {
      dataSize: 1,
      method,
      success: true,
    });
  });

g.test('texel_block_alignment_on_offset')
  .cases(
    params()
      .combine(poptions('method', kImageCopyTypes))
      .combine(poptions('format', kSizedTextureFormats))
      .filter(formatCopyableWithMethod)
  )
  .subcases(texelBlockAlignmentTestExpanderForOffset)
  .fn(async t => {
    const { format, offset, method } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    const size = { width: 0, height: 0, depthOrArrayLayers: 0 };

    const texture = t.createAlignedTexture(format, size);

    const success =
      method === 'WriteTexture' || offset % kSizedTextureFormatInfo[format].bytesPerBlock === 0;

    t.testRun({ texture }, { offset, bytesPerRow: 0 }, size, { dataSize: offset, method, success });
  });

g.test('bound_on_bytes_per_row')
  .cases(
    params()
      .combine(poptions('method', kImageCopyTypes))
      .combine(poptions('format', kSizedTextureFormats))
      .filter(formatCopyableWithMethod)
  )
  .subcases(() =>
    params()
      .combine([
        { blocksPerRow: 2, additionalPaddingPerRow: 0, copyWidthInBlocks: 2 }, // success
        { blocksPerRow: 2, additionalPaddingPerRow: 5, copyWidthInBlocks: 3 }, // success if bytesPerBlock <= 5
        { blocksPerRow: 1, additionalPaddingPerRow: 0, copyWidthInBlocks: 2 }, // failure, bytesPerRow > 0
        { blocksPerRow: 0, additionalPaddingPerRow: 0, copyWidthInBlocks: 1 }, // failure, bytesPerRow = 0
      ])
      .combine([
        { copyHeightInBlocks: 0, copyDepth: 1 }, // we don't have to check the bound
        { copyHeightInBlocks: 1, copyDepth: 0 }, // we don't have to check the bound
        { copyHeightInBlocks: 2, copyDepth: 1 }, // we have to check the bound
        { copyHeightInBlocks: 0, copyDepth: 2 }, // we have to check the bound
      ])
  )
  .fn(async t => {
    const {
      blocksPerRow,
      additionalPaddingPerRow,
      copyWidthInBlocks,
      copyHeightInBlocks,
      copyDepth,
      format,
      method,
    } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    // In the CopyB2T and CopyT2B cases we need to have bytesPerRow 256-aligned.
    const bytesPerRowAlignment = method === 'WriteTexture' ? 1 : 256;

    const copyWidth = align(copyWidthInBlocks * info.blockWidth, bytesPerRowAlignment);
    const copyHeight = copyHeightInBlocks * info.blockHeight;
    const bytesPerRow = align(
      blocksPerRow * info.bytesPerBlock + additionalPaddingPerRow,
      bytesPerRowAlignment
    );

    const copySize = { width: copyWidth, height: copyHeight, depthOrArrayLayers: copyDepth };

    const texture = t.createAlignedTexture(format, {
      width: copyWidth,
      // size 0 is not valid; round up if needed
      height: copyHeight || info.blockHeight,
      depthOrArrayLayers: copyDepth || 1,
    });

    const layout = { bytesPerRow, rowsPerImage: copyHeight };
    const { minDataSizeOrOverestimate, copyValid } = dataBytesForCopyOrOverestimate({
      layout,
      format,
      copySize,
      method,
    });

    t.testRun({ texture }, layout, copySize, {
      dataSize: minDataSizeOrOverestimate,
      method,
      success: copyValid,
    });
  });

g.test('bound_on_offset')
  .cases(poptions('method', kImageCopyTypes))
  .subcases(() =>
    params()
      .combine(poptions('offsetInBlocks', [0, 1, 2]))
      .combine(poptions('dataSizeInBlocks', [0, 1, 2]))
  )
  .fn(async t => {
    const { offsetInBlocks, dataSizeInBlocks, method } = t.params;

    const format = 'rgba8unorm';
    const info = kSizedTextureFormatInfo[format];
    const offset = offsetInBlocks * info.bytesPerBlock;
    const dataSize = dataSizeInBlocks * info.bytesPerBlock;

    const texture = t.device.createTexture({
      size: { width: 4, height: 4, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const success = offset <= dataSize;

    t.testRun(
      { texture },
      { offset, bytesPerRow: 0 },
      { width: 0, height: 0, depthOrArrayLayers: 0 },
      { dataSize, method, success }
    );
  });
