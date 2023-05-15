// Adds all used Mojo classes/structs/enums to globalThis. This allows the
// bindings-tests.js script to be used for both various types of tests. See
// README.md for more information.

if (typeof liteJsTest?.mojom !== 'undefined') {
  globalThis.TestMessageTargetReceiver = liteJsTest.mojom.TestMessageTargetReceiver;
  globalThis.TestMessageTarget_NestedEnum = liteJsTest.mojom.TestMessageTarget_NestedEnum;
  globalThis.TestMessageTarget = liteJsTest.mojom.TestMessageTarget;
  globalThis.TestMessageTargetCallbackRouter = liteJsTest.mojom.TestMessageTargetCallbackRouter;
  globalThis.SubinterfaceCallbackRouter = liteJsTest.mojom.SubinterfaceCallbackRouter;
  globalThis.SubinterfaceRemote = liteJsTest.mojom.SubinterfaceRemote;
  globalThis.SubinterfaceClientCallbackRouter = liteJsTest.mojom.SubinterfaceClientCallbackRouter;
}

if (typeof content?.mojom?.MojoEchoRemote !== 'undefined') {
  globalThis.MojoEchoRemote = content.mojom.MojoEchoRemote;
}

if (typeof content?.optionalNumericsUnittest !== 'undefined') {
  Object.assign(globalThis, {
    OptionalNumericsParamsRemote:
      content.optionalNumericsUnittest.mojom.ParamsRemote,
    OptionalNumericsParamsReceiver:
      content.optionalNumericsUnittest.mojom.ParamsReceiver,
    OptionalNumericsResponseParamsRemote:
      content.optionalNumericsUnittest.mojom.ResponseParamsRemote,
    OptionalNumericsResponseParamsReceiver:
      content.optionalNumericsUnittest.mojom.ResponseParamsReceiver,
    OptionalNumericsRegularEnum:
      content.optionalNumericsUnittest.mojom.RegularEnum,
  });
}
