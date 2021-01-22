/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../../common/framework/util/util.js';
import { kAllTextureFormatInfo } from '../../capability_info.js';
import { align } from '../../util/math.js';

function endOfRange(r) {
  return 'count' in r ? r.begin + r.count : r.end;
}

function* rangeAsIterator(r) {
  for (let i = r.begin; i < endOfRange(r); ++i) {
    yield i;
  }
}

export class SubresourceRange {
  constructor(subresources) {
    this.mipRange = {
      begin: subresources.mipRange.begin,
      end: endOfRange(subresources.mipRange),
    };

    this.sliceRange = {
      begin: subresources.sliceRange.begin,
      end: endOfRange(subresources.sliceRange),
    };
  }

  *each() {
    for (let level = this.mipRange.begin; level < this.mipRange.end; ++level) {
      for (let slice = this.sliceRange.begin; slice < this.sliceRange.end; ++slice) {
        yield { level, slice };
      }
    }
  }

  *mipLevels() {
    for (let level = this.mipRange.begin; level < this.mipRange.end; ++level) {
      yield {
        level,
        slices: rangeAsIterator(this.sliceRange),
      };
    }
  }
}

export function mipSize(size, level) {
  const rShiftMax1 = s => Math.max(s >> level, 1);
  if (size instanceof Array) {
    return size.map(rShiftMax1);
  } else {
    return {
      width: rShiftMax1(size.width),
      height: rShiftMax1(size.height),
      depth: rShiftMax1(size.depth),
    };
  }
}

// TODO(jiawei.shao@intel.com): support 1D and 3D textures
export function physicalMipSize(size, format, dimension, level) {
  assert(dimension === '2d');
  assert(Math.max(size.width, size.height) >> level > 0);

  const virtualWidthAtLevel = Math.max(size.width >> level, 1);
  const virtualHeightAtLevel = Math.max(size.height >> level, 1);
  const physicalWidthAtLevel = align(virtualWidthAtLevel, kAllTextureFormatInfo[format].blockWidth);
  const physicalHeightAtLevel = align(
    virtualHeightAtLevel,
    kAllTextureFormatInfo[format].blockHeight
  );

  return { width: physicalWidthAtLevel, height: physicalHeightAtLevel, depth: size.depth };
}
