import {
  TestMessageTargetReceiver, TestMessageTarget_NestedEnum, TestMessageTarget,
  TestMessageTargetCallbackRouter, SubinterfaceCallbackRouter,
  SubinterfaceRemote,
  SubinterfaceClientCallbackRouter} from "/gen/content/test/data/lite_js_test.mojom.m.js";

import {MojoEchoRemote} from "/gen/content/web_test/common/mojo_echo.mojom.m.js"

import {
  ParamsRemote as OptionalNumericsParamsRemote,
  ParamsReceiver as OptionalNumericsParamsReceiver,
  ResponseParamsRemote as OptionalNumericsResponseParamsRemote,
  ResponseParamsReceiver as OptionalNumericsResponseParamsReceiver,
  RegularEnum as OptionalNumericsRegularEnum,
} from "/gen/content/web_test/common/mojo_optional_numerics_unittest.mojom.m.js"

globalThis.TestMessageTargetReceiver = TestMessageTargetReceiver;
globalThis.TestMessageTarget_NestedEnum = TestMessageTarget_NestedEnum;
globalThis.TestMessageTarget = TestMessageTarget;
globalThis.TestMessageTargetCallbackRouter = TestMessageTargetCallbackRouter;
globalThis.SubinterfaceCallbackRouter = SubinterfaceCallbackRouter;
globalThis.SubinterfaceRemote = SubinterfaceRemote;
globalThis.SubinterfaceClientCallbackRouter = SubinterfaceClientCallbackRouter;

globalThis.MojoEchoRemote = MojoEchoRemote;

Object.assign(globalThis, {
  OptionalNumericsParamsRemote: OptionalNumericsParamsRemote,
  OptionalNumericsParamsReceiver: OptionalNumericsParamsReceiver,
  OptionalNumericsResponseParamsRemote: OptionalNumericsResponseParamsRemote,
  OptionalNumericsResponseParamsReceiver:
     OptionalNumericsResponseParamsReceiver,
  OptionalNumericsRegularEnum: OptionalNumericsRegularEnum,
});
