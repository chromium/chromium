
# Overview
`views_examples` and `views_examples_with_content` are executable tools to
showcase all the [Views](docs/ui/views/overview.md) components with some varied
styles. The latter has extra support for `web_view` as well.

## How to build and run
`views_examples`
```
$ autoninja -C out/Default views_examples

$ out/Default/views_examples
```

`views_examples_with_content`
```
$ autoninja -C out/Default views_examples_with_content

$ out/Default/views_examples_with_content
```

**Note: Both programs are available on all desktop platforms which support Views
toolkit, except `views_examples` is unavailable on Mac.**

### Optional args
*To run programs but only with specific examples enabled:*

`<output_dir>/<program> [--enable-examples=<example1,[example2...]>]`

*To get help from the programs:*

`<output_dir>/<program> --help` will print out the above usage info.

*To list examples:*

`<output_dir>/<program> --enable-examples` will print out all names of available
examples and execute as if all (the default) are specified.

## How to use

After launching the executable, there is a list of all available examples to
browse.

![Showcase gif](/docs/ui/views/images/views_examples_showcase.gif)

Use the scroll bar and select the example you'd like to browse. You can find how
how these examples are created below in the [examples](#examples) section. Each
example is linked to a corresponding source file with basic implementation
details.

[Contact the Views team](/docs/ui/ask/index.md) if you have any questions.

## Examples

The current available examples are listed below:

- [Accessibility Features](/ui/views/examples/ax_example.cc)
- [Animated Image View](/ui/views/examples/animated_image_view_example.cc)
- [Badge](/ui/views/examples/badge_example.cc)
- [Box Layout](/ui/views/examples/box_layout_example.cc)
- [Bubble](/ui/views/examples/bubble_example.cc)
- [Button](/ui/views/examples/button_example.cc)
- [Button (Sticker Sheet)](/ui/views/examples/button_sticker_sheet.cc)
- [Checkbox](/ui/views/examples/checkbox_example.cc)
- [Colored Dialog](/ui/views/examples/colored_dialog_example.cc)
- [Colors](/ui/views/examples/colors_example.cc)
- [Combo Box](/ui/views/examples/combobox_example.cc)
- [Designer](/ui/views/examples/designer_example.cc)
- [Dialog](/ui/views/examples/dialog_example.cc)
- [Fade Animation](/ui/views/examples/fade_animation.cc)
- [Flex Layout](/ui/views/examples/flex_layout_example.cc)
- [FloodFill Ink Drop](/ui/views/examples/ink_drop_example.cc)
- [Label](/ui/views/examples/label_example.cc)
- [Link](/ui/views/examples/link_example.cc)
- [Login Bubble Dialog](/ui/views/examples/login_bubble_dialog_example.cc)
- [Menu](/ui/views/examples/menu_example.cc)
- [Message Box View](/ui/views/examples/message_box_example.cc)
- [Multiline RenderText](/ui/views/examples/multiline_example.cc)
- [Notification](/ui/views/examples/notification_example.cc)
- [Progress Bar](/ui/views/examples/progress_bar_example.cc)
- [Radio Button](/ui/views/examples/radio_button_example.cc)
- [Scroll View](/ui/views/examples/scroll_view_example.cc)
- [Slider](/ui/views/examples/slider_example.cc)
- [Square Ink Drop](/ui/views/examples/square_ink_drop_example.cc)
- [Tabbed Pane](/ui/views/examples/tabbed_pane_example.cc)
- [Table](/ui/views/examples/table_example.cc)
- [Text Styles](/ui/views/examples/text_example.cc)
- [Textarea](/ui/views/examples/textarea_example.cc)
- [Textfield](/ui/views/examples/textfield_example.cc)
- [Throbber](/ui/views/examples/throbber_example.cc)
- [Toggle Button](/ui/views/examples/toggle_button_example.cc)
- [Tree View](/ui/views/examples/tree_view_example.cc)
- [Vector Icon](/ui/views/examples/vector_example.cc)
- [Widget](/ui/views/examples/widget_example.cc)

