# chromeexts - Chrome's Windbg Extension

`chromeexts` is a debugger extension that works in `cdb`, `ntsd`, and `windbg`.

[TOC]

## Loading chromeexts

First build the `chromeexts` build target with `ninja` and then in the debugger,
run the following command:

```
> .load [Path to the chromeexts.dll]
```

After that, the following commands will work in the debugger.

## UI Commands

### !view

```
> !views [options] <Address To View Object>
```

Provides information about a `view` object in a easy to read format.

| Option      | Description |
|:-----------:|-------------|
| `/children` | Output the addresses of all children |
| `/r`        | Requires `/children`. Output all children recursively. |

#### Examples

Simple Output

```
> !view 0x250a57f0
Bounds: 0,9 (1042x1077)
Parent: 0x250801a8
```

Viewing the Children

```
> !view /children 0x250a57f0
Child Count: 4
25b9e368 25b91110 2507e0a8 2507fd78
```

Viewing the Children Recursively

```
> !view /children /r 0x25ee8098
Child Count: 4
<views!views::MdTextButton::`vftable' address="0x25ee8098" bounds="274 8 32x106" enabled="false" id="0" needs-layout="false" visible="true">
  <views!views::FocusRing::`vftable' address="0x25e2f328" bounds="-1 -1 34x108" enabled="false" id="0" needs-layout="false" visible="false" />
  <views!views::InkDropContainerView::`vftable' address="0x25e2b9d8" bounds="0 0 32x106" enabled="false" id="0" needs-layout="false" visible="false" />
  <views!views::ImageView::`vftable' address="0x25e2d4f8" bounds="16 16 0x0" enabled="false" id="0" needs-layout="false" visible="true" />
  <views!views::internal::LabelButtonLabel::`vftable' address="0x25f75690" bounds="16 0 32x74" enabled="false" id="0" needs-layout="false" visible="true" />
</views!views::MdTextButton::`vftable'>
```

### !hwnd

```
> !hwnd <Window Handle>
```

**Requires live debugger.** Provides information about an `HWND`.

#### Example

```
> !hwnd 008F0638
Title: chromeexts - Chrome's Windbg Extension - Google Chrome
Class: Chrome_WidgetWin_1
Hierarchy:
   Owner: 00000000 Parent: 00000000
   Prev:  000900c8 Next:   000402f4
Styles: 16cf0000 (Ex: 00000100)
Bounds: (253, 246) 1718x816
```
