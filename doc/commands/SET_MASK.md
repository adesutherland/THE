# SET MASK

Prefill newly created lines with a file-specific template.

```text
SET MASK IMMED [text]
SET MASK DEFINE
SET MASK MODIFY
QUERY MASK
EXTRACT /MASK/
```

IMMED replaces the mask. An omitted text clears it. Leading and trailing spaces
in text are significant. EXTRACT sets MASK.0 to 1 and MASK.1 to the mask text.

ADD, INPUT, INPUTSTEM, REPLACE, Enter line creation and prefix A/I overlay the
provided text on this template. Copied/recovered lines keep their original bytes.
PRESERVE/RESTORE saves/restores the mask. It is shared by views of the same file.
The mask is limited to 32767 bytes and the configured maximum line length.
UTF-8 insertion uses logical editor cells and never cuts a grapheme; a partially
overwritten wide mask character becomes spaces before the remaining mask text.

DEFINE and MODIFY prefill an editable `set mask immed ...` command in THE's
command line, with a scale or the current mask. Edit it and press Enter. This
uses the common input model in curses, LLM and browser frontends. These two
interactive forms are not available in batch mode.

The XEDIT reference is [IBM SET MASK](https://www.ibm.com/docs/en/zvm/7.3.0?topic=macros-set-mask).
DEFINE/MODIFY use THE's command-line editing flow rather than XEDIT's scale-only
prompt.
