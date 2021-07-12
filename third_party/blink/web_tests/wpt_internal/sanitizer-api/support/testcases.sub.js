const testcases = [
  {config_input: {}, value: "<plaintext><p>text</p>", result: "&lt;p&gt;text&lt;/p&gt;", message: "plaintext"},
  {config_input: {}, value: "<xmp>TEXT</xmp>", result: "TEXT", message: "xmp"},
  {config_input: {allowElements: ["template", "div"]}, value: "<template><script>test</script><div>hello</div></template>", result: "<template><div>hello</div></template>", message: "Template element"},
  {config_input: {}, value: "<a href='javascript:evil.com'>Click.</a>", result: "<a>Click.</a>", message: "HTMLAnchorElement with javascript protocal"},
  {config_input: {}, value: "<a href='  javascript:evil.com'>Click.</a>", result: "<a>Click.</a>", message: "HTMLAnchorElement with javascript protocal start with space"},
  {config_input: {}, value: "<a href='http:evil.com'>Click.</a>", result: "<a href=\"http:evil.com\">Click.</a>", message: "HTMLAnchorElement"},
  {config_input: {}, value: "<area href='javascript:evil.com'>Click.</area>", result: "<area>Click.", message: "HTMLAreaElement with javascript protocal"},
  {config_input: {}, value: "<area href=' javascript:evil.com'>Click.</area>", result: "<area>Click.", message: "HTMLAreaElement with javascript protocal start with space"},
  {config_input: {}, value: "<area href='http:evil.com'>Click.</area>", result: "<area href=\"http:evil.com\">Click.", message: "HTMLAreaElement"},
  {config_input: {}, value: "<form action='javascript:evil.com'>Click.</form>", result: "<form>Click.</form>", message: "HTMLFormElement with javascript action"},
  {config_input: {}, value: "<form action=' javascript:evil.com'>Click.</form>", result: "<form>Click.</form>", message: "HTMLFormElement with javascript action start with space"},
  {config_input: {}, value: "<form action='http:evil.com'>Click.</form>", result: "<form action=\"http:evil.com\">Click.</form>", message: "HTMLFormElement"},
  {config_input: {}, value: "<input formaction='javascript:evil.com'>Click.</input>", result: "<input>Click.", message: "HTMLInputElement with javascript formaction"},
  {config_input: {}, value: "<input formaction=' javascript:evil.com'>Click.</input>", result: "<input>Click.", message: "HTMLInputElement with javascript formaction start with space"},
  {config_input: {}, value: "<input formaction='http:evil.com'>Click.</input>", result: "<input formaction=\"http:evil.com\">Click.", message: "HTMLInputElement"},
  {config_input: {}, value: "<button formaction='javascript:evil.com'>Click.</button>", result: "<button>Click.</button>", message: "HTMLButtonElement with javascript formaction"},
  {config_input: {}, value: "<button formaction=' javascript:evil.com'>Click.</button>", result: "<button>Click.</button>", message: "HTMLButtonElement with javascript formaction start with space"},
  {config_input: {}, value: "<button formaction='http:evil.com'>Click.</button>", result: "<button formaction=\"http:evil.com\">Click.</button>", message: "HTMLButtonElement"},
  {config_input: {}, value: "<p>Some text</p></body><!-- 1 --></html><!-- 2 --><p>Some more text</p>", result: "<p>Some text</p><p>Some more text</p>", message: "malformed HTML"},
  {config_input: {}, value: "<p>Some text</p><!-- 1 --><!-- 2 --><p>Some more text</p>", result: "<p>Some text</p><p>Some more text</p>", message: "HTML with comments; comments not allowed"},
  {config_input: {allowComments: true}, value: "<p>Some text</p><!-- 1 --><!-- 2 --><p>Some more text</p>", result: "<p>Some text</p><!-- 1 --><!-- 2 --><p>Some more text</p>", message: "HTML with comments; allowComments"},
  {config_input: {allowComments: false}, value: "<p>Some text</p><!-- 1 --><!-- 2 --><p>Some more text</p>", result: "<p>Some text</p><p>Some more text</p>", message: "HTML with comments; !allowComments"},
  {config_input: {}, value: "<p>comment<!-- hello -->in<!-- </p> -->text</p>", result: "<p>commentintext</p>", message: "HTML with comments deeper in the tree"},
  {config_input: {allowComments: true}, value: "<p>comment<!-- hello -->in<!-- </p> -->text</p>", result: "<p>comment<!-- hello -->in<!-- </p> -->text</p>", message: "HTML with comments deeper in the tree, allowComments"},
  {config_input: {allowComments: false}, value: "<p>comment<!-- hello -->in<!-- </p> -->text</p>", result: "<p>commentintext</p>", message: "HTML with comments deeper in the tree, !allowComments"},

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
