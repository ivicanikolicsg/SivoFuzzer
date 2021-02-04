#ifndef CONF
#define CONF

#include <stdint.h>


#define MAP_BITS 16         
#define MAP_SIZE (1<<MAP_BITS)


#define MAX_BRANCHES_INSTRUMENTED ( 1<<24 )   


#define SHM_ENV_VAR_AFL_MAP               "__SHM_ID_AFL_MAP"
#define SHM_ENV_VAR_BRANCH_TRACE          "__SHM_ID_BRANCH_TRACE"
#define SHM_ENV_VAR_BRANCH_TAKEN          "__SHM_ID_BRANCH_TAKEN"
#define SHM_ENV_VAR_BRANCH_VALUE1         "__SHM_ID_BRANCH_VALUE1"
#define SHM_ENV_VAR_BRANCH_VALUE2         "__SHM_ID_BRANCH_VALUE2"
#define SHM_ENV_VAR_BRANCH_DOUBLE         "__SHM_ID_BRANCH_DOUBLE"
#define SHM_ENV_VAR_BRANCH_OPERTIONS      "__SHM_ID_BRANCH_OPERATIONS"
#define SHM_ENV_VAR_BRANCH_INDEX          "__SHM_ID_BRANCH_INDEX"
#define SHM_ENV_VAR_BRANCH_INSTRUMENT     "__SHM_ID_BRANCH_INSTRUMENT"

#define ENV_VAR_MY_SERVER_ID              "__MY_SERVER_ID"
#define ENV_VAR_BINARY_NAME               "__BINARY_COMPILE_NAME"
#define ENV_VAR_CALL_PATH                 "__RUN_PATH"

#define FORKSRV_FD          198



#define KNRM  "\x1B[0m"
#define KERR  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KINFO  "\x1B[33m"


#endif
