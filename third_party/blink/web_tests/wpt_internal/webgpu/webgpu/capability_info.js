/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { GPUConst } from './constants.js';

function keysOf(obj) {
  return Object.keys(obj);
}

function numericKeysOf(obj) {
  return Object.keys(obj).map(n => Number(n));
}

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

export const kRegularTextureFormatInfo = {
  // 8-bit formats
  r8unorm: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 1,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'unorm',
    componentType: 'float',
  },
  r8snorm: {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 1,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'snorm',
    componentType: 'float',
  },
  r8uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 1,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  r8sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 1,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  // 16-bit formats
  r16uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 2,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  r16sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 2,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  r16float: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 2,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'float',
    componentType: 'float',
  },
  rg8unorm: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 2,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'unorm',
    componentType: 'float',
  },
  rg8snorm: {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 2,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'snorm',
    componentType: 'float',
  },
  rg8uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 2,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  rg8sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 2,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  // 32-bit formats
  r32uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  r32sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  r32float: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'float',
    componentType: 'float',
  },
  rg16uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  rg16sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  rg16float: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'float',
    componentType: 'float',
  },
  rgba8unorm: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'unorm',
    componentType: 'float',
  },
  'rgba8unorm-srgb': {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'unorm',
    componentType: 'float',
  },
  rgba8snorm: {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'snorm',
    componentType: 'float',
  },
  rgba8uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  rgba8sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  bgra8unorm: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'unorm',
    componentType: 'float',
  },
  'bgra8unorm-srgb': {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'unorm',
    componentType: 'float',
  },
  // Packed 32-bit formats
  rgb10a2unorm: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'unorm',
    componentType: 'float',
  },
  rg11b10ufloat: {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'ufloat',
    componentType: 'float',
  },
  rgb9e5ufloat: {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'ufloat',
    componentType: 'float',
  },
  // 64-bit formats
  rg32uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  rg32sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  rg32float: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'float',
    componentType: 'float',
  },
  rgba16uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  rgba16sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  rgba16float: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'float',
    componentType: 'float',
  },
  // 128-bit formats
  rgba32uint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'uint',
    componentType: 'uint',
  },
  rgba32sint: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'sint',
    componentType: 'sint',
  },
  rgba32float: {
    renderable: true,
    color: true,
    depth: false,
    stencil: false,
    storage: true,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'float',
    componentType: 'float',
  },
};

export const kRegularTextureFormats = keysOf(kRegularTextureFormatInfo);

export const kSizedDepthStencilFormatInfo = {
  depth32float: {
    renderable: true,
    color: false,
    depth: true,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: false,
    bytesPerBlock: 4,
    blockWidth: 1,
    blockHeight: 1,
    dataType: 'float',
    componentType: 'float',
  },
};

export const kSizedDepthStencilFormats = keysOf(kSizedDepthStencilFormatInfo);

export const kUnsizedDepthStencilFormatInfo = {
  depth24plus: {
    renderable: true,
    color: false,
    depth: true,
    stencil: false,
    storage: false,
    copySrc: false,
    copyDst: false,
    blockWidth: 1,
    blockHeight: 1,
  },
  'depth24plus-stencil8': {
    renderable: true,
    color: false,
    depth: true,
    stencil: true,
    storage: false,
    copySrc: false,
    copyDst: false,
    blockWidth: 1,
    blockHeight: 1,
  },
};

export const kUnsizedDepthStencilFormats = keysOf(kUnsizedDepthStencilFormatInfo);

export const kCompressedTextureFormatInfo = {
  // BC formats
  'bc1-rgba-unorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc1-rgba-unorm-srgb': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc2-rgba-unorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc2-rgba-unorm-srgb': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc3-rgba-unorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc3-rgba-unorm-srgb': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc4-r-unorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc4-r-snorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 8,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc5-rg-unorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc5-rg-snorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc6h-rgb-ufloat': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc6h-rgb-float': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc7-rgba-unorm': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
  'bc7-rgba-unorm-srgb': {
    renderable: false,
    color: true,
    depth: false,
    stencil: false,
    storage: false,
    copySrc: true,
    copyDst: true,
    bytesPerBlock: 16,
    blockWidth: 4,
    blockHeight: 4,
    extension: 'texture-compression-bc',
  },
};

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

export const kTextureUsageInfo = {
  [GPUConst.TextureUsage.COPY_SRC]: {},
  [GPUConst.TextureUsage.COPY_DST]: {},
  [GPUConst.TextureUsage.SAMPLED]: {},
  [GPUConst.TextureUsage.STORAGE]: {},
  [GPUConst.TextureUsage.OUTPUT_ATTACHMENT]: {},
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
    ...kBindingKind.sampledTex,
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
