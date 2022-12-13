module.exports = {
  root: true,
  env: {
    node: true,
    es6: true,
  },

  parser: '@typescript-eslint/parser',

  plugins: ['mocha', '@typescript-eslint', 'import'],

  extends: ['plugin:prettier/recommended'],

  rules: {},
  overrides: [
    {
      files: ['*.ts'],
      extends: [
        'plugin:@typescript-eslint/eslint-recommended',
        'plugin:@typescript-eslint/recommended',
      ],
      rules: {
        '@typescript-eslint/no-namespace': 'off',
        'prefer-const': 'off',
        '@typescript-eslint/no-explicit-any': 'off',
        '@typescript-eslint/no-inferrable-types': 'off',
        '@typescript-eslint/no-empty-function': 'off',
        '@typescript-eslint/ban-types': 'off',
        '@typescript-eslint/no-non-null-assertion': 'off',
        '@typescript-eslint/no-unused-vars': 'off',
        '@typescript-eslint/no-array-constructor': 'off',
        '@typescript-eslint/no-this-alias': 'off',
        'import/extensions': ['error', 'ignorePackages'],
      },
    },
  ],
};
