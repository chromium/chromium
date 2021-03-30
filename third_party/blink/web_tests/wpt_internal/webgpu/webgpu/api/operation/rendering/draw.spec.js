/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for the general aspects of draw/drawIndexed/drawIndirect/drawIndexedIndirect.

Primitive topology tested in api/operation/render_pipeline/primitive_topology.spec.ts.
Index format tested in api/operation/command_buffer/render/state_tracking.spec.ts.

* arguments - Test that draw arguments are passed correctly.

TODO:
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
import { params, pbool, poptions } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { assert } from '../../../../common/framework/util/util.js';
import { GPUTest } from '../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('arguments')
  .desc(
    `Test that draw arguments are passed correctly by drawing triangles in a grid.
Horizontally across the texture are triangles with increasing "primitive id".
Vertically down the screen are triangles with increasing instance id.
Increasing the |first| param should skip some of the beginning triangles on the horizontal axis.
Increasing the |first_instance| param should skip of the beginning triangles on the vertical axis.
The vertex buffer contains two sets of disjoint triangles, and base_vertex is used to select the second set.
The test checks that the center of all of the expected triangles is drawn, and the others are empty.
The fragment shader also writes out to a storage buffer. If the draw is zero-sized, check that no value is written.

Params:
  - first= {0, 3} - either the firstVertex or firstIndex
  - count= {0, 3, 6} - either the vertexCount or indexCount
  - first_instance= {0, 2}
  - instance_count= {0, 1, 4}
  - indexed= {true, false}
  - indirect= {true, false}
  - vertex_buffer_offset= {0, 32}
  - index_buffer_offset= {0, 16} - only for indexed draws
  - base_vertex= {0, 9} - only for indexed draws
  `
  )
  .cases(
    params()
      .combine(poptions('first', [0, 3]))
      .combine(poptions('count', [0, 3, 6]))
      .combine(poptions('first_instance', [0, 2]))
      .combine(poptions('instance_count', [0, 1, 4]))
      .combine(pbool('indexed'))
      .combine(pbool('indirect'))
      .combine(poptions('vertex_buffer_offset', [0, 32]))
      .expand(p => poptions('index_buffer_offset', p.indexed ? [0, 16] : [undefined]))
      .expand(p => poptions('base_vertex', p.indexed ? [0, 9] : [undefined]))
  )
  .fn(t => {
    const renderTargetSize = [72, 36];

    // The test will split up the render target into a grid where triangles of
    // increasing primitive id will be placed along the X axis, and triangles
    // of increasing instance id will be placed along the Y axis. The size of the
    // grid is based on the max primitive id and instance id used.
    const numX = 6;
    const numY = 6;
    const tileSizeX = renderTargetSize[0] / numX;
    const tileSizeY = renderTargetSize[1] / numY;

    // |\
    // |   \
    // |______\
    // Unit triangle shaped like this. 0-1 Y-down.
    const triangleVertices = [0.0, 0.0, 0.0, 1.0, 1.0, 1.0];

    const renderTarget = t.device.createTexture({
      size: renderTargetSize,
      usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.COPY_SRC,
      format: 'rgba8unorm',
    });

    const vertexModule = t.device.createShaderModule({
      code: `
[[builtin(vertex_index)]] var<in> vertex_index : u32;
[[builtin(instance_index)]] var<in> instance_id : u32;
[[location(0)]] var<in> vertexPosition : vec2<f32>;

[[builtin(position)]] var<out> Position : vec4<f32>;
[[stage(vertex)]] fn vert_main() -> void {
  // 3u is the number of points in a triangle to convert from index
  // to id.
  var vertex_id : u32 = vertex_index / 3u;

  var x : f32 = (vertexPosition.x + f32(vertex_id)) / ${numX}.0;
  var y : f32 = (vertexPosition.y + f32(instance_id)) / ${numY}.0;

  // (0,1) y-down space to (-1,1) y-up NDC
  x = 2.0 * x - 1.0;
  y = -2.0 * y + 1.0;
  Position = vec4<f32>(x, y, 0.0, 1.0);
}
`,
    });

    const fragmentModule = t.device.createShaderModule({
      code: `
[[block]] struct Output {
  value : u32;
};

[[group(0), binding(0)]] var<storage> output : Output;

[[location(0)]] var<out> fragColor : vec4<f32>;
[[stage(fragment)]] fn frag_main() -> void {
  output.value = 1u;
  fragColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
}
`,
    });

    const pipeline = t.device.createRenderPipeline({
      vertex: {
        module: vertexModule,
        entryPoint: 'vert_main',
        buffers: [
          {
            attributes: [
              {
                shaderLocation: 0,
                format: 'float32x2',
                offset: 0,
              },
            ],

            arrayStride: 2 * Float32Array.BYTES_PER_ELEMENT,
          },
        ],
      },

      fragment: {
        module: fragmentModule,
        entryPoint: 'frag_main',
        targets: [
          {
            format: 'rgba8unorm',
          },
        ],
      },
    });

    const resultBuffer = t.device.createBuffer({
      size: Uint32Array.BYTES_PER_ELEMENT,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });

    const resultBindGroup = t.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        {
          binding: 0,
          resource: {
            buffer: resultBuffer,
          },
        },
      ],
    });

    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          attachment: renderTarget.createView(),
          loadValue: [0, 0, 0, 0],
        },
      ],
    });

    renderPass.setPipeline(pipeline);
    renderPass.setBindGroup(0, resultBindGroup);

    if (t.params.indexed) {
      // INDEXED DRAW
      assert(t.params.base_vertex !== undefined);
      assert(t.params.index_buffer_offset !== undefined);

      renderPass.setIndexBuffer(
        t.makeBufferWithContents(
          new Uint32Array([
            // Offset the index buffer contents by empty data.
            ...new Array(t.params.index_buffer_offset / Uint32Array.BYTES_PER_ELEMENT),

            0,
            1,
            2, //
            3,
            4,
            5, //
            6,
            7,
            8, //
          ]),
          GPUBufferUsage.INDEX
        ),

        'uint32',
        t.params.index_buffer_offset
      );

      renderPass.setVertexBuffer(
        0,
        t.makeBufferWithContents(
          new Float32Array([
            // Offset the vertex buffer contents by empty data.
            ...new Array(t.params.vertex_buffer_offset / Float32Array.BYTES_PER_ELEMENT),

            // selected with base_vertex=0
            // count=6
            ...triangleVertices, //   |   count=6;first=3
            ...triangleVertices, //   |       |
            ...triangleVertices, //           |

            // selected with base_vertex=9
            // count=6
            ...triangleVertices, //   |   count=6;first=3
            ...triangleVertices, //   |       |
            ...triangleVertices, //           |
          ]),
          GPUBufferUsage.VERTEX
        ),

        t.params.vertex_buffer_offset
      );

      const args = [
        t.params.count,
        t.params.instance_count,
        t.params.first,
        t.params.base_vertex,
        t.params.first_instance,
      ];

      if (t.params.indirect) {
        renderPass.drawIndexedIndirect(
          t.makeBufferWithContents(new Uint32Array(args), GPUBufferUsage.INDIRECT),
          0
        );
      } else {
        renderPass.drawIndexed.apply(renderPass, [...args]);
      }
    } else {
      // NON-INDEXED DRAW
      renderPass.setVertexBuffer(
        0,
        t.makeBufferWithContents(
          new Float32Array([
            // Offset the vertex buffer contents by empty data.
            ...new Array(t.params.vertex_buffer_offset / Float32Array.BYTES_PER_ELEMENT),

            // count=6
            ...triangleVertices, //   |   count=6;first=3
            ...triangleVertices, //   |       |
            ...triangleVertices, //           |
          ]),
          GPUBufferUsage.VERTEX
        ),

        t.params.vertex_buffer_offset
      );

      const args = [
        t.params.count,
        t.params.instance_count,
        t.params.first,
        t.params.first_instance,
      ];

      if (t.params.indirect) {
        renderPass.drawIndirect(
          t.makeBufferWithContents(new Uint32Array(args), GPUBufferUsage.INDIRECT),
          0
        );
      } else {
        renderPass.draw.apply(renderPass, [...args]);
      }
    }

    renderPass.endPass();
    t.queue.submit([commandEncoder.finish()]);

    const green = new Uint8Array([0, 255, 0, 255]);
    const transparentBlack = new Uint8Array([0, 0, 0, 0]);

    const didDraw = t.params.count && t.params.instance_count;

    t.expectContents(resultBuffer, new Uint32Array([didDraw ? 1 : 0]));

    const baseVertex = t.params.base_vertex ?? 0;
    for (let primitiveId = 0; primitiveId < numX; ++primitiveId) {
      for (let instanceId = 0; instanceId < numY; ++instanceId) {
        let expectedColor = didDraw ? green : transparentBlack;
        if (
          primitiveId * 3 < t.params.first + baseVertex ||
          primitiveId * 3 >= t.params.first + baseVertex + t.params.count
        ) {
          expectedColor = transparentBlack;
        }

        if (
          instanceId < t.params.first_instance ||
          instanceId >= t.params.first_instance + t.params.instance_count
        ) {
          expectedColor = transparentBlack;
        }

        t.expectSinglePixelIn2DTexture(
          renderTarget,
          'rgba8unorm',
          {
            x: (1 / 3 + primitiveId) * tileSizeX,
            y: (2 / 3 + instanceId) * tileSizeY,
          },

          {
            exp: expectedColor,
          }
        );
      }
    }
  });
