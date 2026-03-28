import path from 'path';

import {defaultConfig, } from '%(path_to_build_dir)s/gen/ui/webui/resources/tools/eslint/eslint_ts.config_base.js';

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