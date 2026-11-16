/* Editor-facing ooRexx bridge. No interpreter ABI escapes this header. */
#ifndef THE_OOREXX_H
#define THE_OOREXX_H
short oorexx_initialise_rexx(void);
short oorexx_finalise_rexx(void);
short oorexx_execute_macro_file(CHARTYPE *,CHARTYPE *,short *,bool);
short oorexx_execute_macro_instore(CHARTYPE *,short *,CHARTYPE **,int *,int *,int);
short oorexx_set_rexx_variable(CHARTYPE *,CHARTYPE *,LENGTHTYPE,int);
short oorexx_get_rexx_variable(CHARTYPE *,CHARTYPE **,int *);
CHARTYPE *oorexx_get_rexx_interpreter_version(CHARTYPE *);
unsigned long oorexx_MyRexxDeregisterFunction(CHARTYPE *);
unsigned long oorexx_MyRexxRegisterFunctionExe(CHARTYPE *);
int oorexx_read_queue_depth(void);
short oorexx_read_queue_push(const char *);
#endif
