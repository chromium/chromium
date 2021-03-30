/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { standardizeExtent3D } from '../../util/unions.js';
export function maxMipLevelCount({ size, dimension = '2d' }) {
  const sizeDict = standardizeExtent3D(size);

  let maxMippedDimension = sizeDict.width;
  if (dimension !== '1d') maxMippedDimension = Math.max(maxMippedDimension, sizeDict.height);
  if (dimension === '3d')
    maxMippedDimension = Math.max(maxMippedDimension, sizeDict.depthOrArrayLayers);
  return Math.floor(Math.log2(maxMippedDimension)) + 1;
}
