/**
 * Helper functions for attribution reporting API tests.
 */

const blankURL = (base = location.origin) => new URL('/wpt_internal/attribution-reporting/resources/empty.py', base);

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
  // The view of the stash is path-specific (https://web-platform-tests.org/tools/wptserve/docs/stash.html),
  // therefore the origin doesn't need to be specified.
  url = `${url}?clear_stash=true`;
  const options = {
    method: 'POST',
  };
  return fetch(url, options);
};

const pipeHeaderPattern = /[,)]/g;

// , and ) in pipe values must be escaped with \
const encodeForPipe = urlString => urlString.replace(pipeHeaderPattern, '\\$&');

const blankURLWithHeaders = (headers, origin, status) => {
  const url = blankURL(origin);

  const parts = headers.map(h => `header(${h.name},${encodeForPipe(h.value)})`);

  if (status !== undefined) {
    parts.push(`status(${encodeForPipe(status)})`);
  }

  if (parts.length > 0) {
    url.searchParams.set('pipe', parts.join('|'));
  }

  return url;
};

const getFetchParams = (origin, cookie) => {
  let credentials;
  const headers = [];

  if (!origin || origin === location.origin) {
    return {credentials, headers};
  }

  // https://fetch.spec.whatwg.org/#http-cors-protocol

  const allowOriginHeader = 'Access-Control-Allow-Origin';
  const allowHeadersHeader = 'Access-Control-Allow-Headers';

  if (cookie) {
    credentials = 'include';
    headers.push({
      name: 'Access-Control-Allow-Credentials',
      value: 'true',
    });
    headers.push({
      name: allowOriginHeader,
      value: `${location.origin}`,
    });
    headers.push({
      name: allowHeadersHeader,
      value: 'Attribution-Reporting-Eligible, Attribution-Reporting-Support',
    })
  } else {
    headers.push({
      name: allowOriginHeader,
      value: '*',
    });
    headers.push({
      name: allowHeadersHeader,
      value: '*',
    })
  }
  return {credentials, headers};
};

const getDefaultReportingOrigin = () => {
  // cross-origin means that the reporting origin differs from the source/destination origin.
  const crossOrigin = new URLSearchParams(location.search).get('cross-origin');
  return crossOrigin === null ? location.origin : get_host_info().HTTPS_REMOTE_ORIGIN;
};

const eligibleHeader = 'Attribution-Reporting-Eligible';
const supportHeader = 'Attribution-Reporting-Support';

const registerAttributionSrc = async (t, {
  source,
  trigger,
  cookie,
  method = 'img',
  extraQueryParams = {},
  reportingOrigin,
}) => {
  const searchParams = new URLSearchParams(location.search);

  if (method === 'variant') {
    method = searchParams.get('method');
  }

  const eligible = searchParams.get('eligible');

  let status;
  let headers = [];

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
    const params = getFetchParams(reportingOrigin, cookie);
    t.add_cleanup(() => fetch(blankURLWithHeaders(params.headers.concat([{
                    name,
                    value: `${cookie};Max-Age=0`,
                  }]), reportingOrigin), {credentials: params.credentials}));
  }

  // a and open with valueless attributionsrc support registrations on all
  // but the last request in a redirect chain, so add a no-op redirect.
  if (eligible !== null && (method === 'a' || method === 'open')) {
    headers.push({name: 'Location', value: blankURL().toString()});
    status = '302';
  }

  let credentials;
  if (method === 'fetch') {
    const params = getFetchParams(reportingOrigin, cookie);
    credentials = params.credentials;
    headers = headers.concat(params.headers);
  }

  const url = blankURLWithHeaders(headers, reportingOrigin, status);

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
      await fetch(url, {headers, credentials});
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
const pollAttributionReports = async (url, origin = location.origin, interval = 100) => {
  const resp = await fetch(new URL(url, origin));
  const payload = await resp.json();
  if (payload.reports.length === 0) {
    await delay(interval);
    return pollAttributionReports(url, origin, interval);
  }
  return new Promise(resolve => resolve(payload));
};

const pollEventLevelReports = (origin, interval) =>
    pollAttributionReports(eventLevelReportsUrl, origin, interval);
const pollEventLevelDebugReports = (origin, interval) =>
    pollAttributionReports(eventLevelDebugReportsUrl, origin, interval);
const pollAggregatableReports = (origin, interval) =>
    pollAttributionReports(aggregatableReportsUrl, origin, interval);
const pollAggregatableDebugReports = (origin, interval) =>
    pollAttributionReports(aggregatableDebugReportsUrl, origin, interval);
const pollVerboseDebugReports = (origin, interval) =>
    pollAttributionReports(verboseDebugReportsUrl, origin, interval);

const validateReportHeaders = headers => {
  assert_array_equals(headers['content-type'], ['application/json']);
  assert_array_equals(headers['cache-control'], ['no-cache']);
  assert_own_property(headers, 'user-agent');
  assert_not_own_property(headers, 'cookie');
  assert_not_own_property(headers, 'referer');
};
