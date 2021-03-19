/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { GPUTest } from '../../gpu_test.js';
export class ShaderValidationTest extends GPUTest {
  /**
   * Add a test expectation for whether a createShaderModule call succeeds or not.
   *
   * @example
   * ```ts
   * t.expectCompileResult(true, `wgsl code`); // Expect success
   * t.expectCompileResult(false, `wgsl code`); // Expect validation error with any error string
   * t.expectCompileResult('v-0000', `wgsl code`); // Expect validation error containing 'v-0000'
   * ```
   */
  expectCompileResult(result, code) {
    // If an error is expected, push an error scope to catch it.
    // Otherwise, the test harness will catch unexpected errors.
    if (result !== true) {
      this.device.pushErrorScope('validation');
    }

    const shaderModule = this.device.createShaderModule({ code });

    if (result !== true) {
      const promise = this.device.popErrorScope();

      this.eventualAsyncExpectation(async niceStack => {
        // TODO: This is a non-compliant fallback path for Chrome, which doesn't
        // implement .compilationInfo() yet. Remove it.
        if (!shaderModule.compilationInfo) {
          const gpuValidationError = await promise;
          if (!gpuValidationError) {
            niceStack.message = 'Compilation succeeded unexpectedly.';
            this.rec.validationFailed(niceStack);
          } else if (gpuValidationError instanceof GPUValidationError) {
            if (typeof result === 'string' && gpuValidationError.message.indexOf(result) === -1) {
              niceStack.message = `Compilation failed, but message missing expected substring \
«${result}» - ${gpuValidationError.message}`;
              this.rec.validationFailed(niceStack);
            } else {
              niceStack.message = `Compilation failed, as expected - ${gpuValidationError.message}`;
              this.rec.debug(niceStack);
            }
          }
          return;
        }

        if (typeof result === 'string') {
          const info = await shaderModule.compilationInfo();
          for (const message of info.messages) {
            if (message.type === 'error' && message.message.indexOf(result) !== -1) {
              niceStack.message = `Compilation failed, as expected - \
${message.lineNum}:${message.linePos}: ${message.message}`;
              this.rec.debug(niceStack);
              return;
            }
          }
          // Here, the expected string was not found.

          // TODO: Pretty-print error messages, with source context.
          const messagesLog = info.messages
            .map(m => `${m.lineNum}:${m.linePos}: ${m.type}: ${m.message}`)
            .join('\n');
          niceStack.message = `Compilation failed, but no error message with expected substring \
«${result}»\n${messagesLog}`;
          this.rec.validationFailed(niceStack);
        }
      });
    }
  }
}
