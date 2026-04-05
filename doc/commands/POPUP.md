# POPUP
**display popup menu**

## Syntax
```text
POPUP [MOUSE|TEXT|CENTER|CENTRE|BELOW|ABOVE] [INITIAL n] [ESCAPE keyname]
[KEYS keyname,keyname,...] /item1[/item2/...]
```

## Description
The POPUP command allows the user to create and display a popup menu containing a list of
selectable options.
The location of the popup menu is specified by the first parameter.
MOUSE specifies that the top left corner of the popup menu is to be displayed where the mouse
cursor currently is displayed. This option is only valid if the popup window is initiated from a macro
assigned to a mouse event.
TEXT specifies that the top left corner of the popup menu is to be displayed where the text cursor is
displayed. If the text starts with a dash, then a divider line is inserted.
ABOVE specifies that the bottom row of the popup window is to be displayed above the line where
the text cursor is displayed. The popup window will use at most from the line above the text cursor to
the top of the screen.
BELOW specifies that the top row of the popup window is to be displayed below the line where the
text cursor is displayed. The popup window will use at most from the line below the text cursor to the
bottom of the screen.
CENTRE or CENTER specifies that the popup window is centred in the middle of the screen. This
option will use all of the screen to display the popup window if necessary.
If the location is not specified, then the default is CENTRE
INITIAL specifies the item to be highlighted when the popup window is first displayed. This value
must be within the bounds of the items specified.
ESCAPE specifies the keyname that can be used to quit from the popup window without making a
selection. By default 'q' will quit. Only keynames that are valid with the DEFINE command are
allowed.

KEYS specifies a list of keynames that can be used to exit from the popup with a selection. Only
keynames that are valid with the DEFINE command are allowed. A maximum of 20 keynames can be
specified.
On return from the popup menu, the following Rexx variables are set:
popup.0 = 4
popup.1 = Item selected or empty string if no item selected.
popup.2 = Item number selected or zero if no item selected.
popup.3 = Item number on which the cursor was last positioned.
popup.4 = The index into the list of 'KEYS' that terminated the popup or 0 if ENTER used.
If mouse support is available, an item is selectable by clicking the first mouse button on the item. To
quit from the popup window without making a selection, click the mouse outside the popup window,
or on the border of the window.
Keyboard keys that take effect in the POPUP command are CURU, CURD, CURL, CURR, PGUP,
PGDN and ENTER.
The colours used for the popup are:
Border - SET COLOR POPUPBORDER
Non-current line - SET COLOR POPUP
Current line - SET COLOR POPUPCURLINE
Divider line - SET COLOR POPUPDIVIDER

## Compatibility
XEDIT: N/A
KEDIT: KEDIT does not support INITIAL, ESCAPE, KEYS, ABOVE or BELOW options.

## See Also
DIALOG, ALERT

## Status
Complete.
