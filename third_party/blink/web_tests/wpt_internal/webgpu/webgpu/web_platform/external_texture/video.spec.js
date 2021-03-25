/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for external textures from HTMLVideoElement (and other video-type sources?).

- videos with various encodings, color spaces, metadata

TODO: get test videos from WebGL CTS
TODO(kainino0x): (and set up the build to deal with data files)
TODO: consider whether external_texture and copyToTexture video tests should be in the same file
TODO: plan
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { GPUTest } from '../../gpu_test.js';

export const g = makeTestGroup(GPUTest);
