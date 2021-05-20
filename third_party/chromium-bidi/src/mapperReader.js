/**
 * Copyright 2021 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
`use strict`;

import { rollup } from 'rollup';
import typescript from '@rollup/plugin-typescript';

export default async function read() {
    const bundle = await rollup({
        input: "src/bidiMapper/mapper.js",
        plugins: [typescript({
            tsconfig: "src/bidiMapper/tsconfig.json"
        })]
    });

    const result = await bundle.generate({
        format: 'iife'
    });
    return result.output[0].code;
};