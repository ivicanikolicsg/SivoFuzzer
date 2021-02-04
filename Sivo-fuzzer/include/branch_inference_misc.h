#include "branch_inference.h"


void init_differences();
void setup_differences( int lenx );
void free_differences();

bool branch_inference_detect_important_bytes( string original_input, vector <int> &ib, 
        float rtime_max_slow_inference, uint32_t *origin_values, float execs_per_sec );
bool branch_inference_detect_unmutable_bytes( 
        string original_input, vector <int> &ib, 
        float rtime_max_slow_inference,
        float percentage, bool forced_inference );

uint64_t count_br_with_dep( vector  < branch_address_pair > branches_with_dependency );
uint64_t count_deps( vector  < branch_address_pair > branches_with_dependency );
uint64_t count_deps_vars( vector  < branch_address_pair > branches_with_dependency );
