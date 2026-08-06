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
  "description": "Schema for declarative Views hierarchy JSON used by ViewsCanvas and JsonViewBuilder.",
  "$ref": "#/$defs/ViewNode",
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

### Common Universal Properties
All views inherit these properties via reflection or dynamic property handlers:
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
| **`ScrollView`** | • **Children rule**: Must have exactly one child in `children` (installed as the scroll contents). |
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

## 4. Example Layout

```json
{
  "type": "BoxLayoutView",
  "properties": {
    "Orientation": "kVertical",
    "BetweenChildSpacing": 10,
    "InsideBorderInsets": "10,10,10,10"
  },
  "children": [
    {
      "type": "Label",
      "properties": {
        "Text": "Sample Views Header",
        "TextStyle": "STYLE_HEADLINE_4_BOLD"
      }
    },
    {
      "type": "StyledLabel",
      "properties": {
        "Text": "Click here to learn more about Chromium Views.",
        "ranges": [
          {
            "start": 0,
            "length": 10,
            "style": "STYLE_LINK",
            "tooltip": "Documentation link"
          }
        ]
      }
    },
    {
      "type": "TabbedPane",
      "children": [
        {
          "title": "General",
          "type": "BoxLayoutView",
          "properties": {
            "Orientation": "kHorizontal",
            "BetweenChildSpacing": 8
          },
          "children": [
            {
              "type": "MdTextButton",
              "properties": {
                "Text": "Submit",
                "Style": "kProminent"
              }
            }
          ]
        },
        {
          "title": "Details",
          "type": "TableView",
          "properties": {
            "columns": [
              { "id": 0, "title": "Item", "percent": 0.6 },
              { "id": 1, "title": "Status", "percent": 0.4 }
            ],
            "rows": [
              ["Task 1", "Complete"],
              ["Task 2", "In Progress"]
            ]
          }
        }
      ]
    }
  ]
}
```
