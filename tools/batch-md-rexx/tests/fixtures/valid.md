# Batch Markdown REXX Sample

This paragraph is ordinary Markdown. It includes punctuation, `inline code`,
and spacing that the parser must preserve exactly.

```rexx id=source-only run=false output=text
options levelb
answer = 41 + 1
```

The text between examples is also ordinary Markdown.

```rexx id=hello-run run=true kind=standalone output=text timeout=5000 expect=hello-output
options levelb
say "hello from CREXX"
```

```text
This non-REXX fence is not an example and should pass through untouched.
```

```rexx id=markdown-output run=true kind=standalone output=markdown fail-on-diagnostics=true
options levelb
say "| name | value |"
say "| --- | --- |"
say "| answer | 42 |"
```

```the id=buffer-message run=true kind=the-macro output=text
options levelb
address the
'emsg BATCH_DOC_EXAMPLE'
```

Final Markdown stays outside generated sections.

