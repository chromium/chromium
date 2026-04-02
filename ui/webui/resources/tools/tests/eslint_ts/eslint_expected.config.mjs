import {defaultConfig, } from '%(path_to_build_dir)s/gen/ui/webui/resources/tools/eslint/eslint_ts.config_base.js';

export default [
  ...defaultConfig,
  
  {
    languageOptions: {
      parserOptions: {
        'project': ['tests/eslint_ts/tsconfig.json'],
      },
    },
  },
];