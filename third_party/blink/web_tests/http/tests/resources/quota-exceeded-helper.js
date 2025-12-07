/**
 * Assert a QuotaExceededError with the expected parameters are thrown.
 *
 * @param {Function} func Function which should throw.
 * @param {number} quota - The expected quota value.
 * @param {number} requested - The expected requested value.
 * @param {string} description - Description of the condition being tested.
 *
 */
function assert_throws_quota_exceeded(func, quota, requested, description) {
  try {
    func.call(this);
    throw new Error(make_error(description, `${func} did not throw`));
  } catch (e) {
    // Basic sanity-checks on the thrown exception.
    if (typeof e !== 'object') {
      throw new Error(make_error(
          description,
          `${func} threw ${e} with type ${typeof e}, not an object`));
    }
    if (typeof e === null) {
      throw new Error(
          make_error(description, `${func} threw null, not an object`));
    }

    // Check that the exception and their properties match.
    if (e.name !== 'QuotaExceededError') {
      throw new Error(make_error(
          description, `${func} threw ${e} that is not a QuotaExceptionError`));
    }
    if (e.quota !== quota) {
      throw new Error(make_error(
          description,
          `${func} threw ${e} with quota ${e.quota}, expected ${quota}`));
    }
    if (e.requested !== requested) {
      throw new Error(make_error(
          description,
          `${func} threw ${e} with requested ${e.requested},` +
              ` expected ${requested}`));
    }

    // Check that the exception is from the right global.  This check is
    // last so more specific, and more informative, checks on the properties
    // can happen in case a totally incorrect exception is thrown.
    if (e.constructor !== self.QuotaExceededError) {
      throw new Error(make_error(
          description, `${func} threw an exception from the wrong global`));
    }
  }
}

function make_error(description, error) {
  return `assert_throws_quota_exceeded: ${description} ${error}`
}
