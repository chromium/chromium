/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
API validation test for debug groups and markers

Test Coverage:
  - For each encoder type (GPUCommandEncoder, GPUComputeEncoder, GPURenderPassEncoder,
  GPURenderBundleEncoder):
    - Test that all pushDebugGroup must have a corresponding popDebugGroup
      - Push and pop counts of 0, 1, and 2 will be used.
      - An error must be generated for non matching counts.
    - Test calling pushDebugGroup with empty and non-empty strings.
    - Test inserting a debug marker with empty and non-empty strings.
`;
import { poptions, params } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest, kEncoderTypes } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('debug_group_balanced')
  .params(
    params()
      .combine(poptions('encoderType', kEncoderTypes))
      .combine(poptions('pushCount', [0, 1, 2]))
      .combine(poptions('popCount', [0, 1, 2]))
  )
  .fn(t => {
    const { encoder, finish } = t.createEncoder(t.params.encoderType);
    for (let i = 0; i < t.params.pushCount; ++i) {
      encoder.pushDebugGroup(`${i}`);
    }
    for (let i = 0; i < t.params.popCount; ++i) {
      encoder.popDebugGroup();
    }
    const shouldError = t.params.popCount !== t.params.pushCount;
    t.expectValidationError(() => {
      const commandBuffer = finish();
      t.queue.submit([commandBuffer]);
    }, shouldError);
  });

g.test('debug_group')
  .params(
    params()
      .combine(poptions('encoderType', kEncoderTypes))
      .combine(poptions('label', ['', 'group']))
  )
  .fn(t => {
    const { encoder, finish } = t.createEncoder(t.params.encoderType);
    encoder.pushDebugGroup(t.params.label);
    encoder.popDebugGroup();
    const commandBuffer = finish();
    t.queue.submit([commandBuffer]);
  });

g.test('debug_marker')
  .params(
    params()
      .combine(poptions('encoderType', kEncoderTypes))
      .combine(poptions('label', ['', 'marker']))
  )
  .fn(t => {
    const maker = t.createEncoder(t.params.encoderType);
    maker.encoder.insertDebugMarker(t.params.label);
    const commandBuffer = maker.finish();
    t.queue.submit([commandBuffer]);
  });
