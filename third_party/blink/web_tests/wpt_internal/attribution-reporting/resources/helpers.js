/**
 * Helper functions for attribution reporting API tests.
 */

const blankURL = () => new URL('/resources/blank.html', location);

const attribution_reporting_promise_test = (f, name) =>
    promise_test(async t => {
      t.add_cleanup(() => internals.resetAttributionReporting());
      t.add_cleanup(() => resetEventLevelReports());
      t.add_cleanup(() => resetAggregatableReports());
      return f(t);
    }, name);

const eventLevelReportsUrl =
    '/.well-known/attribution-reporting/report-event-attribution';
const aggregatableReportsUrl =
    '/.well-known/attribution-reporting/report-aggregate-attribution';

/**
 * Method to clear the stash. Takes the URL as parameter. This could be for
 * event-level or aggregatable reports.
 */
const resetAttributionReports = url => {
  url = `${url}?clear_stash=true`;
  const options = {
    method: 'POST',
  };
  return fetch(url, options);
};

const resetEventLevelReports = () =>
    resetAttributionReports(eventLevelReportsUrl);
const resetAggregatableReports = () =>
    resetAttributionReports(aggregatableReportsUrl);

const pipeHeaderPattern = /[,)]/g;

// , and ) in pipe values must be escaped with \
const encodeForPipe = urlString => urlString.replace(pipeHeaderPattern, '\\$&');

const blankURLWithHeaders = headers => {
  const url = blankURL();

  if (headers.length > 0) {
    url.searchParams.set(
        'pipe',
        headers.map(h => `header(${h.name},${encodeForPipe(h.value)})`)
            .join('|'));
  }

  return url;
};

const registerAttributionSrc = (t, {
  source,
  trigger,
  cookie,
}) => {
  const headers = [];

  if (source) {
    headers.push({
      name: 'Attribution-Reporting-Register-Source',
      value: JSON.stringify(source),
    });
  }

  if (trigger) {
    headers.push({
      name: 'Attribution-Reporting-Register-Trigger',
      value: JSON.stringify(trigger),
    });
  }

  if (cookie) {
    const name = 'Set-Cookie';
    headers.push({name, value: cookie});

    // Delete the cookie at the end of the test.
    t.add_cleanup(() => fetch(blankURLWithHeaders([{
                    name,
                    value: `${cookie};Max-Age=0`,
                  }])));
  }

  const img = document.createElement('img');
  img.attributionSrc = blankURLWithHeaders(headers);
};

/**
 * Delay method that waits for prescribed number of milliseconds.
 */
const delay = ms => new Promise(resolve => step_timeout(resolve, ms));

/**
 * Method that polls a particular URL every interval for reports. Once reports
 * are received, returns the payload as promise.
 */
const pollAttributionReports = async (url, interval = 100) => {
  const resp = await fetch(url);
  const payload = await resp.json();
  if (payload.reports.length === 0) {
    await delay(interval);
    return pollAttributionReports(url, interval);
  }
  return new Promise(resolve => resolve(payload));
};

const pollEventLevelReports = interval =>
    pollAttributionReports(eventLevelReportsUrl, interval);
const pollAggregatableReports = interval =>
    pollAttributionReports(aggregatableReportsUrl, interval);

const validateReportHeaders = headers => {
  assert_array_equals(headers['content-type'], ['application/json']);
  assert_array_equals(headers['cache-control'], ['no-cache']);
  assert_own_property(headers, 'user-agent');
  assert_not_own_property(headers, 'cookie');
  assert_not_own_property(headers, 'referer');
};
