# LiteRT Security Policy

This directory contains the LiteRT library, which is used in Chromium for
on-device machine learning.

## Security Boundary and Trust Model

The primary security boundary for LiteRT in Chromium is the **source of the
model**.

### Trusted Models
For the vast majority of LiteRT use cases in Chromium (such as the Optimization
Guide, Autofill, Page Content Annotations, etc.), the models are considered
**trusted**. These models are:
1.  Bundled with the Chromium binary at build time, OR
2.  Downloaded from Google-controlled servers over secure channels (HTTPS).

**A malicious LiteRT model is NOT a valid entry point for an attack against
these components.**

Vulnerability reports that rely on a "malicious model" to trigger memory
corruption, type confusion, or other issues in LiteRT are generally **invalid**
unless they also demonstrate a way for an unprivileged attacker (e.g., a
malicious website) to provide that model to the component.

### Untrusted Models (WebNN)
The [WebNN API](https://webmachinelearning.github.io/webnn/) is the only
feature that may execute graphs derived from untrusted (website-provided)
content. WebNN has its own security architecture, including validation and
sandboxing, to mitigate risks from malicious graphs.

## Guidance for Researchers and AI Agents

When evaluating a potential security issue in LiteRT:

*   **Do not assume that an attacker can provide a `.tflite` model.**
*   If an issue requires a specifically crafted model, you **must** identify a
    standard Chromium feature that allows an untrusted party to supply such a
    model, such as the ability to generate the model using WebNN.
*   Bugs that require manual placement of a model file on disk or the use of
    developer-only command-line flags (like `--model-override`) are **not**
    security vulnerabilities.
