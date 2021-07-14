import typescript from '@rollup/plugin-typescript';
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
    typescript({
      tsconfig: 'src/bidiMapper/tsconfig.json',
    }),
  ],
};
