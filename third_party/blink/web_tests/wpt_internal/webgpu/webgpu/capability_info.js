/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../common/framework/util/util.js';

import { GPUConst } from './constants.js';

function keysOf(obj) {
  return Object.keys(obj);
}

function numericKeysOf(obj) {
  return Object.keys(obj).map(n => Number(n));
}

/**
 * Creates an info lookup object from a more nicely-formatted table. See below for examples.
 *
 * Note: Using `as const` on the arguments to this function is necessary to infer the correct type.
 */
function makeTable(members, defaults, table) {
  const result = {};
  for (const [k, v] of Object.entries(table)) {
    const item = {};
    for (let i = 0; i < members.length; ++i) {
      item[members[i]] = v[i] ?? defaults[i];
    }
    result[k] = item;
  }

  return result;
}

// Queries

export const kMaxQueryCount = 8192;
export const kQueryTypes = ['occlusion', 'pipeline-statistics', 'timestamp'];

// Buffers

export const kBufferSizeAlignment = 4;

export const kBufferUsageInfo = {
  [GPUConst.BufferUsage.MAP_READ]: {},
  [GPUConst.BufferUsage.MAP_WRITE]: {},
  [GPUConst.BufferUsage.COPY_SRC]: {},
  [GPUConst.BufferUsage.COPY_DST]: {},
  [GPUConst.BufferUsage.INDEX]: {},
  [GPUConst.BufferUsage.VERTEX]: {},
  [GPUConst.BufferUsage.UNIFORM]: {},
  [GPUConst.BufferUsage.STORAGE]: {},
  [GPUConst.BufferUsage.INDIRECT]: {},
  [GPUConst.BufferUsage.QUERY_RESOLVE]: {},
};

export const kBufferUsages = numericKeysOf(kBufferUsageInfo);

// Textures

// Note that we repeat the header multiple times in order to make it easier to read.
export const kRegularTextureFormatInfo = makeTable(
  [
    'renderable',
    'multisample',
    'color',
    'depth',
    'stencil',
    'storage',
    'copySrc',
    'copyDst',
    'bytesPerBlock',
    'blockWidth',
    'blockHeight',
    'extension',
  ],
  [, true, true, false, false, , true, true, , 1, 1],
  {
    // 8-bit formats
    r8unorm: [true, , , , , false, , , 1],
    r8snorm: [false, , , , , false, , , 1],
    r8uint: [true, , , , , false, , , 1],
    r8sint: [true, , , , , false, , , 1],
    // 16-bit formats
    r16uint: [true, , , , , false, , , 2],
    r16sint: [true, , , , , false, , , 2],
    r16float: [true, , , , , false, , , 2],
    rg8unorm: [true, , , , , false, , , 2],
    rg8snorm: [false, , , , , false, , , 2],
    rg8uint: [true, , , , , false, , , 2],
    rg8sint: [true, , , , , false, , , 2],
    // 32-bit formats
    r32uint: [true, , , , , true, , , 4],
    r32sint: [true, , , , , true, , , 4],
    r32float: [true, , , , , true, , , 4],
    rg16uint: [true, , , , , false, , , 4],
    rg16sint: [true, , , , , false, , , 4],
    rg16float: [true, , , , , false, , , 4],
    rgba8unorm: [true, , , , , true, , , 4],
    'rgba8unorm-srgb': [true, , , , , false, , , 4],
    rgba8snorm: [false, , , , , true, , , 4],
    rgba8uint: [true, , , , , true, , , 4],
    rgba8sint: [true, , , , , true, , , 4],
    bgra8unorm: [true, , , , , false, , , 4],
    'bgra8unorm-srgb': [true, , , , , false, , , 4],
    // Packed 32-bit formats
    rgb10a2unorm: [true, , , , , false, , , 4],
    rg11b10ufloat: [false, , , , , false, , , 4],
    rgb9e5ufloat: [false, , , , , false, , , 4],
    // 64-bit formats
    rg32uint: [true, , , , , true, , , 8],
    rg32sint: [true, , , , , true, , , 8],
    rg32float: [true, , , , , true, , , 8],
    rgba16uint: [true, , , , , true, , , 8],
    rgba16sint: [true, , , , , true, , , 8],
    rgba16float: [true, , , , , true, , , 8],
    // 128-bit formats
    rgba32uint: [true, , , , , true, , , 16],
    rgba32sint: [true, , , , , true, , , 16],
    rgba32float: [true, , , , , true, , , 16],
  }
);

const kTexFmtInfoHeader = [
  'renderable',
  'multisample',
  'color',
  'depth',
  'stencil',
  'storage',
  'copySrc',
  'copyDst',
  'bytesPerBlock',
  'blockWidth',
  'blockHeight',
  'extension',
];
export const kSizedDepthStencilFormatInfo = makeTable(
  kTexFmtInfoHeader,
  [true, true, false, , , false, false, false, , 1, 1],
  {
    depth32float: [true, , , true, false, , , , 4],
    depth16unorm: [true, , , true, false, , , , 2],
    stencil8: [true, , , false, true, , , , 1],
  }
);

export const kUnsizedDepthStencilFormatInfo = makeTable(
  kTexFmtInfoHeader,
  [true, true, false, , , false, false, false, undefined, 1, 1],
  {
    depth24plus: [, , , true, false, , ,],
    'depth24plus-stencil8': [, , , true, true, , ,],
    // bytesPerBlock only makes sense on a per-aspect basis. But this table can't express that. So we put depth24unorm-stencil8 and depth32float-stencil8 to be unsized formats for now.
    'depth24unorm-stencil8': [, , , true, true, , , , , , , 'depth24unorm-stencil8'],
    'depth32float-stencil8': [, , , true, true, , , , , , , 'depth32float-stencil8'],
  }
);

export const kCompressedTextureFormatInfo = makeTable(
  kTexFmtInfoHeader,
  [false, false, true, false, false, false, true, true, , 4, 4],
  {
    'bc1-rgba-unorm': [, , , , , , , , 8, 4, 4, 'texture-compression-bc'],
    'bc1-rgba-unorm-srgb': [, , , , , , , , 8, 4, 4, 'texture-compression-bc'],
    'bc2-rgba-unorm': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc2-rgba-unorm-srgb': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc3-rgba-unorm': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc3-rgba-unorm-srgb': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc4-r-unorm': [, , , , , , , , 8, 4, 4, 'texture-compression-bc'],
    'bc4-r-snorm': [, , , , , , , , 8, 4, 4, 'texture-compression-bc'],
    'bc5-rg-unorm': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc5-rg-snorm': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc6h-rgb-ufloat': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc6h-rgb-float': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc7-rgba-unorm': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
    'bc7-rgba-unorm-srgb': [, , , , , , , , 16, 4, 4, 'texture-compression-bc'],
  }
);

export const kRegularTextureFormats = keysOf(kRegularTextureFormatInfo);
export const kSizedDepthStencilFormats = keysOf(kSizedDepthStencilFormatInfo);
export const kUnsizedDepthStencilFormats = keysOf(kUnsizedDepthStencilFormatInfo);
export const kCompressedTextureFormats = keysOf(kCompressedTextureFormatInfo);

export const kColorTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kCompressedTextureFormatInfo,
};

export const kColorTextureFormats = keysOf(kColorTextureFormatInfo);

export const kEncodableTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
};

export const kEncodableTextureFormats = keysOf(kEncodableTextureFormatInfo);

export const kSizedTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
  ...kCompressedTextureFormatInfo,
};

export const kSizedTextureFormats = keysOf(kSizedTextureFormatInfo);

export const kDepthStencilFormatInfo = {
  ...kSizedDepthStencilFormatInfo,
  ...kUnsizedDepthStencilFormatInfo,
};

export const kDepthStencilFormats = keysOf(kDepthStencilFormatInfo);

export const kUncompressedTextureFormatInfo = {
  ...kRegularTextureFormatInfo,
  ...kSizedDepthStencilFormatInfo,
  ...kUnsizedDepthStencilFormatInfo,
};

export const kUncompressedTextureFormats = keysOf(kUncompressedTextureFormatInfo);

export const kAllTextureFormatInfo = {
  ...kUncompressedTextureFormatInfo,
  ...kCompressedTextureFormatInfo,
};

export const kAllTextureFormats = keysOf(kAllTextureFormatInfo);
// Assert every GPUTextureFormat is covered by one of the tables.
(x => x)(kAllTextureFormatInfo);

export const kTextureDimensionInfo = {
  '1d': {},
  '2d': {},
  '3d': {},
};

export const kTextureDimensions = keysOf(kTextureDimensionInfo);

export const kTextureAspectInfo = {
  all: {},
  'depth-only': {},
  'stencil-only': {},
};

export const kTextureAspects = keysOf(kTextureAspectInfo);

const kDepthStencilFormatCapabilityInBufferTextureCopy = {
  // kUnsizedDepthStencilFormats
  depth24plus: {
    CopyB2T: [],
    CopyT2B: [],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': -1 },
  },

  'depth24plus-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },

  // kSizedDepthStencilFormats
  depth16unorm: {
    CopyB2T: ['all', 'depth-only'],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 2, 'stencil-only': -1 },
  },

  depth32float: {
    CopyB2T: [],
    CopyT2B: ['all', 'depth-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': -1 },
  },

  'depth24unorm-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['depth-only', 'stencil-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': 1 },
  },

  'depth32float-stencil8': {
    CopyB2T: ['stencil-only'],
    CopyT2B: ['depth-only', 'stencil-only'],
    texelAspectSize: { 'depth-only': 4, 'stencil-only': 1 },
  },

  stencil8: {
    CopyB2T: ['all', 'stencil-only'],
    CopyT2B: ['all', 'stencil-only'],
    texelAspectSize: { 'depth-only': -1, 'stencil-only': 1 },
  },
};

export function depthStencilBufferTextureCopySupported(type, format, aspect) {
  const supportedAspects = kDepthStencilFormatCapabilityInBufferTextureCopy[format][type];
  return supportedAspects.includes(aspect);
}

export function depthStencilFormatAspectSize(format, aspect) {
  const texelAspectSize =
    kDepthStencilFormatCapabilityInBufferTextureCopy[format].texelAspectSize[aspect];
  assert(texelAspectSize > 0);
  return texelAspectSize;
}

export const kTextureUsageInfo = {
  [GPUConst.TextureUsage.COPY_SRC]: {},
  [GPUConst.TextureUsage.COPY_DST]: {},
  [GPUConst.TextureUsage.SAMPLED]: {},
  [GPUConst.TextureUsage.STORAGE]: {},
  [GPUConst.TextureUsage.RENDER_ATTACHMENT]: {},
};

export const kTextureUsages = numericKeysOf(kTextureUsageInfo);

export const kTextureComponentTypeInfo = {
  float: {},
  sint: {},
  uint: {},
  'depth-comparison': {},
};

export const kTextureComponentTypes = keysOf(kTextureComponentTypeInfo);

// Texture View

export const kTextureViewDimensionInfo = {
  '1d': { storage: true },
  '2d': { storage: true },
  '2d-array': { storage: true },
  cube: { storage: false },
  'cube-array': { storage: false },
  '3d': { storage: true },
};

export const kTextureViewDimensions = keysOf(kTextureViewDimensionInfo);

// Vertex formats

export const kVertexFormatInfo = makeTable(['bytesPerComponent', 'type', 'componentCount'], [, ,], {
  // 8 bit components
  uint8x2: [1, 'uint', 2],
  uint8x4: [1, 'uint', 4],
  sint8x2: [1, 'sint', 2],
  sint8x4: [1, 'sint', 4],
  unorm8x2: [1, 'unorm', 2],
  unorm8x4: [1, 'unorm', 4],
  snorm8x2: [1, 'snorm', 2],
  snorm8x4: [1, 'snorm', 4],
  // 16 bit components
  uint16x2: [2, 'uint', 2],
  uint16x4: [2, 'uint', 4],
  sint16x2: [2, 'sint', 2],
  sint16x4: [2, 'sint', 4],
  unorm16x2: [2, 'unorm', 2],
  unorm16x4: [2, 'unorm', 4],
  snorm16x2: [2, 'snorm', 2],
  snorm16x4: [2, 'snorm', 4],
  float16x2: [2, 'float', 2],
  float16x4: [2, 'float', 4],
  // 32 bit components
  float32: [4, 'float', 1],
  float32x2: [4, 'float', 2],
  float32x3: [4, 'float', 3],
  float32x4: [4, 'float', 4],
  uint32: [4, 'uint', 1],
  uint32x2: [4, 'uint', 2],
  uint32x3: [4, 'uint', 3],
  uint32x4: [4, 'uint', 4],
  sint32: [4, 'sint', 1],
  sint32x2: [4, 'sint', 2],
  sint32x3: [4, 'sint', 3],
  sint32x4: [4, 'sint', 4],
});

export const kVertexFormats = keysOf(kVertexFormatInfo);

// Typedefs for bindings

// Bindings

export const kMaxBindingsPerBindGroup = 16;

export const kPerStageBindingLimits = {
  uniformBuf: { class: 'uniformBuf', max: 12 },
  storageBuf: { class: 'storageBuf', max: 4 },
  sampler: { class: 'sampler', max: 16 },
  sampledTex: { class: 'sampledTex', max: 16 },
  storageTex: { class: 'storageTex', max: 4 },
};

export const kPerPipelineBindingLimits = {
  uniformBuf: { class: 'uniformBuf', maxDynamic: 8 },
  storageBuf: { class: 'storageBuf', maxDynamic: 4 },
  sampler: { class: 'sampler', maxDynamic: 0 },
  sampledTex: { class: 'sampledTex', maxDynamic: 0 },
  storageTex: { class: 'storageTex', maxDynamic: 0 },
};

const kBindableResource = {
  uniformBuf: {},
  storageBuf: {},
  plainSamp: {},
  compareSamp: {},
  sampledTex: {},
  sampledTexMS: {},
  storageTex: {},
  errorBuf: {},
  errorSamp: {},
  errorTex: {},
};

export const kBindableResources = keysOf(kBindableResource);

const kBindingKind = {
  uniformBuf: {
    resource: 'uniformBuf',
    perStageLimitClass: kPerStageBindingLimits.uniformBuf,
    perPipelineLimitClass: kPerPipelineBindingLimits.uniformBuf,
  },
  storageBuf: {
    resource: 'storageBuf',
    perStageLimitClass: kPerStageBindingLimits.storageBuf,
    perPipelineLimitClass: kPerPipelineBindingLimits.storageBuf,
  },
  plainSamp: {
    resource: 'plainSamp',
    perStageLimitClass: kPerStageBindingLimits.sampler,
    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,
  },
  compareSamp: {
    resource: 'compareSamp',
    perStageLimitClass: kPerStageBindingLimits.sampler,
    perPipelineLimitClass: kPerPipelineBindingLimits.sampler,
  },
  sampledTex: {
    resource: 'sampledTex',
    perStageLimitClass: kPerStageBindingLimits.sampledTex,
    perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex,
  },
  sampledTexMS: {
    resource: 'sampledTexMS',
    perStageLimitClass: kPerStageBindingLimits.sampledTex,
    perPipelineLimitClass: kPerPipelineBindingLimits.sampledTex,
  },
  storageTex: {
    resource: 'storageTex',
    perStageLimitClass: kPerStageBindingLimits.storageTex,
    perPipelineLimitClass: kPerPipelineBindingLimits.storageTex,
  },
};

// Binding type info

const kValidStagesAll = {
  validStages:
    GPUConst.ShaderStage.VERTEX | GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
};

const kValidStagesStorageWrite = {
  validStages: GPUConst.ShaderStage.FRAGMENT | GPUConst.ShaderStage.COMPUTE,
};

export const kBufferBindingTypeInfo = {
  'uniform-buffer': {
    usage: GPUConst.BufferUsage.UNIFORM,
    ...kBindingKind.uniformBuf,
    ...kValidStagesAll,
  },
  'storage-buffer': {
    usage: GPUConst.BufferUsage.STORAGE,
    ...kBindingKind.storageBuf,
    ...kValidStagesStorageWrite,
  },
  'readonly-storage-buffer': {
    usage: GPUConst.BufferUsage.STORAGE,
    ...kBindingKind.storageBuf,
    ...kValidStagesAll,
  },
};

export const kBufferBindingTypes = keysOf(kBufferBindingTypeInfo);

export const kSamplerBindingTypeInfo = {
  sampler: { ...kBindingKind.plainSamp, ...kValidStagesAll },
  'comparison-sampler': { ...kBindingKind.compareSamp, ...kValidStagesAll },
};

export const kSamplerBindingTypes = keysOf(kSamplerBindingTypeInfo);

export const kTextureBindingTypeInfo = {
  'sampled-texture': {
    usage: GPUConst.TextureUsage.SAMPLED,
    ...kBindingKind.sampledTex,
    ...kValidStagesAll,
  },
  'multisampled-texture': {
    usage: GPUConst.TextureUsage.SAMPLED,
    ...kBindingKind.sampledTexMS,
    ...kValidStagesAll,
  },
  'writeonly-storage-texture': {
    usage: GPUConst.TextureUsage.STORAGE,
    ...kBindingKind.storageTex,
    ...kValidStagesStorageWrite,
  },
  'readonly-storage-texture': {
    usage: GPUConst.TextureUsage.STORAGE,
    ...kBindingKind.storageTex,
    ...kValidStagesAll,
  },
};

export const kTextureBindingTypes = keysOf(kTextureBindingTypeInfo);

// All binding types (merged from above)

export const kBindingTypeInfo = {
  ...kBufferBindingTypeInfo,
  ...kSamplerBindingTypeInfo,
  ...kTextureBindingTypeInfo,
};

export const kBindingTypes = keysOf(kBindingTypeInfo);

export const kShaderStages = [
  GPUConst.ShaderStage.VERTEX,
  GPUConst.ShaderStage.FRAGMENT,
  GPUConst.ShaderStage.COMPUTE,
];

export const kShaderStageCombinations = [0, 1, 2, 3, 4, 5, 6, 7];

// TODO: Update with all possible sample counts when defined
// TODO: Switch existing tests to use kTextureSampleCounts
export const kTextureSampleCounts = [1, 4];

// Pipeline limits

// TODO: Update maximum color attachments when defined
export const kMaxColorAttachments = 4;

export const kMaxVertexBuffers = 8;
export const kMaxVertexAttributes = 16;
export const kMaxVertexBufferArrayStride = 2048;
