/**
 * Helper functions for attribution reporting API tests.
 */

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

/**
 * Registers either a source or trigger.
 */
const registerAttributionSrc = (header, body) => {
  const url = new URL('resources/blank.html', window.location);
  // , and ) in header values must be escaped with \
  url.searchParams.set(
      'pipe',
      `header(${header},${
          JSON.stringify(body).replace(pipeHeaderPattern, '\\$&')})`);
  const image = document.createElement('img');
  image.setAttribute('attributionsrc', url);
};

/**
 * Delay method that waits for prescribed number of milliseconds.
 */
const delay = ms => new Promise(resolve => step_timeout(resolve, ms));

/**
 * Method that polls a particular URL every interval for reports. Once reports
 * are received, returns the payload as promise.
 */
const pollAttributionReports = async (url, interval) => {
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
