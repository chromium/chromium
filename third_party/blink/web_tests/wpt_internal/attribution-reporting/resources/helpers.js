/**
 * Helper functions for attribution reporting API tests.
 */

const blankURL = () => new URL('resources/empty.txt', location);

const attribution_reporting_promise_test = (f, name) =>
    promise_test(async t => {
      t.add_cleanup(() => internals.resetAttributionReporting());
      t.add_cleanup(() => resetAttributionReports(eventLevelReportsUrl));
      t.add_cleanup(() => resetAttributionReports(aggregatableReportsUrl));
      t.add_cleanup(() => resetAttributionReports(eventLevelDebugReportsUrl));
      t.add_cleanup(() => resetAttributionReports(aggregatableDebugReportsUrl));
      t.add_cleanup(() => resetAttributionReports(verboseDebugReportsUrl));
      return f(t);
    }, name);

const eventLevelReportsUrl =
    '/.well-known/attribution-reporting/report-event-attribution';
const eventLevelDebugReportsUrl =
    '/.well-known/attribution-reporting/debug/report-event-attribution';
const aggregatableReportsUrl =
    '/.well-known/attribution-reporting/report-aggregate-attribution';
const aggregatableDebugReportsUrl =
    '/.well-known/attribution-reporting/debug/report-aggregate-attribution';
const verboseDebugReportsUrl =
    '/.well-known/attribution-reporting/debug/verbose';

const attributionDebugCookie = 'ar_debug=1;Secure;HttpOnly;SameSite=None;Path=/';

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

const pipeHeaderPattern = /[,)]/g;

// , and ) in pipe values must be escaped with \
const encodeForPipe = urlString => urlString.replace(pipeHeaderPattern, '\\$&');

const blankURLWithHeaders = (headers, status) => {
  const url = blankURL();

  const parts = headers.map(h => `header(${h.name},${encodeForPipe(h.value)})`);

  if (status !== undefined) {
    parts.push(`status(${encodeForPipe(status)})`);
  }

  if (parts.length > 0) {
    url.searchParams.set('pipe', parts.join('|'));
  }

  return url;
};

const eligibleHeader = 'Attribution-Reporting-Eligible';
const supportHeader = 'Attribution-Reporting-Support';

const registerAttributionSrc = async (t, {
  source,
  trigger,
  cookie,
  method = 'img',
  extraQueryParams = {},
}) => {
  const searchParams = new URLSearchParams(location.search);

  if (method === 'variant') {
    method = searchParams.get('method');
  }

  const eligible = searchParams.get('eligible');

  let status;
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

  // a and open with valueless attributionsrc support registrations on all
  // but the last request in a redirect chain, so add a no-op redirect.
  if (eligible !== null && (method === 'a' || method === 'open')) {
    headers.push({name: 'Location', value: blankURL().toString()});
    status = '302';
  }

  const url = blankURLWithHeaders(headers, status);

  Object.entries(extraQueryParams)
      .forEach(([key, value]) => url.searchParams.set(key, value));

  switch (method) {
    case 'img':
      const img = document.createElement('img');
      if (eligible === null) {
        img.attributionSrc = url;
      } else {
        await new Promise(resolve => {
          img.onload = resolve;
          // Since the resource being fetched isn't a valid image, onerror will
          // be fired, but the browser will still process the
          // attribution-related headers, so resolve the promise instead of
          // rejecting.
          img.onerror = resolve;
          img.attributionSrc = '';
          img.src = url;
        });
      }
      return 'event';
    case 'script':
      const script = document.createElement('script');
      if (eligible === null) {
        script.attributionSrc = url;
      } else {
        await new Promise(resolve => {
          script.onload = resolve;
          script.attributionSrc = '';
          script.src = url;
          document.body.appendChild(script);
        });
      }
      return 'event';
    case 'a':
      const a = document.createElement('a');
      a.target = '_blank';
      a.textContent = 'link';
      if (eligible === null) {
        a.attributionSrc = url;
        a.href = blankURL();
      } else {
        a.attributionSrc = '';
        a.href = url;
      }
      document.body.appendChild(a);
      await test_driver.click(a);
      return 'navigation';
    case 'open':
      await test_driver.bless('open window', () => {
        if (eligible === null) {
          open(
              blankURL(), '_blank',
              `attributionsrc=${encodeURIComponent(url)}`);
        } else {
          open(url, '_blank', 'attributionsrc');
        }
      });
      return 'navigation';
    case 'fetch':
      const headers = {};
      if (eligible !== null) {
        headers[eligibleHeader] = eligible;
      }
      const support = searchParams.get('support');
      if (support !== null) {
        headers[supportHeader] = support;
      }
      await fetch(url, {headers});
      return 'event';
    case 'xhr':
      await new Promise((resolve, reject) => {
        const req = new XMLHttpRequest();
        req.open('GET', url);
        if (eligible !== null) {
          req.setRequestHeader(eligibleHeader, eligible);
        }
        req.onload = resolve;
        req.onerror = () => reject(req.statusText);
        req.send();
      });
      return 'event';
    default:
      throw `unknown method "${method}"`;
  }
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
const pollEventLevelDebugReports = interval =>
    pollAttributionReports(eventLevelDebugReportsUrl, interval);
const pollAggregatableReports = interval =>
    pollAttributionReports(aggregatableReportsUrl, interval);
const pollAggregatableDebugReports = interval =>
    pollAttributionReports(aggregatableDebugReportsUrl, interval);
const pollVerboseDebugReports = interval =>
    pollAttributionReports(verboseDebugReportsUrl, interval);

const validateReportHeaders = headers => {
  assert_array_equals(headers['content-type'], ['application/json']);
  assert_array_equals(headers['cache-control'], ['no-cache']);
  assert_own_property(headers, 'user-agent');
  assert_not_own_property(headers, 'cookie');
  assert_not_own_property(headers, 'referer');
};
