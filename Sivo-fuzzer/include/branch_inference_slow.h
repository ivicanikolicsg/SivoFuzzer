#include "branch_inference.h"


int branch_inference_target_bytes( 
    string original_input, bool forced_inference, 
    vector<int>& byte_positions, float time_max_slow_inference,
    set<int> &set_bytes_passed, set<int> &set_bytes_significant );

int  infer_depedency_of_randomly_chosen_chunk_of_bytes(
    string original_input, int original_input_size, bool forced_inference, 
    int CHUNK_SIZE, float time_max_slow_inference );

int branch_inference_try_all_bytes( 
    string original_input,  int original_input_size, 
    float time_max_slow_inference,
    set <int> &set_passed, set<int> &set_important );

