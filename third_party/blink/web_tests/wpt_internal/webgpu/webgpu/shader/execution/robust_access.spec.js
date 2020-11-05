/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests to check datatype clamping in shaders is correctly implemented including vector / matrix indexing

- For each shader stage (TODO):
  - For various memory spaces (storage, uniform, global, function local, shared memory, TODO(in, out))
    - For dynamic vs. non dynamic buffer bindings (when testing storage or uniform)
      - For many data types (float, uint, int) x (sized array, unsized array, vec, mat...)
        - For various types of accesses (read, write, atomic ops)
          - For various indices in bounds and out of bounds of the data type
            - Check that accesses are in bounds or discarded (as much as we can tell)

TODO add tests to check that texel fetch operations stay in-bounds.
`;
import { params, poptions } from '../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { assert } from '../../../common/framework/util/util.js';
import { GPUTest } from '../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

// Utilities that should probably live in some shared place.
function copyArrayBuffer(src) {
  const dst = new ArrayBuffer(src.byteLength);
  new Uint8Array(dst).set(new Uint8Array(src));
  return dst;
}

const kUintMax = 4294967295;
const kIntMax = 2147483647;

// A small utility to test shaders:
//  - it wraps the source into a small harness that checks the runTest() function returns 0.
//  - it runs the shader with the testBindings set as bindgroup 0.
//
// The shader also has access to a uniform value that's equal to 1u to avoid constant propagation
// in the shader compiler.
function runShaderTest(t, stage, testSource, testBindings) {
  assert(stage === GPUShaderStage.COMPUTE, 'Only know how to deal with compute for now');

  const constantsBuffer = t.device.createBuffer({
    mappedAtCreation: true,
    size: 4,
    usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.UNIFORM,
  });

  const constantsData = new Uint32Array(constantsBuffer.getMappedRange());
  constantsData[0] = 1;
  constantsBuffer.unmap();

  const resultBuffer = t.device.createBuffer({
    size: 4,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE,
  });

  const source = `#version 450
    layout(std140, set = 1, binding = 0) uniform Constants {
      uint one;
    };
    layout(std430, set = 1, binding = 1) buffer Result {
      uint result;
    };

    ${testSource}

    void main() {
      result = runTest();
    }`;

  const pipeline = t.device.createComputePipeline({
    computeStage: {
      entryPoint: 'main',
      module: t.makeShaderModule('compute', { glsl: source }),
    },
  });

  const group = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(1),
    entries: [
      { binding: 0, resource: { buffer: constantsBuffer } },
      { binding: 1, resource: { buffer: resultBuffer } },
    ],
  });

  const testGroup = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: testBindings,
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, testGroup);
  pass.setBindGroup(1, group);
  pass.dispatch(1);
  pass.endPass();

  t.queue.submit([encoder.finish()]);

  t.expectContents(resultBuffer, new Uint32Array([0]));
}

// The definition of base types for aggregate types, for example float, uint, etc.

const baseTypes = {
  // TODO bools
  uint: {
    name: 'uint',
    byteSize: 4,
    glslPrefix: 'u',
    glslZero: '0u',
    fillBuffer(data, zeroStart, size) {
      const typedData = new Uint32Array(data);
      typedData.fill(42);
      for (let i = 0; i < size / 4; i++) {
        typedData[zeroStart / 4 + i] = 0;
      }
    },
  },

  int: {
    name: 'int',
    byteSize: 4,
    glslPrefix: 'i',
    glslZero: '0',
    fillBuffer(data, zeroStart, size) {
      const typedData = new Int32Array(data);
      typedData.fill(42);
      for (let i = 0; i < size / 4; i++) {
        typedData[zeroStart / 4 + i] = 0;
      }
    },
  },

  float: {
    name: 'float',
    byteSize: 4,
    glslPrefix: '',
    glslZero: '0.0f',
    fillBuffer(data, zeroStart, size) {
      const typedData = new Float32Array(data);
      typedData.fill(42);
      for (let i = 0; i < size / 4; i++) {
        typedData[zeroStart / 4 + i] = 0;
      }
    },
  },

  bool: {
    name: 'bool',
    byteSize: 4,
    glslPrefix: 'b',
    glslZero: 'false',
    fillBuffer(data, zeroStart, size) {
      const typedData = new Uint32Array(data);
      typedData.fill(42);
      for (let i = 0; i < size / 4; i++) {
        typedData[zeroStart / 4 + i] = 0;
      }
    },
  },
};

// The definition of aggregate types.

const typeParams = (() => {
  const types = {};
  for (const baseTypeName of Object.keys(baseTypes)) {
    const baseType = baseTypes[baseTypeName];

    // Arrays
    types[`${baseTypeName}_sizedArray`] = {
      declaration: `${baseTypeName} data[3]`,
      length: 3,
      // TODO should really be std140Length: 2 * 4 + 1?
      std140Length: 3 * 4,
      std430Length: 3,
      zero: baseType.glslZero,
      baseType,
    };

    types[`${baseTypeName}_unsizedArray`] = {
      declaration: `${baseTypeName} data[]`,
      length: 3,
      std140Length: 0, // Unused
      std430Length: 3,
      zero: baseType.glslZero,
      baseType,
      isUnsizedArray: true,
    };

    // Vectors
    for (let dimension = 2; dimension <= 4; dimension++) {
      types[`${baseTypeName}_vector${dimension}`] = {
        declaration: `${baseType.glslPrefix}vec${dimension} data`,
        length: dimension,
        std140Length: dimension,
        std430Length: dimension,
        zero: baseType.glslZero,
        baseType,
      };
    }
  }

  // Matrices, there are only float matrices in GLSL.
  for (const transposed of [false, true]) {
    for (let numColumns = 2; numColumns <= 4; numColumns++) {
      for (let numRows = 2; numRows <= 4; numRows++) {
        const majorDim = transposed ? numRows : numColumns;
        const minorDim = transposed ? numColumns : numRows;

        const std140SizePerMinorDim = 4;
        const std430SizePerMinorDim = minorDim === 3 ? 4 : minorDim;

        let typeName = `mat${numColumns}`;
        if (numColumns !== numRows) {
          typeName += `x${numRows}`;
        }

        types[(transposed ? 'transposed_' : '') + typeName] = {
          declaration: (transposed ? 'layout(row_major) ' : '') + `${typeName} data`,
          length: numColumns,
          // TODO should really be std140Length: std140SizePerMinorDim * (majorDim - 1) + minorDim,
          std140Length: std140SizePerMinorDim * majorDim,
          std430Length: std430SizePerMinorDim * (majorDim - 1) + minorDim,
          zero: `vec${numRows}(0.0f)`,
          baseType: baseTypes['float'],
        };
      }
    }
  }

  return types;
})();

g.test('bufferMemory')
  .params(
    params()
      .combine(poptions('type', Object.keys(typeParams)))
      .combine([
        { memory: 'storage', access: 'read' },
        { memory: 'storage', access: 'write' },
        { memory: 'storage', access: 'atomic' },
        { memory: 'uniform', access: 'read' },
        { memory: 'global', access: 'read' },
        { memory: 'global', access: 'write' },
        { memory: 'function', access: 'read' },
        { memory: 'function', access: 'write' },
        { memory: 'shared', access: 'read' },
        { memory: 'shared', access: 'write' },
      ])

      // Unsized arrays are only supported with SSBOs
      .unless(p => typeParams[p.type].isUnsizedArray === true && p.memory !== 'storage')
      // Atomics are only supported with integers
      .unless(
        p => p.access === 'atomic' && !['uint', 'int'].includes(typeParams[p.type].baseType.name)
      )

      // Layouts are only supported on interfaces
      .unless(
        p => p.type.indexOf('transposed') !== -1 && !['uniform', 'storage'].includes(p.memory)
      )
  )
  .fn(async t => {
    const { memory, access } = t.params;

    const type = typeParams[t.params.type];
    const baseType = type.baseType;

    const usesCanary = ['global', 'function', 'shared'].includes(memory);
    const usesBuffer = ['uniform', 'storage'].includes(memory);

    let globalSource = '';
    let testFunctionSource = '';
    let bufferByteSize = 0;

    // Declare the data that will be accessed to check robust access, as a buffer or a struct
    // in the global scope or inside the test function itself.
    const structDecl = `
      struct S {
          uint startCanary[10];
          ${type.declaration};
          uint endCanary[10];
      };`;

    switch (memory) {
      case 'uniform':
        globalSource += `
          layout(std140, set = 0, binding = 0) uniform TestData {
            ${type.declaration};
          } s;`;
        bufferByteSize = baseType.byteSize * type.std140Length;
        break;

      case 'storage':
        globalSource += `
          layout(std430, set = 0, binding = 0) buffer TestData {
            ${type.declaration};
          } s;`;
        bufferByteSize = baseType.byteSize * type.std430Length;
        break;

      case 'global':
        globalSource += `
          ${structDecl}
          S s;`;
        break;

      case 'function':
        globalSource += `${structDecl}`;
        testFunctionSource += 'S s;';
        break;

      case 'shared':
        globalSource += `
          ${structDecl}
          shared S s;`;
        break;
    }

    // Build the test function that will do the tests.

    // If we use a local canary declared in the shader, initialize it.
    if (usesCanary) {
      testFunctionSource += `
        for (uint i = 0; i < 10; i++) {
          s.startCanary[i] = s.endCanary[i] = 0xFFFFFFFFu;
        }`;
    }

    const indicesToTest = [
      // Write to the inside of the type so we can check the size computations were correct.
      '0',
      `${type.length} - 1`,

      // Check exact bounds
      '-1',
      `${type.length}`,

      // Check large offset
      '-1000000',
      '1000000',

      // Check with max uint
      `${kUintMax}`,
      `-1 * ${kUintMax}`,

      // Check with max int
      `${kIntMax}`,
      `-1 * ${kIntMax}`,
    ];

    // Produce the accesses to the variable.
    for (const indexToTest of indicesToTest) {
      // TODO check with constants too if WGSL allows it.
      const index = `(${indexToTest}) * one`;

      switch (access) {
        case 'read':
          testFunctionSource += `
            if(s.data[${index}] != ${type.zero}) {
              return __LINE__;
            }`;
          break;

        case 'write':
          testFunctionSource += `s.data[${index}] = ${type.zero};`;
          break;

        case 'atomic':
          testFunctionSource += `atomicAdd(s.data[${index}], 1);`;
          break;
      }
    }

    // Check that the canaries haven't been modified
    if (usesCanary) {
      testFunctionSource += `
        for (uint i = 0; i < 10; i++) {
          if (s.startCanary[i] != 0xFFFFFFFFu) {
            return __LINE__;
          }
          if (s.endCanary[i] != 0xFFFFFFFFu) {
            return __LINE__;
          }
        }`;
    }

    // Run the test

    // First aggregate the test source
    const testSource = `
      ${globalSource}

      uint runTest() {
        ${testFunctionSource}
        return 0;
      }`;

    // Run it.
    if (usesBuffer) {
      // Create a buffer that contains zeroes in the allowed access area, and 42s everywhere else.
      const testBuffer = t.device.createBuffer({
        mappedAtCreation: true,
        size: 512,
        usage:
          GPUBufferUsage.COPY_SRC |
          GPUBufferUsage.UNIFORM |
          GPUBufferUsage.STORAGE |
          GPUBufferUsage.COPY_DST,
      });

      const testInit = testBuffer.getMappedRange();
      baseType.fillBuffer(testInit, 256, bufferByteSize);
      const testInitCopy = copyArrayBuffer(testInit);
      testBuffer.unmap();

      // Run the shader, accessing the buffer.
      runShaderTest(t, GPUShaderStage.COMPUTE, testSource, [
        { binding: 0, resource: { buffer: testBuffer, offset: 256, size: bufferByteSize } },
      ]);

      // Check that content of the buffer outside of the allowed area didn't change.
      t.expectSubContents(testBuffer, 0, new Uint8Array(testInitCopy.slice(0, 256)));
      const dataEnd = 256 + bufferByteSize;
      t.expectSubContents(testBuffer, dataEnd, new Uint8Array(testInitCopy.slice(dataEnd, 512)));
    } else {
      runShaderTest(t, GPUShaderStage.COMPUTE, testSource, []);
    }
  });
