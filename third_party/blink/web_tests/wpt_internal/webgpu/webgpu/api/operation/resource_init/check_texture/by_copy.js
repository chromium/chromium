/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../../../../common/framework/util/util.js';
import { kEncodableTextureFormatInfo } from '../../../../capability_info.js';

export const checkContentsByBufferCopy = (t, params, texture, state, subresourceRange) => {
  for (const { level: mipLevel, slice } of subresourceRange.each()) {
    assert(params.dimension === '2d');
    assert(params.format in kEncodableTextureFormatInfo);
    const format = params.format;

    t.expectSingleColor(texture, format, {
      size: [t.textureWidth, t.textureHeight, 1],
      dimension: params.dimension,
      slice,
      layout: { mipLevel },
      exp: t.stateToTexelComponents[state],
    });
  }
};

export const checkContentsByTextureCopy = (t, params, texture, state, subresourceRange) => {
  for (const { level, slice } of subresourceRange.each()) {
    assert(params.dimension === '2d');
    assert(params.format in kEncodableTextureFormatInfo);
    const format = params.format;

    const width = t.textureWidth >> level;
    const height = t.textureHeight >> level;

    const dst = t.device.createTexture({
      size: [width, height, 1],
      format: params.format,
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
    });

    const commandEncoder = t.device.createCommandEncoder();
    commandEncoder.copyTextureToTexture(
      { texture, mipLevel: level, origin: { x: 0, y: 0, z: slice } },
      { texture: dst, mipLevel: 0 },
      { width, height, depth: 1 }
    );

    t.queue.submit([commandEncoder.finish()]);

    t.expectSingleColor(dst, format, {
      size: [width, height, 1],
      exp: t.stateToTexelComponents[state],
    });
  }
};
