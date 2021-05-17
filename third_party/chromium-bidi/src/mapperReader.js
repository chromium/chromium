`use strict`;

import { rollup } from 'rollup';

export default async function read() {
    const bundle = await rollup({
        input: "src/bidiMapper/mapper.js",
    });

    const result = await bundle.generate({
        format: 'iife'
    });
    return result.output[0].code;
};