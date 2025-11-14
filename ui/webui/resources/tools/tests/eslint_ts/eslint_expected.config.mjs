import path from 'path';

import {defaultConfig} from '../../../eslint_ts.config_base.mjs';

export default [
  ...defaultConfig,
  {
    languageOptions: {
      parserOptions: {
        'project': [path.join(import.meta.dirname, './../tsconfig.json')],
      },
    },
  },
];