This tests for a bug where expansion for smart delete would not consider editable boundaries. Only 'foo' should be deleted. You should see ' bar'. <radr://problem/5390681>
| <span>
|   contenteditable="false"
|   " bar"
