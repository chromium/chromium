/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for GPUSwapChain.getCurrentTexture.
`;
import { Fixture } from '../../../common/framework/fixture.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';

export const g = makeTestGroup(Fixture);

g.test('multiple_frames')
  .desc(`Checks the value of getCurrentTexture within one frame and across multiple frames.`)
  .unimplemented();
