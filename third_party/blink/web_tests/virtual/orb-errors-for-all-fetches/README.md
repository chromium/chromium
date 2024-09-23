# Test suite for OpaqueResponseBlockingErrorsForAllFetches

Since this feature is Fetch-related, this suite tests some fetch tests
with `--enable-features=OpaqueResponseBlockingErrorsForAllFetches`.
Tests which are expected to behave differently for ORB "errors-for-all-fetches"
have separate expectations.

Background:
- ORB "v0.1": Change blocking behaviour, but inject empty responses.
  (This is non-spec behaviour left over from CORB.)
- ORB "v0.2": Return error responses for fetches, except for script-initiated
  ones.
- ORB "errors for all fetches": Return error responses for all fetches.
  (This one doesn't have a number yet, since we're not sure when it launches.)
- ORB "v0.1" + "v0.2" have meanwhile been launched.
