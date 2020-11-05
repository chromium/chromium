/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
API validation test for compute pass
`;
import { params, poptions } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';

import { ValidationTest } from './../../validation_test.js';

class F extends ValidationTest {
  createComputePipeline(state) {
    if (state === 'valid') {
      return this.createNoOpComputePipeline();
    }

    return this.createErrorComputePipeline();
  }

  createIndirectBuffer(state, data) {
    const descriptor = {
      size: data.byteLength,
      usage: GPUBufferUsage.INDIRECT | GPUBufferUsage.COPY_DST,
    };

    if (state === 'invalid') {
      descriptor.usage = 0xffff; // Invalid GPUBufferUsage
    }

    this.device.pushErrorScope('validation');
    const buffer = this.device.createBuffer(descriptor);
    this.device.popErrorScope();

    if (state === 'valid') {
      this.queue.writeBuffer(buffer, 0, data);
    }

    if (state === 'destroyed') {
      buffer.destroy();
    }

    return buffer;
  }
}

export const g = makeTestGroup(F);

g.test('set_pipeline')
  .desc(
    `
setPipeline should generate an error iff using an 'invalid' pipeline.
`
  )
  .params(poptions('state', ['valid', 'invalid']))
  .fn(t => {
    const pipeline = t.createComputePipeline(t.params.state);
    const { encoder, finish } = t.createEncoder('compute pass');
    encoder.setPipeline(pipeline);
    t.expectValidationError(() => {
      finish();
    }, t.params.state === 'invalid');
  });

g.test('dispatch_sizes')
  .desc(
    `
Test 'direct' and 'indirect' dispatch with various sizes.
  - workgroup sizes:
    - valid, {[0, 0, 0], [1, 1, 1]}
    - invalid,  <fill number here>
`
  )
  .params(
    params()
      .combine(poptions('dispatchType', ['direct', 'indirect']))
      .combine(
        poptions('workSizes', [
          [0, 0, 0],
          [1, 1, 1],
          // TODO: Add tests for workSizes right under and above upper limit once the limit has
          //  been decided.
        ])
      )
  )
  .fn(t => {
    const pipeline = t.createNoOpComputePipeline();
    const [x, y, z] = t.params.workSizes;
    const { encoder, finish } = t.createEncoder('compute pass');
    encoder.setPipeline(pipeline);
    if (t.params.dispatchType === 'direct') {
      encoder.dispatch(x, y, z);
    } else if (t.params.dispatchType === 'indirect') {
      encoder.dispatchIndirect(t.createIndirectBuffer('valid', new Uint32Array([x, y, z])), 0);
    }
    t.queue.submit([finish()]);
  });

const kBufferData = new Uint32Array(6).fill(1);
g.test('indirect_dispatch_buffer')
  .desc(
    `
Test dispatchIndirect validation by submitting various dispatches with a no-op pipeline and an
indirectBuffer with 6 elements.
- indirectBuffer: {'valid', 'invalid', 'destroyed'}
- indirectOffset:
  - valid, within the buffer: {beginning, middle, end} of the buffer
  - invalid, non-multiple of 4
  - invalid, the last element is outside the buffer
`
  )
  .params(
    params()
      .combine(poptions('state', ['valid', 'invalid', 'destroyed']))
      .combine(
        poptions('offset', [
          0, // valid for 'valid' buffers
          Uint32Array.BYTES_PER_ELEMENT, // valid for 'valid' buffers
          kBufferData.byteLength - 3 * Uint32Array.BYTES_PER_ELEMENT, // valid for 'valid' buffers
          1, // invalid, non-multiple of 4 offset
          // invalid, last element outside buffer
          kBufferData.byteLength - Uint32Array.BYTES_PER_ELEMENT,
        ])
      )
  )
  .fn(t => {
    const { state, offset } = t.params;
    const pipeline = t.createNoOpComputePipeline();
    const buffer = t.createIndirectBuffer(state, kBufferData);
    const { encoder, finish } = t.createEncoder('compute pass');
    encoder.setPipeline(pipeline);
    t.expectValidationError(() => {
      encoder.dispatchIndirect(buffer, offset);
      t.queue.submit([finish()]);
    }, state !== 'valid' || offset % 4 !== 0 || offset + 3 * Uint32Array.BYTES_PER_ELEMENT > kBufferData.byteLength);
  });
