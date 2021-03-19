/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `writeTexture + copyBufferToTexture + copyTextureToBuffer operation tests.

* copy_with_various_rows_per_image_and_bytes_per_row: test that copying data with various bytesPerRow (including { ==, > } bytesInACompleteRow) and\
 rowsPerImage (including { ==, > } copyExtent.height) values and minimum required bytes in copy works for every format. Also covers special code paths:
  - bufferSize - offset < bytesPerImage * copyExtent.depthOrArrayLayers
  - when bytesPerRow is not a multiple of 512 and copyExtent.depthOrArrayLayers > 1: copyExtent.depthOrArrayLayers % 2 == { 0, 1 }
  - bytesPerRow == bytesInACompleteCopyImage

* copy_with_various_offsets_and_data_sizes: test that copying data with various offset (including { ==, > } 0 and is/isn't power of 2) values and additional\
 data paddings works for every format with 2d and 2d-array textures. Also covers special code paths:
  - offset + bytesInCopyExtentPerRow { ==, > } bytesPerRow
  - offset > bytesInACompleteCopyImage

* copy_with_various_origins_and_copy_extents: test that copying slices of a texture works with various origin (including { origin.x, origin.y, origin.z }\
 { ==, > } 0 and is/isn't power of 2) and copyExtent (including { copyExtent.x, copyExtent.y, copyExtent.z } { ==, > } 0 and is/isn't power of 2) values\
 (also including {origin._ + copyExtent._ { ==, < } the subresource size of textureCopyView) works for all formats. origin and copyExtent values are passed\
 as [number, number, number] instead of GPUExtent3DDict.

* copy_various_mip_levels: test that copying various mip levels works for all formats. Also covers special code paths:
  - the physical size of the subresouce is not equal to the logical size
  - bufferSize - offset < bytesPerImage * copyExtent.depthOrArrayLayers and copyExtent needs to be clamped

* copy_with_no_image_or_slice_padding_and_undefined_values: test that when copying a single row we can set any bytesPerRow value and when copying a single\
 slice we can set rowsPerImage to 0. Also test setting offset, rowsPerImage, mipLevel, origin, origin.{x,y,z} to undefined.

* TODO:
  - add another initMethod which renders the texture
  - test copyT2B with buffer size not divisible by 4 (not done because expectContents 4-byte alignment)
  - add tests for 1d / 3d textures

TODO: Fix this test for the various skipped formats:
- snorm tests failing due to rounding
- float tests failing because float values are not byte-preserved
- compressed formats
`;
import { params, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert, unreachable } from '../../../../common/framework/util/util.js';
import { kSizedTextureFormatInfo, kSizedTextureFormats } from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';
import { align } from '../../../util/math.js';
import { bytesInACompleteRow, dataBytesForCopyOrFail } from '../../../util/texture/image_copy.js';
import { getTextureCopyLayout } from '../../../util/texture/layout.js';

/** Each combination of methods assume that the ones before it were tested and work correctly. */
const kMethodsToTest = [
  // We make sure that CopyT2B works when copying the whole texture for renderable formats:
  // TODO
  // Then we make sure that WriteTexture works for all formats:
  { initMethod: 'WriteTexture', checkMethod: 'FullCopyT2B' },
  // Then we make sure that CopyB2T works for all formats:
  { initMethod: 'CopyB2T', checkMethod: 'FullCopyT2B' },
  // Then we make sure that CopyT2B works for all formats:
  { initMethod: 'WriteTexture', checkMethod: 'PartialCopyT2B' },
];

// TODO: Fix things so this list can be reduced to zero (see file description)
const kExcludedFormats = new Set([
  'r8snorm',
  'rg8snorm',
  'rgba8snorm',
  'rg11b10ufloat',
  'rg16float',
  'rgba16float',
  'r32float',
  'rg32float',
  'rgba32float',
]);

const kWorkingTextureFormats = kSizedTextureFormats.filter(x => !kExcludedFormats.has(x));

class ImageCopyTest extends GPUTest {
  /** Offset for a particular texel in the linear texture data */
  getTexelOffsetInBytes(textureDataLayout, format, texel, origin = { x: 0, y: 0, z: 0 }) {
    const { offset, bytesPerRow, rowsPerImage } = textureDataLayout;
    const info = kSizedTextureFormatInfo[format];

    assert(texel.x >= origin.x && texel.y >= origin.y && texel.z >= origin.z);
    assert(texel.x % info.blockWidth === 0);
    assert(texel.y % info.blockHeight === 0);
    assert(origin.x % info.blockWidth === 0);
    assert(origin.y % info.blockHeight === 0);

    const bytesPerImage = rowsPerImage * bytesPerRow;

    return (
      offset +
      (texel.z - origin.z) * bytesPerImage +
      ((texel.y - origin.y) / info.blockHeight) * bytesPerRow +
      ((texel.x - origin.x) / info.blockWidth) * info.bytesPerBlock
    );
  }

  *iterateBlockRows(size, origin, format) {
    if (size.width === 0 || size.height === 0 || size.depthOrArrayLayers === 0) {
      // do not iterate anything for an empty region
      return;
    }
    const info = kSizedTextureFormatInfo[format];
    assert(size.height % info.blockHeight === 0);
    for (let y = 0; y < size.height; y += info.blockHeight) {
      for (let z = 0; z < size.depthOrArrayLayers; ++z) {
        yield {
          x: origin.x,
          y: origin.y + y,
          z: origin.z + z,
        };
      }
    }
  }

  generateData(byteSize, start = 0) {
    const arr = new Uint8Array(byteSize);
    for (let i = 0; i < byteSize; ++i) {
      arr[i] = (i ** 3 + i + start) % 251;
    }
    return arr;
  }

  /**
   * This is used for testing passing undefined members of `GPUTextureDataLayout` instead of actual
   * values where possible. Passing arguments as values and not as objects so that they are passed
   * by copy and not by reference.
   */
  undefDataLayoutIfNeeded(offset, rowsPerImage, bytesPerRow, changeBeforePass) {
    if (changeBeforePass === 'undefined') {
      if (offset === 0) {
        offset = undefined;
      }
      if (bytesPerRow === 0) {
        bytesPerRow = undefined;
      }
      if (rowsPerImage === 0) {
        rowsPerImage = undefined;
      }
    }
    return { offset, bytesPerRow, rowsPerImage };
  }

  /**
   * This is used for testing passing undefined members of `GPUTextureCopyView` instead of actual
   * values where possible and also for testing passing the origin as `[number, number, number]`.
   * Passing arguments as values and not as objects so that they are passed by copy and not by
   * reference.
   */
  undefOrArrayCopyViewIfNeeded(texture, origin_x, origin_y, origin_z, mipLevel, changeBeforePass) {
    let origin = { x: origin_x, y: origin_y, z: origin_z };

    if (changeBeforePass === 'undefined') {
      if (origin_x === 0 && origin_y === 0 && origin_z === 0) {
        origin = undefined;
      } else {
        if (origin_x === 0) {
          origin_x = undefined;
        }
        if (origin_y === 0) {
          origin_y = undefined;
        }
        if (origin_z === 0) {
          origin_z = undefined;
        }
        origin = { x: origin_x, y: origin_y, z: origin_z };
      }

      if (mipLevel === 0) {
        mipLevel = undefined;
      }
    }

    if (changeBeforePass === 'arrays') {
      origin = [origin_x, origin_y, origin_z];
    }

    return { texture, origin, mipLevel };
  }

  /**
   * This is used for testing passing `GPUExtent3D` as `[number, number, number]` instead of
   * `GPUExtent3DDict`. Passing arguments as values and not as objects so that they are passed by
   * copy and not by reference.
   */
  arrayCopySizeIfNeeded(width, height, depthOrArrayLayers, changeBeforePass) {
    if (changeBeforePass === 'arrays') {
      return [width, height, depthOrArrayLayers];
    } else {
      return { width, height, depthOrArrayLayers };
    }
  }

  /** Run a CopyT2B command with appropriate arguments corresponding to `ChangeBeforePass` */
  copyTextureToBufferWithAppliedArguments(
    buffer,
    { offset, rowsPerImage, bytesPerRow },
    { width, height, depthOrArrayLayers },
    { texture, mipLevel, origin },
    changeBeforePass
  ) {
    const { x, y, z } = origin;

    const appliedCopyView = this.undefOrArrayCopyViewIfNeeded(
      texture,
      x,
      y,
      z,
      mipLevel,
      changeBeforePass
    );

    const appliedDataLayout = this.undefDataLayoutIfNeeded(
      offset,
      rowsPerImage,
      bytesPerRow,
      changeBeforePass
    );

    const appliedCheckSize = this.arrayCopySizeIfNeeded(
      width,
      height,
      depthOrArrayLayers,
      changeBeforePass
    );

    const encoder = this.device.createCommandEncoder();
    encoder.copyTextureToBuffer(
      appliedCopyView,
      { buffer, ...appliedDataLayout },
      appliedCheckSize
    );

    this.device.queue.submit([encoder.finish()]);
  }

  /** Put data into a part of the texture with an appropriate method. */
  uploadLinearTextureDataToTextureSubBox(
    textureCopyView,
    textureDataLayout,
    copySize,
    partialData,
    method,
    changeBeforePass
  ) {
    const { texture, mipLevel, origin } = textureCopyView;
    const { offset, rowsPerImage, bytesPerRow } = textureDataLayout;
    const { x, y, z } = origin;
    const { width, height, depthOrArrayLayers } = copySize;

    const appliedCopyView = this.undefOrArrayCopyViewIfNeeded(
      texture,
      x,
      y,
      z,
      mipLevel,
      changeBeforePass
    );

    const appliedDataLayout = this.undefDataLayoutIfNeeded(
      offset,
      rowsPerImage,
      bytesPerRow,
      changeBeforePass
    );

    const appliedCopySize = this.arrayCopySizeIfNeeded(
      width,
      height,
      depthOrArrayLayers,
      changeBeforePass
    );

    switch (method) {
      case 'WriteTexture': {
        this.device.queue.writeTexture(
          appliedCopyView,
          partialData,
          appliedDataLayout,
          appliedCopySize
        );

        break;
      }
      case 'CopyB2T': {
        const buffer = this.device.createBuffer({
          mappedAtCreation: true,
          size: align(partialData.byteLength, 4),
          usage: GPUBufferUsage.COPY_SRC,
        });

        new Uint8Array(buffer.getMappedRange()).set(partialData);
        buffer.unmap();

        const encoder = this.device.createCommandEncoder();
        encoder.copyBufferToTexture(
          { buffer, ...appliedDataLayout },
          appliedCopyView,
          appliedCopySize
        );

        this.device.queue.submit([encoder.finish()]);

        break;
      }
      default:
        unreachable();
    }
  }

  /**
   * We check an appropriate part of the texture against the given data.
   * Used directly with PartialCopyT2B check method (for a subpart of the texture)
   * and by `copyWholeTextureToBufferAndCheckContentsWithUpdatedData` with FullCopyT2B check method
   * (for the whole texture). We also ensure that CopyT2B doesn't overwrite bytes it's not supposed
   * to if validateOtherBytesInBuffer is set to true.
   */
  copyPartialTextureToBufferAndCheckContents(
    { texture, mipLevel, origin },
    checkSize,
    format,
    expected,
    expectedDataLayout,
    changeBeforePass = 'none'
  ) {
    // The alignment is necessary because we need to copy and map data from this buffer.
    const bufferSize = align(expected.byteLength, 4);
    // The start value ensures generated data here doesn't match the expected data.
    const bufferData = this.generateData(bufferSize, 17);

    const buffer = this.device.createBuffer({
      mappedAtCreation: true,
      size: bufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    new Uint8Array(buffer.getMappedRange()).set(bufferData);
    buffer.unmap();

    this.copyTextureToBufferWithAppliedArguments(
      buffer,
      expectedDataLayout,
      checkSize,
      { texture, mipLevel, origin },
      changeBeforePass
    );

    this.updateLinearTextureDataSubBox(
      expectedDataLayout,
      expectedDataLayout,
      checkSize,
      origin,
      origin,
      format,
      bufferData,
      expected
    );

    this.expectContents(buffer, bufferData);
  }

  /**
   * Copies the whole texture into linear data stored in a buffer for further checks.
   *
   * Used for `copyWholeTextureToBufferAndCheckContentsWithUpdatedData`.
   */
  copyWholeTextureToNewBuffer({ texture, mipLevel }, resultDataLayout) {
    const { mipSize, byteLength, bytesPerRow, rowsPerImage } = resultDataLayout;
    const buffer = this.device.createBuffer({
      size: align(byteLength, 4), // this is necessary because we need to copy and map data from this buffer
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const encoder = this.device.createCommandEncoder();
    encoder.copyTextureToBuffer(
      { texture, mipLevel },
      { buffer, bytesPerRow, rowsPerImage },
      mipSize
    );

    this.device.queue.submit([encoder.finish()]);

    return buffer;
  }

  copyFromArrayToArray(src, srcOffset, dst, dstOffset, size) {
    dst.set(src.subarray(srcOffset, srcOffset + size), dstOffset);
  }

  /**
   * Takes the data returned by `copyWholeTextureToNewBuffer` and updates it after a copy operation
   * on the texture by emulating the copy behaviour here directly.
   */
  updateLinearTextureDataSubBox(
    destinationDataLayout,
    sourceDataLayout,
    copySize,
    destinationOrigin,
    sourceOrigin,
    format,
    destination,
    source
  ) {
    for (const texel of this.iterateBlockRows(copySize, sourceOrigin, format)) {
      const sourceOffset = this.getTexelOffsetInBytes(
        sourceDataLayout,
        format,
        texel,
        sourceOrigin
      );

      const destinationOffset = this.getTexelOffsetInBytes(
        destinationDataLayout,
        format,
        texel,
        destinationOrigin
      );

      const rowLength = bytesInACompleteRow(copySize.width, format);
      this.copyFromArrayToArray(source, sourceOffset, destination, destinationOffset, rowLength);
    }
  }

  /**
   * Used for checking whether the whole texture was updated correctly by
   * `uploadLinearTextureDataToTextureSubpart`. Takes fullData returned by
   * `copyWholeTextureToNewBuffer` before the copy operation which is the original texture data,
   * then updates it with `updateLinearTextureDataSubpart` and checks the texture against the
   * updated data after the copy operation.
   */
  copyWholeTextureToBufferAndCheckContentsWithUpdatedData(
    { texture, mipLevel, origin },
    fullTextureCopyLayout,
    texturePartialDataLayout,
    copySize,
    format,
    fullData,
    partialData
  ) {
    const { mipSize, bytesPerRow, rowsPerImage, byteLength } = fullTextureCopyLayout;
    const { dst, begin, end } = this.createAlignedCopyForMapRead(fullData, byteLength, 0);

    const destinationOrigin = { x: 0, y: 0, z: 0 };

    // We add an eventual async expectation which will update the full data and then add
    // other eventual async expectations to ensure it will be correct.
    this.eventualAsyncExpectation(async () => {
      await dst.mapAsync(GPUMapMode.READ);
      const actual = new Uint8Array(dst.getMappedRange()).subarray(begin, end);
      this.updateLinearTextureDataSubBox(
        { offset: 0, ...fullTextureCopyLayout },
        texturePartialDataLayout,
        copySize,
        destinationOrigin,
        origin,
        format,
        actual,
        partialData
      );

      this.copyPartialTextureToBufferAndCheckContents(
        { texture, mipLevel, origin: destinationOrigin },
        { width: mipSize[0], height: mipSize[1], depthOrArrayLayers: mipSize[2] },
        format,
        actual,
        { bytesPerRow, rowsPerImage, offset: 0 }
      );

      dst.destroy();
    });
  }

  /**
   * Tests copy between linear data and texture by creating a texture, putting some data into it
   * with WriteTexture/CopyB2T, then getting data for the whole texture/for a part of it back and
   * comparing it with the expectation.
   */
  uploadTextureAndVerifyCopy({
    textureDataLayout,
    copySize,
    dataSize,
    mipLevel = 0,
    origin = { x: 0, y: 0, z: 0 },
    textureSize,
    format,
    dimension = '2d',
    initMethod,
    checkMethod,
    changeBeforePass = 'none',
  }) {
    const texture = this.device.createTexture({
      size: textureSize,
      format,
      dimension,
      mipLevelCount: mipLevel + 1,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const data = this.generateData(dataSize);

    switch (checkMethod) {
      case 'PartialCopyT2B': {
        this.uploadLinearTextureDataToTextureSubBox(
          { texture, mipLevel, origin },
          textureDataLayout,
          copySize,
          data,
          initMethod,
          changeBeforePass
        );

        this.copyPartialTextureToBufferAndCheckContents(
          { texture, mipLevel, origin },
          copySize,
          format,
          data,
          textureDataLayout,
          changeBeforePass
        );

        break;
      }
      case 'FullCopyT2B': {
        const fullTextureCopyLayout = getTextureCopyLayout(format, dimension, textureSize, {
          mipLevel,
        });

        const fullData = this.copyWholeTextureToNewBuffer(
          { texture, mipLevel },
          fullTextureCopyLayout
        );

        this.uploadLinearTextureDataToTextureSubBox(
          { texture, mipLevel, origin },
          textureDataLayout,
          copySize,
          data,
          initMethod,
          changeBeforePass
        );

        this.copyWholeTextureToBufferAndCheckContentsWithUpdatedData(
          { texture, mipLevel, origin },
          fullTextureCopyLayout,
          textureDataLayout,
          copySize,
          format,
          fullData,
          data
        );

        break;
      }
      default:
        unreachable();
    }
  }
}

/**
 * This is a helper function used for filtering test parameters
 *
 * TODO: Modify this after introducing tests with rendering.
 */
function formatCanBeTested({ format }) {
  return kSizedTextureFormatInfo[format].copyDst && kSizedTextureFormatInfo[format].copySrc;
}

export const g = makeTestGroup(ImageCopyTest);

g.test('rowsPerImage_and_bytesPerRow')
  .desc(
    `Test that copying data with various bytesPerRow and rowsPerImage values and minimum required
bytes in copy works for every format.

  Covers a special code path for Metal:
    bufferSize - offset < bytesPerImage * copyExtent.depthOrArrayLayers
  Covers a special code path for D3D12:
    when bytesPerRow is not a multiple of 512 and copyExtent.depthOrArrayLayers > 1: copyExtent.depthOrArrayLayers % 2 == { 0, 1 }
    bytesPerRow == bytesInACompleteCopyImage
  `
  )
  .cases(
    params()
      .combine(kMethodsToTest)
      .combine(poptions('format', kWorkingTextureFormats))
      .filter(formatCanBeTested)
  )
  .subcases(() =>
    params()
      .combine([
        { bytesPerRowPadding: 0, rowsPerImagePadding: 0 }, // no padding
        { bytesPerRowPadding: 0, rowsPerImagePadding: 6 }, // rowsPerImage padding
        { bytesPerRowPadding: 6, rowsPerImagePadding: 0 }, // bytesPerRow padding
        { bytesPerRowPadding: 15, rowsPerImagePadding: 17 }, // both paddings
      ])
      .combine([
        // In the two cases below, for (WriteTexture, PartialCopyB2T) and (CopyB2T, FullCopyT2B)
        // sets of methods we will have bytesPerRow = 256 and copyDepth % 2 == { 0, 1 }
        // respectively. This covers a special code path for D3D12.
        { copyWidthInBlocks: 3, copyHeightInBlocks: 4, copyDepth: 5 }, // standard copy
        { copyWidthInBlocks: 5, copyHeightInBlocks: 4, copyDepth: 2 }, // standard copy

        { copyWidthInBlocks: 256, copyHeightInBlocks: 3, copyDepth: 2 }, // copyWidth is 256-aligned
        { copyWidthInBlocks: 0, copyHeightInBlocks: 4, copyDepth: 5 }, // empty copy because of width
        { copyWidthInBlocks: 3, copyHeightInBlocks: 0, copyDepth: 5 }, // empty copy because of height
        { copyWidthInBlocks: 3, copyHeightInBlocks: 4, copyDepth: 0 }, // empty copy because of depthOrArrayLayers
        { copyWidthInBlocks: 1, copyHeightInBlocks: 3, copyDepth: 5 }, // copyWidth = 1

        // The two cases below cover another special code path for D3D12.
        //   - For (WriteTexture, FullCopyT2B) with r8unorm:
        //         bytesPerRow = 15 = 3 * 5 = bytesInACompleteCopyImage.
        { copyWidthInBlocks: 32, copyHeightInBlocks: 1, copyDepth: 8 }, // copyHeight = 1
        //   - For (CopyB2T, FullCopyT2B) and (WriteTexture, PartialCopyT2B) with r8unorm:
        //         bytesPerRow = 256 = 8 * 32 = bytesInACompleteCopyImage.
        { copyWidthInBlocks: 5, copyHeightInBlocks: 4, copyDepth: 1 }, // copyDepth = 1

        { copyWidthInBlocks: 7, copyHeightInBlocks: 1, copyDepth: 1 }, // copyHeight = 1 and copyDepth = 1
      ])
  )
  .fn(async t => {
    const {
      bytesPerRowPadding,
      rowsPerImagePadding,
      copyWidthInBlocks,
      copyHeightInBlocks,
      copyDepth,
      format,
      initMethod,
      checkMethod,
    } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    // For CopyB2T and CopyT2B we need to have bytesPerRow 256-aligned,
    // to make this happen we align the bytesInACompleteRow value and multiply
    // bytesPerRowPadding by 256.
    const bytesPerRowAlignment =
      initMethod === 'WriteTexture' && checkMethod === 'FullCopyT2B' ? 1 : 256;

    const copyWidth = copyWidthInBlocks * info.blockWidth;
    const copyHeight = copyHeightInBlocks * info.blockHeight;
    const rowsPerImage = copyHeightInBlocks + rowsPerImagePadding;
    const bytesPerRow =
      align(bytesInACompleteRow(copyWidth, format), bytesPerRowAlignment) +
      bytesPerRowPadding * bytesPerRowAlignment;
    const copySize = { width: copyWidth, height: copyHeight, depthOrArrayLayers: copyDepth };

    const dataSize = dataBytesForCopyOrFail({
      layout: { offset: 0, bytesPerRow, rowsPerImage },
      format,
      copySize,
      method: initMethod,
    });

    t.uploadTextureAndVerifyCopy({
      textureDataLayout: { offset: 0, bytesPerRow, rowsPerImage },
      copySize,
      dataSize,
      textureSize: [
        Math.max(copyWidth, info.blockWidth),
        Math.max(copyHeight, info.blockHeight),
        Math.max(copyDepth, 1),
      ],
      /* making sure the texture is non-empty */ format,
      initMethod,
      checkMethod,
    });
  });

g.test('offsets_and_sizes')
  .desc(
    `Test that copying data with various offset values and additional data paddings
works for every format with 2d and 2d-array textures.

  Covers two special code paths for D3D12:
    offset + bytesInCopyExtentPerRow { ==, > } bytesPerRow
    offset > bytesInACompleteCopyImage
`
  )
  .cases(
    params()
      .combine(kMethodsToTest)
      .combine(poptions('format', kWorkingTextureFormats))
      .filter(formatCanBeTested)
  )
  .subcases(
    () =>
      params()
        .combine([
          { offsetInBlocks: 0, dataPaddingInBytes: 0 }, // no offset and no padding
          { offsetInBlocks: 1, dataPaddingInBytes: 0 }, // offset = 1
          { offsetInBlocks: 2, dataPaddingInBytes: 0 }, // offset = 2
          { offsetInBlocks: 15, dataPaddingInBytes: 0 }, // offset = 15
          { offsetInBlocks: 16, dataPaddingInBytes: 0 }, // offset = 16
          { offsetInBlocks: 242, dataPaddingInBytes: 0 }, // for rgba8unorm format: offset + bytesInCopyExtentPerRow = 242 + 12 = 256 = bytesPerRow
          { offsetInBlocks: 243, dataPaddingInBytes: 0 }, // for rgba8unorm format: offset + bytesInCopyExtentPerRow = 243 + 12 > 256 = bytesPerRow
          { offsetInBlocks: 768, dataPaddingInBytes: 0 }, // for copyDepth = 1, blockWidth = 1 and bytesPerBlock = 1: offset = 768 = 3 * 256 = bytesInACompleteCopyImage
          { offsetInBlocks: 769, dataPaddingInBytes: 0 }, // for copyDepth = 1, blockWidth = 1 and bytesPerBlock = 1: offset = 769 > 768 = bytesInACompleteCopyImage
          { offsetInBlocks: 0, dataPaddingInBytes: 1 }, // dataPaddingInBytes > 0
          { offsetInBlocks: 1, dataPaddingInBytes: 8 }, // offset > 0 and dataPaddingInBytes > 0
        ])
        .combine(poptions('copyDepth', [1, 2])) // 2d and 2d-array textures
  )
  .fn(async t => {
    const {
      offsetInBlocks,
      dataPaddingInBytes,
      copyDepth,
      format,
      initMethod,
      checkMethod,
    } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    const offset = offsetInBlocks * info.bytesPerBlock;
    const copySize = {
      width: 3 * info.blockWidth,
      height: 3 * info.blockHeight,
      depthOrArrayLayers: copyDepth,
    };

    const rowsPerImage = 3;
    const bytesPerRow = 256;

    const minDataSize = dataBytesForCopyOrFail({
      layout: { offset, bytesPerRow, rowsPerImage },
      format,
      copySize,
      method: initMethod,
    });

    const dataSize = minDataSize + dataPaddingInBytes;

    // We're copying a (3 x 3 x copyDepth) (in texel blocks) part of a (4 x 4 x copyDepth)
    // (in texel blocks) texture with no origin.
    t.uploadTextureAndVerifyCopy({
      textureDataLayout: { offset, bytesPerRow, rowsPerImage },
      copySize,
      dataSize,
      textureSize: [4 * info.blockWidth, 4 * info.blockHeight, copyDepth],
      format,
      initMethod,
      checkMethod,
    });
  });

g.test('origins_and_extents')
  .desc(
    `Test that copying slices of a texture works with various origin and copyExtent values
for all formats. We pass origin and copyExtent as [number, number, number].`
  )
  .cases(
    params()
      .combine(kMethodsToTest)
      .combine(poptions('format', kWorkingTextureFormats))
      .filter(formatCanBeTested)
  )
  .subcases(() =>
    params()
      .combine(poptions('originValueInBlocks', [0, 7, 8]))
      .combine(poptions('copySizeValueInBlocks', [0, 7, 8]))
      .combine(poptions('textureSizePaddingValueInBlocks', [0, 7, 8]))
      .unless(
        p =>
          // we can't create an empty texture
          p.copySizeValueInBlocks + p.originValueInBlocks + p.textureSizePaddingValueInBlocks === 0
      )
      .combine(poptions('coordinateToTest', [0, 1, 2]))
  )
  .fn(async t => {
    const {
      originValueInBlocks,
      copySizeValueInBlocks,
      textureSizePaddingValueInBlocks,
      format,
      initMethod,
      checkMethod,
    } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    const originBlocks = [1, 1, 1];
    const copySizeBlocks = [2, 2, 2];
    const texSizeBlocks = [3, 3, 3];

    {
      const ctt = t.params.coordinateToTest;
      originBlocks[ctt] = originValueInBlocks;
      copySizeBlocks[ctt] = copySizeValueInBlocks;
      texSizeBlocks[ctt] =
        originBlocks[ctt] + copySizeBlocks[ctt] + textureSizePaddingValueInBlocks;
    }

    const origin = {
      x: originBlocks[0] * info.blockWidth,
      y: originBlocks[1] * info.blockHeight,
      z: originBlocks[2],
    };

    const copySize = {
      width: copySizeBlocks[0] * info.blockWidth,
      height: copySizeBlocks[1] * info.blockHeight,
      depthOrArrayLayers: copySizeBlocks[2],
    };

    const textureSize = [
      texSizeBlocks[0] * info.blockWidth,
      texSizeBlocks[1] * info.blockHeight,
      texSizeBlocks[2],
    ];

    const rowsPerImage = copySizeBlocks[1];
    const bytesPerRow = align(copySizeBlocks[0] * info.bytesPerBlock, 256);

    const dataSize = dataBytesForCopyOrFail({
      layout: { offset: 0, bytesPerRow, rowsPerImage },
      format,
      copySize,
      method: initMethod,
    });

    // For testing width: we copy a (_ x 2 x 2) (in texel blocks) part of a (_ x 3 x 3)
    // (in texel blocks) texture with origin (_, 1, 1) (in texel blocks).
    // Similarly for other coordinates.
    t.uploadTextureAndVerifyCopy({
      textureDataLayout: { offset: 0, bytesPerRow, rowsPerImage },
      copySize,
      dataSize,
      origin,
      textureSize,
      format,
      initMethod,
      checkMethod,
      changeBeforePass: 'arrays',
    });
  });

/**
 * Generates textureSizes which correspond to the same physicalSizeAtMipLevel including virtual
 * sizes at mip level different from the physical ones.
 */
function* generateTestTextureSizes({ format, mipLevel, _mipSizeInBlocks }) {
  const info = kSizedTextureFormatInfo[format];

  const widthAtThisLevel = _mipSizeInBlocks.width * info.blockWidth;
  const heightAtThisLevel = _mipSizeInBlocks.height * info.blockHeight;
  const textureSize = [
    widthAtThisLevel << mipLevel,
    heightAtThisLevel << mipLevel,
    _mipSizeInBlocks.depthOrArrayLayers,
  ];

  yield {
    textureSize,
  };

  // We choose width and height of the texture so that the values are divisible by blockWidth and
  // blockHeight respectively and so that the virtual size at mip level corresponds to the same
  // physical size.
  // Virtual size at mip level with modified width has width = (physical size width) - (blockWidth / 2).
  // Virtual size at mip level with modified height has height = (physical size height) - (blockHeight / 2).
  const widthAtPrevLevel = widthAtThisLevel << 1;
  const heightAtPrevLevel = heightAtThisLevel << 1;
  assert(mipLevel > 0);
  assert(widthAtPrevLevel >= info.blockWidth && heightAtPrevLevel >= info.blockHeight);
  const modifiedWidth = (widthAtPrevLevel - info.blockWidth) << (mipLevel - 1);
  const modifiedHeight = (heightAtPrevLevel - info.blockHeight) << (mipLevel - 1);

  const modifyWidth = info.blockWidth > 1 && modifiedWidth !== textureSize[0];
  const modifyHeight = info.blockHeight > 1 && modifiedHeight !== textureSize[1];

  if (modifyWidth) {
    yield {
      textureSize: [modifiedWidth, textureSize[1], textureSize[2]],
    };
  }
  if (modifyHeight) {
    yield {
      textureSize: [textureSize[0], modifiedHeight, textureSize[2]],
    };
  }
  if (modifyWidth && modifyHeight) {
    yield {
      textureSize: [modifiedWidth, modifiedHeight, textureSize[2]],
    };
  }
}

g.test('mip_levels')
  .desc(
    `Test that copying various mip levels works. Covers two special code paths:
  - The physical size of the subresource is not equal to the logical size.
  - bufferSize - offset < bytesPerImage * copyExtent.depthOrArrayLayers, and copyExtent needs to be clamped for all block formats.
  `
  )
  .cases(
    params()
      .combine(kMethodsToTest)
      .combine(poptions('format', kWorkingTextureFormats))
      .filter(formatCanBeTested)
  )
  .subcases(p =>
    params()
      .combine([
        // origin + copySize = texturePhysicalSizeAtMipLevel for all coordinates, 2d texture */
        {
          copySizeInBlocks: { width: 5, height: 4, depthOrArrayLayers: 1 },
          originInBlocks: { x: 3, y: 2, z: 0 },
          _mipSizeInBlocks: { width: 8, height: 6, depthOrArrayLayers: 1 },
          mipLevel: 1,
        },

        // origin + copySize = texturePhysicalSizeAtMipLevel for all coordinates, 2d-array texture
        {
          copySizeInBlocks: { width: 5, height: 4, depthOrArrayLayers: 2 },
          originInBlocks: { x: 3, y: 2, z: 1 },
          _mipSizeInBlocks: { width: 8, height: 6, depthOrArrayLayers: 3 },
          mipLevel: 2,
        },

        // origin.x + copySize.width = texturePhysicalSizeAtMipLevel.width
        {
          copySizeInBlocks: { width: 5, height: 4, depthOrArrayLayers: 2 },
          originInBlocks: { x: 3, y: 2, z: 1 },
          _mipSizeInBlocks: { width: 8, height: 7, depthOrArrayLayers: 4 },
          mipLevel: 3,
        },

        // origin.y + copySize.height = texturePhysicalSizeAtMipLevel.height
        {
          copySizeInBlocks: { width: 5, height: 4, depthOrArrayLayers: 2 },
          originInBlocks: { x: 3, y: 2, z: 1 },
          _mipSizeInBlocks: { width: 9, height: 6, depthOrArrayLayers: 4 },
          mipLevel: 4,
        },

        // origin.z + copySize.depthOrArrayLayers = texturePhysicalSizeAtMipLevel.depthOrArrayLayers
        {
          copySizeInBlocks: { width: 5, height: 4, depthOrArrayLayers: 2 },
          originInBlocks: { x: 3, y: 2, z: 1 },
          _mipSizeInBlocks: { width: 9, height: 7, depthOrArrayLayers: 3 },
          mipLevel: 5,
        },

        // origin + copySize < texturePhysicalSizeAtMipLevel for all coordinates
        {
          copySizeInBlocks: { width: 5, height: 4, depthOrArrayLayers: 2 },
          originInBlocks: { x: 3, y: 2, z: 1 },
          _mipSizeInBlocks: { width: 9, height: 7, depthOrArrayLayers: 4 },
          mipLevel: 6,
        },
      ])
      .expand(({ mipLevel, _mipSizeInBlocks }) =>
        generateTestTextureSizes({ mipLevel, _mipSizeInBlocks, format: p.format })
      )
  )
  .fn(async t => {
    const {
      copySizeInBlocks,
      originInBlocks,
      textureSize,
      mipLevel,
      format,
      initMethod,
      checkMethod,
    } = t.params;
    const info = kSizedTextureFormatInfo[format];
    await t.selectDeviceOrSkipTestCase(info.extension);

    const origin = {
      x: originInBlocks.x * info.blockWidth,
      y: originInBlocks.y * info.blockHeight,
      z: originInBlocks.z,
    };

    const copySize = {
      width: copySizeInBlocks.width * info.blockWidth,
      height: copySizeInBlocks.height * info.blockHeight,
      depthOrArrayLayers: copySizeInBlocks.depthOrArrayLayers,
    };

    const rowsPerImage = copySizeInBlocks.height + 1;
    const bytesPerRow = align(copySize.width, 256);

    const dataSize = dataBytesForCopyOrFail({
      layout: { offset: 0, bytesPerRow, rowsPerImage },
      format,
      copySize,
      method: initMethod,
    });

    t.uploadTextureAndVerifyCopy({
      textureDataLayout: { offset: 0, bytesPerRow, rowsPerImage },
      copySize,
      dataSize,
      origin,
      mipLevel,
      textureSize,
      format,
      initMethod,
      checkMethod,
    });
  });

const UND = undefined;
g.test('undefined_params')
  .desc(
    `Tests undefined values of bytesPerRow, rowsPerImage, and origin.x/y/z.
  Ensures bytesPerRow/rowsPerImage=undefined are valid and behave as expected.
  Ensures origin.x/y/z undefined default to 0.`
  )
  .cases(kMethodsToTest)
  .subcases(() =>
    params().combine([
      // copying one row: bytesPerRow and rowsPerImage can be undefined
      { copySize: [3, 1, 1], origin: [UND, UND, UND], bytesPerRow: UND, rowsPerImage: UND },
      // copying one slice: rowsPerImage can be undefined
      { copySize: [3, 3, 1], origin: [UND, UND, UND], bytesPerRow: 256, rowsPerImage: UND },
      // copying two slices
      { copySize: [3, 3, 2], origin: [UND, UND, UND], bytesPerRow: 256, rowsPerImage: 3 },
      // origin.x = undefined
      { copySize: [1, 1, 1], origin: [UND, 1, 1], bytesPerRow: UND, rowsPerImage: UND },
      // origin.y = undefined
      { copySize: [1, 1, 1], origin: [1, UND, 1], bytesPerRow: UND, rowsPerImage: UND },
      // origin.z = undefined
      { copySize: [1, 1, 1], origin: [1, 1, UND], bytesPerRow: UND, rowsPerImage: UND },
    ])
  )
  .fn(async t => {
    const { bytesPerRow, rowsPerImage, copySize, origin, initMethod, checkMethod } = t.params;

    t.uploadTextureAndVerifyCopy({
      textureDataLayout: {
        offset: 0,
        // Zero will get turned back into undefined later.
        bytesPerRow: bytesPerRow ?? 0,
        // Zero will get turned back into undefined later.
        rowsPerImage: rowsPerImage ?? 0,
      },

      copySize: { width: copySize[0], height: copySize[1], depthOrArrayLayers: copySize[2] },
      dataSize: 2000,
      textureSize: [100, 3, 2],
      // Zeros will get turned back into undefined later.
      origin: { x: origin[0] ?? 0, y: origin[1] ?? 0, z: origin[2] ?? 0 },
      format: 'rgba8unorm',
      initMethod,
      checkMethod,
      changeBeforePass: 'undefined',
    });
  });
