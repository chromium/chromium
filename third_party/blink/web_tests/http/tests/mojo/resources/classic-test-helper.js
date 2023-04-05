// Adds all used Mojo classes/structs/enums to globalThis. This allows the
// bindings-tests.js script to be used for both various types of tests. See
// README.md for more information.

globalThis.TestMessageTargetReceiver = liteJsTest.mojom.TestMessageTargetReceiver;
globalThis.TestMessageTarget_NestedEnum = liteJsTest.mojom.TestMessageTarget_NestedEnum;
globalThis.TestMessageTarget = liteJsTest.mojom.TestMessageTarget;
globalThis.TestMessageTargetCallbackRouter = liteJsTest.mojom.TestMessageTargetCallbackRouter;
globalThis.SubinterfaceCallbackRouter = liteJsTest.mojom.SubinterfaceCallbackRouter;
globalThis.SubinterfaceRemote = liteJsTest.mojom.SubinterfaceRemote;
globalThis.SubinterfaceClientCallbackRouter = liteJsTest.mojom.SubinterfaceClientCallbackRouter;
globalThis.MojoEchoRemote = content.mojom.MojoEchoRemote;
