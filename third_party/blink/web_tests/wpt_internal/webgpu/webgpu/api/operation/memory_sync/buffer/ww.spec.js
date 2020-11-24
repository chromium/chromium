/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Memory Synchronization Tests for Buffer: write after write.

- Create one single buffer and initialize it to 0. Wait on the fence to ensure the data is initialized.
Write a number (say 1) into the buffer via render pass, compute pass, copy or writeBuffer.
Write another number (say 2) into the same buffer via render pass, compute pass, copy, or writeBuffer.
Wait on another fence, then call expectContents to verify the written buffer.
  - x= 1st write type: {storage buffer in {compute, render, render-via-bundle}, t2b-copy, b2b-copy, writeBuffer}
  - x= 2nd write type: {storage buffer in {compute, render, render-via-bundle}, t2b-copy, b2b-copy, writeBuffer}
  - if pass type is the same, x= {single pass, separate passes} (note: render has loose guarantees)
  - if not single pass, x= writes in {same cmdbuf, separate cmdbufs, separate submits, separate queues}
`;
import { poptions, params } from '../../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';

import { kAllWriteOps, BufferSyncTest } from './buffer_sync_test.js';

export const g = makeTestGroup(BufferSyncTest);

g.test('same_cmdbuf')
  .desc('Test write-after-write operations in the same command buffer.')
  .params(
    params()
      .combine(poptions('firstWriteOp', kAllWriteOps))
      .combine(poptions('secondWriteOp', kAllWriteOps))
  )
  .fn(async t => {
    const { firstWriteOp, secondWriteOp } = t.params;
    const buffer = await t.createBufferWithValue(0);

    const encoder = t.device.createCommandEncoder();
    await t.encodeWriteOp(encoder, firstWriteOp, buffer, 1);
    await t.encodeWriteOp(encoder, secondWriteOp, buffer, 2);
    t.device.defaultQueue.submit([encoder.finish()]);

    t.verifyData(buffer, 2);
  });

g.test('separate_cmdbufs')
  .desc('Test write-after-write operations in separate command buffers via the same submit.')
  .params(
    params()
      .combine(poptions('firstWriteOp', kAllWriteOps))
      .combine(poptions('secondWriteOp', kAllWriteOps))
  )
  .fn(async t => {
    const { firstWriteOp, secondWriteOp } = t.params;
    const buffer = await t.createBufferWithValue(0);

    const command_buffers = [];
    command_buffers.push(await t.createCommandBufferWithWriteOp(firstWriteOp, buffer, 1));
    command_buffers.push(await t.createCommandBufferWithWriteOp(secondWriteOp, buffer, 2));
    t.device.defaultQueue.submit(command_buffers);

    t.verifyData(buffer, 2);
  });

g.test('separate_submits')
  .desc('Test write-after-write operations via separate submits in the same queue.')
  .params(
    params()
      .combine(poptions('firstWriteOp', ['write-buffer', ...kAllWriteOps]))
      .combine(poptions('secondWriteOp', ['write-buffer', ...kAllWriteOps]))
  )
  .fn(async t => {
    const { firstWriteOp, secondWriteOp } = t.params;
    const buffer = await t.createBufferWithValue(0);

    await t.submitWriteOp(firstWriteOp, buffer, 1);
    await t.submitWriteOp(secondWriteOp, buffer, 2);

    t.verifyData(buffer, 2);
  });

g.test('separate_queues')
  .desc('Test write-after-write operations in separate queues.')
  .unimplemented();

g.test('two_draws_in_the_same_render_pass')
  .desc(
    `Test write-after-write operations in the same render pass. The first write will write 1 into
    a storage buffer. The second write will write 2 into the same buffer in the same pass. Expected
    data in buffer is either 1 or 2. It may use bundle in each draw.`
  )
  .unimplemented();

g.test('two_dispatches_in_the_same_compute_pass')
  .desc(
    `Test write-after-write operations in the same compute pass. The first write will write 1 into
    a storage buffer. The second write will write 2 into the same buffer in the same pass. Expected
    data in buffer is 2.`
  )
  .unimplemented();
