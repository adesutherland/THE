# EDITV
**set and retrieve persistent macro variables**

## Syntax
```text
EDITV GET|PUT|GETF|PUTF var1 [var2 ...]
EDITV SET|SETF var1 value1 [var2 value2 ...]
EDITV SETL|SETLF|SETFL var1 value1
EDITV LIST|LISTF [var1 ...]
EDITV GETSTEM|GETSTEMF var
```

## Description
The EDITV command manipulates variables for the lifetime of the edit session or the file, depending
on the subcommand used.
Edit variables are useful for maintaining variable values from one execution of a macro to another.
EDITV GET, PUT, GETF, PUTF, GETSTEM and GETSTEMF are only valid from within a macro
as they reference Rexx variables. All other subcommands are valid from within a macro or from the
command line.
EDITV GET sets a Rexx macro variable, with the same name as the edit variable, to the value of the
edit variable.
EDITV PUT stores the value of a Rexx macro variable as an edit variable.
EDITV SET stores an edit variable with a value.
EDITV SET can only work with variable values comprising a single space-separated word. To
specify a variable value that contains spaces, use EDITV SETL.
EDITV LIST displays the values of the specified edit variables, or all variables if no edit variables are
specified.
EDITV GETSTEM enables setting multiple Rexx variables from the equivalent EDITV variables
based on the passed stem variable. The stem variable passed must contain a trailing period.
EDITV GETF, PUTF, SETF, SETLF, SETFL, LISTF and GETSTEMF all work the same way as
their counterparts without the F, but the variables are only available while the particular file is the
current file. This enables you to use the same edit variable name but with different values for different
files.

## Compatibility
XEDIT: N/A
KEDIT: Compatible. GETSTEM, GETSTEMF are THE extensions.

## Status
  Complete.
