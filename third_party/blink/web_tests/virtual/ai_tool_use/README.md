# AI Tool Use Virtual Test Suite

This virtual test suite enables the Tool Use functionality for the AI Prompt
API.

## Purpose

This suite runs Web Platform Tests (WPT) for the Language Model API with Tool
Use capabilities enabled via the `AIPromptAPIToolUse` runtime flag.

## What is Tool Use?

Tool Use allows AI language models to call external functions/tools during
conversation. The model can:
1. Generate tool call requests with function names and arguments
2. Receive tool execution results from the application
3. Continue the conversation with context from tool results

## Test Coverage

The suite covers:
- Tool declaration and registration
- Tool call generation by the model
- Tool response handling (success and error cases)
- Tool result serialization edge cases (circular refs, functions, BigInt, etc.)
- Multi-turn conversations with tool interactions

## Runtime Flag

- `AIPromptAPIToolUse`: Enables Tool Use functionality

## Related Files

- Implementation: `third_party/blink/renderer/modules/ai/`
- Test specs: `third_party/blink/web_tests/external/wpt/ai/language-model/`
