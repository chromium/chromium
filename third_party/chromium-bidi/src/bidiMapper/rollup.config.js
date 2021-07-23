import typescript from '@rollup/plugin-typescript';
import nodePolyfills from 'rollup-plugin-node-polyfills';
import json from '@rollup/plugin-json';

export default {
  input: 'src/bidiMapper/mapper.ts',
  output: {
    file: 'src/.build/mapper.js',
    sourcemap: true,
    format: 'iife',
  },
  plugins: [
    json(),
    nodePolyfills(),
    typescript({
      tsconfig: 'src/tsconfig.json',
    }),
  ],
};
