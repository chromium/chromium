/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description =
  'Test helpers for texel data produce the expected data in the shader';
import { params, poptions } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert } from '../../../common/framework/util/util.js';
import { kEncodableTextureFormats, kEncodableTextureFormatInfo } from '../../capability_info.js';
import { GPUTest } from '../../gpu_test.js';

import {
  kTexelRepresentationInfo,
  getSingleDataType,
  getComponentReadbackTraits,
} from './texel_data.js';

export const g = makeTestGroup(GPUTest);

function doTest(t) {
  const { format } = t.params;
  const componentData = (() => {
    const { R, G, B, A } = t.params;
    return { R, G, B, A };
  })();

  const rep = kTexelRepresentationInfo[format];
  const texelData = rep.pack(componentData);
  const texture = t.device.createTexture({
    format,
    size: [1, 1, 1],
    usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.SAMPLED,
  });

  t.device.defaultQueue.writeTexture(
    { texture },
    texelData,
    {
      bytesPerRow: texelData.byteLength,
    },

    [1]
  );

  const { ReadbackTypedArray, shaderType } = getComponentReadbackTraits(getSingleDataType(format));

  const shader = `
  [[set(0), binding(0)]] var<uniform_constant> tex : texture_2d<${shaderType}>;

  [[block]] struct Output {
    ${rep.componentOrder
      .map((C, i) => `[[offset(${i * 4})]] result${C} : ${shaderType};`)
      .join('\n')}
  };
  [[set(0), binding(1)]] var<storage_buffer> output : Output;

  [[stage(compute)]]
  fn main() -> void {
      var texel : vec4<${shaderType}> = textureLoad(tex, vec2<i32>(0, 0), 0);
      ${rep.componentOrder.map(C => `output.result${C} = texel.${C.toLowerCase()};`).join('\n')}
      return;
  }`;

  const pipeline = t.device.createComputePipeline({
    computeStage: {
      module: t.device.createShaderModule({
        code: shader,
      }),

      entryPoint: 'main',
    },
  });

  const outputBuffer = t.device.createBuffer({
    size: rep.componentOrder.length * 4,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
  });

  const bindGroup = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      {
        binding: 0,
        resource: texture.createView(),
      },

      {
        binding: 1,
        resource: {
          buffer: outputBuffer,
        },
      },
    ],
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bindGroup);
  pass.dispatch(1);
  pass.endPass();
  t.device.defaultQueue.submit([encoder.finish()]);

  t.expectContents(
    outputBuffer,
    new ReadbackTypedArray(
      rep.componentOrder.map(c => {
        const value = rep.decode(componentData)[c];
        assert(value !== undefined);
        return value;
      })
    )
  );
}

// Make a test parameter by mapping a format and each component to a texel component
// data value.
function makeParam(format, fn) {
  const rep = kTexelRepresentationInfo[format];
  return {
    R: rep.componentInfo.R ? fn(rep.componentInfo.R.bitLength, 0) : undefined,
    G: rep.componentInfo.G ? fn(rep.componentInfo.G.bitLength, 1) : undefined,
    B: rep.componentInfo.B ? fn(rep.componentInfo.B.bitLength, 2) : undefined,
    A: rep.componentInfo.A ? fn(rep.componentInfo.A.bitLength, 3) : undefined,
  };
}

g.test('unorm_texel_data_in_shader')
  .params(
    params()
      .combine(poptions('format', kEncodableTextureFormats))
      .filter(({ format }) => {
        return (
          kEncodableTextureFormatInfo[format].copyDst &&
          kEncodableTextureFormatInfo[format].color &&
          getSingleDataType(format) === 'unorm'
        );
      })
      .expand(({ format }) => {
        const max = bitLength => Math.pow(2, bitLength) - 1;
        return [
          // Test extrema
          makeParam(format, () => 0),
          makeParam(format, bitLength => max(bitLength)),

          // Test a middle value
          makeParam(format, bitLength => Math.floor(max(bitLength) / 2)),

          // Test mixed values
          makeParam(format, (bitLength, i) => {
            const offset = [0.13, 0.63, 0.42, 0.89];
            return Math.floor(offset[i] * max(bitLength));
          }),
        ];
      })
  )
  .fn(doTest);

g.test('snorm_texel_data_in_shader')
  .params(
    params()
      .combine(poptions('format', kEncodableTextureFormats))
      .filter(({ format }) => {
        return (
          kEncodableTextureFormatInfo[format].copyDst &&
          kEncodableTextureFormatInfo[format].color &&
          getSingleDataType(format) === 'snorm'
        );
      })
      .expand(({ format }) => {
        const max = bitLength => Math.pow(2, bitLength - 1) - 1;
        return [
          // Test extrema
          makeParam(format, () => 0),
          makeParam(format, bitLength => max(bitLength)),
          makeParam(format, bitLength => -max(bitLength)),
          makeParam(format, bitLength => -max(bitLength) - 1),

          // Test a middle value
          makeParam(format, bitLength => Math.floor(max(bitLength) / 2)),

          // Test mixed values
          makeParam(format, (bitLength, i) => {
            const offset = [0.13, 0.63, 0.42, 0.89];
            const range = 2 * max(bitLength);
            return -max(bitLength) + Math.floor(offset[i] * range);
          }),
        ];
      })
  )
  .fn(doTest);

g.test('uint_texel_data_in_shader')
  .params(
    params()
      .combine(poptions('format', kEncodableTextureFormats))
      .filter(({ format }) => {
        return (
          kEncodableTextureFormatInfo[format].copyDst &&
          kEncodableTextureFormatInfo[format].color &&
          getSingleDataType(format) === 'uint'
        );
      })
      .expand(({ format }) => {
        const max = bitLength => Math.pow(2, bitLength) - 1;
        return [
          // Test extrema
          makeParam(format, () => 0),
          makeParam(format, bitLength => max(bitLength)),

          // Test a middle value
          makeParam(format, bitLength => Math.floor(max(bitLength) / 2)),

          // Test mixed values
          makeParam(format, (bitLength, i) => {
            const offset = [0.13, 0.63, 0.42, 0.89];
            return Math.floor(offset[i] * max(bitLength));
          }),
        ];
      })
  )
  .fn(doTest);

g.test('sint_texel_data_in_shader')
  .params(
    params()
      .combine(poptions('format', kEncodableTextureFormats))
      .filter(({ format }) => {
        return (
          kEncodableTextureFormatInfo[format].copyDst &&
          kEncodableTextureFormatInfo[format].color &&
          getSingleDataType(format) === 'sint'
        );
      })
      .expand(({ format }) => {
        const max = bitLength => Math.pow(2, bitLength - 1) - 1;
        return [
          // Test extrema
          makeParam(format, () => 0),
          makeParam(format, bitLength => max(bitLength)),
          makeParam(format, bitLength => -max(bitLength) - 1),

          // Test a middle value
          makeParam(format, bitLength => Math.floor(max(bitLength) / 2)),

          // Test mixed values
          makeParam(format, (bitLength, i) => {
            const offset = [0.13, 0.63, 0.42, 0.89];
            const range = 2 * max(bitLength);
            return -max(bitLength) + Math.floor(offset[i] * range);
          }),
        ];
      })
  )
  .fn(doTest);

g.test('float_texel_data_in_shader')
  .params(
    params()
      .combine(poptions('format', kEncodableTextureFormats))
      .filter(({ format }) => {
        return (
          kEncodableTextureFormatInfo[format].copyDst &&
          kEncodableTextureFormatInfo[format].color &&
          getSingleDataType(format) === 'float'
        );
      })
      .expand(({ format }) => {
        return [
          // Test extrema
          makeParam(format, () => 0),

          // TODO: Test NaN, Infinity, -Infinity

          // Test some values
          makeParam(format, () => 0.1199951171875),
          makeParam(format, () => 1.4072265625),
          makeParam(format, () => 24928),
          makeParam(format, () => -0.1319580078125),
          makeParam(format, () => -323.25),
          makeParam(format, () => -7440),

          // Test mixed values
          makeParam(format, (bitLength, i) => {
            return [24896, -0.1319580078125, -323.25, -234.375][i];
          }),
        ];
      })
  )
  .fn(doTest);

g.test('ufloat_texel_data_in_shader')
  .params(
    params()
      .combine(poptions('format', kEncodableTextureFormats))
      .filter(({ format }) => {
        return (
          kEncodableTextureFormatInfo[format].copyDst &&
          kEncodableTextureFormatInfo[format].color &&
          getSingleDataType(format) === 'ufloat'
        );
      })
      .expand(({ format }) => {
        return [
          // Test extrema
          makeParam(format, () => 0),

          // TODO: Test NaN, Infinity

          // Test some values
          makeParam(format, () => 0.119140625),
          makeParam(format, () => 1.40625),
          makeParam(format, () => 24896),

          // Test scattered mixed values
          makeParam(format, (bitLength, i) => {
            return [24896, 1.40625, 0.119140625, 0.23095703125][i];
          }),

          // Test mixed values that are close in magnitude.
          makeParam(format, (bitLength, i) => {
            return [0.1337890625, 0.17919921875, 0.119140625, 0.125][i];
          }),
        ];
      })
  )
  .fn(doTest);
