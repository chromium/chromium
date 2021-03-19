/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ShaderValidationTest } from '../shader_validation_test.js';
export const description = `
Basic WGSL validation tests to test the ShaderValidationTest fixture.
`;

export const g = makeTestGroup(ShaderValidationTest);

g.test('trivial')
  .desc(`A trivial correct and incorrect shader.`)
  .fn(t => {
    t.expectCompileResult(true, `[[stage(vertex)]] fn main() -> void {}`);
    t.expectCompileResult('v-0020', `[[stage(vertex), stage(fragment)]] fn main() -> void {}`);
  });

g.test('nonsense')
  .desc(`Pass short nonsense strings as WGSL.`)
  .fn(t => {
    t.expectCompileResult(false, `nonsense`);
  });
