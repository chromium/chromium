// Generated using:
/*
  ./tools/origin_trials/generate_token.py \
    --expire-days 5000 \
    --version 3 \
    https://web-platform.test:8444 \
    CrossOriginEmbedderPolicyCredentialless
    */
const origin_trial_header = {
  enabled: encodeURIComponent("|header(Origin-Trial, A3wpaaKI+zH+hIsy8KWV6iGysula08sFk44B4yEuODp9xSL6OrSb80nVB88bI001xN/12fyCmP9slgMUdJALhgkAAACDeyJvcmlnaW4iOiAiaHR0cHM6Ly93ZWItcGxhdGZvcm0udGVzdDo4NDQ0IiwgImZlYXR1cmUiOiAiQ3Jvc3NPcmlnaW5FbWJlZGRlclBvbGljeUNyZWRlbnRpYWxsZXNzT3JpZ2luVHJpYWwiLCAiZXhwaXJ5IjogMjA1NzU2MzE4NX0=)"),
  disabled: "|header(Origin-Trial, nothing)",
}

const same_origin = get_host_info().HTTPS_ORIGIN;
const cross_origin = get_host_info().HTTPS_REMOTE_ORIGIN;
const cookie_key = "coep_credentialless_origin_trial";
const cookie_value = "cookie";

const originTrialTest = (environment, origin_trial, expectation) => {
  promise_test_parallel(async test => {
    // Create the execution context:
    const env_constructor = environments[environment];
    const env_headers = origin_trial_header[origin_trial] + coep_credentialless;
    const [env_token, env_error] = env_constructor(env_headers);

    // Fetch a no-cors cross-origin resource:
    const fetch_from_env = async () => {
      const resource_token = token();
      const resource_url = showRequestHeaders(cross_origin, resource_token);
      send(env_token, `
        fetch("${resource_url}", {mode: 'no-cors', credentials: 'include'});
      `);

      // Check the cookies sent to the server.
      const headers = JSON.parse(await receive(resource_token));
      const cookies = parseCookies(headers);

      return cookies[cookie_key];
    };

    // Wait either for the environment to be blocked by COEP, or the cookies.
    let result = await Promise.race([
      env_error.then(() => "blocked"),
      fetch_from_env()
    ]);

    assert_equals(result, expectation);
  }, `environment="${environment}" origin_trial="${origin_trial}"`)
};
