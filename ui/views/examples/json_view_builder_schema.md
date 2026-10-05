# Views JSON Schema Specification

This document defines the formal JSON Schema for the Views Canvas declarative UI builder (`JsonViewBuilder`).

Any time [`JsonViewBuilder`](file:///C:/src/chromium/src/ui/views/examples/json_view_builder.h) ([`json_view_builder.cc`](file:///C:/src/chromium/src/ui/views/examples/json_view_builder.cc)) is updated to add, remove, or modify components, properties, or token resolvers, this specification must be updated in sync.

---

## 1. JSON Schema Definition (Draft 2020-12)

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://chromium.org/schemas/views-json-builder.json",
  "title": "ViewsJsonBuilderSchema",
  "description": "Schema for declarative Views hierarchy, Dialogs, Bubbles, and DialogModels used by ViewsCanvas and JsonViewBuilder.",
  "oneOf": [
    { "$ref": "#/$defs/ViewNode" },
    { "$ref": "#/$defs/DialogModelNode" },
    { "$ref": "#/$defs/DialogNode" },
    { "$ref": "#/$defs/BubbleNode" }
  ],
  "$defs": {
    "ViewNode": {
      "type": "object",
      "properties": {
        "type": {
          "type": "string",
          "description": "The class name of the View component to construct.",
          "enum": [
            "BoxLayoutView",
            "Checkbox",
            "FlexLayoutView",
            "ImageView",
            "Label",
            "Link",
            "MdTextButton",
            "RadioButton",
            "ScrollView",
            "Slider",
            "SmoothedThrobber",
            "StyledLabel",
            "TabbedPane",
            "TableLayoutView",
            "TableView",
            "Textarea",
            "Textfield",
            "Throbber",
            "ToggleButton",
            "View"
          ],
          "default": "View"
        },
        "title": {
          "type": "string",
          "description": "Optional tab title when this ViewNode is a child of a TabbedPane."
        },
        "TabTitle": {
          "type": "string",
          "description": "Alias for tab title when this ViewNode is a child of a TabbedPane."
        },
        "properties": {
          "type": "object",
          "description": "Property key-value pairs applied to the instantiated View or LayoutManager.",
          "additionalProperties": true,
          "properties": {
            "ID": { "type": ["integer", "string"], "description": "Integer identifier for the View." },
            "Enabled": { "type": ["boolean", "string"], "description": "Controls whether the view is enabled." },
            "Visible": { "type": ["boolean", "string"], "description": "Controls whether the view is visible." },
            "background": {
              "type": "string",
              "description": "Solid background specification. Format: 'solid,<color>'",
              "examples": ["solid,blue", "solid,ColorId:kColorAlertHighSeverity"]
            },
            "border": {
              "type": "string",
              "description": "Border specification. Format: 'solid,<thickness>,<color>', 'empty,<thickness>', 'empty,<top>,<left>,<bottom>,<right>', or 'empty,InsetsMetric:<MetricName>'",
              "examples": [
                "solid,1,gray",
                "empty,8",
                "empty,4,8,4,8",
                "empty,InsetsMetric:INSETS_DIALOG"
              ]
            },
            "layout_flex": {
              "type": ["integer", "string"],
              "description": "Flex weight when the parent uses BoxLayoutView or FlexLayoutView.",
              "examples": [1, "2"]
            },
            "accessiblename": {
              "type": "string",
              "description": "Accessible name set via ViewAccessibility."
            },
            "layout_manager": {
              "description": "Layout manager configuration when attached directly via properties.",
              "oneOf": [
                {
                  "type": "string",
                  "enum": ["BoxLayout", "FlexLayout", "TableLayout"]
                },
                {
                  "type": "object",
                  "properties": {
                    "type": {
                      "type": "string",
                      "enum": ["BoxLayout", "FlexLayout", "TableLayout"]
                    },
                    "Orientation": { "type": "string", "enum": ["kHorizontal", "kVertical"] },
                    "BetweenChildSpacing": { "type": ["integer", "string"] },
                    "InsideBorderInsets": { "type": "string" },
                    "CrossAxisAlignment": { "type": "string" },
                    "MainAxisAlignment": { "type": "string" },
                    "CollapseMarginsSpacing": { "type": "boolean" },
                    "InteriorMargin": { "type": "string" },
                    "CollapseMargins": { "type": "boolean" },
                    "columns": {
                      "type": "array",
                      "items": { "$ref": "#/$defs/TableLayoutColumn" }
                    },
                    "rows": {
                      "type": "array",
                      "items": { "$ref": "#/$defs/TableLayoutRow" }
                    }
                  },
                  "required": ["type"]
                }
              ]
            }
          }
        },
        "children": {
          "type": "array",
          "description": "Child views in the component hierarchy. For ScrollView, exactly one child is allowed. For TabbedPane, children represent individual tab pages.",
          "items": { "$ref": "#/$defs/ViewNode" }
        }
      }
    },
    "DialogModelNode": {
      "type": "object",
      "properties": {
        "type": { "type": "string", "enum": ["DialogModel"] },
        "title": { "type": "string", "description": "Dialog or bubble title." },
        "subtitle": { "type": "string", "description": "Dialog or bubble subtitle." },
        "accessible_title": { "type": "string", "description": "Screen reader title." },
        "modal_type": {
          "type": "string",
          "enum": ["kNone", "kWindow", "kChild", "kSystem"],
          "default": "kChild",
          "description": "Modality style for the dialog or bubble host."
        },
        "arrow": {
          "type": "string",
          "description": "Arrow orientation when anchored as a bubble (e.g. 'TOP_LEFT', 'BOTTOM_CENTER', 'NONE', 'FLOAT')."
        },
        "is_alert_dialog": { "type": "boolean", "default": false },
        "close_on_deactivate": { "type": "boolean", "default": true },
        "override_show_close_button": { "type": "boolean" },
        "icon": { "type": "string", "description": "Vector icon or solid color icon specification." },
        "banner": { "type": "string", "description": "Banner image specification." },
        "main_image": { "type": "string", "description": "Main image specification." },
        "buttons": {
          "description": "Dialog action buttons.",
          "oneOf": [
            {
              "type": "object",
              "properties": {
                "ok": { "$ref": "#/$defs/DialogModelButton" },
                "cancel": { "$ref": "#/$defs/DialogModelButton" },
                "extra": { "$ref": "#/$defs/DialogModelButton" }
              }
            },
            {
              "type": "array",
              "items": { "type": "string" }
            }
          ]
        },
        "override_default_button": {
          "type": "string",
          "enum": ["kOk", "kCancel", "kNone"]
        },
        "fields": {
          "type": "array",
          "description": "Ordered fields within the DialogModel body.",
          "items": { "$ref": "#/$defs/DialogModelField" }
        },
        "use_desktop_widget_override": {
          "type": "boolean",
          "default": true,
          "description": "Forces the dialog to create a top-level desktop window above the invoking window."
        },
        "footnote": { "type": "string", "description": "Footnote label text." }
      },
      "required": ["type"]
    },
    "DialogModelButton": {
      "type": "object",
      "properties": {
        "label": { "type": "string" },
        "style": { "type": "string", "enum": ["kProminent", "kTonal", "kText", "kDefault"] },
        "enabled": { "type": "boolean", "default": true }
      }
    },
    "DialogModelField": {
      "type": "object",
      "properties": {
        "type": {
          "type": "string",
          "enum": [
            "paragraph",
            "checkbox",
            "combobox",
            "textfield",
            "password_field",
            "separator",
            "title_item",
            "menu_item",
            "custom_view"
          ]
        },
        "id": { "type": ["integer", "string"] },
        "label": { "type": "string" },
        "text": { "type": "string" },
        "header": { "type": "string" },
        "accessible_name": { "type": "string", "description": "Screen reader name for textfield or control." },
        "accessible_text": { "type": "string", "description": "Screen reader name for password field." },
        "incorrect_password_text": { "type": "string", "description": "Error text displayed when password validation fails." },
        "checked": { "type": "boolean" },
        "placeholder": { "type": "string", "description": "Fallback alias for accessible_name in textfield." },
        "icon": { "type": "string", "description": "Vector icon or solid color specification for menu items." },
        "is_enabled": { "type": "boolean", "description": "Whether the menu item is enabled.", "default": true },
        "options": {
          "type": "array",
          "items": { "type": "string" }
        },
        "selected_index": { "type": "integer" },
        "field_type": { "type": "string", "enum": ["kControl", "kText", "kMenuItem"], "default": "kControl" },
        "view": { "$ref": "#/$defs/ViewNode" }
      },
      "required": ["type"]
    },
    "DialogNode": {
      "type": "object",
      "properties": {
        "type": { "type": "string", "enum": ["Dialog"] },
        "title": { "type": "string" },
        "accessible_title": { "type": "string" },
        "modal_type": {
          "type": "string",
          "enum": ["kNone", "kWindow", "kChild", "kSystem"],
          "default": "kWindow"
        },
        "close_on_deactivate": { "type": "boolean", "default": false },
        "show_close_button": { "type": "boolean", "default": true },
        "use_desktop_widget_override": {
          "type": "boolean",
          "default": true,
          "description": "Forces the dialog to create a top-level desktop window above the invoking window."
        },
        "buttons": {
          "oneOf": [
            { "type": "array", "items": { "type": "string" } },
            { "type": "string" },
            { "type": "integer" }
          ]
        },
        "ok_button_label": { "type": "string" },
        "cancel_button_label": { "type": "string" },
        "extra_button_label": { "type": "string" },
        "default_button": { "type": "string", "enum": ["kOk", "kCancel", "kNone"] },
        "contents": { "$ref": "#/$defs/ViewNode" },
        "children": { "type": "array", "items": { "$ref": "#/$defs/ViewNode" } },
        "footnote": { "type": "string" }
      },
      "required": ["type"]
    },
    "BubbleNode": {
      "type": "object",
      "properties": {
        "type": { "type": "string", "enum": ["Bubble"] },
        "title": { "type": "string" },
        "modal_type": {
          "type": "string",
          "enum": ["kNone", "kWindow", "kChild", "kSystem"],
          "default": "kNone"
        },
        "arrow": {
          "type": "string",
          "enum": [
            "TOP_LEFT", "TOP_RIGHT", "BOTTOM_LEFT", "BOTTOM_RIGHT",
            "LEFT_TOP", "RIGHT_TOP", "LEFT_BOTTOM", "RIGHT_BOTTOM",
            "TOP_CENTER", "BOTTOM_CENTER", "LEFT_CENTER", "RIGHT_CENTER",
            "NONE", "FLOAT"
          ],
          "default": "TOP_LEFT"
        },
        "close_on_deactivate": { "type": "boolean", "default": true },
        "show_close_button": { "type": "boolean", "default": true },
        "use_desktop_widget_override": {
          "type": "boolean",
          "default": true,
          "description": "Forces the bubble to create a top-level desktop window above the invoking window."
        },
        "shadow": {
          "type": "string",
          "enum": ["DIALOG_SHADOW", "STANDARD_SHADOW", "NO_SHADOW"],
          "default": "DIALOG_SHADOW"
        },
        "buttons": {
          "oneOf": [
            { "type": "array", "items": { "type": "string" } },
            { "type": "string" },
            { "type": "integer" }
          ]
        },
        "ok_button_label": { "type": "string" },
        "cancel_button_label": { "type": "string" },
        "extra_button_label": { "type": "string" },
        "default_button": { "type": "string", "enum": ["kOk", "kCancel", "kNone"] },
        "contents": { "$ref": "#/$defs/ViewNode" },
        "children": { "type": "array", "items": { "$ref": "#/$defs/ViewNode" } },
        "footnote": { "type": "string" }
      },
      "required": ["type"]
    },
    "TableLayoutColumn": {
      "type": "object",
      "properties": {
        "is_padding": { "type": "boolean", "default": false },
        "width": { "type": "integer", "description": "Fixed width for padding column." },
        "h_align": {
          "type": "string",
          "enum": ["kStart", "kCenter", "kEnd", "kStretch"],
          "default": "kStretch"
        },
        "v_align": {
          "type": "string",
          "enum": ["kStart", "kCenter", "kEnd", "kStretch"],
          "default": "kStretch"
        },
        "horizontal_resize": { "type": "number", "default": 0.0 },
        "size_type": {
          "type": "string",
          "enum": ["kUsePreferred", "kFixed"],
          "default": "kUsePreferred"
        },
        "fixed_width": { "type": "integer", "default": 0 },
        "min_width": { "type": "integer", "default": 0 }
      }
    },
    "TableLayoutRow": {
      "type": "object",
      "properties": {
        "is_padding": { "type": "boolean", "default": false },
        "height": { "type": "integer", "default": 0 },
        "vertical_resize": { "type": "number", "default": 0.0 }
      }
    },
    "TableViewColumn": {
      "type": "object",
      "properties": {
        "id": { "type": "integer" },
        "title": { "type": "string" },
        "alignment": { "type": "string", "enum": ["LEFT", "CENTER", "RIGHT"], "default": "LEFT" },
        "width": { "type": "integer", "default": -1 },
        "percent": { "type": "number" },
        "min_width": { "type": "integer" },
        "sortable": { "type": "boolean", "default": true }
      },
      "required": ["title"]
    },
    "StyledLabelRange": {
      "type": "object",
      "properties": {
        "start": { "type": "integer", "minimum": 0 },
        "end": { "type": "integer", "minimum": 0 },
        "length": { "type": "integer", "minimum": 1 },
        "style": { "type": "string", "description": "TextStyle identifier (e.g., 'STYLE_LINK', 'STYLE_HEADLINE_4_BOLD')." },
        "color": { "type": "string", "description": "Color name, hex, or 'ColorId:<Name>'." },
        "tooltip": { "type": "string" },
        "accessible_name": { "type": "string" }
      },
      "required": ["start"]
    }
  }
}
```

---

## 2. Supported Components & Properties

### Dialog & Bubble Paradigms

| Component (`type`) | Descriptions & Top-Level Properties |
| :--- | :--- |
| **`DialogModel`** | Abstract model mapping to Chromium's `ui::DialogModel`.<br>• `title`, `subtitle`, `accessible_title` (`string`)<br>• `modal_type` (`string`): `"kWindow"`, `"kChild"`, `"kNone"`, `"kSystem"`<br>• `close_on_deactivate` (`boolean`): Dismiss on focus loss<br>• `override_show_close_button` (`boolean`)<br>• `icon`, `banner`, `main_image` (`string`): Vector icons or colors<br>• `buttons` (`object` / `array`): Action buttons (`ok`, `cancel`, `extra`) with styles<br>• `fields` (`array`): Ordered list of field objects (`paragraph`, `checkbox`, `combobox`, `textfield`, `password_field`, `separator`, `title_item`, `menu_item`, `custom_view`)<br>• `footnote` (`string`) |
| **`Dialog`** | Explicit views-based window dialog delegate.<br>• `title`, `accessible_title` (`string`)<br>• `modal_type` (`string`): `"kWindow"` (default), `"kNone"`<br>• `buttons` (`array` / `string`): `["kOk", "kCancel"]`, `["kOk"]`, etc.<br>• `ok_button_label`, `cancel_button_label`, `extra_button_label` (`string`)<br>• `contents` / `children`: Declarative Views subtree |
| **`Bubble`** | Explicit views-based anchored popup bubble.<br>• `title` (`string`)<br>• `arrow` (`string`): `"TOP_LEFT"`, `"TOP_RIGHT"`, `"BOTTOM_CENTER"`, etc.<br>• `close_on_deactivate` (`boolean`): Default `true`<br>• `shadow` (`string`): `"DIALOG_SHADOW"`, `"STANDARD_SHADOW"`, `"NO_SHADOW"`<br>• `modal_type` (`string`): `"kNone"` (default), `"kChild"`<br>• `contents` / `children`: Declarative Views subtree |

---

### Universal View Properties
All views inherit these properties:
- **`ID`** (`integer` | `string`): Assigns an integer ID to the view.
- **`Enabled`** (`boolean` | `string`): Enables or disables user interaction.
- **`Visible`** (`boolean` | `string`): Toggles view visibility.
- **`background`** (`string`): Sets a background (e.g. `"solid,red"`, `"solid,ColorId:kColorPrimaryBackground"`).
- **`border`** (`string`): Sets solid or empty borders (e.g. `"solid,1,black"`, `"empty,10"`, `"empty,4,8,4,8"`, `"empty,InsetsMetric:INSETS_DIALOG"`).
- **`layout_flex`** (`integer` | `string`): Assigns layout weight inside a flex or box layout.
- **`accessiblename`** (`string`): Sets accessible text for screen readers.
- **`layout_manager`** (`string` | `object`): Sets and configures the layout manager.

---

### Component-Specific Properties

| Component (`type`) | Properties & Descriptions |
| :--- | :--- |
| **`Label`** | • `Text` (`string`): Text content.<br>• `TextStyle` (`string`): Typography style (`STYLE_PRIMARY`, `STYLE_HEADLINE_1`, etc.).<br>• `TextContext` (`string`): Typography context (`CONTEXT_LABEL`, `CONTEXT_DIALOG_TITLE`, etc.).<br>• `HorizontalAlignment` (`string`): `ALIGN_LEFT`, `ALIGN_CENTER`, `ALIGN_RIGHT`, `ALIGN_TO_HEAD`.<br>• `MultiLine` (`boolean`): Enable multiline wrapping.<br>• `MaxLines` (`integer`): Maximum visible lines.<br>• `Selectable` (`boolean`): Allow user text selection. |
| **`StyledLabel`** | • `Text` (`string`): Full string text.<br>• `DefaultTextStyle` (`string`): Default typography style.<br>• `HorizontalAlignment` (`string`): Alignment.<br>• `ranges` / `style_ranges` (`array`): Array of style range objects (`start`, `length`/`end`, `style`, `color`, `tooltip`, `accessible_name`). |
| **`ImageView`** | • `image` / `vector_icon` (`string`): `"solid,<color>[,w,h]"` or `"<icon_name>[,size,color]"` or `"vector_icon:<name>"`.<br>• `imagesize` (`string`): Preferred image dimensions (e.g., `"24,24"`, `"24 x 24"`).<br>• `cornerradius` (`integer`): Corner radius for rounded rendering.<br>• `tooltiptext` (`string`): Tooltip text. |
| **`MdTextButton`** | • `Text` (`string`): Button label.<br>• `Style` (`string`): Button visual style (`kProminent`, `kTonal`, `kText`, `kFilled`).<br>• `IsDefault` (`boolean`): Whether this is the default action button. |
| **`Checkbox`** | • `Text` (`string`): Checkbox label.<br>• `Checked` (`boolean`): Checked state. |
| **`RadioButton`** | • `Text` (`string`): Radio button label.<br>• `Checked` (`boolean`): Selected state.<br>• `Group` (`integer`): Mutual exclusion group index. |
| **`ToggleButton`** | • `Text` (`string`): Toggle label.<br>• `IsOn` (`boolean`): On/off state. |
| **`Textfield`** | • `Text` (`string`): Input text.<br>• `PlaceholderText` (`string`): Placeholder hint text.<br>• `ReadOnly` (`boolean`): Read-only flag.<br>• `TextInputType` (`string`): Virtual keyboard input type. |
| **`Textarea`** | • `Text` (`string`): Multi-line text.<br>• `PlaceholderText` (`string`): Placeholder hint. |
| **`Slider`** | • `Value` (`number`): Position from `0.0` to `1.0`.<br>• `style` / `renderingstyle` (`string`): `kDefaultStyle`, `kMinimalStyle`. |
| **`Throbber`** | • `Checked` (`boolean`): Show checkmark state.<br>• `running` / `isrunning` (`boolean`): Starts or stops animation. |
| **`SmoothedThrobber`** | • `StartDelayMs` (`integer`): Delay before animating.<br>• `StopDelayMs` (`integer`): Delay before stopping.<br>• `running` (`boolean`): Start/stop animation. |
| **`ScrollView`** | • `UseContentsPreferredSize` (`boolean`): Uses the contents' preferred size when laying out if one exists.<br>• `HorizontalScrollBarMode` (`string`): `kDisabled`, `kHiddenButEnabled`, `kEnabled`.<br>• `VerticalScrollBarMode` (`string`): `kDisabled`, `kHiddenButEnabled`, `kEnabled`.<br>• `AllowKeyboardScrolling` (`boolean`): Enable arrow key scrolling.<br>• `DrawOverflowIndicator` (`boolean`): Draw overflow edge indicators.<br>• `HasFocusIndicator` (`boolean`): Show focus indicator.<br>• `TreatAllScrollEventsAsHorizontal` (`boolean`): Treat all scroll events as horizontal.<br>• **Children rule**: Must have exactly one child in `children` (installed as the scroll contents). |
| **`TabbedPane`** | • `SelectedTabIndex` (`integer`): 0-based selected tab index.<br>• `DrawTabDivider` (`boolean`): Draw line divider below tab strip.<br>• **Children rule**: Each child represents a tab page and can specify `"title"` or `"TabTitle"`. |
| **`TableView`** | • `TableType` (`string`): `TEXT_ONLY` or `ICON_AND_TEXT`.<br>• `SingleSelection` (`boolean`): Single row selection mode.<br>• `columns` (`array`): Column definitions (`id`, `title`, `alignment`, `width`, `percent`, `min_width`, `sortable`).<br>• `rows` / `data` (`array`): 2D array of row cell values `[["Cell 0,0", "Cell 0,1"], ...]`. |
| **`BoxLayoutView`** / **`FlexLayoutView`** / **`TableLayoutView`** | • Host views that incorporate layout managers and expose layout properties directly on the view. |

---

## 3. Dynamic Token Resolvers

### Colors
- **Standard Names**: `"red"`, `"green"`, `"blue"`, `"white"`, `"black"`, `"gray"`, `"lightgray"`, `"darkgray"`, `"cyan"`, `"magenta"`, `"yellow"`, `"transparent"`.
- **Hex/RGB Format**: `"0xAARRGGBB"`, `"#RRGGBB"`, `"#AARRGGBB"`, `"rgb(r,g,b)"`, `"rgba(r,g,b,a)"`.
- **System ColorId Token**: `"ColorId:<ColorIdName>"` (e.g. `"ColorId:kColorAlertHighSeverity"`, `"ColorId:kColorButtonBackground"`).

### Insets & Spacing Metrics
- **Insets**: `"top,left,bottom,right"` or `"InsetsMetric:<InsetsMetricName>"` (e.g. `"InsetsMetric:INSETS_DIALOG"`, `"InsetsMetric:INSETS_CHECKBOX_RADIO_BUTTON"`).
- **Distances**: Integer or `"DistanceMetric:<DistanceMetricName>"` (e.g. `"DistanceMetric:DISTANCE_RELATED_CONTROL_HORIZONTAL"`, `"DistanceMetric:DISTANCE_BUTTON_HORIZONTAL_PADDING"`).

### Typography
- **TextStyle**: `"STYLE_<NAME>"` or `"TextStyle:STYLE_<NAME>"` (e.g. `"STYLE_PRIMARY"`, `"STYLE_BODY_1"`, `"STYLE_HEADLINE_4_BOLD"`, `"STYLE_LINK"`).
- **TextContext**: `"CONTEXT_<NAME>"` or `"TextContext:CONTEXT_<NAME>"` (e.g. `"CONTEXT_LABEL"`, `"CONTEXT_DIALOG_TITLE"`, `"CONTEXT_BUTTON"`).

---

## 4. Examples

### Example A: Declarative `DialogModel`
```json
{
  "type": "DialogModel",
  "title": "Clear Browsing Data",
  "subtitle": "Basic and advanced history clearing",
  "modal_type": "kChild",
  "close_on_deactivate": true,
  "buttons": {
    "ok": {
      "label": "Clear Data",
      "style": "kProminent"
    },
    "cancel": {
      "label": "Cancel"
    }
  },
  "fields": [
    {
      "type": "paragraph",
      "header": "Time Range",
      "text": "Choose how far back you want to clear browsing data."
    },
    {
      "type": "combobox",
      "id": 1,
      "label": "Time range",
      "options": ["Last hour", "Last 24 hours", "Last 7 days", "All time"],
      "selected_index": 1
    },
    {
      "type": "checkbox",
      "id": 2,
      "label": "Browsing history (42 items)",
      "checked": true
    },
    {
      "type": "separator"
    },
    {
      "type": "textfield",
      "id": 3,
      "label": "Confirm Keyword",
      "placeholder": "Type 'DELETE' to confirm"
    }
  ],
  "footnote": "Your Google Account sync data will not be removed."
}
```

### Example B: Explicit `Bubble` with Custom Views Subtree
```json
{
  "type": "Bubble",
  "title": "Quick Profile Settings",
  "arrow": "TOP_LEFT",
  "close_on_deactivate": true,
  "buttons": ["kOk", "kCancel"],
  "ok_button_label": "Save",
  "contents": {
    "type": "BoxLayoutView",
    "properties": {
      "Orientation": "kVertical",
      "BetweenChildSpacing": 10,
      "InsideBorderInsets": "12,12,12,12"
    },
    "children": [
      {
        "type": "Label",
        "properties": {
          "Text": "Profile Display Name",
          "TextStyle": "STYLE_BODY_1_BOLD"
        }
      },
      {
        "type": "Textfield",
        "properties": {
          "Text": "Alex Developer",
          "PlaceholderText": "Enter profile name"
        }
      },
      {
        "type": "Checkbox",
        "properties": {
          "Text": "Show avatar in toolbar",
          "Checked": true
        }
      }
    ]
  }
}
```
