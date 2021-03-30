/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
- Tests that render N points, using a generated pipeline with:
  (1) a vertex shader that has necessary vertex inputs and a static array of
  expected data (as indexed by vertexID + instanceID * verticesPerInstance),
  which checks they're equal and sends the bool to the fragment shader;
  (2) a fragment shader which writes the result out to a storage buffer
  (or renders a red/green fragment if we can't do fragmentStoresAndAtomics,
  maybe with some depth or stencil test magic to do the '&&' of all fragments).
    - Fill some GPUBuffers with testable data, e.g.
      [[1.0, 2.0, ...], [-1.0, -2.0, ...]], for use as vertex buffers.
    - With no/trivial indexing
        - Either non-indexed, or indexed with a passthrough index buffer ([0, 1, 2, ...])
            - Of either format
            - If non-indexed, index format has no effect
        - Vertex data is read from the buffer correctly
            - setVertexBuffer offset
            - Several vertex buffers with several attributes each
                - Two setVertexBuffers pointing at the same GPUBuffer (if possible)
                    - Overlapping, non-overlapping
                - Overlapping attributes (iff that's supposed to work)
                - Overlapping vertex buffer elements
                  (an attribute offset + its size > arrayStride)
                  (iff that's supposed to work)
                - Zero, one, or two vertex buffers have stepMode "instance"
                - Discontiguous vertex buffer slots, e.g.
                  [1, some large number (API doesn't practically allow huge numbers here)]
                - Discontiguous shader locations, e.g.
                  [2, some large number (max if possible)]
             - Bind everything possible up to limits
                 - Also with maxed out attributes?
             - x= all vertex formats
        - Data is fed into the shader correctly
            - Swap attribute order (should have no effect)
            - Vertex formats x shader input types (should all be valid, I think?)
        - Maybe a test of one buffer with two attributes, with every possible
          pair of vertex formats
    - With indexing. For each index format:
        - Indices are read from the buffer correctly
            - setIndexBuffer offset
        - For each vertex format:
            - Basic test with several vertex buffers and several attributes
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
