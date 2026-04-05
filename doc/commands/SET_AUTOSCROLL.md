# SET AUTOSCROLL
**set rate of automatic horizontal scrolling**

## Syntax
```text
[SET] AUTOSCroll n|OFF|Half
```

## Description
The SET AUTOSCROLL allows the user to set the rate at which automatic horizontal scrolling
occurs.
When the cursor reaches the last (or first) column of the filearea the filearea can automatically scroll
if AUTOSCROLL is not OFF and a CURSOR RIGHT or CURSOR LEFT command is issued. How
many columns are scrolled is determined by the setting of AUTOSCROLL.
If AUTOSCROLL is set to HALF , then half the number of columns in the filearea window are
scrolled. Any other value will result in that many columns scrolled, or the full width of the filearea
window if the set number of columns is larger.
Autoscrolling does not occur if the key pressed is assigned to CURSOR SCREEN LEFT or RIGHT,
which is the case if SET COMPAT XEDIT key definitions are active.

## Compatibility
XEDIT: N/A
KEDIT: Compatible.

## Default
HALF

## Status
Complete.
