/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for external textures with HTMLCanvasElement and OffscreenCanvas sources.

- x= {HTMLCanvasElement, OffscreenCanvas}
- x= {2d, webgl, webgl2, gpupresent, bitmaprenderer} context, {various context creation attributes}

TODO: consider whether external_texture and copyToTexture video tests should be in the same file
TODO: plan
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { GPUTest } from '../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
