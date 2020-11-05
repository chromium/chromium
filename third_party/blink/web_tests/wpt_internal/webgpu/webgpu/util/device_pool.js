/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { SkipTestCase } from '../../common/framework/fixture.js';
import {
  assert,
  raceWithRejectOnTimeout,
  unreachable,
  assertReject,
} from '../../common/framework/util/util.js';
import { DefaultLimits } from '../constants.js';

import { getGPU } from './navigator_gpu.js';

class TestFailedButDeviceReusable extends Error {}
export class TestOOMedShouldAttemptGC extends Error {}

export class DevicePool {
  /** Device with no descriptor. */
  defaultHolder = 'uninitialized';
  /** Devices with descriptors. */
  nonDefaultHolders = new DescriptorToHolderMap();

  /** Request a device from the pool. */
  async reserve(descriptor) {
    // Always attempt to initialize default device, to see if it succeeds.
    if (this.defaultHolder === 'uninitialized') {
      try {
        this.defaultHolder = await DeviceHolder.create(undefined);
      } catch (ex) {
        this.defaultHolder = 'failed';
      }
    }
    assert(this.defaultHolder !== 'failed', 'WebGPU device failed to initialize; not retrying');

    let holder;
    if (descriptor === undefined) {
      holder = this.defaultHolder;
    } else {
      holder = await this.nonDefaultHolders.getOrCreate(descriptor);
    }

    assert(holder.state === 'free', 'Device was in use on DevicePool.acquire');
    holder.state = 'reserved';
    return holder;
  }

  // When a test is done using a device, it's released back into the pool.
  // This waits for error scopes, checks their results, and checks for various error conditions.
  async release(holder) {
    assert(this.defaultHolder instanceof DeviceHolder);
    assert(holder instanceof DeviceHolder);

    assert(holder.state !== 'free', 'trying to release a device while already released');

    try {
      await holder.ensureRelease();

      // (Hopefully if the device was lost, it has been reported by the time endErrorScopes()
      // has finished (or timed out). If not, it could cause a finite number of extra test
      // failures following this one (but should recover eventually).)
      const lostReason = holder.lostReason;
      if (lostReason !== undefined) {
        // Fail the current test.
        unreachable(`Device was lost: ${lostReason}`);
      }
    } catch (ex) {
      // Any error that isn't explicitly TestFailedButDeviceReusable forces a new device to be
      // created for the next test.
      if (!(ex instanceof TestFailedButDeviceReusable)) {
        if (holder === this.defaultHolder) {
          this.defaultHolder = 'uninitialized';
        } else {
          this.nonDefaultHolders.deleteByDevice(holder.device);
        }
        // TODO: device.destroy()
      }
      throw ex;
    } finally {
      // Mark the holder as free. (This only has an effect if the pool still has the holder.)
      // This could be done at the top but is done here to guard against async-races during release.
      holder.state = 'free';
    }
  }
}

/**
 * Map from GPUDeviceDescriptor to DeviceHolder.
 */
class DescriptorToHolderMap {
  unsupported = new Set();
  holders = new Map();

  /** Deletes an item from the map by GPUDevice value. */
  deleteByDevice(device) {
    for (const [k, v] of this.holders) {
      if (v.device === device) {
        this.holders.delete(k);
        return;
      }
    }
  }

  /**
   * Gets a DeviceHolder from the map if it exists; otherwise, calls create() to create one,
   * inserts it, and returns it.
   *
   * Throws SkipTestCase if devices with this descriptor are unsupported.
   */
  async getOrCreate(uncanonicalizedDescriptor) {
    const [descriptor, key] = canonicalizeDescriptor(uncanonicalizedDescriptor);
    // Never retry unsupported configurations.
    if (this.unsupported.has(key)) {
      throw new SkipTestCase(
        `GPUDeviceDescriptor previously failed: ${JSON.stringify(descriptor)}`
      );
    }

    // Search for an existing device with the same descriptor.
    {
      const value = this.holders.get(key);
      if (value) {
        // Move it to the end of the Map (most-recently-used).
        this.holders.delete(key);
        this.holders.set(key, value);
        return value;
      }
    }

    // No existing item was found; add a new one.
    let value;
    try {
      value = await DeviceHolder.create(descriptor);
    } catch (ex) {
      var _ex$message;
      this.unsupported.add(key);
      throw new SkipTestCase(
        `GPUDeviceDescriptor not supported: ${JSON.stringify(descriptor)}\n${
          (_ex$message = ex === null || ex === void 0 ? void 0 : ex.message) !== null &&
          _ex$message !== void 0
            ? _ex$message
            : ''
        }`
      );
    }
    this.insertAndCleanUp(key, value);
    return value;
  }

  /** Insert an entry, then remove the least-recently-used items if there are too many. */
  insertAndCleanUp(key, value) {
    this.holders.set(key, value);

    const kMaxEntries = 5;
    if (this.holders.size > kMaxEntries) {
      // Delete the first (least recently used) item in the set.
      for (const [key] of this.holders) {
        this.holders.delete(key);
        return;
      }
    }
  }
}

/**
 * Make a stringified map-key from a GPUDeviceDescriptor.
 * Tries to make sure all defaults are resolved, first - but it's okay if some are missed
 * (it just means some GPUDevice objects won't get deduplicated).
 */
function canonicalizeDescriptor(desc) {
  const extensionsCanonicalized = desc.extensions ? Array.from(desc.extensions).sort() : [];
  const limits = { ...desc.limits };

  const limitsCanonicalized = { ...DefaultLimits };
  for (const k of Object.keys(limits)) {
    if (limits[k] !== undefined) {
      limitsCanonicalized[k] = limits[k];
    }
  }

  // Type ensures every field is carried through.
  const descriptorCanonicalized = {
    extensions: extensionsCanonicalized,
    limits: limitsCanonicalized,
  };

  return [descriptorCanonicalized, JSON.stringify(descriptorCanonicalized)];
}

/**
 * DeviceHolder has three states:
 * - 'free': Free to be used for a new test.
 * - 'reserved': Reserved by a running test, but has not had error scopes created yet.
 * - 'acquired': Reserved by a running test, and has had error scopes created.
 */

/**
 * Holds a GPUDevice and tracks its state (free/reserved/acquired) and handles device loss.
 */
class DeviceHolder {
  state = 'free';
  // initially undefined; becomes set when the device is lost

  // Gets a device and creates a DeviceHolder.
  // If the device is lost, DeviceHolder.lostReason gets set.
  static async create(descriptor) {
    const gpu = getGPU();
    const adapter = await gpu.requestAdapter();
    assert(adapter !== null, 'requestAdapter returned null');
    const device = await adapter.requestDevice(descriptor);
    assert(device !== null, 'requestDevice returned null');

    return new DeviceHolder(device);
  }

  constructor(device) {
    this.device = device;
    this.device.lost.then(ev => {
      this.lostReason = ev.message;
    });
  }

  acquire() {
    assert(this.state === 'reserved');
    this.state = 'acquired';
    this.device.pushErrorScope('out-of-memory');
    this.device.pushErrorScope('validation');
    return this.device;
  }

  async ensureRelease() {
    const kPopErrorScopeTimeoutMS = 5000;

    assert(this.state !== 'free');
    try {
      if (this.state === 'acquired') {
        // Time out if popErrorScope never completes. This could happen due to a browser bug - e.g.,
        // as of this writing, on Chrome GPU process crash, popErrorScope just hangs.
        await raceWithRejectOnTimeout(
          this.release(),
          kPopErrorScopeTimeoutMS,
          'finalization popErrorScope timed out'
        );
      }
    } finally {
      this.state = 'free';
    }
  }

  async release() {
    // End the whole-test error scopes. Check that there are no extra error scopes, and that no
    // otherwise-uncaptured errors occurred during the test.
    let gpuValidationError;
    let gpuOutOfMemoryError;

    try {
      // May reject if the device was lost.
      gpuValidationError = await this.device.popErrorScope();
      gpuOutOfMemoryError = await this.device.popErrorScope();
    } catch (ex) {
      assert(
        this.lostReason !== undefined,
        'popErrorScope failed; should only happen if device has been lost'
      );

      throw ex;
    }

    await assertReject(
      this.device.popErrorScope(),
      'There was an extra error scope on the stack after a test'
    );

    if (gpuValidationError !== null) {
      assert(gpuValidationError instanceof GPUValidationError);
      // Allow the device to be reused.
      throw new TestFailedButDeviceReusable(
        `Unexpected validation error occurred: ${gpuValidationError.message}`
      );
    }
    if (gpuOutOfMemoryError !== null) {
      assert(gpuOutOfMemoryError instanceof GPUOutOfMemoryError);
      // Don't allow the device to be reused; unexpected OOM could break the device.
      throw new TestOOMedShouldAttemptGC('Unexpected out-of-memory error occurred');
    }
  }
}
