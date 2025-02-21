const routerRules = {
  'condition-compatible-regex-pattern': [{
    condition: {urlPattern: new URLPattern({pathname: '/**/direct.txt'})},
    source: 'network'
  }],
  'condition-incompatible-regex-pattern': [{
    condition: {urlPattern: {search: ':s' }},
    source: 'network'
  }],
};

export {routerRules};
