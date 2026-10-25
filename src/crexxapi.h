#ifndef THE_CREXXAPI_H
#define THE_CREXXAPI_H
#define THE_CREXX_FUNCTIONS(X) \
 X(crexxsaa_create,int,(const char *,const char *,crexxsaa_context **)) \
 X(crexxsaa_destroy,void,(crexxsaa_context *)) \
 X(crexxsaa_set_compiler,int,(crexxsaa_context *,const char *,const char *,const char *)) \
 X(crexxsaa_set_cache_dir,int,(crexxsaa_context *,const char *)) \
 X(crexxsaa_register_address_environment,int,(crexxsaa_context *,const char *,crexxsaa_address_callback,void *)) \
 X(crexxsaa_set_address_environment,int,(crexxsaa_context *,const char *)) \
 X(crexxsaa_run_rxbin,int,(crexxsaa_context *,const char *,int,const char **,int *)) \
 X(crexxsaa_run_source,int,(crexxsaa_context *,const char *,const char *,unsigned,int,const char **,int *)) \
 X(crexxsaa_last_error,const char *,(crexxsaa_context *)) \
 X(crexxsaa_address_variable_set,int,(crexxsaa_context *,const char *,const char *,size_t)) \
 X(crexxsaa_address_variable_get_alloc,int,(crexxsaa_context *,const char *,char **,size_t *)) \
 X(crexxsaa_free,void,(void *))
#endif
