/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../../../../common/framework/util/util.js';
import { kEncodableTextureFormatInfo } from '../../../../capability_info.js';
import {
  kTexelRepresentationInfo,
  getSingleDataType,
  getComponentReadbackTraits,
} from '../../../../util/texture/texel_data.js';

export const checkContentsBySampling = (t, params, texture, state, subresourceRange) => {
  assert(params.dimension === '2d');
  assert(params.format in kEncodableTextureFormatInfo);
  const format = params.format;
  const rep = kTexelRepresentationInfo[format];

  for (const { level, slices } of subresourceRange.mipLevels()) {
    const width = t.textureWidth >> level;
    const height = t.textureHeight >> level;

    const { ReadbackTypedArray, shaderType } = getComponentReadbackTraits(
      getSingleDataType(format)
    );

    const componentOrder = rep.componentOrder;
    const componentCount = componentOrder.length;

    // For single-component textures, generates .r
    // For multi-component textures, generates ex.)
    //  .rgba[i], .bgra[i], .rgb[i]
    const indexExpression =
      componentCount === 1
        ? componentOrder[0].toLowerCase()
        : componentOrder.map(c => c.toLowerCase()).join('') + '[i]';

    const _xd = '_' + params.dimension;
    const _multisampled = params.sampleCount > 1 ? '_multisampled' : '';
    const computePipeline = t.device.createComputePipeline({
      computeStage: {
        entryPoint: 'main',
        module: t.device.createShaderModule({
          code: `
            [[block]] struct Constants {
              [[offset(0)]] level : i32;
            };

            [[set(0), binding(0)]] var<uniform> constants : Constants;
            [[set(0), binding(1)]] var<uniform_constant> myTexture : texture${_multisampled}${_xd}<${shaderType}>;

            [[block]] struct Result {
              [[offset(0)]] values : [[stride(4)]] array<${shaderType}>;
            };
            [[set(0), binding(3)]] var<storage_buffer> result : Result;

            [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

            [[stage(compute)]]
            fn main() -> void {
              var flatIndex : u32 = ${width}u * GlobalInvocationID.y + GlobalInvocationID.x;
              flatIndex = flatIndex * ${componentCount}u;
              var texel : vec4<${shaderType}> = textureLoad(
                myTexture, vec2<i32>(GlobalInvocationID.xy), constants.level);

              for (var i : u32 = flatIndex; i < flatIndex + ${componentCount}u; i = i + 1) {
                result.values[i] = texel.${indexExpression};
              }
              return;
            }`,
        }),
      },
    });

    for (const slice of slices) {
      const ubo = t.device.createBuffer({
        mappedAtCreation: true,
        size: 4,
        usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
      });

      new Int32Array(ubo.getMappedRange(), 0, 1)[0] = level;
      ubo.unmap();

      const byteLength =
        width * height * ReadbackTypedArray.BYTES_PER_ELEMENT * rep.componentOrder.length;
      const resultBuffer = t.device.createBuffer({
        size: byteLength,
        usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
      });

      const bindGroup = t.device.createBindGroup({
        layout: computePipeline.getBindGroupLayout(0),
        entries: [
          {
            binding: 0,
            resource: { buffer: ubo },
          },

          {
            binding: 1,
            resource: texture.createView({
              baseMipLevel: 0,
              mipLevelCount: params.mipLevelCount,
              baseArrayLayer: slice,
              arrayLayerCount: 1,
            }),
          },

          {
            binding: 3,
            resource: {
              buffer: resultBuffer,
            },
          },
        ],
      });

      const commandEncoder = t.device.createCommandEncoder();
      const pass = commandEncoder.beginComputePass();
      pass.setPipeline(computePipeline);
      pass.setBindGroup(0, bindGroup);
      pass.dispatch(width, height);
      pass.endPass();
      t.queue.submit([commandEncoder.finish()]);
      ubo.destroy();

      const expectedValues = new ReadbackTypedArray(new ArrayBuffer(byteLength));
      const expectedState = t.stateToTexelComponents[state];
      let i = 0;
      for (let h = 0; h < height; ++h) {
        for (let w = 0; w < height; ++w) {
          for (const c of rep.componentOrder) {
            const value = expectedState[c];
            assert(value !== undefined);
            expectedValues[i++] = value;
          }
        }
      }
      t.expectContents(resultBuffer, expectedValues);
    }
  }
};
