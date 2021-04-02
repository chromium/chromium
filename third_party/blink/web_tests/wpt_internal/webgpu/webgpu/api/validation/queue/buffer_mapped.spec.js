/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for map-state of mappable buffers used in submitted command buffers.

- x= just before queue op, buffer in {BufferMapStatesToTest}
- x= in every possible place for mappable buffer:
  {submit, writeBuffer, copyB2B {src,dst}, copyB2T, copyT2B, ..?}

TODO: generalize existing test
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('submit').fn(async t => {
  const buffer = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.MAP_WRITE | GPUBufferUsage.COPY_SRC,
  });

  const targetBuffer = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_DST,
  });

  const getCommandBuffer = () => {
    const commandEncoder = t.device.createCommandEncoder();
    commandEncoder.copyBufferToBuffer(buffer, 0, targetBuffer, 0, 4);
    return commandEncoder.finish();
  };

  // Submitting when the buffer has never been mapped should succeed
  t.queue.submit([getCommandBuffer()]);

  // Map the buffer, submitting when the buffer is mapped (even with no getMappedRange) should fail
  await buffer.mapAsync(GPUMapMode.WRITE);
  t.queue.submit([]);

  t.expectValidationError(() => {
    t.queue.submit([getCommandBuffer()]);
  });

  // Unmap the buffer, queue submit should succeed
  buffer.unmap();
  t.queue.submit([getCommandBuffer()]);
});
