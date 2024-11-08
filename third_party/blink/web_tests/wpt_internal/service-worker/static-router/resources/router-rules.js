// The value is coming from:
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/service_worker/service_worker_router_rule.h;l=28;drc=6f3f85b321146cfc0f9eb81a74c7c2257821461e
const CONDITION_MAX_RECURSION_DEPTH = 10;

const routerRules = {
  'condition-compatible-regex-pattern': [{
    condition: {urlPattern: new URLPattern({pathname: '/**/direct.txt'})},
    source: 'network'
  }],
  'condition-incompatible-regex-pattern': [{
    condition: {urlPattern: {search: ':s' }},
    source: 'network'
  }],
  'condition-invalid-or-condition-depth': (() => {
    const addOrCondition = (depth) => {
      if (depth > CONDITION_MAX_RECURSION_DEPTH) {
        return {urlPattern: '/foo'};
      }
      return {
        or: [addOrCondition(depth + 1)]
      };
    };
    return {condition: addOrCondition(1), source: 'network'};
  })(),
  'condition-invalid-not-condition-depth': (() => {
    const generateNotCondition = (depth) => {
      if (depth > CONDITION_MAX_RECURSION_DEPTH) {
        return {
          urlPattern: '/**/example.txt',
        };
      }
      return {not: generateNotCondition(depth + 1)};
    };
    return {condition: generateNotCondition(1), source: 'network'};
  })(),
  'condition-invalid-router-size': [...Array(512)].map((val, i) => {
    return {
      condition: {urlPattern: `/foo-${i}`},
      source: 'network'
    };
  }),
};

export {routerRules};
