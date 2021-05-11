`use strict`;

const rollup = require('rollup');

module.exports = {
    readMapper: async function () {
        return await mapperReader();
    }
};

const mapperReader = async () => {
    const inputOptions = {
        input: "src/bidiMapper/mapper.js",
    };
    const outputOptions = {
        format: 'iife'
    };
    const bundle = await rollup.rollup(inputOptions);

    const result = await bundle.generate(outputOptions);

    console.log(result);

    return result.output[0].code;
};