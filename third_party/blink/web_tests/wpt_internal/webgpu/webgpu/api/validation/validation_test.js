/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../../common/framework/util/util.js';
import { GPUTest } from '../../gpu_test.js';

export const kEncoderTypes = ['non-pass', 'compute pass', 'render pass', 'render bundle'];

export class ValidationTest extends GPUTest {
  createTextureWithState(state, descriptor) {
    var _descriptor;
    descriptor =
      (_descriptor = descriptor) !== null && _descriptor !== void 0
        ? _descriptor
        : {
            size: { width: 1, height: 1, depth: 1 },
            format: 'rgba8unorm',
            usage:
              GPUTextureUsage.COPY_SRC |
              GPUTextureUsage.COPY_DST |
              GPUTextureUsage.SAMPLED |
              GPUTextureUsage.STORAGE |
              GPUTextureUsage.OUTPUT_ATTACHMENT,
          };

    switch (state) {
      case 'valid':
        return this.device.createTexture(descriptor);
      case 'invalid':
        return this.getErrorTexture();
      case 'destroyed': {
        const texture = this.device.createTexture(descriptor);
        texture.destroy();
        return texture;
      }
    }
  }

  createBufferWithState(state, descriptor) {
    var _descriptor2;
    descriptor =
      (_descriptor2 = descriptor) !== null && _descriptor2 !== void 0
        ? _descriptor2
        : {
            size: 4,
            usage: GPUBufferUsage.VERTEX,
          };

    switch (state) {
      case 'valid':
        return this.device.createBuffer(descriptor);

      case 'invalid': {
        // Make the buffer invalid because of an invalid combination of usages but keep the
        // descriptor passed as much as possible (for mappedAtCreation and friends).
        this.device.pushErrorScope('validation');
        const buffer = this.device.createBuffer({
          ...descriptor,
          usage: descriptor.usage | GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_SRC,
        });

        this.device.popErrorScope();
        return buffer;
      }
      case 'destroyed': {
        const buffer = this.device.createBuffer(descriptor);
        buffer.destroy();
        return buffer;
      }
    }
  }

  getStorageBuffer() {
    return this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.STORAGE });
  }

  getUniformBuffer() {
    return this.device.createBuffer({ size: 1024, usage: GPUBufferUsage.UNIFORM });
  }

  getErrorBuffer() {
    return this.createBufferWithState('invalid');
  }

  getSampler() {
    return this.device.createSampler();
  }

  getComparisonSampler() {
    return this.device.createSampler({ compare: 'never' });
  }

  getErrorSampler() {
    this.device.pushErrorScope('validation');
    const sampler = this.device.createSampler({ lodMinClamp: -1 });
    this.device.popErrorScope();
    return sampler;
  }

  getSampledTexture(sampleCount = 1) {
    return this.device.createTexture({
      size: { width: 16, height: 16, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.SAMPLED,
      sampleCount,
    });
  }

  getStorageTexture() {
    return this.device.createTexture({
      size: { width: 16, height: 16, depth: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.STORAGE,
    });
  }

  getErrorTexture() {
    this.device.pushErrorScope('validation');
    const texture = this.device.createTexture({
      size: { width: 0, height: 0, depth: 0 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.SAMPLED,
    });

    this.device.popErrorScope();
    return texture;
  }

  getErrorTextureView() {
    this.device.pushErrorScope('validation');
    const view = this.getErrorTexture().createView();
    this.device.popErrorScope();
    return view;
  }

  getBindingResource(bindingType) {
    switch (bindingType) {
      case 'errorBuf':
        return { buffer: this.getErrorBuffer() };
      case 'errorSamp':
        return this.getErrorSampler();
      case 'errorTex':
        return this.getErrorTextureView();
      case 'uniformBuf':
        return { buffer: this.getUniformBuffer() };
      case 'storageBuf':
        return { buffer: this.getStorageBuffer() };
      case 'plainSamp':
        return this.getSampler();
      case 'compareSamp':
        return this.getComparisonSampler();
      case 'sampledTex':
        return this.getSampledTexture(1).createView();
      case 'sampledTexMS':
        return this.getSampledTexture(4).createView();
      case 'storageTex':
        return this.getStorageTexture().createView();
    }
  }

  createNoOpRenderPipeline() {
    return this.device.createRenderPipeline({
      vertexStage: {
        module: this.device.createShaderModule({
          code: '[[stage(vertex)]] fn main() -> void {}',
        }),

        entryPoint: 'main',
      },

      fragmentStage: {
        module: this.device.createShaderModule({
          code: '[[stage(fragment)]] fn main() -> void {}',
        }),

        entryPoint: 'main',
      },

      primitiveTopology: 'triangle-list',
      colorStates: [{ format: 'rgba8unorm' }],
    });
  }

  createNoOpComputePipeline() {
    return this.device.createComputePipeline({
      computeStage: {
        module: this.device.createShaderModule({
          code: '[[stage(compute)]] fn main() -> void {}',
        }),

        entryPoint: 'main',
      },
    });
  }

  createErrorComputePipeline() {
    this.device.pushErrorScope('validation');
    const pipeline = this.device.createComputePipeline({
      computeStage: {
        module: this.device.createShaderModule({
          code: '',
        }),

        entryPoint: '',
      },
    });

    this.device.popErrorScope();
    return pipeline;
  }

  createEncoder(encoderType) {
    const colorFormat = 'rgba8unorm';
    switch (encoderType) {
      case 'non-pass': {
        const encoder = this.device.createCommandEncoder();
        return {
          encoder,

          finish: () => {
            return encoder.finish();
          },
        };
      }
      case 'render bundle': {
        const device = this.device;
        const encoder = device.createRenderBundleEncoder({
          colorFormats: [colorFormat],
        });

        const pass = this.createEncoder('render pass');
        return {
          encoder,
          finish: () => {
            const bundle = encoder.finish();
            pass.encoder.executeBundles([bundle]);
            return pass.finish();
          },
        };
      }
      case 'compute pass': {
        const commandEncoder = this.device.createCommandEncoder();
        const encoder = commandEncoder.beginComputePass();
        return {
          encoder,
          finish: () => {
            encoder.endPass();
            return commandEncoder.finish();
          },
        };
      }
      case 'render pass': {
        const commandEncoder = this.device.createCommandEncoder();
        const attachment = this.device
          .createTexture({
            format: colorFormat,
            size: { width: 16, height: 16, depth: 1 },
            usage: GPUTextureUsage.OUTPUT_ATTACHMENT,
          })
          .createView();
        const encoder = commandEncoder.beginRenderPass({
          colorAttachments: [
            {
              attachment,
              loadValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
            },
          ],
        });

        return {
          encoder,
          finish: () => {
            encoder.endPass();
            return commandEncoder.finish();
          },
        };
      }
    }
  }

  /**
   * Expect a validation error inside the callback.
   *
   * Tests should always do just one WebGPU call in the callback, to make sure that's what's tested.
   */
  // Note: A return value is not allowed for the callback function. This is to avoid confusion
  // about what the actual behavior would be. We could either:
  //   - Make expectValidationError async, and have it await on fn(). This causes an async split
  //     between pushErrorScope and popErrorScope, so if the caller doesn't `await` on
  //     expectValidationError (either accidentally or because it doesn't care to do so), then
  //     other test code will be (nondeterministically) caught by the error scope.
  //   - Make expectValidationError NOT await fn(), but just execute its first block (until the
  //     first await) and return the return value (a Promise). This would be confusing because it
  //     would look like the error scope includes the whole async function, but doesn't.
  expectValidationError(fn, shouldError = true) {
    // If no error is expected, we let the scope surrounding the test catch it.
    if (shouldError) {
      this.device.pushErrorScope('validation');
    }

    const returnValue = fn();
    assert(
      returnValue === undefined,
      'expectValidationError callback should not return a value (or be async)'
    );

    if (shouldError) {
      const promise = this.device.popErrorScope();

      this.eventualAsyncExpectation(async niceStack => {
        const gpuValidationError = await promise;
        if (!gpuValidationError) {
          niceStack.message = 'Validation error was expected.';
          this.rec.validationFailed(niceStack);
        } else if (gpuValidationError instanceof GPUValidationError) {
          niceStack.message = `Captured validation error - ${gpuValidationError.message}`;
          this.rec.debug(niceStack);
        }
      });
    }
  }
}
