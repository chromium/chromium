# Icon Checker

Validates icon names in HTML files against
[Google Fonts](https://fonts.google.com/icons) naming.

## Usage

Used in presubmits to ensure icons match Google Fonts names.
Applies to files in:
- `chrome/browser/resources`
- `components/vector_icons`
- `ui/webui/resources`

## Triggers

The icon checker presubmit is triggered automatically when you upload or
commit changes that affect:

1.  **HTML or TypeScript files** in `chrome/browser/resources` or
    `ui/webui/resources` that contain a `cr-iconset` element. The checker
    scans these files for `<g id="icon-name">` tags and extracts the icon
    names.
2.  **New `.icon` files** added to `components/vector_icons`. The checker
    uses the filename as the icon name.

In both cases, the extracted icon names are compared against a known list of
valid Material Symbol names stored in `icon_list.json`. If a name does not
match, a warning is issued to ensure consistency with Google Fonts naming
conventions.


## How to Update

Run to update `icon_list.json` from Google Symbols API:
```bash
python3 tools/resources/icon_checker/update_icon_list.py
```
