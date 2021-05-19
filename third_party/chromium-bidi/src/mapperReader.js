`use strict`;

import { rollup } from 'rollup';
import typescript from '@rollup/plugin-typescript';

export default async function read() {
    const bundle = await rollup({
        input: "src/bidiMapper/mapper.js",
        plugins: [typescript()]
    });

    const result = await bundle.generate({
        format: 'iife'
    });
    return result.output[0].code;
};