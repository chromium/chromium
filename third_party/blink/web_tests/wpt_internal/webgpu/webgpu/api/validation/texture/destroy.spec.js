/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Destroying a texture more than once is allowed.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('base')
  .desc(`Test that it is valid to destroy a texture.`)
  .fn(t => {
    const texture = t.getSampledTexture();
    texture.destroy();
  });

g.test('twice')
  .desc(`Test that it is valid to destroy a destroyed texture.`)
  .fn(t => {
    const texture = t.getSampledTexture();
    texture.destroy();
    texture.destroy();
  });

g.test('submit_a_destroyed_texture')
  .desc(
    `Test that it is invalid to submit with a texture that was destroyed {before, after} encoding finishes.`
  )
  .params([
    { destroyBeforeEncode: false, destroyAfterEncode: false, _success: true },
    { destroyBeforeEncode: true, destroyAfterEncode: false, _success: false },
    { destroyBeforeEncode: false, destroyAfterEncode: true, _success: false },
  ])
  .fn(async t => {
    const { destroyBeforeEncode, destroyAfterEncode, _success } = t.params;

    const texture = t.getRenderTexture();
    const textureView = texture.createView();

    if (destroyBeforeEncode) {
      texture.destroy();
    }

    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: textureView,
          loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
        },
      ],
    });

    renderPass.endPass();
    const commandBuffer = commandEncoder.finish();

    if (destroyAfterEncode) {
      texture.destroy();
    }

    t.expectValidationError(() => {
      t.queue.submit([commandBuffer]);
    }, !_success);
  });
