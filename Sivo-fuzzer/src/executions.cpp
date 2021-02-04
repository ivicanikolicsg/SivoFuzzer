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


#include "executions.h"
#include "fork_servers.h"
#include "nodes.h"
#include "branches.h"


//
//
// Execution function related to the coverage information that is obtained
// by running the programs
//

// look-up table for the logarithmic counts
uint8_t ectb[256];          

// found completely new edge (rather than only new logarithmic count)
int glob_found_different;   

// arrays holding the hashed edge logarithmic counts 
uint8_t showmap[1<<16];
uint8_t showmap_timeout[1<<16];
uint8_t minimized_showmap[1<<16];


// logarithmic counts produced from the actual edge counts, 
// according to how AFL does it 
void create_lookup_for_compressed_bit()
{
    // look-up table for the logarithic count 
    for( int i=0; i<256; i++){
        int bit = 0;
        if     ( i == 0  )   bit = 0;
        else if( i <= 1  )   bit = 1;
        else if( i <= 2  )   bit = 2;
        else if( i <= 3  )   bit = 4;
        else if( i <= 7  )   bit = 8;
        else if( i <= 15 )   bit = 16;
        else if( i <= 31  )  bit = 32;
        else if( i <= 127 )  bit = 64;
        else                 bit = 128;

        ectb[i] = bit;
    }
}

// initialize coverage and memory for traces
void init_coverage()
{
    memset( showmap,            0, 1<<16 );
    memset( minimized_showmap,  0, 1<<16 );
    memset( showmap_timeout,    0, 1<<16 );

    store_branches_trace = (uint32_t *)malloc( MAX_BRANCHES_INSTRUMENTED * sizeof(uint32_t) )  ;
    store_branches_taken = (uint32_t *)malloc( MAX_BRANCHES_INSTRUMENTED * sizeof(uint32_t) )  ;
    store_branches_opera = (uint32_t *)malloc( MAX_BRANCHES_INSTRUMENTED * sizeof(uint32_t) )  ;

}

// free memory from traces
void fini_coverage()
{

    free( store_branches_trace)  ;
    free( store_branches_taken) ;
    free( store_branches_opera ) ;

}

// store the current trace
void store_branch_trace()
{
    store_branches_tot=0;
    while( store_branches_tot < __new_branch_index[0] ){
        store_branches_trace[store_branches_tot] = branches_trace[store_branches_tot];
        store_branches_taken[store_branches_tot] = branches_taken[store_branches_tot];
        store_branches_opera[store_branches_tot] = branches_operations[store_branches_tot];
        store_branches_tot++;
    }
}



void load_from_stored_branch_trace()
{

    for( int i=0; i<store_branches_tot; i++)
    {
        branches_trace[i] = store_branches_trace[i];
        branches_taken[i] = store_branches_taken[i];
    }

}

// get the trace index at which this execution (new seed) differs from the original execution (initial seed) 
int execution_get_divergent_index( )
{

    int i=0;
    while( i < __new_branch_index[0] && i < store_branches_tot){

        // if branch operation && different value of the branch condition
        if( BRANCH_IS_IMPORTANT(store_branches_opera[i])  && 
            branches_taken[i] != store_branches_taken[i] ){
            break;
        }

        // if different branch (or any operation) ID, then probabivly previous one
        if( branches_trace[i] != store_branches_trace[i] && i > 0){
            i --;
            break;
        }
        
        i++;
    }

    return i;
}



// update the coverage information (the array showmap)
bool execution_edge_coverage_update()
{
    START_TIMER(begin_time)

    hash_showmap = 0;
    for( int i=0; i< (1<<16)/ 8; i++)
        hash_showmap ^= *(uint64_t *)(aflshowmap + 8*i);
    
    coverage_update_total++;    
    coverage_update_total_bb += __new_branch_index[0]  > 0;
    trace_lengths += __new_branch_index[0];

    int found_new_cov = 0;
    for( int i=0; i< 1<<16; i++){
        
        if( (showmap[i] | ectb[ aflshowmap[i] ] ) != showmap[i] ){

            if( !showmap[i] )
                found_new_cov = 2;
            if ( !found_new_cov ) 
                found_new_cov = 1;
            showmap[i] |= ectb[ aflshowmap[i] ];
        }
    }


    if(ONLY_EDGE_COVERAGE)
        found_new_cov = 2 == found_new_cov ? 2 : 0;

    glob_found_different = found_new_cov;


    return found_new_cov;
}



// get the number of different compressed edges 
int get_coverage_entries()
{
    int s=0;
    for( int i=0; i< 1<<16; i++)
        s += !!(showmap[i] > 0); 
    return s;
}

// #edges + their logarithmic multiplicies (basically AFL objective function)
int get_coverage_entries_all()
{
    int s=0;
    for( int i=0; i< 1<<16; i++)
        if( showmap[i] ){
            for( int j=0; j<8; j++)
                s += (showmap[i]>>j) & 1; 
        }
    return s;
}

// coverage info but separate for each log multiplicity
void get_coverage_entries_separate(int s[8])
{
    for( int j=0; j<8; j++) s[j] = 0;
    for( int i=0; i< 1<<16; i++)
        if( showmap[i] ){
            for( int j=0; j<8; j++)
                s[j] += (showmap[i]>>j) & 1; 
        }
    
}

float get_coverage_byte_perct()
{
    int t = 0;
    int s=0;
    for( int i=0; i< 1<<16; i++)
        if( showmap[i] ){
            t++;
            for( int j=0; j<8; j++)
                s += (showmap[i]>>j) & 1; 
        }
    
    return float(s)/t;;

}

// store AFL-type of coverage in file 
void execution_afl_coverages_produce_and_save_to_file(string file_afl)
{
    // AFL showmap
    FILE *binFile = fopen(file_afl.c_str(),"wb");
    byte *comp_afl = (byte *)malloc ( 1 << 16 );
    for( int i=0; i< 1<<16; i++)
        comp_afl[i] = ectb[ aflshowmap[i] ];
    fwrite(comp_afl,sizeof(unsigned char),1<<16,binFile);
    fclose(binFile);
    free( comp_afl );

}

// store all coverage types (AFL + real traces + nodes + edges ) in files
void execution_all_coverages_produce_and_save_to_file( string filename, string file_nodecove, string file_afl,
    uint32_t &no_ifs_switches, uint32_t &trace_length )
{

    run_one_fork( RUN_GETTER );
    trace_length = __new_branch_index[0];

    if( PRINT_TESTCASES_INFO)
        printf("trace length: %d\n", __new_branch_index[0]);


    // AFL showmap
    execution_afl_coverages_produce_and_save_to_file(file_afl);   


    // Needed for edge and node coverages
    map < pair<int, int>,int> counters; 
    map < uint32_t , set<uint32_t> > node_to_num_of_nodes;
    int i = 0;
    no_ifs_switches = 0;
    while( i<__new_branch_index[0] ){

        if( i > 0){
            pair<int, int> p = make_pair( branches_trace[i-1], branches_trace[i] );

            auto zz = counters.find(p); 
            if( zz == counters.end() ) counters.insert( { p, 1} );
            else
                zz->second ++;

            // only branches and switches
            if(  UNPACK_BRANCH_TYPE(branches_operations[i-1]) > 0){
                no_ifs_switches ++;

                if( node_to_num_of_nodes.find(branches_trace[i-1]) == node_to_num_of_nodes.end() )
                    node_to_num_of_nodes.insert( { branches_trace[i-1] , set<uint32_t>() });
                auto it = node_to_num_of_nodes.find(branches_trace[i-1]); 
                it->second.insert( branches_trace[i] );
            }

        }
        i++;
    }    

    ofstream myfile;

    // Edge coverage
    myfile.open( filename );
    if (!myfile.is_open()){
        printf( KERR "Cannot open edge coverage save file:%s\n" KNRM, filename.c_str());
        return;
    }
    for( auto zz=counters.begin(); zz!= counters.end(); zz++){

        auto p = zz->first;         // edge
        auto count = zz->second;    // count
        int compressed_bit = count >=256 ? 128 : ectb[count];
        
        myfile << hex << p.first << " " << p.second <<" " << compressed_bit << endl;
    }
    myfile.close();

    // node cover
    myfile.open( file_nodecove );
    if (!myfile.is_open()){
        printf( KERR "Cannot open node coverage save file:%s\n" KNRM, file_nodecove.c_str());
        return;
    }
    for( auto zz=node_to_num_of_nodes.begin(); zz!= node_to_num_of_nodes.end(); zz++){
        myfile << hex <<  zz->first << " " << zz->second.size() << endl;
    }
    myfile.close();

}


// get edge coverage from file  
void get_edge_coverage_from_file( string filename, vector < vector<int>> &coverage )
{
    ifstream myfile;
    myfile.open( filename );
    int edge1,edge2,compressed_count;
    while( myfile >> hex >> edge1 >> edge2 >> compressed_count ){
        vector <int> o;
        o.push_back( edge1 );
        o.push_back( edge2 );
        o.push_back( compressed_count );
        coverage.push_back( o );
    }
    myfile.close();

}

// get node coverage from file 
void get_node_coverage_from_file( string filename, vector < vector<int>> &coverage )
{
    ifstream myfile;
    myfile.open( filename );
    uint32_t node, num ;
    while( myfile >> hex >> node >> num ){
        vector <int> o;
        o.push_back( node );
        o.push_back( num );
        coverage.push_back( o );
    }
    myfile.close();
}


// Get all branches from the trace. 
// The trace is produced when the program is executed. 
// Obviously, this is possible by previously instrumenting the program, 
// by compiling with our special compiler which adds the instrumentation code. 
vector < vector<uint32_t>  >  execution_get_branches( )
{
    vector < vector<uint32_t>  >  v; 
    
    run_one_fork( RUN_GETTER );

    map <int,int> counters;
    for( int i=0; i < __new_branch_index[0]; i++ )
    {
        // 1 is if , 2 is switch
        if( UNPACK_BRANCH_TYPE(branches_operations[i]) )
        {

            auto it = counters.find( branches_trace[i] );
            int count = 0;
            if( it == counters.end() )
                counters.insert( {branches_trace[i], 0});
            else
                it->second ++;

            if( v.size() > MAX_START_ANALYZE_BRANCHES && !jumping_jindices( it->second) ) 
                continue;



            it = counters.find( branches_trace[i] );    
            vector <uint32_t> e;
            e.push_back( branches_trace[i] );           // address.first            INT 
            e.push_back( it->second );                  // address.second           INT
            e.push_back( !!branches_taken[i] );         // DO NOT ERASE THIS (if so, check branch_type )
            e.push_back( UNPACK_BRANCH_TYPE(branches_operations[i]) );      // no! branchyep !taken or not (or switch number)
            e.push_back( i );                           // trace Id
            e.push_back( UNPACK_SWITCH_NUM_SUC(branches_operations[i]) );      // number of successors of switch 
            e.push_back( UNPACK_SWITCH_NUM_CAS(branches_operations[i]) );      // number of cases of switch 
            e.push_back( branches_taken[i] );
            v.push_back( e ) ;

        }
    }

    return v;
}


// get the nodes and their counts from a file
void fill_node_coverage_counts_with_inserts( string filename )
{
    ifstream myfile;
    myfile.open( filename );
    uint32_t node, num ;
    while( myfile >> hex >> node >> num ){
        
        nodes_increment_testcase_count( node );
    }
    myfile.close();
}


void populate_node_to_max_nodes(VM &all_branch_addresses)
{
    for(int i=0; i< all_branch_addresses.size(); i++){
        nodes_add_number_of_edges( all_branch_addresses[i][0] , 
            -1,         
            all_branch_addresses[i][3] == 1 ? 2 :  all_branch_addresses[i][5]
            );
    }
}

// compute hash value of branch condition variable 
// that is used to check if a branch address hash changed 
uint32_t compute_val(int ii)
{
    uint32_t val = 0;
    if( USED_BRANCH_VAL1(branches_operations[ii]) )
        val ^= (branches_value1[ii] << 3 ) ^  (branches_value1[ii] >> 8 );
    if( USED_BRANCH_VAL2(branches_operations[ii]) )
        val ^= branches_value2[ii];
    if( USED_BRANCH_DOUBLE(branches_operations[ii]) )
        val ^= extract_32bit_val_from_double( branches_valuedouble[ii] ) ;

    val ^= branches_taken[ii] << 7;
    val ^= branches_taken[ii] >> 2;

    return val;
}

// coverage info for the case of timeouts,
// i.e. to check if timeouted program produces the same coverage
bool execution_showmap_timeout_new()
{

    bool found_new_ = false;

    for( int i=0; i< 1<<16; i++){
        
        if( (showmap_timeout[i] | ectb[ aflshowmap[i] ] ) != showmap_timeout[i] ){
            found_new_ = true;
            showmap_timeout[i] |= ectb[ aflshowmap[i] ];
        }
    }

    return found_new_;
}


// coverage info used during minimization of seeds
bool execution_consistent_minimized_edge_coverage( uint8_t initial_showmap[1<<16], bool only_update )
{
 
    int found_new_cov = 0;
    for( int i=0; i< 1<<16; i++){


        if( only_update )
            minimized_showmap[i] |= ectb[ aflshowmap[i] ];
        else
            if( (initial_showmap[i] | ectb[ aflshowmap[i] ] ) != minimized_showmap[i] )
                return false;
    }

    return true;

}

void execution_get_minimized_edge_coverage( uint8_t initial_showmap[1<<16] )
{
    memcpy( initial_showmap, minimized_showmap, 1<<16 );
}

