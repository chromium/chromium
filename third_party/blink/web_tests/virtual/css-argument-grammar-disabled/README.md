Tests in this virtual suite are running with the `CSSArgumentGrammar` runtime
flag disabled.

The introduction of CSS argument grammar allows certain arbitrary substitution
values to be accepted at parse time, even if they are eventually disallowed at
computed-value time. This change in behavior causes some existing tests to fail
because they expect parse-time rejection. We are waiting for UseCounter results
to assess the web-compatibility impact before updating these tests. In the
meantime, they are run in this virtual suite with `CSSArgumentGrammar` disabled
to maintain the existing baselines.

See https://drafts.csswg.org/css-values-5/#argument-grammar
