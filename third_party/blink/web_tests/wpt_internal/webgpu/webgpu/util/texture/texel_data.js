/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert, unreachable } from '../../../common/framework/util/util.js';
import {
  assertInIntegerRange,
  float32ToFloatBits,
  floatAsNormalizedInteger,
  gammaCompress,
  gammaDecompress,
  normalizedIntegerAsFloat,
  packRGB9E5UFloat,
} from '../conversion.js';

export let TexelComponent;
(function (TexelComponent) {
  TexelComponent['R'] = 'R';
  TexelComponent['G'] = 'G';
  TexelComponent['B'] = 'B';
  TexelComponent['A'] = 'A';
  TexelComponent['Depth'] = 'Depth';
  TexelComponent['Stencil'] = 'Stencil';
})(TexelComponent || (TexelComponent = {}));

/**
 * Create a PerTexelComponent object filled with the same value for all components.
 * @param {TexelComponent[]} components - The component names.
 * @param {T} value - The value to assign to each component.
 * @returns {PerTexelComponent<T>}
 */
function makePerTexelComponent(components, value) {
  const values = {};
  for (const c of components) {
    values[c] = value;
  }
  return values;
}

/**
 * Create a function which applies clones a `PerTexelComponent<number>` and then applies the
 * function `fn` to each component of `components`.
 * @param {(value: number) => number} fn - The mapping function to apply to component values.
 * @param {TexelComponent[]} components - The component names.
 * @returns {ComponentMapFn} The map function which clones the input component values, and applies
 *                           `fn` to each of component of `components`.
 */
function applyEach(fn, components) {
  return values => {
    values = Object.assign({}, values);
    for (const c of components) {
      assert(values[c] !== undefined);
      values[c] = fn(values[c]);
    }
    return values;
  };
}

/**
 * A `ComponentMapFn` for encoding sRGB.
 * @param {PerTexelComponent<number>} components - The input component values.
 * @returns {TexelComponent<number>} Gamma-compressed copy of `components`.
 */
const encodeSRGB = components => {
  assert(
    components.R !== undefined && components.G !== undefined && components.B !== undefined,
    'sRGB requires all of R, G, and B components'
  );

  return applyEach(gammaCompress, kRGB)(components);
};

/**
 * A `ComponentMapFn` for decoding sRGB.
 * @param {PerTexelComponent<number>} components - The input component values.
 * @returns {TexelComponent<number>} Gamma-decompressed copy of `components`.
 */
const decodeSRGB = components => {
  components = Object.assign({}, components);
  assert(
    components.R !== undefined && components.G !== undefined && components.B !== undefined,
    'sRGB requires all of R, G, and B components'
  );

  return applyEach(gammaDecompress, kRGB)(components);
};

/**
 * Helper function to pack components as an ArrayBuffer.
 * @param {TexelComponent[]} componentOrder - The order of the component data.
 * @param {PerTexelComponent<number>} components - The input component values.
 * @param {number | PerTexelComponent<number>} bitLengths - The length in bits of each component.
 *   If a single number, all components are the same length, otherwise this is a dictionary of
 *   per-component bit lengths.
 * @param {ComponentDataType | PerTexelComponent<ComponentDataType>} componentDataTypes -
 *   The type of the data in `components`. If a single value, all components have the same value.
 *   Otherwise, this is a dictionary of per-component data types.
 * @returns {ArrayBuffer} The packed component data.
 */
function packComponents(componentOrder, components, bitLengths, componentDataTypes) {
  const bitLengthMap =
    typeof bitLengths === 'number' ? makePerTexelComponent(componentOrder, bitLengths) : bitLengths;

  const componentDataTypeMap =
    typeof componentDataTypes === 'string' || componentDataTypes === null
      ? makePerTexelComponent(componentOrder, componentDataTypes)
      : componentDataTypes;

  const totalBitLength = Object.entries(bitLengthMap).reduce((acc, [, value]) => {
    assert(value !== undefined);
    return acc + value;
  }, 0);
  assert(totalBitLength % 8 === 0);

  const data = new ArrayBuffer(totalBitLength / 8);
  let bitOffset = 0;
  for (const c of componentOrder) {
    const value = components[c];
    const type = componentDataTypeMap[c];
    const bitLength = bitLengthMap[c];
    assert(value !== undefined);
    assert(type !== undefined);
    assert(bitLength !== undefined);

    const byteOffset = Math.floor(bitOffset / 8);
    const byteLength = Math.ceil(bitLength / 8);
    switch (type) {
      case 'uint':
      case 'unorm':
        if (byteOffset === bitOffset / 8 && byteLength === bitLength / 8) {
          switch (byteLength) {
            case 1:
              new DataView(data, byteOffset, byteLength).setUint8(0, value);
              break;
            case 2:
              new DataView(data, byteOffset, byteLength).setUint16(0, value, true);
              break;
            case 4:
              new DataView(data, byteOffset, byteLength).setUint32(0, value, true);
              break;
            default:
              unreachable();
          }
        } else {
          // Packed representations are all 32-bit and use Uint as the data type.
          // ex.) rg10b11float, rgb10a2unorm
          const view = new DataView(data);
          switch (view.byteLength) {
            case 4: {
              const currentValue = view.getUint32(0, true);

              let mask = 0xffffffff;
              const bitsToClearRight = bitOffset;
              const bitsToClearLeft = 32 - (bitLength + bitOffset);

              mask = (mask >>> bitsToClearRight) << bitsToClearRight;
              mask = (mask << bitsToClearLeft) >>> bitsToClearLeft;

              const newValue = (currentValue & ~mask) | (value << bitOffset);

              view.setUint32(0, newValue, true);
              break;
            }
            default:
              unreachable();
          }
        }
        break;
      case 'sint':
      case 'snorm':
        assert(byteOffset === bitOffset / 8 && byteLength === bitLength / 8);
        switch (byteLength) {
          case 1:
            new DataView(data, byteOffset, byteLength).setInt8(0, value);
            break;
          case 2:
            new DataView(data, byteOffset, byteLength).setInt16(0, value, true);
            break;
          case 4:
            new DataView(data, byteOffset, byteLength).setInt32(0, value, true);
            break;
          default:
            unreachable();
        }

        break;
      case 'float':
        assert(byteOffset === bitOffset / 8 && byteLength === bitLength / 8);
        switch (byteLength) {
          case 4:
            new DataView(data, byteOffset, byteLength).setFloat32(0, value, true);
            break;
          default:
            unreachable();
        }

        break;
      case 'ufloat':
      case null:
        unreachable();
    }

    bitOffset += bitLength;
  }

  return data;
}

/**
 * Create an entry in `kTexelRepresentationInfo` for normalized integer texel data with constant
 * bitlength.
 * @param {TexelComponent[]} componentOrder - The order of the component data.
 * @param {number} bitLength - The number of bits in each component.
 * @param {{signed: boolean; sRGB: boolean}} opt - Boolean flags for `signed` and `sRGB`.
 */
function makeNormalizedInfo(componentOrder, bitLength, opt) {
  const encodeNonSRGB = applyEach(
    n => floatAsNormalizedInteger(n, bitLength, opt.signed),
    componentOrder
  );

  const decodeNonSRGB = applyEach(
    n => normalizedIntegerAsFloat(n, bitLength, opt.signed),
    componentOrder
  );

  let encode;
  let decode;
  if (opt.sRGB) {
    encode = components => encodeNonSRGB(encodeSRGB(components));
    decode = components => decodeSRGB(decodeNonSRGB(components));
  } else {
    encode = encodeNonSRGB;
    decode = decodeNonSRGB;
  }

  const dataType = opt.signed ? 'snorm' : 'unorm';
  return {
    componentOrder,
    componentInfo: makePerTexelComponent(componentOrder, {
      dataType,
      bitLength,
    }),

    encode,
    decode,
    pack: components => packComponents(componentOrder, components, bitLength, dataType),
  };
}

/**
 * Create an entry in `kTexelRepresentationInfo` for integer texel data with constant bitlength.
 * @param {TexelComponent[]} componentOrder - The order of the component data.
 * @param {number} bitLength - The number of bits in each component.
 * @param {{signed: boolean}} opt - Boolean flag for `signed`.
 */
function makeIntegerInfo(componentOrder, bitLength, opt) {
  const encode = applyEach(
    n => (assertInIntegerRange(n, bitLength, opt.signed), n),
    componentOrder
  );

  const decode = applyEach(
    n => (assertInIntegerRange(n, bitLength, opt.signed), n),
    componentOrder
  );

  const dataType = opt.signed ? 'sint' : 'uint';
  return {
    componentOrder,
    componentInfo: makePerTexelComponent(componentOrder, {
      dataType,
      bitLength,
    }),

    encode,
    decode,
    pack: components => packComponents(componentOrder, components, bitLength, dataType),
  };
}

/**
 * Create an entry in `kTexelRepresentationInfo` for floating point texel data with constant
 * bitlength.
 * @param {TexelComponent[]} componentOrder - The order of the component data.
 * @param {number} bitLength - The number of bits in each component.
 */
function makeFloatInfo(componentOrder, bitLength) {
  // TODO: Use |bitLength| to round float values based on precision.
  const encode = applyEach(identity, componentOrder);
  const decode = applyEach(identity, componentOrder);

  return {
    componentOrder,
    componentInfo: makePerTexelComponent(componentOrder, {
      dataType: 'float',
      bitLength,
    }),

    encode,
    decode,
    pack: components => {
      switch (bitLength) {
        case 16:
          components = applyEach(
            n => float32ToFloatBits(n, 1, 5, 10, 15),
            componentOrder
          )(components);
          return packComponents(componentOrder, components, 16, 'uint');
        case 32:
          return packComponents(componentOrder, components, bitLength, 'float');
        default:
          unreachable();
      }
    },
  };
}

const kR = [TexelComponent.R];
const kRG = [TexelComponent.R, TexelComponent.G];
const kRGB = [TexelComponent.R, TexelComponent.G, TexelComponent.B];
const kRGBA = [TexelComponent.R, TexelComponent.G, TexelComponent.B, TexelComponent.A];
const kBGRA = [TexelComponent.B, TexelComponent.G, TexelComponent.R, TexelComponent.A];

const identity = n => n;

export const kTexelRepresentationInfo = {
  ...{
    r8unorm: makeNormalizedInfo(kR, 8, { signed: false, sRGB: false }),
    r8snorm: makeNormalizedInfo(kR, 8, { signed: true, sRGB: false }),
    r8uint: makeIntegerInfo(kR, 8, { signed: false }),
    r8sint: makeIntegerInfo(kR, 8, { signed: true }),
    r16uint: makeIntegerInfo(kR, 16, { signed: false }),
    r16sint: makeIntegerInfo(kR, 16, { signed: true }),
    r16float: makeFloatInfo(kR, 16),
    rg8unorm: makeNormalizedInfo(kRG, 8, { signed: false, sRGB: false }),
    rg8snorm: makeNormalizedInfo(kRG, 8, { signed: true, sRGB: false }),
    rg8uint: makeIntegerInfo(kRG, 8, { signed: false }),
    rg8sint: makeIntegerInfo(kRG, 8, { signed: true }),
    r32uint: makeIntegerInfo(kR, 32, { signed: false }),
    r32sint: makeIntegerInfo(kR, 32, { signed: true }),
    r32float: makeFloatInfo(kR, 32),
    rg16uint: makeIntegerInfo(kRG, 16, { signed: false }),
    rg16sint: makeIntegerInfo(kRG, 16, { signed: true }),
    rg16float: makeFloatInfo(kRG, 16),
    rgba8unorm: makeNormalizedInfo(kRGBA, 8, { signed: false, sRGB: false }),
    'rgba8unorm-srgb': makeNormalizedInfo(kRGBA, 8, { signed: false, sRGB: true }),
    rgba8snorm: makeNormalizedInfo(kRGBA, 8, { signed: true, sRGB: false }),
    rgba8uint: makeIntegerInfo(kRGBA, 8, { signed: false }),
    rgba8sint: makeIntegerInfo(kRGBA, 8, { signed: true }),
    bgra8unorm: makeNormalizedInfo(kBGRA, 8, { signed: false, sRGB: false }),
    'bgra8unorm-srgb': makeNormalizedInfo(kBGRA, 8, { signed: false, sRGB: true }),
    rg32uint: makeIntegerInfo(kRG, 32, { signed: false }),
    rg32sint: makeIntegerInfo(kRG, 32, { signed: true }),
    rg32float: makeFloatInfo(kRG, 32),
    rgba16uint: makeIntegerInfo(kRGBA, 16, { signed: false }),
    rgba16sint: makeIntegerInfo(kRGBA, 16, { signed: true }),
    rgba16float: makeFloatInfo(kRGBA, 16),
    rgba32uint: makeIntegerInfo(kRGBA, 32, { signed: false }),
    rgba32sint: makeIntegerInfo(kRGBA, 32, { signed: true }),
    rgba32float: makeFloatInfo(kRGBA, 32),
  },

  ...{
    rgb10a2unorm: {
      componentOrder: kRGBA,
      componentInfo: {
        R: { dataType: 'unorm', bitLength: 10 },
        G: { dataType: 'unorm', bitLength: 10 },
        B: { dataType: 'unorm', bitLength: 10 },
        A: { dataType: 'unorm', bitLength: 2 },
      },

      encode: components => {
        var _components$R, _components$G, _components$B, _components$A;
        return {
          R: floatAsNormalizedInteger(
            (_components$R = components.R) !== null && _components$R !== void 0
              ? _components$R
              : unreachable(),
            10,
            false
          ),
          G: floatAsNormalizedInteger(
            (_components$G = components.G) !== null && _components$G !== void 0
              ? _components$G
              : unreachable(),
            10,
            false
          ),
          B: floatAsNormalizedInteger(
            (_components$B = components.B) !== null && _components$B !== void 0
              ? _components$B
              : unreachable(),
            10,
            false
          ),
          A: floatAsNormalizedInteger(
            (_components$A = components.A) !== null && _components$A !== void 0
              ? _components$A
              : unreachable(),
            2,
            false
          ),
        };
      },
      decode: components => {
        var _components$R2, _components$G2, _components$B2, _components$A2;
        return {
          R: normalizedIntegerAsFloat(
            (_components$R2 = components.R) !== null && _components$R2 !== void 0
              ? _components$R2
              : unreachable(),
            10,
            false
          ),
          G: normalizedIntegerAsFloat(
            (_components$G2 = components.G) !== null && _components$G2 !== void 0
              ? _components$G2
              : unreachable(),
            10,
            false
          ),
          B: normalizedIntegerAsFloat(
            (_components$B2 = components.B) !== null && _components$B2 !== void 0
              ? _components$B2
              : unreachable(),
            10,
            false
          ),
          A: normalizedIntegerAsFloat(
            (_components$A2 = components.A) !== null && _components$A2 !== void 0
              ? _components$A2
              : unreachable(),
            2,
            false
          ),
        };
      },
      pack: components =>
        packComponents(
          kRGBA,
          components,
          {
            R: 10,
            G: 10,
            B: 10,
            A: 2,
          },

          'uint'
        ),
    },

    rg11b10ufloat: {
      componentOrder: kRGB,
      encode: applyEach(identity, kRGB),
      decode: applyEach(identity, kRGB),
      componentInfo: {
        R: { dataType: 'ufloat', bitLength: 11 },
        G: { dataType: 'ufloat', bitLength: 11 },
        B: { dataType: 'ufloat', bitLength: 10 },
      },

      pack: components => {
        var _components$R3, _components$G3, _components$B3;
        components = {
          R: float32ToFloatBits(
            (_components$R3 = components.R) !== null && _components$R3 !== void 0
              ? _components$R3
              : unreachable(),
            0,
            5,
            6,
            15
          ),
          G: float32ToFloatBits(
            (_components$G3 = components.G) !== null && _components$G3 !== void 0
              ? _components$G3
              : unreachable(),
            0,
            5,
            6,
            15
          ),
          B: float32ToFloatBits(
            (_components$B3 = components.B) !== null && _components$B3 !== void 0
              ? _components$B3
              : unreachable(),
            0,
            5,
            5,
            15
          ),
        };

        return packComponents(
          kRGB,
          components,
          {
            R: 11,
            G: 11,
            B: 10,
          },

          'uint'
        );
      },
    },

    rgb9e5ufloat: {
      componentOrder: kRGB,
      componentInfo: makePerTexelComponent(kRGB, {
        dataType: 'ufloat',
        bitLength: -1, // Components don't really have a bitLength since the format is packed.
      }),
      encode: applyEach(identity, kRGB),
      decode: applyEach(identity, kRGB),
      pack: components => {
        var _components$R4, _components$G4, _components$B4;
        return new Uint32Array([
          packRGB9E5UFloat(
            (_components$R4 = components.R) !== null && _components$R4 !== void 0
              ? _components$R4
              : unreachable(),
            (_components$G4 = components.G) !== null && _components$G4 !== void 0
              ? _components$G4
              : unreachable(),
            (_components$B4 = components.B) !== null && _components$B4 !== void 0
              ? _components$B4
              : unreachable()
          ),
        ]).buffer;
      },
    },

    depth32float: {
      componentOrder: [TexelComponent.Depth],
      encode: applyEach(n => (assert(n >= 0 && n <= 1.0), n), [TexelComponent.Depth]),
      decode: applyEach(n => (assert(n >= 0 && n <= 1.0), n), [TexelComponent.Depth]),
      componentInfo: { Depth: { dataType: 'float', bitLength: 32 } },
      pack: components => packComponents([TexelComponent.Depth], components, 32, 'float'),
    },

    depth24plus: {
      componentOrder: [TexelComponent.Depth],
      componentInfo: { Depth: { dataType: null, bitLength: 24 } },
      encode: applyEach(() => unreachable('depth24plus cannot be encoded'), [TexelComponent.Depth]),
      decode: applyEach(() => unreachable('depth24plus cannot be decoded'), [TexelComponent.Depth]),
      pack: () => unreachable('depth24plus data cannot be packed'),
    },

    'depth24plus-stencil8': {
      componentOrder: [TexelComponent.Depth, TexelComponent.Stencil],
      componentInfo: {
        Depth: {
          dataType: null,
          bitLength: 24,
        },

        Stencil: {
          dataType: 'uint',
          bitLength: 8,
        },
      },

      encode: components => {
        assert(components.Depth === undefined, 'depth24plus cannot be encoded');
        assert(components.Stencil !== undefined);
        assertInIntegerRange(components.Stencil, 8, false);
        return components;
      },
      decode: components => {
        assert(components.Depth === undefined, 'depth24plus cannot be decoded');
        assert(components.Stencil !== undefined);
        assertInIntegerRange(components.Stencil, 8, false);
        return components;
      },
      pack: () => unreachable('depth24plus-stencil8 data cannot be packed'),
    },
  },
};

/**
 * Get the `ComponentDataType` for a format. All components must have the same type.
 * @param {UncompressedTextureFormat} format - The input format.
 * @returns {ComponentDataType} The data of the components.
 */
export function getSingleDataType(format) {
  const infos = Object.values(kTexelRepresentationInfo[format].componentInfo);
  assert(infos.length > 0);
  return infos.reduce((acc, cur) => {
    assert(cur !== undefined);
    assert(acc === undefined || acc === cur.dataType);
    return cur.dataType;
  }, infos[0].dataType);
}

/**
 *  Get traits for generating code to readback data from a component.
 * @param {ComponentDataType} dataType - The input component data type.
 * @returns A dictionary containing the respective `ReadbackTypedArray` and `shaderType`.
 */
export function getComponentReadbackTraits(dataType) {
  switch (dataType) {
    case 'ufloat':
    case 'float':
    case 'unorm':
    case 'snorm':
      return {
        ReadbackTypedArray: Float32Array,
        shaderType: 'f32',
      };

    case 'uint':
      return {
        ReadbackTypedArray: Uint32Array,
        shaderType: 'u32',
      };

    case 'sint':
      return {
        ReadbackTypedArray: Int32Array,
        shaderType: 'i32',
      };

    default:
      unreachable();
  }
}
