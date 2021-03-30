/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
copyTextureToBuffer and copyBufferToTexture validation tests not covered by
the general image_copy tests, or by destroyed,*.

TODO:
- Move all the tests here to image_copy/ and test writeTexture() with depth/stencil formats.
`;
import { poptions, params } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { assert } from '../../../../../common/framework/util/util.js';
import {
  kDepthStencilFormats,
  depthStencilBufferTextureCopySupported,
  depthStencilFormatAspectSize,
} from '../../../../capability_info.js';
import { align } from '../../../../util/math.js';
import { kBufferCopyAlignment, kBytesPerRowAlignment } from '../../../../util/texture/layout.js';
import { ValidationTest } from '../../validation_test.js';

class ImageCopyTest extends ValidationTest {
  testCopyBufferToTexture(source, destination, copySize, isSuccess) {
    const encoder = this.device.createCommandEncoder();
    encoder.copyBufferToTexture(source, destination, copySize);
    this.expectValidationError(() => {
      this.device.queue.submit([encoder.finish()]);
    }, !isSuccess);
  }

  testCopyTextureToBuffer(source, destination, copySize, isSuccess) {
    const encoder = this.device.createCommandEncoder();
    encoder.copyTextureToBuffer(source, destination, copySize);
    this.expectValidationError(() => {
      this.device.queue.submit([encoder.finish()]);
    }, !isSuccess);
  }
}

export const g = makeTestGroup(ImageCopyTest);

g.test('depth_stencil_format,copy_usage_and_aspect')
  .desc(
    `
  Validate the combination of usage and aspect of each depth stencil format in copyBufferToTexture
  and copyTextureToBuffer. See https://gpuweb.github.io/gpuweb/#depth-formats for more details.
`
  )
  .cases(params().combine(poptions('format', kDepthStencilFormats)))
  .subcases(() => params().combine(poptions('aspect', ['all', 'depth-only', 'stencil-only'])))
  .fn(async t => {
    const { format, aspect } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const textureSize = { width: 1, height: 1, depthOrArrayLayers: 1 };
    const texture = t.device.createTexture({
      size: textureSize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const buffer = t.device.createBuffer({
      size: 32,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    {
      const success = depthStencilBufferTextureCopySupported('CopyB2T', format, aspect);
      t.testCopyBufferToTexture({ buffer }, { texture, aspect }, textureSize, success);
    }

    {
      const success = depthStencilBufferTextureCopySupported('CopyT2B', format, aspect);
      t.testCopyTextureToBuffer({ texture, aspect }, { buffer }, textureSize, success);
    }
  });

g.test('depth_stencil_format,copy_buffer_size')
  .desc(
    `
  Validate the minimum buffer size for each depth stencil format in copyBufferToTexture
  and copyTextureToBuffer.

  Given a depth stencil format, a copy aspect ('depth-only' or 'stencil-only'), the copy method
  (buffer-to-texture or texture-to-buffer) and the copy size, validate
  - if the copy can be successfully executed with the minimum required buffer size.
  - if the copy fails with a validation error when the buffer size is less than the minimum
  required buffer size.
`
  )
  .cases(
    params()
      .combine(poptions('format', kDepthStencilFormats))
      .combine(poptions('aspect', ['depth-only', 'stencil-only']))
      .combine(poptions('copyType', ['CopyB2T', 'CopyT2B']))
      .filter(param =>
        depthStencilBufferTextureCopySupported(param.copyType, param.format, param.aspect)
      )
  )
  .subcases(() =>
    params().combine(
      poptions('copySize', [
        { width: 8, height: 1, depthOrArrayLayers: 1 },
        { width: 4, height: 4, depthOrArrayLayers: 1 },
        { width: 4, height: 4, depthOrArrayLayers: 3 },
      ])
    )
  )
  .fn(async t => {
    const { format, aspect, copyType, copySize } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const texture = t.device.createTexture({
      size: copySize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const texelAspectSize = depthStencilFormatAspectSize(format, aspect);
    assert(texelAspectSize > 0);

    const bytesPerRow = align(texelAspectSize * copySize.width, kBytesPerRowAlignment);
    const rowsPerImage = copySize.height;
    const minimumBufferSize =
      bytesPerRow * (rowsPerImage * copySize.depthOrArrayLayers - 1) +
      align(texelAspectSize * copySize.width, kBufferCopyAlignment);
    assert(minimumBufferSize > kBufferCopyAlignment);

    const bigEnoughBuffer = t.device.createBuffer({
      size: minimumBufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const smallerBuffer = t.device.createBuffer({
      size: minimumBufferSize - kBufferCopyAlignment,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    if (copyType === 'CopyB2T') {
      t.testCopyBufferToTexture(
        { buffer: bigEnoughBuffer, bytesPerRow, rowsPerImage },
        { texture, aspect },
        copySize,
        true
      );

      t.testCopyBufferToTexture(
        { buffer: smallerBuffer, bytesPerRow, rowsPerImage },
        { texture, aspect },
        copySize,
        false
      );
    } else {
      assert(copyType === 'CopyT2B');
      t.testCopyTextureToBuffer(
        { texture, aspect },
        { buffer: bigEnoughBuffer, bytesPerRow, rowsPerImage },
        copySize,
        true
      );

      t.testCopyTextureToBuffer(
        { texture, aspect },
        { buffer: smallerBuffer, bytesPerRow, rowsPerImage },
        copySize,
        false
      );
    }
  });

g.test('depth_stencil_format,copy_buffer_offset')
  .desc(
    `
    Validate for every depth stencil formats the buffer offset must be a multiple of 4 in
    copyBufferToTexture() and copyTextureToBuffer().
    `
  )
  .cases(
    params()
      .combine(poptions('format', kDepthStencilFormats))
      .combine(poptions('aspect', ['depth-only', 'stencil-only']))
      .combine(poptions('copyType', ['CopyB2T', 'CopyT2B']))
      .filter(param =>
        depthStencilBufferTextureCopySupported(param.copyType, param.format, param.aspect)
      )
  )
  .subcases(() => poptions('offset', [1, 2, 4, 6, 8]))
  .fn(async t => {
    const { format, aspect, copyType, offset } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const textureSize = { width: 4, height: 4, depthOrArrayLayers: 1 };

    const texture = t.device.createTexture({
      size: textureSize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const texelAspectSize = depthStencilFormatAspectSize(format, aspect);
    assert(texelAspectSize > 0);

    const bytesPerRow = align(texelAspectSize * textureSize.width, kBytesPerRowAlignment);
    const rowsPerImage = textureSize.height;
    const minimumBufferSize =
      bytesPerRow * (rowsPerImage * textureSize.depthOrArrayLayers - 1) +
      align(texelAspectSize * textureSize.width, kBufferCopyAlignment);
    assert(minimumBufferSize > kBufferCopyAlignment);

    const buffer = t.device.createBuffer({
      size: align(minimumBufferSize + offset, kBufferCopyAlignment),
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const isSuccess = offset % 4 === 0;

    if (copyType === 'CopyB2T') {
      t.testCopyBufferToTexture(
        { buffer, offset, bytesPerRow, rowsPerImage },
        { texture, aspect },
        textureSize,
        isSuccess
      );
    } else {
      assert(copyType === 'CopyT2B');
      t.testCopyTextureToBuffer(
        { texture, aspect },
        { buffer, offset, bytesPerRow, rowsPerImage },
        textureSize,
        isSuccess
      );
    }
  });
