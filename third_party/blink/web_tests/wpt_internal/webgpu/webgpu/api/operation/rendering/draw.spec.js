/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for the general aspects of draw/drawIndexed/drawIndirect/drawIndexedIndirect.

Primitive topology tested in api/operation/render_pipeline/primitive_topology.spec.ts.
Index format tested in api/operation/command_buffer/render/state_tracking.spec.ts.

TODO:
* arguments - Test that draw arguments are passed correctly.
  Test works by drawing triangles to the screen.
  Horizontally across the screen are triangles with increasing "primitive id".
  Vertically down the screen are triangles with increasing instance id.
  Increasing the |first| param should skip some of the beginning triangles on the horizontal axis.
  Increasing the |first_instance| param should skip of the beginning triangles on the vertical axis.
  The vertex buffer contains two side-by-side triangles, and base_vertex is used to offset to select the second.
  The test checks that the center of all of the expected triangles is drawn, and the others are empty.
  The fragment shader also writes out to a storage buffer. If the draw is zero-sized, check that no value is written.

  Params:
  - count= {0, non-zero} either the vertexCount or indexCount
  - instance_count= {0, non-zero}
  - first={0, non-zero} - either the firstVertex or firstIndex
  - first_instance={0, non-zero}
  - mode= {draw, drawIndexed, drawIndirect, drawIndexedIndirect}
  - base_vertex= {0, non-zero} - only for indexed draws

* default_arguments - Test defaults to draw / drawIndexed.
  - arg= {instance_count, first, first_instance, base_vertex}
  - mode= {draw, drawIndexed}

* vertex_attributes - Test fetching of vertex attributes
  Each vertex attribute is a single value and written to one component of an output attachment.
  4 components x 4 attachments is enough for 16 attributes. The test draws a grid of points
  with a fixed number of primitives and instances.
  Horizontally across the screen are primitives with increasing "primitive id".
  Vertically down the screen are primitives with increasing instance id.

  Params:
  - vertex_attributes= {0, 1, max}
  - vertex_buffer_count={0, 1, max} - where # attributes is > 0
  - step_mode= {vertex, instanced, mixed} - where mixed only applies for vertex_attributes > 1

* unaligned_vertex_count - Test that drawing with a number of vertices that's not a multiple of the vertices a given primitive list topology is not an error. The last primitive is not drawn.
  - primitive_topology= {line-list, triangle-list}
  - mode= {draw, drawIndexed, drawIndirect, drawIndexedIndirect}
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
