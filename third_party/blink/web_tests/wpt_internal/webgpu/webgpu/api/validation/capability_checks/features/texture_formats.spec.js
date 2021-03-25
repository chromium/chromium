/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for capability checking for features enabling optional texture formats.

TODO:
- x= every optional texture format.
- x= every place in the API that takes a GPUTextureFormat (GPUTextureDescriptor,
  GPUTextureViewDescriptor, GPUStorageTextureBindingLayout, GPUColorTargetState,
  GPUDepthStencilState, GPURenderBundleEncoderDescriptor, maybe GPUSwapChainDescriptor).
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);
