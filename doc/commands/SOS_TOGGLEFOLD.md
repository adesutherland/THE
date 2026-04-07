# SOS TOGGLEFOLD
**Fold or unfold the AST node at the cursor**

## Syntax
[SOS] TOGGLEFOLD

## Description
The SOS TOGGLEFOLD command folds or unfolds the AST code block starting at the cursor. If the code block is already folded (hidden using exclude lines), it will be unfolded. If it is currently visible, it will be folded. 

This command requires THE to be compiled with DSLSH (`USE_SDSLH=ON`) and the current file to have an active `SET PARSER` configuration.

## Compatibility
XEDIT: N/A
KEDIT: N/A

## See Also
[SOS PREFIX](SOS_PREFIX.md)

## Status
Complete.