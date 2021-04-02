/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Memory Synchronization Tests for Buffer: read before write and read after write.

- Create a single buffer and initialize it to 0, wait on the fence to ensure the data is initialized.
Write a number (say 1) into the buffer via render pass, compute pass, copy or writeBuffer.
Read the data and use it in render, compute, or copy.
Wait on another fence, then call expectContents to verify the written buffer.
This is a read-after write test but if the write and read operations are reversed, it will be a read-before-write test.
  - x= write op: {storage buffer in {compute, render, render-via-bundle}, t2b copy dst, b2b copy dst, writeBuffer}
  - x= read op: {index buffer, vertex buffer, indirect buffer, uniform buffer, {readonly, readwrite} storage buffer in {compute, render, render-via-bundle}, b2b copy src, b2t copy src}
  - x= read-write sequence: {read then write, write then read}
  - if pass type is the same, x= {single pass, separate passes} (note: render has loose guarantees)
  - if not single pass, x= writes in {same cmdbuf, separate cmdbufs, separate submits, separate queues}

TODO: Tests with more than one buffer to try to stress implementations a little bit more.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';

import { BufferSyncTest } from './buffer_sync_test.js';

export const g = makeTestGroup(BufferSyncTest);
