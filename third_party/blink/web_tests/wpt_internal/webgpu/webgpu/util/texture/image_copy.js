/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../../common/framework/util/util.js';
import { kSizedTextureFormatInfo } from '../../capability_info.js';
import { align } from '../math.js';
import { standardizeExtent3D } from '../unions.js';

export const kImageCopyTypes = ['WriteTexture', 'CopyB2T', 'CopyT2B'];

export function bytesInACompleteRow(copyWidth, format) {
  const info = kSizedTextureFormatInfo[format];
  assert(copyWidth % info.blockWidth === 0);
  return (info.bytesPerBlock * copyWidth) / info.blockWidth;
}

function validateBytesPerRow({ bytesPerRow, bytesInLastRow, sizeInBlocks }) {
  // If specified, layout.bytesPerRow must be greater than or equal to bytesInLastRow.
  if (bytesPerRow !== undefined && bytesPerRow < bytesInLastRow) {
    return false;
  }
  // If heightInBlocks > 1, layout.bytesPerRow must be specified.
  // If copyExtent.depthOrArrayLayers > 1, layout.bytesPerRow and layout.rowsPerImage must be specified.
  if (
    bytesPerRow === undefined &&
    (sizeInBlocks.height > 1 || sizeInBlocks.depthOrArrayLayers > 1)
  ) {
    return false;
  }
  return true;
}

function validateRowsPerImage({ rowsPerImage, sizeInBlocks }) {
  // If specified, layout.rowsPerImage must be greater than or equal to heightInBlocks.
  if (rowsPerImage !== undefined && rowsPerImage < sizeInBlocks.height) {
    return false;
  }
  // If copyExtent.depthOrArrayLayers > 1, layout.bytesPerRow and layout.rowsPerImage must be specified.
  if (rowsPerImage === undefined && sizeInBlocks.depthOrArrayLayers > 1) {
    return false;
  }
  return true;
}

/**
 * Validate a copy and compute the number of bytes it needs. Throws if the copy is invalid.
 */
export function dataBytesForCopyOrFail(args) {
  const { minDataSizeOrOverestimate, copyValid } = dataBytesForCopyOrOverestimate(args);
  assert(copyValid, 'copy was invalid');
  return minDataSizeOrOverestimate;
}

/**
 * Validate a copy and compute the number of bytes it needs. If the copy is invalid, attempts to
 * "conservatively guess" (overestimate) the number of bytes that could be needed for a copy, even
 * if the copy parameters turn out to be invalid. This hopes to avoid "buffer too small" validation
 * errors when attempting to test other validation errors.
 */
export function dataBytesForCopyOrOverestimate({ layout, format, copySize: copySize_, method }) {
  const copyExtent = standardizeExtent3D(copySize_);

  const info = kSizedTextureFormatInfo[format];
  assert(copyExtent.width % info.blockWidth === 0);
  assert(copyExtent.height % info.blockHeight === 0);
  const sizeInBlocks = {
    width: copyExtent.width / info.blockWidth,
    height: copyExtent.height / info.blockHeight,
    depthOrArrayLayers: copyExtent.depthOrArrayLayers,
  };

  const bytesInLastRow = sizeInBlocks.width * info.bytesPerBlock;

  let valid = true;
  const offset = layout.offset ?? 0;
  if (method !== 'WriteTexture') {
    if (offset % info.bytesPerBlock !== 0) valid = false;
    if (layout.bytesPerRow && layout.bytesPerRow % 256 !== 0) valid = false;
  }

  let requiredBytesInCopy = 0;
  {
    let { bytesPerRow, rowsPerImage } = layout;

    // If bytesPerRow or rowsPerImage is invalid, guess a value for the sake of various tests that
    // don't actually care about the exact value.
    // (In particular for validation tests that want to test invalid bytesPerRow or rowsPerImage but
    // need to make sure the total buffer size is still big enough.)
    if (!validateBytesPerRow({ bytesPerRow, bytesInLastRow, sizeInBlocks })) {
      bytesPerRow = undefined;
      valid = false;
    }
    if (!validateRowsPerImage({ rowsPerImage, sizeInBlocks })) {
      rowsPerImage = undefined;
      valid = false;
    }
    // Pick values for cases when (a) bpr/rpi was invalid or (b) they're validly undefined.
    bytesPerRow ??= align(info.bytesPerBlock * sizeInBlocks.width, 256);
    rowsPerImage ??= sizeInBlocks.height;

    if (copyExtent.depthOrArrayLayers > 1) {
      const bytesPerImage = bytesPerRow * rowsPerImage;
      const bytesBeforeLastImage = bytesPerImage * (copyExtent.depthOrArrayLayers - 1);
      requiredBytesInCopy += bytesBeforeLastImage;
    }
    if (copyExtent.depthOrArrayLayers > 0) {
      if (sizeInBlocks.height > 1) requiredBytesInCopy += bytesPerRow * (sizeInBlocks.height - 1);
      if (sizeInBlocks.height > 0) requiredBytesInCopy += bytesInLastRow;
    }
  }

  return { minDataSizeOrOverestimate: offset + requiredBytesInCopy, copyValid: valid };
}
