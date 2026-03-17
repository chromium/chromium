# Security Policy

## Supported Versions

Only the latest released version is supported.

## Reporting a Vulnerability

To report a vulnerability, please go to https://github.com/zip-rs/zip2/security/advisories/new. We'll attempt to:

* Close the report within 7 days if it's invalid, or if a fix has already been released but some old versions needed to be yanked.
* Provide progress reports at least every 7 days to the original reporter.
* Aim to provide a fix within 30 days of the initial report. If a complete fix is not feasible in that timeframe (for example, due to complexity or external dependencies), we will communicate this to the reporter, share any available mitigations or workarounds, and adjust the expected timeline accordingly.

## Disclosure

A vulnerability will only be publicly disclosed once a fix is released. At that point, the delay before full public disclosure
will be determined as follows:

* If the proof-of-concept is very simple, or an exploit is already in the wild (whether or not it specifically targets `zip`),
  all details will be made public right away.
* If the vulnerability is specific to `zip` and cannot easily be reverse-engineered from the code history, then the
  proof-of-concept and most of the details will be withheld until 14 days after the fix is released and all vulnerable
  versions are yanked with `cargo yank`.
* If a potential victim requests more time to deploy a fix based on a credible risk, then the withholding of details can
  be extended up to 30 days. This may be extended to 90 days if the victim is high-value (e.g. manages over US$1 billion
  worth of financial assets or intellectual property, or has evidence that they're a target of nation-state attackers)
  and there's a valid reason why they cannot deploy the fix as fast as most users (e.g. heavy reliance on an old version's
  interface, or infrastructure damage in a war zone).
