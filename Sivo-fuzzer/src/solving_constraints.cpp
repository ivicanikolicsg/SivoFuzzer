/*
Copyright (c) 2021, Ivica Nikolic <cube444@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "solving_constraints.h"
#include "branches.h"
#include "solutions.h"
#include "nodes.h"


// 
//
// Solving systems of intervals
//
//



// Detect branch condition, and according to 
// previously inferred constants and the condition, determine the interval 
void solve_one_constraint( branch_address_pair ba )
{
    if( branch_is_solved_constraint( ba )  || 
        branch_get_cannot_infer_constraints( ba ) ||
        !branch_has_constraints( ba )
    ) return;


    if( PRINT_DEBUG_SYSTEM )
        printf("\n------------------ SOLVING ONE CONSTRAINT %08x %4d\n", ba.first, ba.second );

    auto true_deps      = branch_get_true_dep( ba );
    auto constraints    = branch_get_constraints( ba );
    auto inverted       = branch_get_inverted( ba );


    if( PRINT_DEBUG_SYSTEM ){
        for( auto it=true_deps.begin(); it!=true_deps.end(); it++)
            printf("truedeps: %d %d\n", it->first, it->second);
        fflush(stdout);

        printf("constr: " );
        for( int i=0; i<constraints.size(); i++)
            printf("%d ", constraints[i] );
        printf("\n");
        fflush(stdout);
    }



    uint32_t cv = constraints[4];
        
    set<uint8_t> byte_values[2];
    vector <uint8_t> cv_bytes;
    if( PRINT_DEBUG_SYSTEM ) printf("target val %d   %x : ", cv, cv );
    for(auto it=true_deps.begin(); it!=true_deps.end(); it++){
        cv_bytes.push_back( (cv >> (8*(3- (it->second) ))) & 0xff );
        if( PRINT_DEBUG_SYSTEM )         printf("%d ", cv_bytes[ cv_bytes.size() - 1] );
    } 
    if( PRINT_DEBUG_SYSTEM ) printf("\n");
    if( PRINT_DEBUG_SYSTEM ) printf("Inverted: %d\n", inverted );

    string last_instr = "ukn";
    if( constraints[3] == 51 ) last_instr = "cmp";
    if( constraints[3] ==111 ) last_instr = "swi";

    string prev_instr = "ukn";

    auto prev_instr_num = constraints[1];
    switch( prev_instr_num ){
        case 32: prev_instr = "je"; break;
        case 33: prev_instr = "jne"; break;
        case 34: prev_instr = "ja"; break;
        case 35: prev_instr = "jae"; break;
        case 36: prev_instr = "jb"; break;
        case 37: prev_instr = "jbe"; break;
        case 38: prev_instr = "jg"; break;
        case 39: prev_instr = "jge"; break;
        case 40: prev_instr = "jb"; break;          // this should be singed "jl" !!!!!
        case 41: prev_instr = "jle"; break;

        case 111: prev_instr = "se"; break;         // for switch
    }

    if      ( prev_instr == "jna" ) prev_instr = "jbe";
    else if ( prev_instr == "ja" ) prev_instr = "jnbe";
    else if ( prev_instr == "jng" ) prev_instr = "jle";
    else if ( prev_instr == "jg" ) prev_instr = "jnle";
    else if ( prev_instr == "jnae" || prev_instr == "jc"   ) prev_instr = "jb";
    else if ( prev_instr == "jae" || prev_instr == "jnc"  ) prev_instr = "jnb";
    else if ( prev_instr == "je" ) prev_instr = "jz";
    else if ( prev_instr == "jne" ) prev_instr = "jnz";

    if( inverted ){
        if( PRINT_DEBUG_SYSTEM )         
            printf("Inverting: %s ", prev_instr.c_str() );
        if      ( prev_instr == "jbe" )   prev_instr = "jnbe";
        else if ( prev_instr == "jnbe" )  prev_instr = "jbe";
        
        else if( prev_instr == "jle" )    prev_instr = "jnle";
        else if( prev_instr == "jnle" )   prev_instr = "jle";

        else if( prev_instr == "js" )     prev_instr = "jns";
        else if( prev_instr == "jns" )    prev_instr = "js";

        else if( prev_instr == "jnb" )    prev_instr = "jb";
        else if( prev_instr == "jb" )     prev_instr = "jnb";

        else if( prev_instr == "jnz" )    prev_instr = "jnz";
        else if( prev_instr == "jz" )     prev_instr = "jz";

        else{
            if( PRINT_DEBUG_SYSTEM )             
                printf(KERR "Don't know how to handle %s \n" KNRM, prev_instr.c_str() );
            return;
            exit(0);
        }
    }
        
    branch_set_solved_constraint( ba, false );
    
    // currently IGNORING all signs
    // later improve this
    if( true_deps.size() >= 1 ){

        uint64_t value = 0;
        int max_seen = 0;
        for( auto it=true_deps.begin(); it!= true_deps.end(); it++){
            value ^= ( (cv>>(8*(3-it->second))) & 0xff ) << (8*(3-it->second));
            max_seen = max( max_seen, it->second );
        }
        value >>= 8*(3-max_seen);
        pair <uint64_t,uint64_t> interval;
        vector < pair<uint64_t,uint64_t> > vinterval;
        uint64_t MAX_INTERVAL_VALUE = ( (uint64_t)1 << (8 * true_deps.size()) ) -1 ;
       
        branch_set_solved_constraint( ba, true);

        interval.first = 1;
        interval.second = 0;

        if( last_instr == "cmp" && prev_instr == "jz" ){
            interval.first = value;
            interval.second = value;
            vinterval.push_back( interval );
            branch_set_constraint_interval( ba, vinterval);
        }

        // switch (this only)
        else if( last_instr == "swi" && prev_instr == "se" ){
            interval.first = value;
            interval.second = value;
            vinterval.push_back( interval );
            branch_set_constraint_interval( ba, vinterval);
        }


        else if( last_instr == "cmp" && prev_instr == "jnz" ){
            if( value > 0){
                interval.first = 0;
                interval.second = value-1;
                vinterval.push_back( interval );
            }
            if( value < MAX_INTERVAL_VALUE ){
                interval.first = value+1;
                interval.second = MAX_INTERVAL_VALUE;
                vinterval.push_back( interval );
            } 
            branch_set_constraint_interval( ba, vinterval);
        }
        else if( last_instr== "cmp" && prev_instr  == "jb" )
        {
            if( value > 0){
                interval.first = 0;
                interval.second = value - 1;
                vinterval.push_back( interval );
            }
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr == "cmp" && prev_instr == "jnb"  )
        {
            interval.first = value;
            interval.second = MAX_INTERVAL_VALUE ;
            vinterval.push_back( interval );
            branch_set_constraint_interval( ba, vinterval );
        }        
        else if( last_instr== "cmp" &&  prev_instr  == "jnbe"  )
        {
            if( value + 1 <= MAX_INTERVAL_VALUE ){
                interval.first = value + 1;
                interval.second = MAX_INTERVAL_VALUE ;
                vinterval.push_back( interval );
            }
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "jle"  )
        {
            interval.first = 0;
            interval.second = value;
            vinterval.push_back( interval );
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "jnle"  )
        {
            if( value + 1 <= MAX_INTERVAL_VALUE ){
                interval.first = value + 1;
                interval.second = MAX_INTERVAL_VALUE ;
                vinterval.push_back( interval );
            }
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "jnl"  )
        {
            interval.first = value ;
            interval.second = MAX_INTERVAL_VALUE ;
            vinterval.push_back( interval );
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "jl"  )
        {
            if( value > 0){
                interval.first = value -1 ;
                interval.second = MAX_INTERVAL_VALUE ;
                vinterval.push_back( interval );
            }
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "jbe"  )
        {
            interval.first = 0 ;
            interval.second = value ;
            vinterval.push_back( interval );
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "jnbe"  )
        {
            if( value < MAX_INTERVAL_VALUE){
                interval.first = value + 1 ;
                interval.second = MAX_INTERVAL_VALUE ;
                vinterval.push_back( interval );
            }
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "js"  )
        {
            if( value > 0){
                interval.first = 0 ;
                interval.second = value -1 ;
            }
            vinterval.push_back( interval );
            branch_set_constraint_interval( ba, vinterval );
        }
        else if( last_instr== "cmp" &&  prev_instr  == "jns"  )
        {
            interval.first = value ;
            interval.second = MAX_INTERVAL_VALUE;
        }

        else{
            if( PRINT_DEBUG_SYSTEM)
                printf(KERR "@@@@@@@@@@@ CANNOT SOLVE multi byte  %d %d   %s %s\n" KNRM, 
                    constraints[3], constraints[1],
                    last_instr.c_str(), prev_instr.c_str()
                     );
            branch_set_solved_constraint( ba, true);
        }

        if( PRINT_DEBUG_SYSTEM ){ 
            printf("Constraint value: %d %d  :  %lx %d  ::::  ", constraints[3], constraints[1], value, max_seen);
            for( int i=0; i<vinterval.size(); i++)
                printf(" %lx  %lx  : ", vinterval[i].first, vinterval[i].second );
            printf("\n");
        }

    }
    else{
        printf("@@@@@@@@@@@ MULTIPLE bytes %d %d\n", constraints[3], constraints[1] );
    }


    if( PRINT_DEBUG_SYSTEM ) 
        printf("\n------------------ \n" );


}


// solve all intervals of a system
bool sample_solution(   vector < pair<branch_address_pair,int> > path, byte *x, int lenx, 
        int &solved_constraints, set <int> touch_only_vars, 
        bool &used_random, bool &used_rand_interval, bool &has_random_bytes, 
        set <branch_address_pair> &branches_chosen_randomely,  
        branch_address_pair last_failed, 
        map <int,int> &one_solution
 )
{

    if( PRINT_DEBUG_SYSTEM )
        printf("Sample solution: path size %ld\n", path.size() );

    solved_constraints = 0;
    used_random = false;
    used_rand_interval = false;
    has_random_bytes = false;
    one_solution.clear();
    set < branch_address_pair > passed;
    map < uint32_t, set<uint8_t> > bytes_vals; 

    // Solve all unsolved constraints
    for( int i=0; i<path.size(); i++)
        if( !branch_is_solved_constraint( path[i].first ) && 
            branch_is_multi_byte_constraint( path[i].first ) )
            solve_one_constraint( path[i].first );


    // apply heuristic I
    // If same addresses but different counters have increasing cmp by one, then probably then can be ignored
    // Ignore only with some probability (so, sometimes ignore, other times do not)
    //
    //
    // Make sure later to check that if linear branches exist then 
    // the unsolved system (whitout taking into accoutn linear branches) 
    // is indeed unsolved for all 
    //
    //
    set <uint32_t> linear_branches;
    bool check_linear_branches = mtr.randInt() % 2;
    if (check_linear_branches){
        map < uint32_t, vector<uint64_t> > counters;
        for( int ind=0; ind<path.size(); ind++){

            if( branch_is_solved_constraint( path[ind].first )  && branch_is_constraint_multibyte_dependent( path[ind].first ) ){
                auto con = branch_get_constraints( path[ind].first  );
                if( con.size() < 5 ) continue;

                if( counters.find(path[ind].first.first) == counters.end() ) 
                    counters.insert( {path[ind].first.first, vector<uint64_t>() } );
                auto it=counters.find( path[ind].first.first );
                it->second.push_back( con[4] );
            }
        }
        for( auto it=counters.begin(); it!=counters.end(); it++){
            if( it->second.size() <= 2) continue;
            uint64_t ju = it->second[1] - it->second[0];
            bool is_linear = true;
            for( int i=1; i<it->second.size()-1; i++)
                if( 0 == ju || it->second[i] + ju != it->second[i+1]  ){
                    is_linear = false;
                    break;
                }
            if( is_linear ){
                linear_branches.insert( it->first );
                if( PRINT_DEBUG_SYSTEM )
                    printf("Found linear branch: %08x\n", it->first );
            }
        }
    }
        

    //
    // Pass  multibyte dependent, aka intervals 
    //


    // First pass for intervals
    // Join all systems with overlapping variables
    vector < set< vector<int> > > al;
    //vector < map< int , int > > al_var_to_adep;
    map < vector<int>, map< int , int > > dep_to_adep;
    map < uint32_t, int > var_to_al_index; 
    for( int ind=0; ind<path.size(); ind++){

        if( linear_branches.find( path[ind].first.first) != linear_branches.end() ) continue;
        if( !branch_is_solved_constraint( path[ind].first ) || 
            !branch_is_constraint_multibyte_dependent( path[ind].first ) ) continue ;

        branch_address_pair ba = path[ind].first;
        auto true_deps_full = branch_get_true_dep( ba );
        vector <int> true_deps;
        for( auto it=true_deps_full.begin(); it!= true_deps_full.end(); it++)
            true_deps.push_back( it->first );

        // check if any var is already indexing any of the sets of vectors
        vector <int> found_al_indices;
        for (auto i = 0; i < true_deps.size(); i++){
            auto v = true_deps[i];
            if( var_to_al_index.find(v) != var_to_al_index.end() ){
                found_al_indices.push_back( var_to_al_index.find( v )->second );
            }
        }

        if( found_al_indices.size() == 0 ){
            al.push_back( set<vector<int>>() );
            found_al_indices.push_back( al.size() -1  );
        }

        // join sets if there are more
        set < vector<int> >  uni;
        map <int,int> uni_adeps;
        while( found_al_indices.size() > 1 ){
            for( auto zz= al[ found_al_indices[1] ].begin(); zz != al[ found_al_indices[1] ].end(); zz++ ){
                uni.insert( *zz );
            }
            found_al_indices.erase( found_al_indices.begin() + 1 );
        }
        // insert all to the first 
        for( auto zz= uni.begin(); zz!= uni.end(); zz++)
            al[ found_al_indices[0] ].insert( *zz );

        // add the new vector of dep vars
        al[ found_al_indices[0] ].insert( true_deps  );

        auto zz = dep_to_adep.find( true_deps );
        if( zz != dep_to_adep.end() && zz->second != true_deps_full )
        {
            if( PRINT_DEBUG_SYSTEM ){
                printf( KERR "Found different adeps : " );
                for( int i=0; i<true_deps.size(); i++) printf("%d ", true_deps[i] );
                printf("  : " );
                for( auto it = zz->second.begin(); it != zz->second.end(); it++) printf("%d ", it->second );
                printf("  : " );
                for( auto it = true_deps_full.begin(); it != true_deps_full.end(); it++) printf("%d ", it->second );
                printf("\n" KNRM );
            }
            return false;
        }
        dep_to_adep.insert( {true_deps, true_deps_full } );

        // remap all vars
        for (auto i = 0; i < true_deps.size(); i++){
            auto v = true_deps[i];
            if( var_to_al_index.find(v) == var_to_al_index.end() )
                var_to_al_index.insert( {v,0});
            var_to_al_index.find( v )->second = found_al_indices[0];
        }
    }

   

    // Get dep_vars to al_index
    map < vector<int> , int > dep_var_to_al_index;
    set <int> true_al_indices;
    for( auto it=var_to_al_index.begin(); it!= var_to_al_index.end(); it++){
        int al_index = it->second;
        if( true_al_indices.find( al_index ) == true_al_indices.end() )
            true_al_indices.insert( al_index );
        for( auto zz=al[al_index].begin(); zz!=al[al_index].end(); zz++)
            if( dep_var_to_al_index.find( *zz) == dep_var_to_al_index.end() )
                dep_var_to_al_index.insert( {*zz, al_index } );   
    }

    // Determine min,max values 
    map < int , int > min_al_index;
    map < int , int > max_al_index;
    for( auto it=true_al_indices.begin(); it!= true_al_indices.end(); it++){

        int al_index = *it;
        
        if( PRINT_DEBUG_SYSTEM ){

            printf("true set : ");
            for( auto zz=al[al_index].begin(); zz!=al[al_index].end(); zz++){ 
                for( int i=0; i<(*zz).size(); i++)
                    printf("%d ", (*zz)[i] );
                printf("  #  ");
            }
            printf("\n");
        }

        int min_index = -1, max_index = -1;
        for( auto zz=al[al_index].begin(); zz!=al[al_index].end(); zz++){ 
            for( int i=0; i<(*zz).size(); i++){
                if( min_index <0 || (*zz)[i] < min_index ) min_index = (*zz)[i];
                if( max_index <0 || (*zz)[i] > max_index ) max_index = (*zz)[i];
            }
        }
        if( max_index - min_index >= 4){
            if( PRINT_DEBUG_SYSTEM )
                printf( KERR "For now cannot solve overlap interval with more than 32-bit words\n" KNRM );
            return false;
        }

        min_al_index.insert( {al_index, min_index});
        max_al_index.insert( {al_index, max_index});
    }

    // Go over all branch addresses, and fill the system
    map < int, vector<pair<uint64_t,uint64_t>> > system;
    map < int , set<branch_address_pair> > al_index_to_branch_addresses;
    for( int ind=0; ind<path.size(); ind++){

        if( !branch_is_solved_constraint( path[ind].first ) || 
            !branch_is_constraint_multibyte_dependent( path[ind].first ) ) continue ;

        passed.insert(path[ind].first  );

        if( linear_branches.find( path[ind].first.first) != linear_branches.end() ) continue;

        branch_address_pair ba = path[ind].first;
        auto true_deps_full = branch_get_true_dep( ba );
        vector <int> true_deps;
        for( auto it=true_deps_full.begin(); it!= true_deps_full.end(); it++)
            true_deps.push_back( it->first );
        int nobytes = true_deps.size();
        uint64_t MAX_INTERVAL_VALUE = ( (uint64_t)1 << (8 * nobytes) ) -1 ;
        
        vector <pair<uint64_t,uint64_t>> to_insert_intervals;
        pair<uint64_t,uint64_t>  interval;

        // find al_index
        int al_index;
        if( dep_var_to_al_index.find(true_deps) == dep_var_to_al_index.end() ){
            if( PRINT_FUZZER_BASIC_INFOS)   
                printf( KERR "Cannot find al_index \n" KNRM );
            return false;
        }
        al_index = dep_var_to_al_index.find(true_deps)->second;

        if( al_index_to_branch_addresses.find( al_index ) == al_index_to_branch_addresses.end() )
            al_index_to_branch_addresses.insert( {al_index, set<branch_address_pair>() });
        al_index_to_branch_addresses.find( al_index )->second.insert( ba );


        to_insert_intervals = branch_get_constraint_interval( ba );

        if( PRINT_DEBUG_SYSTEM ){

            printf("---Interval for %d   %08x %4d : %ld  : %lx\n", ind,  
                ba.first, ba.second, to_insert_intervals.size(), MAX_INTERVAL_VALUE );
            for(int i=0; i<to_insert_intervals.size(); i++) 
                printf(" %lx - %lx : \n", to_insert_intervals[i].first, to_insert_intervals[i].second );
            printf("\n");
            fflush(stdout);
        }


        // check if you need to invert them

        // inverting switch
        if( 0 == path[ind].second &&  2 == nodes_get_type_of_branch( path[ind].first.first ) )
        {
            uint32_t v = nodes_choose_random_unsolved_switch( path[ind].first.first  );
            to_insert_intervals.clear();
            to_insert_intervals.push_back( make_pair(v+1, v+1) );
        }

        // ignore switch conditions that are not inverted
        if( 1 == path[ind].second &&  2 == nodes_get_type_of_branch( path[ind].first.first ) )
        {
            to_insert_intervals.clear();
            to_insert_intervals.push_back( make_pair(0, MAX_INTERVAL_VALUE) );

        }

        // check conditional if
        if( 0 == path[ind].second && 1 == nodes_get_type_of_branch( path[ind].first.first ) ){

            uint64_t v;
            // it means it was jnz
            //this is very risky and not universal (by assuming it is jnz)
            if( to_insert_intervals.size() == 2 ){ 
                v = to_insert_intervals[0].second ;
                to_insert_intervals.clear();
                if( v < MAX_INTERVAL_VALUE )
                    to_insert_intervals.push_back( make_pair(v+1, v+1) );
            }
            else if( to_insert_intervals.size() == 1 ){
                // it means it was empty set before, 
                if(  to_insert_intervals[0].first > to_insert_intervals[0].second ){
                    to_insert_intervals.clear();
                    to_insert_intervals.push_back( make_pair(0, MAX_INTERVAL_VALUE) );
                }
                // this is jz
                else if( to_insert_intervals[0].first == to_insert_intervals[0].second ){
                    v = to_insert_intervals[0].first;
                    to_insert_intervals.clear();
                    if( v > 0)
                        to_insert_intervals.push_back( make_pair(0, v-1) );
                    if( v < MAX_INTERVAL_VALUE )
                        to_insert_intervals.push_back( make_pair(v+1, MAX_INTERVAL_VALUE) );
                }
                else{
                    // also risky, it assumes one of the ends of hte interval is 0 or MAX_VAL , or [t,t] interval
                    if( to_insert_intervals[0].first == 0){
                        v = to_insert_intervals[0].second;
                        to_insert_intervals.clear();
                        if( v < MAX_INTERVAL_VALUE ) 
                            to_insert_intervals.push_back( make_pair(v+1, MAX_INTERVAL_VALUE) );
                    }
                    else{
                        v = to_insert_intervals[0].first;
                        to_insert_intervals.clear();
                        if( v > 0 ) 
                            to_insert_intervals.push_back( make_pair( 0, v-1 ) );
                    }
                }
            }
            else{
                if( PRINT_DEBUG_SYSTEM ) printf(" Empty interval \n");
                return false;
            }

            if( PRINT_DEBUG_SYSTEM ) {

                printf("---Inverted \n" );
                for(int i=0; i<to_insert_intervals.size(); i++) 
                    printf(" %lx - %lx \n",to_insert_intervals[i].first, to_insert_intervals[i].second );
                printf("\n");
            }


        }

        // make sure that all the intervals are correct, i.e. left <= right
        int found_bad_interval = -1;
        for( int i=0; i < to_insert_intervals.size(); i++)    
            if( to_insert_intervals[i].first > to_insert_intervals[i].first )
                found_bad_interval = i;
        if( to_insert_intervals.size() == 0 || found_bad_interval >=0 ){
            if( PRINT_DEBUG_SYSTEM ) {
                printf("Found bad interval: %ld : ", to_insert_intervals.size());
                if( found_bad_interval >= 0 ) 
                    printf(" %lx %lx", to_insert_intervals[found_bad_interval].first, to_insert_intervals[found_bad_interval].second );
                printf("\n");
            }
            return false;
        }

        // correct intervals
        int shift_left_bytes = 3;
        for( auto zz=true_deps_full.begin(); zz != true_deps_full.end(); zz++)
            shift_left_bytes = min(shift_left_bytes, 3 - zz->second);

        if( shift_left_bytes > 0 ){
            if( PRINT_DEBUG_SYSTEM ) printf("shift left: %d\n", shift_left_bytes);
            for( int i=0; i<to_insert_intervals.size(); i++){
                to_insert_intervals[i].first <<= 8*shift_left_bytes;
                to_insert_intervals[i].second <<= 8*shift_left_bytes;
                to_insert_intervals[i].second ^=  (1<<(8*shift_left_bytes)) - 1;
            }
        }

        // add to the system
        auto it = system.find( al_index );
        if( it == system.end() ){
            system.insert( {al_index, vector<pair<uint64_t,uint64_t>>()});
            it = system.find( al_index );
        }

        if( it->second.size() == 0)
            for( int j=0; j< to_insert_intervals.size(); j++)
                it->second.push_back( to_insert_intervals[j] );
        else{

            if( PRINT_DEBUG_SYSTEM ){ 
                printf("Current interval system\n");
                for( int i=0; i<it->second.size(); i++)
                    printf("%8lx %8lx\n", it->second[i].first, it->second[i].second );
            }

            int ind_st[2] = {-1,-1};
            int ind_en[2] = {-1,-1};
            for( int j=0; j< to_insert_intervals.size(); j++){

                if( PRINT_DEBUG_SYSTEM )
                    printf("Inserting: %8lx %8lx\n", to_insert_intervals[j].first, to_insert_intervals[j].second );

                for( int i=0; i<it->second.size(); i++){
                    if( ind_st[j] < 0 && (
                        it->second[i].first <= to_insert_intervals[j].first && to_insert_intervals[j].first <= it->second[i].second ||
                        to_insert_intervals[j].first <= it->second[i].first &&  it->second[i].first <+ to_insert_intervals[j].second  
                        ) ) ind_st[j] = i;
                    if( /*ind_en[j] < 0 && */ (
                        it->second[i].first <= to_insert_intervals[j].first && to_insert_intervals[j].first <= it->second[i].second ||
                        to_insert_intervals[j].first <= it->second[i].first &&  it->second[i].first <+ to_insert_intervals[j].second  
                        ) ) ind_en[j] = i;
                }
                if( ind_st[j] < 0 && to_insert_intervals[j].first <= it->second[0].second ) ind_st[j] = 0;
                if( ind_en[j] < 0 && to_insert_intervals[j].second >= it->second[it->second.size()-1].first ) ind_en[j] = it->second.size()-1;

                if( ind_st[j] < 0 || ind_en[j] < 0){
                    if( PRINT_DEBUG_SYSTEM )
                        printf("Cannot insert %d %d \n", ind_st[j], ind_en[j] );
                }
                else{
                    if( PRINT_DEBUG_SYSTEM )
                        printf("Found range: %d %d   \n", ind_st[j], ind_en[j] );
                }
            }

            vector<pair<uint64_t,uint64_t>> new_intervals;
            for( int j=0; j< to_insert_intervals.size(); j++){
                
                if( ind_st[j] < 0 || ind_en[j] < 0) continue;

                for(int i=ind_st[j]; i<= ind_en[j]; i++ ){
                    uint64_t left = max(to_insert_intervals[j].first, it->second[i].first);
                    uint64_t right= min(to_insert_intervals[j].second, it->second[i].second);
                    if( left <= right )
                        new_intervals.push_back( make_pair( left, right ) );
                }
            }
            it->second = new_intervals;
            if( it->second.size() == 0 ){
                if( PRINT_DEBUG_SYSTEM )
                    printf("empty interval set\n");
                return false;
            }
        }
        if( PRINT_DEBUG_SYSTEM ){ 
            printf("New interval system\n");
            for( int i=0; i<it->second.size(); i++)
                printf("%8lx %8lx\n", it->second[i].first, it->second[i].second );   
        }
    }


    // Sample all systems
    for( auto it = system.begin(); it!= system.end(); it++){

        if( PRINT_DEBUG_SYSTEM ){
            printf("Sampling system\n");
            for( int i=0; i<it->second.size(); i++)
                printf("%8lx %8lx\n", it->second[i].first, it->second[i].second ); 
            fflush(stdout);  
        }


        uint64_t tot_interval_length = 0;
        for( int j=0; j<it->second.size(); j++){
            if( it->second[j].second < it->second[j].first ){
                if( PRINT_DEBUG_SYSTEM )
                    printf( "Second less than first: %lx %lx\n", it->second[j].first, it->second[j].second);
                return false;
            }
            tot_interval_length += it->second[j].second - it->second[j].first + 1;
        }
        uint64_t choose_interval_id = mtr.randInt() % tot_interval_length;

        if( tot_interval_length > 1 || !check_linear_branches )
            used_rand_interval   = true;

        int interval_index = 0;
        uint64_t accum_lenght = 0;
        while( interval_index < it->second.size() && 
                accum_lenght + it->second[interval_index].second - it->second[interval_index].first + 1 <= choose_interval_id ){
                    accum_lenght += it->second[interval_index].second - it->second[interval_index].first + 1;
                    interval_index ++;
                }
        if( interval_index >= it->second.size() ){
            if( PRINT_FUZZER_BASIC_INFOS)
                printf( KERR "Mistake in the code in selecting intervals" KNRM );
            exit(1);
        }
        uint64_t val = it->second[interval_index].first + choose_interval_id - accum_lenght;
        if( PRINT_DEBUG_SYSTEM ){ 
            printf("sampled: diff %ld : totlen  %ld  : %lx %lx : %ld   %lx\n", 
                    it->second.size(), tot_interval_length, 
                    it->second[interval_index].first, it->second[interval_index].second, 
                    choose_interval_id, val);
        }

        // add the solution to all dependent branches
        int al_index = it->first;
        auto zz = al_index_to_branch_addresses.find( al_index );
        if( zz == al_index_to_branch_addresses.end() )
        {
            printf( KERR "branch addresses cannot be found for this al_index: %d\n" KNRM, al_index);
            exit(3);
        }
        

        int min_dep_var=0, max_dep_var=100000000;
        if( min_al_index.find( al_index ) != min_al_index.end()  ) 
            min_dep_var = min_al_index.find( al_index )->second;
        if( max_al_index.find( al_index) != max_al_index.end()  ) 
            max_dep_var = max_al_index.find( al_index )->second;

        if( PRINT_DEBUG_SYSTEM )
            printf("min max found: %d %d \n", min_dep_var, max_dep_var );
        set <int > assigned_byte_positions;
        for( auto tt = zz->second.begin(); tt != zz->second.end(); tt++ ){

            if( assigned_byte_positions.size() >= max_dep_var - min_dep_var + 1 ) break;


            branch_address_pair ba = *tt;
            auto true_deps_full = branch_get_true_dep( ba );
            for( auto zz=true_deps_full.begin(); zz!=true_deps_full.end(); zz++ ){
                int byte_position = zz->first;
                uint8_t byte_val = (val >> (8*( 3 - zz->second)) ) & 0xff;
                if( PRINT_DEBUG_SYSTEM )  {
                    printf("INTER: %d   %d %x\n", zz->second, byte_position, byte_val );
                    fflush(stdout);
                }
                assigned_byte_positions.insert( byte_position );

                if( bytes_vals.find(byte_position) == bytes_vals.end() )
                {
                    set <uint8_t> s;
                    s.insert( byte_val );
                    bytes_vals.insert( { byte_position, s });
                }
            }
        }
    }


    if( PRINT_DEBUG_SYSTEM )
        printf("PASSED1 %ld out of %ld  \n", passed.size(), path.size() );

    // Pass random single bytes
    for( int i=path.size()-1; i>=0; i--){

        branch_address_pair ba = path[i].first;

        if( branch_is_multi_byte_constraint( ba ) ) continue;

        auto dependency = get_dependency( ba );
        if( dependency.size() != 1 ) continue;

        int byte_position = dependency [ 0 ];

        if( PRINT_DEBUG_SYSTEM )
            printf("---Rand bytes for %d   %08x %4d :  byte  %d\n", i, ba.first, ba.second, byte_position );
        has_random_bytes = true;


        // use actual rand byte values only if it is: 1) last branch, or 2) failed branch
        // otherwise, you may be unintentially inverting other branches.
        // This is NOT fullproof strategy !!!
        if( i + 1 != path.size() && ba != last_failed){
            if(  bytes_vals.find(byte_position) == bytes_vals.end() ){
                set <uint8_t> s;
                if( byte_position < lenx ) s.insert( x[byte_position] );
                bytes_vals.insert( {byte_position, s} );
            }
            continue;
        }

        // if some other branches depend on this byte position then do nothing
        if(  bytes_vals.find(byte_position) != bytes_vals.end() ) continue;

        auto current_byte_values = branch_get_rand_byte_values( ba, path[i].second );
        bytes_vals.insert( {byte_position, current_byte_values});
        used_random = true;
        branches_chosen_randomely.insert( ba );
        passed.insert( ba );


        if( PRINT_DEBUG_SYSTEM )
            printf("Used RANDOM : %d  : size %ld \n", byte_position, current_byte_values.size() );

    }


    if( PRINT_DEBUG_SYSTEM )
        printf("PASSED2 %ld out of %ld  \n", passed.size(), path.size() );



    // For the remaining multibytes, add all possible byte values
    // to bytes that have not been initialized previously
    for( int i=0; i<path.size(); i++){
        branch_address_pair ba = path[i].first;
        if( passed.find( ba) != passed.end() ) continue;
        auto dependency = get_dependency( ba );
        
        for( int k=0; k<dependency.size(); k++){

            if( i+1 < path.size() && touch_only_vars.find(dependency[k]) == touch_only_vars.end() ) continue;

            has_random_bytes = true;
    
            auto it = bytes_vals.find( dependency[k] ); 
            if( it== bytes_vals.end() ){
                set <uint8_t> allvals;
                for( int j=0; j<256; j++)
                    allvals.insert( j );
                bytes_vals.insert( {dependency[k], allvals } );
                if( PRINT_DEBUG_SYSTEM )  
                    printf("---Rand multi bytes for %d  %08x %4d : byte  %d\n", i, ba.first, ba.second,  dependency[k] );
                used_random = true;
            }
        }
    }


    // for each byte choose one potential value
    for( auto it=bytes_vals.begin(); it!= bytes_vals.end(); it++){
        int byte_pos = it->first;
        if( it->second.size() == 0 ){
            if( PRINT_DEBUG_SYSTEM )
                printf("empty values for byte  %d\n", byte_pos);
            return false;
        }
        int byte_step = mtr.randInt() % it->second.size();
        auto iz = it->second.begin();
        for(; ; iz++)
            if( --byte_step < 0 ) break;
        int byte_val = *iz;
        if( byte_pos < lenx ){
            x[ byte_pos ] = byte_val;
            if( PRINT_DEBUG_SYSTEM )  
                printf("CHOSEN BYTE %d %d\n", byte_pos, byte_val);
            one_solution.insert( {byte_pos, byte_val });
        }
    }

    return true;
}




void add_wrong_byte_guess( branch_address_pair ba, int truth_value, byte *x, int lenx   )
{
    if( PRINT_DEBUG_SYSTEM ){
        printf("WRONG: %08x %4d  :  %d %ld\n",   ba.first,  ba.second, 
            branch_is_solved_constraint(ba), get_dependency(ba).size());
    }

    if( branch_is_solved_constraint(ba) || get_dependency(ba).size() != 1 ) return;



    int chosen_byte_position = get_dependency( ba )[0];
    if( chosen_byte_position >= lenx ) return;

    uint8_t chosen_byte_value = x[ chosen_byte_position ];
    set<uint8_t> byte_values = branch_get_rand_byte_values( ba , truth_value );


    if( byte_values.find( chosen_byte_value ) == byte_values.end ())
    {
        if( PRINT_DEBUG_SYSTEM )
            printf( KERR "Value (%d %d ) cannot be found in the set\n" KNRM, chosen_byte_position, chosen_byte_value );
        return ;
    }
    if( PRINT_DEBUG_SYSTEM )
        printf("Erasing the value %d\n", chosen_byte_value );
    byte_values.erase( chosen_byte_value );

    branch_set_rand_byte_values( ba, truth_value, byte_values  );
}

