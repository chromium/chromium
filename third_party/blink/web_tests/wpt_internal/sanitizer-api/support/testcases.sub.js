const testcases = [
  // Test cases from issue WICG/sanitizer-api#84
  {
    config_input: {"allowElements":["svg","use"], "allowAttributes":{"xlink:href":["use"]}},
    value: `<svg><use xlink:href='data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" id="x" viewBox="0 0 100 50" width="100%" height="100%"><a href="javascript:alert(1)"><circle r="100" /></a></svg>#x'/></svg>`,
    result: "",
    message: "Regression test for WICG/sanitizer-api#84."
  },
  // Test cases from issue WICG/sanitizer-api#85
  {
    config_input: {
      "allowElements": ["svg","set"],
      "allowAttributes": { "onend": ["set"], "dur":["set"] }
    },
    value: `<svg><set onend="alert(1)" dur="1"/></svg>`,
    result: "",
    message: "Regression test for WICG/sanitizer-api#85."
  },
  // Test cases from issue WICG/sanitizer-api#86
  {
    config_input: {},
    value: `<noscript><img title="</noscript><iframe onload=alert(1)>"></noscript>`,
    result: "",
    message: "Regression test for WICG/sanitizer-api#86."
  },
];

