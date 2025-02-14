# Experimental APIs

Experimental APIs are in the early stages of design and are likely to change as the owners incorporate feedback and add support for additional scenarios.

These APIs are flighted externally using [Windows Insider SDKs](https://www.microsoft.com/software-download/windowsinsiderpreviewSDK) so that developers can try them out and provide feedback before they become part of the official platform. While they are flighted, there is no promise of stability or commitment.

## Consuming experimental APIs

By default, these APIs are disabled at runtime and calling them will result in `E_NOTIMPL` error code being returned if function returns an HRESULT, and will have no effect in certain other cases. This is another safeguard to help prevent inadvertent dependencies and broad distribution of apps that consume experimental APIs.

To enable these APIs for experimentation, you may need to use an enablement binary that can be shared with you. These enablement binaries are not redistributable and should not be included in your app package. They are intended for development and testing purposes only.

Documentation for a particular experimental API is at the discretion of the team that owns it.

## Providing feedback

If you've tried an experimental API and would like to provide feedback, use the **Security and Privacy > Passkey** category in the [Windows Feedback Hub](https://aka.ms/PasskeyFeedback).

## Operating System Requirements

Windows 11 Insider Edition Beta Channel: Build Major Version = 22635 and Minor Version >= 4515.

Windows 11 Insider Edition Dev Channel: Build Major Version = 26120 and Minor Version >= 2510.

## Related topics

  [**webauthn**](https://learn.microsoft.com/en-us/windows/win32/api/webauthn/)
