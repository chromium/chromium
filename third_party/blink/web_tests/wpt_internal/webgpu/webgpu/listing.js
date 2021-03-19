// AUTO-GENERATED - DO NOT EDIT. See src/common/tools/gen_listings.ts.

export const listing = [
  {
    "file": [],
    "readme": "WebGPU conformance test suite."
  },
  {
    "file": [
      "api"
    ],
    "readme": "Tests for full coverage of the Javascript API surface of WebGPU."
  },
  {
    "file": [
      "api",
      "operation"
    ],
    "readme": "Tests that check the result of performing valid WebGPU operations, taking advantage of\nparameterization to exercise interactions between features."
  },
  {
    "file": [
      "api",
      "operation",
      "async_ordering"
    ],
    "readme": "Test ordering of async resolutions between the following promises (where there are constraints on the ordering).\nSpec issue: https://github.com/gpuweb/gpuweb/issues/962\n\nTODO: plan and implement\n- createReadyPipeline() (not sure if this actually has any ordering constraints)\n- cmdbuf.executionTime\n- device.popErrorScope()\n- device.lost\n- queue.onSubmittedWorkDone()\n- buffer.mapAsync()\n- shadermodule.compilationInfo()"
  },
  {
    "file": [
      "api",
      "operation",
      "buffers"
    ],
    "readme": "GPUBuffer tests."
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map"
    ],
    "description": "TODO: review and make sure these cases are covered:\n> - making sure writes/reads are to the right address (and get flushed)\n>     - TODO: various mapAsync offset/size\n>     - various getMappedRange offset/size\n>     - TODO: with non-overlapping getMappedRanges\n>     - TODO: with various TypedArray/DataView types\n>     - TODO: mapAsync is not a multiple of 8 but getMappedRange is, if that's allowed (probably won't be allowed, there's an issue in the spec about this)\n>     - x= {read, write, mappedAtCreation {mappable, non-mappable}}"
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_detach"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "map_oom"
    ],
    "description": "TODO: review and make sure these cases are covered:\n> - mapAsync + getMappedRange\n>     - oom on buffer creation should be followed by validation-failure to mapAsync\n>     - ?\n> - createBufferMapped + getMappedRange\n>     - getMappedRange should always be allowed even if the buffer creation was oom\n>         - unless the range is so huge that an ArrayBuffer can't be created\n>\n> These tests should also test ArrayBuffer detaching (anytime the buffer mapping succeeds)\n> because the mapped ranges may be backed by different types of memory (shmem vs local mem vs real\n> mapped mem).\n>\n> TODO: currently test a huge number, but should also test smaller, but still very large allocations (like 128GiB)"
  },
  {
    "file": [
      "api",
      "operation",
      "buffers",
      "threading"
    ],
    "description": "TODO:\n- Copy GPUBuffer to another thread while {pending, mapped mappedAtCreation} on {same,diff} thread\n- Destroy on one thread while {pending, mapped, mappedAtCreation, mappedAtCreation+unmap+mapped}\n  on another thread."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "basic"
    ],
    "description": "Basic tests."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "copyBufferToBuffer"
    ],
    "description": "copyBufferToBuffer operation tests"
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "copyTextureToTexture"
    ],
    "description": "copyTexturetoTexture operation tests\n\n  TODO(jiawei.shao@intel.com): support all WebGPU texture formats."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "image_copy"
    ],
    "description": "writeTexture + copyBufferToTexture + copyTextureToBuffer operation tests.\n\n* copy_with_various_rows_per_image_and_bytes_per_row: test that copying data with various bytesPerRow (including { ==, > } bytesInACompleteRow) and rowsPerImage (including { ==, > } copyExtent.height) values and minimum required bytes in copy works for every format. Also covers special code paths:\n  - bufferSize - offset < bytesPerImage * copyExtent.depthOrArrayLayers\n  - when bytesPerRow is not a multiple of 512 and copyExtent.depthOrArrayLayers > 1: copyExtent.depthOrArrayLayers % 2 == { 0, 1 }\n  - bytesPerRow == bytesInACompleteCopyImage\n\n* copy_with_various_offsets_and_data_sizes: test that copying data with various offset (including { ==, > } 0 and is/isn't power of 2) values and additional data paddings works for every format with 2d and 2d-array textures. Also covers special code paths:\n  - offset + bytesInCopyExtentPerRow { ==, > } bytesPerRow\n  - offset > bytesInACompleteCopyImage\n\n* copy_with_various_origins_and_copy_extents: test that copying slices of a texture works with various origin (including { origin.x, origin.y, origin.z } { ==, > } 0 and is/isn't power of 2) and copyExtent (including { copyExtent.x, copyExtent.y, copyExtent.z } { ==, > } 0 and is/isn't power of 2) values (also including {origin._ + copyExtent._ { ==, < } the subresource size of textureCopyView) works for all formats. origin and copyExtent values are passed as [number, number, number] instead of GPUExtent3DDict.\n\n* copy_various_mip_levels: test that copying various mip levels works for all formats. Also covers special code paths:\n  - the physical size of the subresouce is not equal to the logical size\n  - bufferSize - offset < bytesPerImage * copyExtent.depthOrArrayLayers and copyExtent needs to be clamped\n\n* copy_with_no_image_or_slice_padding_and_undefined_values: test that when copying a single row we can set any bytesPerRow value and when copying a single slice we can set rowsPerImage to 0. Also test setting offset, rowsPerImage, mipLevel, origin, origin.{x,y,z} to undefined.\n\n* TODO:\n  - add another initMethod which renders the texture\n  - test copyT2B with buffer size not divisible by 4 (not done because expectContents 4-byte alignment)\n  - add tests for 1d / 3d textures\n\nTODO: Fix this test for the various skipped formats:\n- snorm tests failing due to rounding\n- float tests failing because float values are not byte-preserved\n- compressed formats"
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "programmable",
      "state_tracking"
    ],
    "description": "Ensure state is set correctly. Tries to stress state caching (setting different states multiple\ntimes in different orders) for setBindGroup and setPipeline.\n\nTODO: for each programmable pass encoder {compute pass, render pass, render bundle encoder}\n- try setting states multiple times in different orders, check state is correct in draw/dispatch.\n    - Changing from pipeline A to B where both have the same layout except for {first,mid,last}\n      bind group index.\n    - Try with a pipeline that e.g. only uses bind group 1, or bind groups 0 and 2."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "queries"
    ],
    "readme": "TODO: test the behavior of creating/using/resolving queries.\n- occlusion\n- pipeline statistics\n- timestamp\n- nested (e.g. timestamp or PS query inside occlusion query), if any such cases are valid. Try\n  writing to the same query set (at same or different indices), if valid. Check results make sense."
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "dynamic_state"
    ],
    "description": "Tests of the behavior of the viewport/scissor/blend/reference states.\n\nTODO:\n- {viewport, scissor rect, blend color, stencil reference}:\n  Test rendering result with {various values}.\n    - Set the state in different ways to make sure it gets the correct value in the end: {\n        - state unset (= default)\n        - state explicitly set once to {default value, another value}\n        - persistence: [set, draw, draw] (fn should differentiate from [set, draw] + [draw])\n        - overwriting: [set(1), draw, set(2), draw] (fn should differentiate from [set(1), set(2), draw, draw])\n        - overwriting: [set(1), set(2), draw] (fn should differentiate from [set(1), draw] but not [set(2), draw])\n        - }"
  },
  {
    "file": [
      "api",
      "operation",
      "command_buffer",
      "render",
      "state_tracking"
    ],
    "description": "Ensure state is set correctly. Tries to stress state caching (setting different states multiple\ntimes in different orders) for setIndexBuffer and setVertexBuffer.\nEquivalent tests for setBindGroup and setPipeline are in programmable/state_tracking.spec.ts.\nEquivalent tests for viewport/scissor/blend/reference are in render/dynamic_state.spec.ts\n\nTODO: plan and implement\n- try setting states multiple times in different orders, check state is correct in a draw call.\n    - setIndexBuffer: specifically test changing the format, offset, size, without changing the buffer\n    - setVertexBuffer: specifically test changing the offset, size, without changing the buffer\n- try changing the pipeline {before,after} the vertex/index buffers.\n  (In D3D12, the vertex buffer stride is part of SetVertexBuffer instead of the pipeline.)\n- Test that drawing after having set vertex buffer slots not used by the pipeline.\n- Test that setting / not setting the index buffer does not impact a non-indexed draw."
  },
  {
    "file": [
      "api",
      "operation",
      "compute",
      "basic"
    ],
    "description": "Basic command buffer compute tests."
  },
  {
    "file": [
      "api",
      "operation",
      "compute_pipeline",
      "entry_point_name"
    ],
    "description": "TODO:\n- Test some weird but valid values for entry point name (both module and pipeline creation\n  should succeed).\n- Test using each of many entry points in the module (should succeed).\n- Test using an entry point with the wrong stage (should fail)."
  },
  {
    "file": [
      "api",
      "operation",
      "device",
      "lost"
    ],
    "description": "Tests for GPUDevice.lost."
  },
  {
    "file": [
      "api",
      "operation",
      "error_scope"
    ],
    "readme": "TODO: plan and implement\n- test very deeply nested error scopes, make sure errors go to the right place, e.g.\n    - validation, ..., validation, out-of-memory\n    - out-of-memory, validation, ..., validation\n    - out-of-memory, ..., out-of-memory, validation\n    - validation, out-of-memory, ..., out-of-memory\n- use error scopes on two different threads and make sure errors go to the right place\n- unhandled errors always go to the \"original\" device object\n    - test they go nowhere if the original was dropped (attemptGarbageCollection to make sure)"
  },
  {
    "file": [
      "api",
      "operation",
      "fences"
    ],
    "description": "TODO: fences are removed; replace still-relevant tests with equivalents for (multiple?) queues"
  },
  {
    "file": [
      "api",
      "operation",
      "labels"
    ],
    "description": "For every create function, the descriptor.label is carried over to the object.label.\n\nTODO: implement"
  },
  {
    "file": [
      "api",
      "operation",
      "memory_allocation"
    ],
    "readme": "Try to stress memory allocators in the implementation and driver.\n\nTODO: plan and implement\n- Tests which (pseudo-randomly?) allocate a bunch of memory and then assert things about the memory\n  (it's not aliased, it's valid to read and write in various ways, accesses read/write the correct data)\n    - Possibly also with OOB accesses/robust buffer access?\n- Tests which are targeted against particular known implementation details"
  },
  {
    "file": [
      "api",
      "operation",
      "memory_sync",
      "buffer",
      "rw_and_wr"
    ],
    "description": "Memory Synchronization Tests for Buffer: read before write and read after write.\n\n- Create a single buffer and initialize it to 0, wait on the fence to ensure the data is initialized.\nWrite a number (say 1) into the buffer via render pass, compute pass, copy or writeBuffer.\nRead the data and use it in render, compute, or copy.\nWait on another fence, then call expectContents to verify the written buffer.\nThis is a read-after write test but if the write and read operations are reversed, it will be a read-before-write test.\n  - x= write op: {storage buffer in {compute, render, render-via-bundle}, t2b copy dst, b2b copy dst, writeBuffer}\n  - x= read op: {index buffer, vertex buffer, indirect buffer, uniform buffer, {readonly, readwrite} storage buffer in {compute, render, render-via-bundle}, b2b copy src, b2t copy src}\n  - x= read-write sequence: {read then write, write then read}\n  - if pass type is the same, x= {single pass, separate passes} (note: render has loose guarantees)\n  - if not single pass, x= writes in {same cmdbuf, separate cmdbufs, separate submits, separate queues}\n\nTODO: Tests with more than one buffer to try to stress implementations a little bit more."
  },
  {
    "file": [
      "api",
      "operation",
      "memory_sync",
      "buffer",
      "ww"
    ],
    "description": "Memory Synchronization Tests for Buffer: write after write.\n\n- Create one single buffer and initialize it to 0. Wait on the fence to ensure the data is initialized.\nWrite a number (say 1) into the buffer via render pass, compute pass, copy or writeBuffer.\nWrite another number (say 2) into the same buffer via render pass, compute pass, copy, or writeBuffer.\nWait on another fence, then call expectContents to verify the written buffer.\n  - x= 1st write type: {storage buffer in {compute, render, render-via-bundle}, t2b-copy, b2b-copy, writeBuffer}\n  - x= 2nd write type: {storage buffer in {compute, render, render-via-bundle}, t2b-copy, b2b-copy, writeBuffer}\n  - if pass type is the same, x= {single pass, separate passes} (note: render has loose guarantees)\n  - if not single pass, x= writes in {same cmdbuf, separate cmdbufs, separate submits, separate queues}\n\nTODO: Tests with more than one buffer to try to stress implementations a little bit more."
  },
  {
    "file": [
      "api",
      "operation",
      "memory_sync",
      "texture",
      "rw_and_wr"
    ],
    "description": "Memory Synchronization Tests for Texture: read before write and read after write.\n\nTODO"
  },
  {
    "file": [
      "api",
      "operation",
      "memory_sync",
      "texture",
      "ww"
    ],
    "description": "Memory Synchronization Tests for Texture: write after write.\n\nTODO"
  },
  {
    "file": [
      "api",
      "operation",
      "onuncapturederror"
    ],
    "description": "Tests for GPUDevice.onuncapturederror."
  },
  {
    "file": [
      "api",
      "operation",
      "queue",
      "writeBuffer"
    ],
    "description": "Operation tests for GPUQueue.writeBuffer()"
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass"
    ],
    "readme": "Render pass stuff other than commands (which are in command_buffer/)."
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "resolve"
    ],
    "description": "API Operation Tests for RenderPass StoreOp.\nTests a render pass with a resolveTarget resolves correctly for many combinations of:\n  - number of color attachments, some with and some without a resolveTarget\n  - renderPass storeOp set to {'store', 'clear'}\n  - resolveTarget mip level {0, >0} (TODO?: different mip level from colorAttachment)\n  - resolveTarget {2d array layer, TODO: 3d slice} {0, >0} with {2d, TODO: 3d} resolveTarget\n    (TODO?: different z from colorAttachment)\n  - TODO: test all renderable color formats\n  - TODO: test that any not-resolved attachments are rendered to correctly.\n  - TODO: test different loadOps\n  - TODO?: resolveTarget mip level {0, >0} (TODO?: different mip level from colorAttachment)\n  - TODO?: resolveTarget {2d array layer, TODO: 3d slice} {0, >0} with {2d, TODO: 3d} resolveTarget\n    (different z from colorAttachment)"
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "storeOp"
    ],
    "description": "API Operation Tests for RenderPass StoreOp.\n\n  Test Coverage:\n\n  - Tests that color and depth-stencil store operations {'clear', 'store'} work correctly for a\n    render pass with both a color attachment and depth-stencil attachment.\n      TODO: use depth24plus-stencil8\n\n  - Tests that store operations {'clear', 'store'} work correctly for a render pass with multiple\n    color attachments.\n      TODO: test with more interesting loadOp values\n\n  - Tests that store operations {'clear', 'store'} work correctly for a render pass with a color\n    attachment for:\n      - All renderable color formats\n      - mip level set to {'0', mip > '0'}\n      - array layer set to {'0', layer > '1'} for 2D textures\n      TODO: depth slice set to {'0', slice > '0'} for 3D textures\n\n  - Tests that store operations {'clear', 'store'} work correctly for a render pass with a\n    depth-stencil attachment for:\n      - All renderable depth-stencil formats\n      - mip level set to {'0', mip > '0'}\n      - array layer set to {'0', layer > '1'} for 2D textures\n      TODO: test depth24plus and depth24plus-stencil8 formats\n      TODO: test that depth and stencil aspects are set seperately\n      TODO: depth slice set to {'0', slice > '0'} for 3D textures\n      TODO: test with more interesting loadOp values"
  },
  {
    "file": [
      "api",
      "operation",
      "render_pass",
      "storeop2"
    ],
    "description": "renderPass store op test that drawn quad is either stored or cleared based on storeop\n\nTODO: is this duplicated with api,operation,render_pass,storeOp?"
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "alpha_to_coverage"
    ],
    "description": "TODO:\n- for sampleCount = 4, alphaToCoverageEnabled = true and various combinations of:\n    - rasterization masks\n    - increasing alpha values of the first color output including { < 0, = 0, = 1/16, = 2/16, ..., = 15/16, = 1, > 1 }\n    - alpha values of the second color output = { 0, 0.5, 1.0 }.\n- test that for a single pixel in { first, second } { color, depth, stencil } output the final sample mask is applied to it, moreover:\n    - if alpha is 0.0 or less then alpha to coverage mask is 0x0,\n    - if alpha is 1.0 or greater then alpha to coverage mask is 0xFFFFFFFF,\n    - that the number of bits in the alpha to coverage mask is non-decreasing,\n    - that the computation of alpha to coverage mask doesn't depend on any other color output than first,\n    - (not included in the spec): that once a sample is included in the alpha to coverage sample mask\n      it will be included for any alpha greater than or equal to the current value."
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "culling_tests"
    ],
    "description": "Test culling and rasterizaion state.\n\nTest coverage:\nTest all culling combinations of GPUFrontFace and GPUCullMode show the correct output.\n\nUse 2 triangles with different winding orders:\n\n- Test that the counter-clock wise triangle has correct output for:\n  - All FrontFaces (ccw, cw)\n  - All CullModes (none, front, back)\n  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)\n  - Some primitive topologies (triangle-list, TODO: triangle-strip)\n\n- Test that the clock wise triangle has correct output for:\n  - All FrontFaces (ccw, cw)\n  - All CullModes (none, front, back)\n  - All depth stencil attachment types (none, depth24plus, depth32float, depth24plus-stencil8)\n  - Some primitive topologies (triangle-list, TODO: triangle-strip)"
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "entry_point_name"
    ],
    "description": "TODO:\n- Test some weird but valid values for entry point name (both module and pipeline creation\n  should succeed).\n- Test using each of many entry points in the module (should succeed).\n- Test using an entry point with the wrong stage (should fail)."
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "primitive_topology"
    ],
    "description": "Test primitive topology rendering.\n\nDraw a primitive using 6 vertices with each topology and check if the pixel is covered.\n\nVertex sequence and coordinates are the same for each topology:\n  - Vertex buffer = [v1, v2, v3, v4, v5, v6]\n  - Topology = [point-list, line-list, line-strip, triangle-list, triangle-strip]\n\nTest locations are framebuffer coordinates:\n  - Pixel value { valid: green, invalid: black, format: 'rgba8unorm'}\n  - Test point is valid if the pixel value equals the covered pixel value at the test location.\n  - Primitive restart occurs for strips (line-strip and triangle-strip) between [v3, v4].\n\n  Topology: point-list         Valid test location(s)           Invalid test location(s)\n\n       v2    v4     v6         Every vertex.                    Line-strip locations.\n                                                                Triangle-list locations.\n                                                                Triangle-strip locations.\n\n   v1     v3     v5\n\n  Topology: line-list (3 lines)\n\n       v2    v4     v6         Center of three line segments:   Line-strip locations.\n      *      *      *          {v1,V2}, {v3,v4}, and {v4,v5}.   Triangle-list locations.\n     *      *      *                                            Triangle-strip locations.\n    *      *      *\n   v1     v3     v5\n\n  Topology: line-strip (5 lines)\n\n       v2    v4     v6\n       **    **     *\n      *  *  *  *   *           Line-list locations              Triangle-list locations.\n     *    **     **          + Center of two line segments:     Triangle-strip locations.\n    v1    v3     v5            {v2,v3} and {v4,v5}.\n                                                                With primitive restart:\n                                                                Line segment {v3, v4}.\n\n  Topology: triangle-list (2 triangles)\n\n      v2       v4    v6\n      **        ******         Center of two triangle(s):       Triangle-strip locations.\n     ****        ****          {v1,v2,v3} and {v4,v5,v6}.\n    ******        **\n   v1     v3      v5\n\n  Topology: triangle-strip (4 triangles)\n\n      v2        v4      v6\n      ** ****** ** ******      Triangle-list locations          None.\n     **** **** **** ****     + Center of two triangle(s):\n    ****** ** ****** **        {v2,v3,v4} and {v3,v4,v5}.       With primitive restart:\n   v1       v3        v5                                        Triangle {v2, v3, v4}\n                                                                and {v3, v4, v5}."
  },
  {
    "file": [
      "api",
      "operation",
      "render_pipeline",
      "sample_mask"
    ],
    "description": "TODO:\n- for sampleCount = { 1, 4 } and various combinations of:\n    - rasterization mask = { 0, 1, 2, 3, 15 }\n    - sample mask = { 0, 1, 2, 3, 15, 30 }\n    - fragment shader output mask (SV_Coverage) = { 0, 1, 2, 3, 15, 30 }\n- test that final sample mask is the logical AND of all the\n  relevant masks -- meaning that the samples not included in the final mask are discarded\n  for all the { color outputs, depth tests, stencil operations } on any attachments.\n- [choosing 30 = 2 + 4 + 8 + 16 because the 5th bit should be ignored]"
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "basic"
    ],
    "description": "Basic command buffer rendering tests."
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "blending"
    ],
    "description": "Test blending results.\n\nTODO:\n- Test result for all combinations of args (make sure each case is distinguishable from others\n- Test underflow/overflow has consistent behavior\n- ?"
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "draw"
    ],
    "description": "Tests for the general aspects of draw/drawIndexed/drawIndirect/drawIndexedIndirect.\n\nPrimitive topology tested in api/operation/render_pipeline/primitive_topology.spec.ts.\nIndex format tested in api/operation/command_buffer/render/state_tracking.spec.ts.\n\nTODO:\n* arguments - Test that draw arguments are passed correctly.\n  Test works by drawing triangles to the screen.\n  Horizontally across the screen are triangles with increasing \"primitive id\".\n  Vertically down the screen are triangles with increasing instance id.\n  Increasing the |first| param should skip some of the beginning triangles on the horizontal axis.\n  Increasing the |first_instance| param should skip of the beginning triangles on the vertical axis.\n  The vertex buffer contains two side-by-side triangles, and base_vertex is used to offset to select the second.\n  The test checks that the center of all of the expected triangles is drawn, and the others are empty.\n  The fragment shader also writes out to a storage buffer. If the draw is zero-sized, check that no value is written.\n\n  Params:\n  - count= {0, non-zero} either the vertexCount or indexCount\n  - instance_count= {0, non-zero}\n  - first={0, non-zero} - either the firstVertex or firstIndex\n  - first_instance={0, non-zero}\n  - mode= {draw, drawIndexed, drawIndirect, drawIndexedIndirect}\n  - base_vertex= {0, non-zero} - only for indexed draws\n\n* default_arguments - Test defaults to draw / drawIndexed.\n  - arg= {instance_count, first, first_instance, base_vertex}\n  - mode= {draw, drawIndexed}\n\n* vertex_attributes - Test fetching of vertex attributes\n  Each vertex attribute is a single value and written to one component of an output attachment.\n  4 components x 4 attachments is enough for 16 attributes. The test draws a grid of points\n  with a fixed number of primitives and instances.\n  Horizontally across the screen are primitives with increasing \"primitive id\".\n  Vertically down the screen are primitives with increasing instance id.\n\n  Params:\n  - vertex_attributes= {0, 1, max}\n  - vertex_buffer_count={0, 1, max} - where # attributes is > 0\n  - step_mode= {vertex, instanced, mixed} - where mixed only applies for vertex_attributes > 1\n\n* unaligned_vertex_count - Test that drawing with a number of vertices that's not a multiple of the vertices a given primitive list topology is not an error. The last primitive is not drawn.\n  - primitive_topology= {line-list, triangle-list}\n  - mode= {draw, drawIndexed, drawIndirect, drawIndexedIndirect}"
  },
  {
    "file": [
      "api",
      "operation",
      "rendering",
      "indirect_draw"
    ],
    "description": "Tests for the indirect-specific aspects of drawIndirect/drawIndexedIndirect.\n\nTODO:\n* parameter_packing - Test that the indirect draw parameters are tightly packed.\n  - offset= {0, 4, k * sizeof(args struct), k * sizeof(args struct) + 4}\n  - mode= {drawIndirect, drawIndexedIndirect}"
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "buffer"
    ],
    "description": "Test uninitialized buffers are initialized to zero when read\n(or read-written, e.g. with depth write or atomics).\n\nTODO"
  },
  {
    "file": [
      "api",
      "operation",
      "resource_init",
      "texture_zero"
    ],
    "description": "Test uninitialized textures are initialized to zero when read.\n\nTODO:\n- 1d, 3d\n- test by sampling depth/stencil\n- test by copying out of stencil"
  },
  {
    "file": [
      "api",
      "operation",
      "sampling",
      "anisotropy"
    ],
    "description": "Tests the behavior of anisotropic filtering.\n\nTODO:\nNote that anisotropic filtering is never guaranteed to occur, but we might be able to test some\nthings. If there are no guarantees we can issue warnings instead of failures. Ideas:\n  - No *more* than the provided maxAnisotropy samples are used, by testing how many unique\n    sample values come out of the sample operation.\n  - Check anisotropy is done in the correct direciton (by having a 2D gradient and checking we get\n    more of the color in the correct direction)."
  },
  {
    "file": [
      "api",
      "operation",
      "sampling",
      "filter_mode"
    ],
    "description": "Tests the behavior of different filtering modes in minFilter/magFilter/mipmapFilter.\n\nTODO:\n- Test exact sampling results with small tolerance. Tests should differentiate between different\n  values for all three filter modes to make sure none are missed or incorrect in implementations.\n- (Likely unnecessary with the above.) Test exactly the expected number of samples are used.\n  Test this by setting up a rendering and asserting how many different shades result."
  },
  {
    "file": [
      "api",
      "operation",
      "sampling",
      "lod_clamp"
    ],
    "description": "Tests the behavior of LOD clamping (lodMinClamp, lodMaxClamp).\n\nTODO:\n- Write a test that can test the exact clamping behavior\n- Test a bunch of values, including very large/small ones."
  },
  {
    "file": [
      "api",
      "operation",
      "texture_view",
      "read"
    ],
    "description": "Test the result of reading textures through texture views with various options.\n\nAll x= every possible view read method: {\n  - {unfiltered, filtered (if valid), comparison (if valid)} sampling\n  - storage read {vertex, fragment, compute}\n  - no-op render pass that loads and then stores\n  - depth comparison\n  - stencil comparison\n}\nTODO: Write helper for this if not already available (see resource_init, buffer_sync_test for related code)."
  },
  {
    "file": [
      "api",
      "operation",
      "texture_view",
      "write"
    ],
    "description": "Test the result of writing textures through texture views with various options.\n\nAll x= every possible view write method: {\n  - storage write {fragment, compute}\n  - render pass store\n  - render pass resolve\n}\nTODO: Write helper for this if not already available (see resource_init, buffer_sync_test for related code)."
  },
  {
    "file": [
      "api",
      "operation",
      "threading"
    ],
    "readme": "Tests for behavior with multiple threads (main thread + workers).\n\nTODO: plan and implement\n- Try postMessage'ing an object of every type (to same or different thread)\n    - {main -> main, main -> worker, worker -> main, worker1 -> worker1, worker1 -> worker2}\n    - through {global postMessage, MessageChannel}\n    - {in, not in} transferrable object list, when valid\n- Short tight loop doing many of an action from two threads at the same time\n    - e.g. {create {buffer, texture, shader, pipeline}}"
  },
  {
    "file": [
      "api",
      "operation",
      "vertex_state",
      "basic"
    ],
    "description": "- Baseline tests checking vertex/instance IDs, with:\n    - No vertexState at all (i.e. no vertex buffers)\n    - One vertex buffer with no attributes"
  },
  {
    "file": [
      "api",
      "operation",
      "vertex_state",
      "correctness"
    ],
    "description": "- Tests that render N points, using a generated pipeline with:\n  (1) a vertex shader that has necessary vertex inputs and a static array of\n  expected data (as indexed by vertexID + instanceID * verticesPerInstance),\n  which checks they're equal and sends the bool to the fragment shader;\n  (2) a fragment shader which writes the result out to a storage buffer\n  (or renders a red/green fragment if we can't do fragmentStoresAndAtomics,\n  maybe with some depth or stencil test magic to do the '&&' of all fragments).\n    - Fill some GPUBuffers with testable data, e.g.\n      [[1.0, 2.0, ...], [-1.0, -2.0, ...]], for use as vertex buffers.\n    - With no/trivial indexing\n        - Either non-indexed, or indexed with a passthrough index buffer ([0, 1, 2, ...])\n            - Of either format\n            - If non-indexed, index format has no effect\n        - Vertex data is read from the buffer correctly\n            - setVertexBuffer offset\n            - Several vertex buffers with several attributes each\n                - Two setVertexBuffers pointing at the same GPUBuffer (if possible)\n                    - Overlapping, non-overlapping\n                - Overlapping attributes (iff that's supposed to work)\n                - Overlapping vertex buffer elements\n                  (an attribute offset + its size > arrayStride)\n                  (iff that's supposed to work)\n                - Zero, one, or two vertex buffers have stepMode \"instance\"\n                - Discontiguous vertex buffer slots, e.g.\n                  [1, some large number (API doesn't practically allow huge numbers here)]\n                - Discontiguous shader locations, e.g.\n                  [2, some large number (max if possible)]\n             - Bind everything possible up to limits\n                 - Also with maxed out attributes?\n             - x= all vertex formats\n        - Data is fed into the shader correctly\n            - Swap attribute order (should have no effect)\n            - Vertex formats x shader input types (should all be valid, I think?)\n        - Maybe a test of one buffer with two attributes, with every possible\n          pair of vertex formats\n    - With indexing. For each index format:\n        - Indices are read from the buffer correctly\n            - setIndexBuffer offset\n        - For each vertex format:\n            - Basic test with several vertex buffers and several attributes"
  },
  {
    "file": [
      "api",
      "operation",
      "vertex_state",
      "index_format"
    ],
    "description": "Test indexing, index format and primitive restart."
  },
  {
    "file": [
      "api",
      "regression"
    ],
    "readme": "One-off tests that reproduce API bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "api",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the API."
  },
  {
    "file": [
      "api",
      "validation",
      "attachment_compatibility"
    ],
    "description": "Validation for attachment compatibility between render passes, bundles, and pipelines\n\nTODO: Add sparse color attachment compatibility test when defined by specification"
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "create"
    ],
    "description": "Tests for validation in createBuffer."
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "destroy"
    ],
    "description": "Destroying a buffer more than once is allowed."
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "mapping"
    ],
    "description": "Validation tests for GPUBuffer.mapAsync, GPUBuffer.unmap and GPUBuffer.getMappedRange.\n\nTODO: review existing tests and merge with this plan:\n> - {mappedAtCreation, await mapAsync}\n>     - -> x = getMappedRange\n>     - check x.size == mapping size\n>     - -> noawait mapAsync\n>     - check x.size == mapping size\n>     - -> await\n>     - check x.size == mapping size\n>     - -> unmap\n>     - check x.size == 0\n>     - -> getMappedRange (should fail)\n> - await mapAsync -> await mapAsync -> getMappedRange\n> - {mappedAtCreation, await mapAsync} -> unmap -> unmap\n> - x = noawait mapAsync -> y = noawait mapAsync\n>     - -> getMappedRange (should fail)\n>     - -> await x\n>     - -> getMappedRange\n>     - -> shouldReject(y)\n> - noawait mapAsync -> unmap\n> - {mappedAtCreation, await mapAsync} -> x = getMappedRange -> unmap -> await mapAsync(subrange) -> y = getMappedRange\n>     - check x.size == 0, y.size == mapping size"
  },
  {
    "file": [
      "api",
      "validation",
      "buffer",
      "threading"
    ],
    "description": "TODO:\n- Try to map on one thread while {pending, mapped, mappedAtCreation, mappedAtCreation+unmap+mapped}\n  on another thread.\n- Invalid to postMessage a mapped range's ArrayBuffer or ArrayBufferView\n  {with, without} it being in the transfer array.\n- Copy GPUBuffer to another thread while {pending, mapped mappedAtCreation} on {same,diff} thread\n  (valid), then try to map on that thread (invalid)"
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "features"
    ],
    "readme": "Test every method or option that shouldn't be valid without a feature enabled.\n\n- x= that feature {enabled, disabled}\n\nOne file for each feature name.\n\nTODO: implement"
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "features",
      "queries"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "validation",
      "capability_checks",
      "limits"
    ],
    "readme": "Test everything that shouldn't be valid without a higher-than-specified limit.\n\n- x= that limit {default, max supported (if different), lower than default (TODO: if allowed)}\n\nOne file for each limit name.\n\nTODO: implement"
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroup"
    ],
    "description": "createBindGroup validation tests.\n\nTODO: review existing tests, write descriptions, and make sure tests are complete."
  },
  {
    "file": [
      "api",
      "validation",
      "createBindGroupLayout"
    ],
    "description": "createBindGroupLayout validation tests.\n\nTODO: review existing tests, write descriptions, and make sure tests are complete."
  },
  {
    "file": [
      "api",
      "validation",
      "createPipelineLayout"
    ],
    "description": "createPipelineLayout validation tests.\n\nTODO: review existing tests, write descriptions, and make sure tests are complete."
  },
  {
    "file": [
      "api",
      "validation",
      "createRenderPipeline"
    ],
    "description": "createRenderPipeline validation tests.\n\nTODO: review existing tests, write descriptions, and make sure tests are complete.\n      Make sure the following is covered. Consider splitting the file if too large/disjointed.\n> - various attachment problems\n>\n> - interface matching between vertex and fragment shader\n>     - superset, subset, etc.\n>\n> - vertexStage {valid, invalid}\n> - fragmentStage {valid, invalid}\n> - primitiveTopology all possible values\n> - rasterizationState various values\n> - sampleCount {0, 1, 3, 4, 8, 16, 1024}\n> - sampleMask {0, 0xFFFFFFFF}\n> - alphaToCoverage:\n>     - alphaToCoverageEnabled is { true, false } and sampleCount { = 1, = 4 }.\n>       The only failing case is (true, 1).\n>     - output SV_Coverage semantics is statically used by fragmentStage and\n>       alphaToCoverageEnabled is { true (fails), false (passes) }.\n>     - sampleMask is being used and alphaToCoverageEnabled is { true (fails), false (passes) }."
  },
  {
    "file": [
      "api",
      "validation",
      "createSampler"
    ],
    "description": "createSampler validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createTexture"
    ],
    "description": "createTexture validation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "createView"
    ],
    "description": "createView validation tests.\n\nTODO: review existing tests and merge with this plan:\n> All x= every texture format for the underlying texture\n>\n> - view format doesn't match/isn't compatible\n>     - with/without flag set <- I don't think this flag exists yet\n>     - x= every possible view format\n> - dimension isn't one of the following compatible options:\n>     - texture 1d -> view 1d\n>     - texture 2d -> view 2d, 2d-array, cube, cube-array\n>     - texture 3d -> view 3d\n> - {cube, cube-array} not enough layers\n> - all aspects\n>     - \"depth-only\" only allowed for D and DS\n>     - \"stencil-only\" only allowed for S and DS\n>     - \"all\" allowed for any format\n> - baseMipLevel+mipLevelCount various values {in, out of} range\n> - baseArrayLayer+arrayLayerCount various values {in, out of} range"
  },
  {
    "file": [
      "api",
      "validation",
      "create_pipeline"
    ],
    "description": "TODO:\nFor {createRenderPipeline, createComputePipeline}, start with a valid descriptor (control case),\nthen for each stage {{vertex, fragment}, compute}, make exactly one of the following errors:\n- one stage's module is an invalid object\n- one stage's entryPoint doesn't exist\n  - {different name, empty string, name that's almost the same but differs in some subtle unicode way}"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "beginRenderPass"
    ],
    "description": "TODO: check for duplication (render_pass/, etc.), plan, and implement.\nNote possibly a lot of this should be operation tests instead.\nNotes:\n> - color attachments {zero, one, multiple}\n>     - many different formats (some are non-renderable)\n>     - is a view on a texture with multiple mip levels or array layers\n>     - two attachments use the same view, or views of {intersecting, disjoint} ranges\n>     - {without, with} resolve target\n>         - resolve format compatibility with multisampled format\n>     - {all possible load ops, load color {in range, negative, too large}}\n>     - all possible store ops\n> - depth/stencil attachment\n>     - {unset, all possible formats}\n>     - {all possible {depth, stencil} load ops, load values {in range, negative, too large}}\n>     - all possible {depth, stencil} store ops\n>     - depthReadOnly {t,f}, stencilReadOnly {t,f}"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "buffer_texture_copies"
    ],
    "description": "copyTextureToBuffer and copyBufferToTexture validation tests not covered by\nthe general image_copy tests, or by destroyed,*.\n\nTODO:\nAdd tests to cover the validation rule that source.offset is a multiple of the texel block size of\ndestination.texture.[[format]]."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "compute_pass"
    ],
    "description": "API validation test for compute pass\n\nDoes **not** test usage scopes (resource_usages/) or programmable pass stuff (programmable_pass)."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "copyBufferToBuffer"
    ],
    "description": "copyBufferToBuffer tests.\n\nTest Plan:\n* Buffer is valid/invalid\n  - the source buffer is invalid\n  - the destination buffer is invalid\n* Buffer usages\n  - the source buffer is created without GPUBufferUsage::COPY_SRC\n  - the destination buffer is created without GPUBufferUsage::COPY_DEST\n* CopySize\n  - copySize is not a multiple of 4\n  - copySize is 0\n* copy offsets\n  - sourceOffset is not a multiple of 4\n  - destinationOffset is not a multiple of 4\n* Arthimetic overflow\n  - (sourceOffset + copySize) is overflow\n  - (destinationOffset + copySize) is overflow\n* Out of bounds\n  - (sourceOffset + copySize) > size of source buffer\n  - (destinationOffset + copySize) > size of destination buffer\n* Source buffer and destination buffer are the same buffer"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "copyTextureToTexture"
    ],
    "description": "copyTextureToTexture tests.\n\nTest Plan: (TODO(jiawei.shao@intel.com): add tests on 1D/3D textures)\n* the source and destination texture\n  - the {source, destination} texture is {invalid, valid}.\n  - mipLevel {>, =, <} the mipmap level count of the {source, destination} texture.\n  - the source texture is created {with, without} GPUTextureUsage::CopySrc.\n  - the destination texture is created {with, without} GPUTextureUsage::CopyDst.\n* sample count\n  - the sample count of the source texture {is, isn't} equal to the one of the destination texture\n  - when the sample count is greater than 1:\n    - it {is, isn't} a copy of the whole subresource of the source texture.\n    - it {is, isn't} a copy of the whole subresource of the destination texture.\n* texture format\n  - the format of the source texture {is, isn't} equal to the one of the destination texture.\n    - including: depth24plus-stencil8 to/from {depth24plus, stencil8}.\n  - for each depth and/or stencil format: a copy between two textures with same format:\n    - it {is, isn't} a copy of the whole subresource of the {source, destination} texture.\n* copy ranges\n  - if the texture dimension is 2D:\n    - (srcOrigin.x + copyExtent.width) {>, =, <} the width of the subresource size of source\n      textureCopyView.\n    - (srcOrigin.y + copyExtent.height) {>, =, <} the height of the subresource size of source\n      textureCopyView.\n    - (srcOrigin.z + copyExtent.depthOrArrayLayers) {>, =, <} the depthOrArrayLayers of the subresource size of source\n      textureCopyView.\n    - (dstOrigin.x + copyExtent.width) {>, =, <} the width of the subresource size of destination\n      textureCopyView.\n    - (dstOrigin.y + copyExtent.height) {>, =, <} the height of the subresource size of destination\n      textureCopyView.\n    - (dstOrigin.z + copyExtent.depthOrArrayLayers) {>, =, <} the depthOrArrayLayers of the subresource size of destination\n      textureCopyView.\n* when the source and destination texture are the same one:\n  - the set of source texture subresources {has, doesn't have} overlaps with the one of destination\n    texture subresources."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "debug"
    ],
    "description": "API validation test for debug groups and markers\n\nTest Coverage:\n  - For each encoder type (GPUCommandEncoder, GPUComputeEncoder, GPURenderPassEncoder,\n  GPURenderBundleEncoder):\n    - Test that all pushDebugGroup must have a corresponding popDebugGroup\n      - Push and pop counts of 0, 1, and 2 will be used.\n      - An error must be generated for non matching counts.\n    - Test calling pushDebugGroup with empty and non-empty strings.\n    - Test inserting a debug marker with empty and non-empty strings."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "index_access"
    ],
    "description": "indexed draws validation tests.\n\nTODO: review and make sure these notes are covered:\n> - indexed draws:\n>     - index access out of bounds (make sure this doesn't overlap with robust access)\n>         - bound index buffer **range** is {exact size, just under exact size} needed for draws with:\n>             - indexCount largeish\n>             - firstIndex {=, >} 0\n>     - x= {drawIndexed, drawIndexedIndirect}\n\nTODO: Since there are no errors here, these should be \"robustness\" operation tests (with multiple\nvalid results)."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "dynamic_state"
    ],
    "description": "API validation tests for dynamic state commands (setViewport/ScissorRect/BlendColor...).\n\nTODO: ensure existing tests cover these notes. Note many of these may be operation tests instead.\n> - setViewport\n>     - {x, y} = {0, invalid values if any}\n>     - {width, height, minDepth, maxDepth} = {\n>         - least possible value that's valid\n>         - greatest possible negative value that's invalid\n>         - greatest possible positive value that's valid\n>         - least possible positive value that's invalid if any\n>         - }\n>     - minDepth {<, =, >} maxDepth\n> - setScissorRect\n>     - {width, height} = 0\n>     - {x+width, y+height} = attachment size + 1\n> - setBlendColor\n>     - color {slightly, very} out of range\n>     - used with a simple pipeline that {does, doesn't} use it\n> - setStencilReference\n>     - {0, max}\n>     - used with a simple pipeline that {does, doesn't} use it"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "other"
    ],
    "description": "Does **not** test usage scopes (resource_usages/), programmable pass stuff (programmable,*),\nor state tracking (state_tracking).\n\nTODO: plan and implement. Notes:\n> All x= {render pass, render bundle}\n>\n> - setPipeline\n>     - {valid, invalid} GPURenderPipeline\n> - setIndexBuffer\n>     - buffer is {valid, invalid, doesn't have usage)\n>     - (offset, size) is\n>         - (0, 0)\n>         - (0, 1)\n>         - (0, 4)\n>         - (0, 5)\n>         - (0, b.size)\n>         - (min alignment, b.size - 4)\n>         - (4, b.size - 4)\n>         - (b.size - 4, 4)\n>         - (b.size, min size)\n>         - (0, min size), and if that's valid:\n>             - (b.size - min size, min size)\n> - setVertexBuffer\n>     - slot is {0, max, max+1}\n>     - buffer is {valid, invalid,  doesn't have usage)\n>     - (offset, size) is like above\n> - drawIndirect / drawIndexedIndirect\n>     - buffer is {valid, invalid, doesn't have usage)\n>     - (offset, b.size) is\n>         - (0, 0)\n>         - (0, min size - min alignment)\n>         - (0, min size - 1)\n>         - (0, min size)\n>         - (min alignment, min size + min alignment)\n>         - (min alignment, min alignment + min size - 1)\n>         - (min alignment +/- 1, min size + alignment)"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render",
      "state_tracking"
    ],
    "description": "Validation tests for setVertexBuffer/setIndexBuffer state (not validation). See also operation tests."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "render_pass"
    ],
    "description": "Validation tests for render pass encoding.\nDoes **not** test usage scopes (resource_usages/), GPUProgrammablePassEncoder (programmable_pass),\ndynamic state (dynamic_render_state.spec.ts), or GPURenderEncoderBase (render.spec.ts).\n\nTODO:\n- executeBundles:\n    - with {zero, one, multiple} bundles where {zero, one} of them are invalid objects"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "cmds",
      "setBindGroup"
    ],
    "description": "setBindGroup validation tests.\n\nTODO: merge these notes and implement.\n> (Note: If there are errors with using certain binding types in certain passes, test those in the file for that pass type, not here.)\n>\n> All x= {compute pass, render pass, render bundle}\n>\n> - setBindGroup\n>     - x= {compute pass, render pass}\n>     - index {0, max, max+1}\n>     - GPUBindGroup object {valid, invalid, valid but refers to destroyed {buffer, texture}}\n>     - bind group {with, without} dynamic offsets with {too few, too many} dynamicOffsets entries\n>         - x= {sequence, Uint32Array} overload\n>     - iff minBufferBindingSize is specified, buffer size is correctly validated against it (make sure static offset + dynamic offset are both accounted for)\n> - state tracking (probably separate file)\n>     - x= {compute pass, render pass}\n>     - {null, compatible, incompatible} current pipeline (should have no effect without draw/dispatch)\n>     - setBindGroup in different orders (e.g. 0,1,2 vs 2,0,1)"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "encoder_state"
    ],
    "description": "TODO:\n- createCommandEncoder\n- non-pass command, or beginPass, during {render, compute} pass\n- {before (control case), after} finish()\n    - x= {finish(), ... all non-pass commands}\n- {before (control case), after} endPass()\n    - x= {render, compute} pass\n    - x= {finish(), ... all relevant pass commands}\n    - x= {\n        - before endPass (control case)\n        - after endPass (no pass open)\n        - after endPass+beginPass (a new pass of the same type is open)\n        - }\n    - should make whole encoder invalid\n- ?"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "programmable",
      "pipeline_bind_group_compat"
    ],
    "description": "TODO:\n- test compatibility between bind groups and pipelines\n    - bind groups required by the pipeline layout are required.\n    - bind groups unused by the pipeline layout can be set or not.\n        (Even if e.g. bind groups 0 and 2 are used, but 1 is unused.)\n    - bindGroups[i].layout is \"group-equivalent\" (value-equal) to pipelineLayout.bgls[i].\n    - in the test fn, test once without the dispatch/draw (should always be valid) and once with\n      the dispatch/draw, to make sure the validation happens in dispatch/draw.\n    - x= {dispatch, all draws} (dispatch/draw should be size 0 to make sure validation still happens if no-op)\n    - x= all relevant stages\n\nTODO: subsume existing test, rewrite fixture as needed."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "begin_end"
    ],
    "description": "Validation for encoding begin/endable queries.\n\nTODO:\n- balance: {\n    - begin 0, end 1\n    - begin 1, end 0\n    - begin 1, end 1\n    - begin 2, end 2\n    - }\n    - x= {\n        - render pass + occlusion\n        - render pass + pipeline statistics\n        - compute pass + pipeline statistics\n        - }\n- nesting: test whether it's allowed to nest various types of queries\n  (including writeTimestamp inside begin/endable queries)."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "general"
    ],
    "description": "TODO:\n\n- For each way to start a query (all possible types in all possible encoders):\n    - queryIndex {in, out of} range for GPUQuerySet\n    - GPUQuerySet {valid, invalid}\n        - or {undefined}, for occlusionQuerySet\n    - x = {occlusion, pipeline statistics, timestamp} query"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "occlusion"
    ],
    "description": "Validation for encoding occlusion queries.\nExcludes query begin/end balance and nesting (begin_end.spec.ts)\nand querySet/queryIndex (general.spec.ts).\n\nTODO:\n- Test an occlusion query with no draw calls. (If that's valid, move the test to api/operation/.)\n- ?"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "pipeline_statistics"
    ],
    "description": "Validation for encoding pipeline statistics queries.\nExcludes query begin/end balance and nesting (begin_end.spec.ts)\nand querySet/queryIndex (general.spec.ts).\n\nTODO:\n- Test with an invalid querySet.\n- Test an pipeline statistics query with no draw/dispatch calls.\n  (If that's valid, move the test to api/operation/.)\n- ?"
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "resolveQuerySet"
    ],
    "description": "Validation tests for resolveQuerySet."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "queries",
      "timestamp"
    ],
    "description": "Validation for encoding timestamp queries.\nExcludes query nesting (begin_end.spec.ts) and querySet/queryIndex (general.spec.ts).\n\nTODO: Is there anything to test here? If not, delete this file."
  },
  {
    "file": [
      "api",
      "validation",
      "encoding",
      "render_bundle"
    ],
    "description": "TODO:\n- test creating a render bundle, and if it's valid, test that executing it is not an error\n    - color formats {all possible formats} {zero, one, multiple}\n    - depth/stencil format {unset, all possible formats}\n- ?"
  },
  {
    "file": [
      "api",
      "validation",
      "error_scope"
    ],
    "description": "Error scope validation tests.\n\nNote these must create their own device, not use GPUTest (that one already has error scopes on it).\n\nTODO: shorten test names; detail should move to the description.)\n\nTODO: consider slightly revising these tests to make sure they're complete. {\n    - push 0, pop 1\n    - push validation, push oom, pop, pop, pop\n    - push oom, push validation, pop, pop, pop\n    - push validation, pop, pop\n    - push oom, pop, pop\n    - push various x100000 (or some other large number), pop x100000, pop\n    - }"
  },
  {
    "file": [
      "api",
      "validation",
      "fences"
    ],
    "description": "fences validation tests.\n\nTODO: per-test descriptions, make names more succinct\nTODO: fences are removed; replace still-relevant tests with equivalents for (multiple?) queues\nTODO: Add some more tests/cases (may replace some existing tests), e.g.:\n  For fence values 0 < x < y < z:\n  - initialValue=0, signal(0)\n  - initialValue=x, signal(x)\n  - initialValue=x, signal(y)\n  - initialValue=y, signal(x)\n  - initialValue=x, signal(y), signal(y)\n  - initialValue=x, signal(y), signal(z), wait(z)\n  - initialValue=x, signal(z), signal(y)\n  - initialValue=x, wait(x)\n  - initialValue=y, wait(x)\n  - initialValue=x, wait(y)\n  - initialValue=x, signal(y), wait(x)\n  - initialValue=x, signal(y), wait(y)\n  - etc."
  },
  {
    "file": [
      "api",
      "validation",
      "image_copy"
    ],
    "readme": "writeTexture + copyBufferToTexture + copyTextureToBuffer validation tests.\n\nTest coverage:\n* resource usages:\n\t- texture_usage_must_be_valid: for GPUTextureUsage::COPY_SRC, GPUTextureUsage::COPY_DST flags.\n\t- TODO: buffer_usage_must_be_valid\n\n* textureCopyView:\n\t- texture_must_be_valid: for valid, destroyed, error textures.\n\t- sample_count_must_be_1: for sample count 1 and 4.\n\t- mip_level_must_be_in_range: for various combinations of mipLevel and mipLevelCount.\n\t- texel_block_alignment_on_origin: for all formats and coordinates.\n\n* bufferCopyView:\n\t- TODO: buffer_must_be_valid\n\t- TODO: bytes_per_row_alignment\n\n* linear texture data:\n\t- bound_on_rows_per_image: for various combinations of copyDepth (1, >1), copyHeight, rowsPerImage.\n\t- offset_plus_required_bytes_in_copy_overflow\n\t- required_bytes_in_copy: testing minimal data size and data size too small for various combinations of bytesPerRow, rowsPerImage, copyExtent and offset. for the copy method, bytesPerRow is computed as bytesInACompleteRow aligned to be a multiple of 256 + bytesPerRowPadding * 256.\n\t- texel_block_alignment_on_rows_per_image: for all formats.\n\t- texel_block_alignment_on_offset: for all formats.\n\t- bound_on_bytes_per_row: for all formats and various combinations of bytesPerRow and copyExtent. for writeTexture, bytesPerRow is computed as (blocksPerRow * blockWidth * bytesPerBlock + additionalBytesPerRow) and copyExtent.width is computed as copyWidthInBlocks * blockWidth. for the copy methods, both values are mutliplied by 256.\n\t- bound_on_offset: for various combinations of offset and dataSize.\n\n* texture copy range:\n\t- 1d_texture: copyExtent.height isn't 1, copyExtent.depthOrArrayLayers isn't 1.\n\t- texel_block_alignment_on_size: for all formats and coordinates.\n\t- texture_range_conditons: for all coordinate and various combinations of origin, copyExtent, textureSize and mipLevel.\n\nTODO: more test coverage for 1D and 3D textures."
  },
  {
    "file": [
      "api",
      "validation",
      "image_copy",
      "layout_related"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "validation",
      "image_copy",
      "texture_related"
    ],
    "description": ""
  },
  {
    "file": [
      "api",
      "validation",
      "initialization",
      "requestDevice"
    ],
    "description": "Test validation conditions for requestDevice."
  },
  {
    "file": [
      "api",
      "validation",
      "layout_shader_compat"
    ],
    "description": "TODO:\n- interface matching between pipeline layout and shader\n    - x= {compute, vertex, fragment, vertex+fragment}, visibilities\n    - x= bind group index values, binding index values, multiple bindings\n    - x= types of bindings\n    - x= {equal, superset, subset}"
  },
  {
    "file": [
      "api",
      "validation",
      "query_set",
      "create"
    ],
    "description": "Tests for validation in createQuerySet."
  },
  {
    "file": [
      "api",
      "validation",
      "query_set",
      "destroy"
    ],
    "description": "Destroying a query set more than once is allowed."
  },
  {
    "file": [
      "api",
      "validation",
      "queue"
    ],
    "readme": "Tests for validation that occurs inside queued operations\n(submit, writeBuffer, writeTexture, copyImageBitmapToTexture).\n\nBufferMapStatesToTest = {\n  mapped -> unmapped,\n  mapped at creation -> unmapped,\n  mapping pending -> unmapped,\n  pending -> mapped (await map),\n  unmapped -> pending (noawait map),\n  created mapped-at-creation,\n}\n\nNote writeTexture is tested in image_copy."
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "buffer_mapped"
    ],
    "description": "Tests for map-state of mappable buffers used in submitted command buffers.\n\n- x= just before queue op, buffer in {BufferMapStatesToTest}\n- x= in every possible place for mappable buffer:\n  {submit, writeBuffer, copyB2B {src,dst}, copyB2T, copyT2B, ..?}\n\nTODO: generalize existing test"
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "copyImageBitmapToTexture"
    ],
    "description": "copyImageBitmapToTexture Validation Tests in Queue.\nTODO: Should this be the same file as, or next to, web_platform/copyImageBitmapToTexture.spec.ts?\n\nTODO: Split this test plan per-test.\n\nTest Plan:\n- For source.imageBitmap:\n  - imageBitmap generated from ImageData:\n    - Check that an error is generated when imageBitmap is closed.\n\n- For destination.texture:\n  - For 2d destination textures:\n    - Check that an error is generated when texture is in destroyed state.\n    - Check that an error is generated when texture is an error texture.\n    - Check that an error is generated when texture is created without usage COPY_DST.\n    - Check that an error is generated when sample count is not 1.\n    - Check that an error is generated when mipLevel is too large.\n    - Check that an error is generated when texture format is not valid.\n\n- For copySize:\n  - No-op copy shouldn't throw any exception or return any validation error.\n  - Check that an error is generated when destination.texture.origin + copySize is too large.\n\nTODO: copying into slices of 2d array textures. 1d and 3d as well if they're not invalid."
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "destroyed",
      "buffer"
    ],
    "description": "Tests using a destroyed buffer on a queue.\n\n- used in {writeBuffer,\n  setBindGroup, copyB2B {src,dst}, copyB2T, copyT2B,\n  setIndexBuffer, {draw,dispatch}Indirect, ..?}\n- x= if applicable, {in pass, in bundle}\n- x= {destroyed, not destroyed (control case)}\n\nTODO: implement. (Search for other places some of these cases may have already been tested.)\nConsider whether these tests should be distributed throughout the suite, instead of centralized."
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "destroyed",
      "query_set"
    ],
    "description": "Tests using a destroyed query set on a queue.\n\n- used in {resolveQuerySet, timestamp {compute, render, non-pass},\n    pipeline statistics {compute, render}, occlusion}\n- x= {destroyed, not destroyed (control case)}\n\nTODO: implement. (Search for other places some of these cases may have already been tested.)\nConsider whether these tests should be distributed throughout the suite, instead of centralized."
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "destroyed",
      "texture"
    ],
    "description": "Tests using a destroyed texture on a queue.\n\n- used in {writeTexture,\n  setBindGroup, copyT2T {src,dst}, copyB2T, copyT2B, copyImageBitmapToTexture,\n  color attachment {0,>0}, {D,S,DS} attachment, ..?}\n- x= if applicable, {in pass, in bundle}\n- x= {destroyed, not destroyed (control case)}\n\nTODO: implement. (Search for other places some of these cases may have already been tested.)\nConsider whether these tests should be distributed throughout the suite, instead of centralized."
  },
  {
    "file": [
      "api",
      "validation",
      "queue",
      "writeBuffer"
    ],
    "description": "Tests writeBuffer validation.\n\n- buffer missing usage flag\n- bufferOffset {ok, unaligned, too large for buffer}\n- dataOffset {ok, too large for data}\n- buffer size {ok, too small for copy}\n- data size {ok, too small for copy}\n- size {aligned, unaligned}\n- size unspecified; default {ok, too large for buffer}\n\nNote: destroyed buffer is tested in destroyed/.\n\nTODO: implement usage flag validation.\nTODO: validate large write sizes that may overflow."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass"
    ],
    "readme": "Render pass stuff other than commands (which are in encoding/cmds/)."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass",
      "resolve"
    ],
    "description": "Validation tests for render pass resolve."
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass",
      "storeOp"
    ],
    "description": "API Validation Tests for RenderPass StoreOp.\n\nTest Coverage:\n  - Tests that when depthReadOnly is true, depthStoreOp must be 'store'.\n    - When depthReadOnly is true and depthStoreOp is 'clear', an error should be generated.\n\n  - Tests that when stencilReadOnly is true, stencilStoreOp must be 'store'.\n    - When stencilReadOnly is true and stencilStoreOp is 'clear', an error should be generated.\n\n  - Tests that the depthReadOnly value matches the stencilReadOnly value.\n    - When depthReadOnly does not match stencilReadOnly, an error should be generated.\n\n  - Tests that depthReadOnly and stencilReadOnly default to false.\n\nTODO: test interactions with depthLoadValue too"
  },
  {
    "file": [
      "api",
      "validation",
      "render_pass_descriptor"
    ],
    "description": "render pass descriptor validation tests.\n\nTODO: per-test descriptions, make test names more succinct\nTODO: review for completeness"
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "buffer"
    ],
    "readme": "TODO: look at texture,*"
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "texture",
      "in_pass_encoder"
    ],
    "description": "Texture Usages Validation Tests in Render Pass and Compute Pass.\n\nTODO: description per test\n\nTest Coverage:\n  - For each combination of two texture usages:\n    - For various subresource ranges (different mip levels or array layers) that overlap a given\n      subresources or not for color formats:\n      - For various places that resources are used, for example, used in bundle or used in render\n        pass directly.\n        - Check that an error is generated when read-write or write-write usages are binding to the\n          same texture subresource. Otherwise, no error should be generated. One exception is race\n          condition upon two writeonly-storage-texture usages, which is valid.\n\n  - For each combination of two texture usages:\n    - For various aspects (all, depth-only, stencil-only) that overlap a given subresources or not\n      for depth/stencil formats:\n      - Check that an error is generated when read-write or write-write usages are binding to the\n        same aspect. Otherwise, no error should be generated.\n\n  - Test combinations of two shader stages:\n    - Texture usages in bindings with invisible shader stages should be validated. Invisible shader\n      stages include shader stage with visibility none, compute shader stage in render pass, and\n      vertex/fragment shader stage in compute pass.\n\n  - Tests replaced bindings:\n    - Texture usages via bindings replaced by another setBindGroup() upon the same bindGroup index\n      in render pass should be validated. However, replaced bindings should not be validated in\n      compute pass.\n\n  - Test texture usages in bundle:\n    - Texture usages in bundle should be validated if that bundle is executed in the current scope.\n\n  - Test texture usages with unused bindings:\n    - Texture usages should be validated even its bindings is not used in pipeline.\n\n  - Test texture usages validation scope:\n    - Texture usages should be validated per each render pass. And they should be validated per each\n      dispatch call in compute."
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "texture",
      "in_render_common"
    ],
    "description": "TODO:\n- 2 views:\n    - x= {upon the same subresource, or different subresources {mip level, array layer, aspect} of the same texture}\n    - x= possible binding types on each view: read = {sampled texture, readonly storage texture}, write = {storage texture, render target}\n    - x= different shader stages: {0, ..., 7}\n        - maybe first view vis = {1, 2, 4}, second view vis = {0, ..., 7}\n    - x= bindings are in {\n        - same draw call\n        - same pass, different draw call\n        - different pass\n        - }\n(It's probably not necessary to test EVERY possible combination of options in this whole\nblock, so we could break it down into a few smaller ones (one for different types of\nsubresources, one for same draw/same pass/different pass, one for visibilities).)"
  },
  {
    "file": [
      "api",
      "validation",
      "resource_usages",
      "texture",
      "in_render_misc"
    ],
    "description": "TODO:\n- 2 views: upon the same subresource, or different subresources of the same texture\n    - texture usages in copies and in render pass\n    - consecutively set bind groups on the same index (@Richard-Yunchao: Maybe I can combine this one with the above tests. The two bind groups can either have the same index or different indices.)\n    - unused bind groups"
  },
  {
    "file": [
      "api",
      "validation",
      "state",
      "device_lost"
    ],
    "readme": "Tests of behavior while the device is lost.\n\n- x= every method in the API.\n\nTODO: implement"
  },
  {
    "file": [
      "api",
      "validation",
      "state",
      "device_mismatched"
    ],
    "readme": "Tests of behavior on one device using objects from another device.\n\n- x= every place in the API where an object is passed (as this, an arg, or a dictionary member).\n\nTODO: implement"
  },
  {
    "file": [
      "api",
      "validation",
      "texture",
      "destroy"
    ],
    "description": "Destroying a texture more than once is allowed."
  },
  {
    "file": [
      "api",
      "validation",
      "vertex_access"
    ],
    "description": "TODO: make sure this isn't already covered somewhere else, review, organize, and implement.\n> - In encoder.finish():\n>     - setVertexBuffer and setIndexBuffer commands (even if no draw):\n>         - If not valid at draw time, test overlapping {vertex/vertex,vertex/index}\n>           buffers are valid without draw.\n>         - Before, after setPipeline (should have no effect)\n>         - Implicit offset/size are computed correctly. E.g.:\n>             { offset:         0, boundSize:         0, bufferSize: 24 },\n>             { offset:         0, boundSize: undefined, bufferSize: 24 },\n>             { offset: undefined, boundSize:         0, bufferSize: 24 },\n>             { offset: undefined, boundSize: undefined, bufferSize: 24 },\n>             { offset:         8, boundSize:        16, bufferSize: 24 },\n>             { offset:         8, boundSize: undefined, bufferSize: 24 },\n>         - Computed {index, vertex} buffer size is zero.\n>             (Omit draw command if it's not necessary to trigger error, otherwise test both with and without draw command to make sure error happens at the right time.)\n>             { offset: 24, boundSize: undefined, bufferSize: 24, _ok: false },\n>         - Bound range out-of-bounds on the GPUBuffer. E.g.:\n>             - x= offset in {0,8}\n>             - x= boundSize in {8,16,17}\n>             - x= extraSpaceInBuffer in {-1,0}\n>     - All (non/indexed, in/direct) draw commands\n>         - Same GPUBuffer bound to multiple vertex buffer slots\n>             - Non-overlapping, overlapping ranges\n>         - A needed vertex buffer is not bound\n>             - Was bound in another render pass but not the current one\n>             - x= all vertex formats\n>         - setPl, setVB, setIB, draw, {setPl,setVB,setIB,nothing (control)}, then\n>           a larger draw that wouldn't have been valid before that\n>         - Draw call needs to read {=, >} any bound vertex buffer range\n>           (with GPUBuffer that is always large enough)\n>             - x= all vertex formats\n>             - x= weird offset values\n>             - x= weird arrayStride values\n>         - A bound vertex buffer range is significantly larger than necessary\n>     - All non-indexed (in/direct) draw commands, {\n>         - An unused {index (with uselessly small range), vertex} buffer\n>           is bound (immediately before draw call)\n>         - }\n>     - All indexed (in/direct) draw commands, {\n>         - No index buffer is bound\n>         - Same GPUBuffer bound to index buffer and a vertex buffer slot\n>             - Non-overlapping, overlapping ranges\n>         - Draw call needs to read {=, >} the bound index buffer range\n>           (with GPUBuffer that is always large enough)\n>             - range is too small and GPUBuffer is large enough\n>             - range and GPUBuffer are exact size\n>             - x= all index formats\n>         - Bound index buffer range is significantly larger than necessary\n>         - }\n>     - Alignment constraints on setVertexBuffer if any\n>     - Alignment constraints on setIndexBuffer if any\n> - In queue.submit():\n>     - Indexed draw call with index buffer containing:\n>         - Index value that goes out-of-bounds on a bound vertex buffer range\n>         - Index value that is extremely large (but not the primitive restart value)\n>     - Line strip or triangle strip with index buffer containing:\n>         - Primitive restart value\n>         - Primitive restart value minus one (and the bound vertex buffers are < that size)\n>     - Indirect draw call with arguments that:\n>         - Go out-of-bounds on the bound index buffer range\n>         - Go out-of-bounds on the bound vertex buffer range\n\nTODO: Had two plans with roughly the same name. Figure out where to categorize these notes:\n> All x= {render pass, render bundle}\n>\n> - non-indexed draws:\n>     - vertex access out of bounds (make sure this doesn't overlap with robust access)\n>         - bound vertex buffer **ranges** are {exact size, just under exact size} needed for draws with:\n>             - vertexCount largeish\n>             - firstVertex {=, >} 0\n>             - instanceCount largeish\n>             - firstInstance {=, >} 0\n>         - include VBs with both step modes\n>     - x= {draw, drawIndirect}\n> - indexed draws:\n>     - vertex access out of bounds (make sure this doesn't overlap with robust access)\n>         - bound vertex buffer **ranges** are {exact size, just under exact size} needed for draws with:\n>             - a vertex index in the buffer is largeish\n>             - baseVertex {=, >} 0\n>             - instanceCount largeish\n>             - firstInstance {=, >} 0\n>         - include VBs with both step modes\n>     - x= {drawIndirect, drawIndexedIndirect}"
  },
  {
    "file": [
      "api",
      "validation",
      "vertex_state"
    ],
    "description": "vertexState validation tests."
  },
  {
    "file": [
      "examples"
    ],
    "description": "Examples of writing CTS tests with various features.\n\nStart here when looking for examples of basic framework usage."
  },
  {
    "file": [
      "idl"
    ],
    "readme": "Tests to check that the WebGPU IDL is correctly implemented, for examples that objects exposed\nexactly the correct members, and that methods throw when passed incomplete dictionaries.\n\nSee https://github.com/gpuweb/cts/issues/332"
  },
  {
    "file": [
      "idl",
      "constants",
      "flags"
    ],
    "description": "Test the values of flags interfaces (e.g. GPUTextureUsage)."
  },
  {
    "file": [
      "shader"
    ],
    "readme": "Tests for full coverage of the shaders that can be passed to WebGPU."
  },
  {
    "file": [
      "shader",
      "execution"
    ],
    "readme": "Tests that check the result of valid shader execution."
  },
  {
    "file": [
      "shader",
      "execution",
      "robust_access_vertex"
    ],
    "description": "Test vertex attributes behave correctly (no crash / data leak) when accessed out of bounds\n\nTest coverage:\n\nThe following will be parameterized (all combinations tested):\n\n1) Draw call indexed? (false / true)\n  - Run the draw call using an index buffer\n\n2) Draw call indirect? (false / true)\n  - Run the draw call using an indirect buffer\n\n3) Draw call parameter (vertexCount, firstVertex, indexCount, firstIndex, baseVertex, instanceCount,\n  firstInstance)\n  - The parameter which will go out of bounds. Filtered depending on if the draw call is indexed.\n\n4) Attribute type (float, vec2, vec3, vec4)\n  - The input attribute type in the vertex shader\n\n5) Error scale (1, 4, 10^2, 10^4, 10^6)\n  - Offset to add to the correct draw call parameter\n\n6) Additional vertex buffers (0, +4)\n  - Tests that no OOB occurs if more vertex buffers are used\n\nThe tests will also have another vertex buffer bound for an instanced attribute, to make sure\ninstanceCount / firstInstance are tested.\n\nThe tests will include multiple attributes per vertex buffer.\n\nThe vertex buffers will be filled by repeating a few chosen values until the end of the buffer.\n\nThe test will run a render pipeline which verifies the following:\n1) All vertex attribute values occur in the buffer or are zero\n2) All gl_VertexIndex values are within the index buffer or 0\n\nTODO:\n\nA suppression may be needed for d3d12 on tests that have non-zero baseVertex, since d3d12 counts\nfrom 0 instead of from baseVertex (will fail check for gl_VertexIndex).\n\nVertex buffer contents could be randomized to prevent the case where a previous test creates\na similar buffer to ours and the OOB-read seems valid. This should be deterministic, which adds\nmore complexity that we may not need."
  },
  {
    "file": [
      "shader",
      "regression"
    ],
    "readme": "One-off tests that reproduce shader bugs found in implementations to prevent the bugs from\nappearing again."
  },
  {
    "file": [
      "shader",
      "validation"
    ],
    "readme": "Positive and negative tests for all the validation rules of the shading language."
  },
  {
    "file": [
      "shader",
      "validation",
      "variable_and_const"
    ],
    "description": "Positive and negative validation tests for variable and const."
  },
  {
    "file": [
      "shader",
      "validation",
      "wgsl",
      "basic"
    ],
    "description": "Basic WGSL validation tests to test the ShaderValidationTest fixture."
  },
  {
    "file": [
      "util",
      "texture",
      "texel_data"
    ],
    "description": "Test helpers for texel data produce the expected data in the shader"
  },
  {
    "file": [
      "web_platform"
    ],
    "readme": "Tests for Web platform-specific interactions like GPUSwapChain and canvas, WebXR,\nImageBitmaps, and video APIs."
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "configureSwapChain"
    ],
    "description": "Tests for GPUCanvasContext.configureSwapChain.\n\nTODO: Test all options of configureSwapChain."
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "context_creation"
    ],
    "description": ""
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "getCurrentTexture"
    ],
    "description": "Tests for GPUSwapChain.getCurrentTexture."
  },
  {
    "file": [
      "web_platform",
      "canvas",
      "getSwapChainPreferredFormat"
    ],
    "description": "Tests for GPUCanvasContext.getSwapChainPreferredFormat."
  },
  {
    "file": [
      "web_platform",
      "copyImageBitmapToTexture"
    ],
    "description": "copyImageBitmapToTexture from ImageBitmaps created from various sources.\n\nTODO: additional sources"
  },
  {
    "file": [
      "web_platform",
      "reftests"
    ],
    "readme": "Reference tests (reftests) for WebGPU canvas presentation.\n\nThese render some contents to a canvas using WebGPU, and WPT compares the rendering result with\nthe \"reference\" versions (in `ref/`) which render with 2D canvas.\n\nThis tests things like:\n- The canvas has the correct orientation.\n- The canvas renders with the correct transfer function.\n- The canvas blends and interpolates in the correct color encoding.\n\nTODO: Test all possible output texture formats (currently only testing bgra8unorm).\nTODO: Test all possible ways to write into those formats (currently only testing B2T copy)."
  }
];
