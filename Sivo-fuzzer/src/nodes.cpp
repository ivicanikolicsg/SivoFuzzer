
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


#include "config.h"
#include "nodes.h"
#include "executions.h"
#include "typez.h"


// 
//
// Nodes are simply basic blocks (so branch addresses but without multiplicity). 
// We keep them in map *Nodes*. The idea is to keep track which nodes are solved, so 
// no need to invert them. Some initial info about the Nodes is obtained and later provided
// during the LLVM compilation. As we execute seeds, more info is added dynamically, from 
// the traces. 
//
//

map <uint32_t,node_type> Nodes;


uint32_t nodes_get_current_size()
{
    return Nodes.size();
}

node_type nodes_init_empty_node()
{
    node_type nod;

    nod.set_of_outcoming_nodes.clear();
    nod.set_of_cases.clear();
    nod.set_of_solved_cases.clear();
    nod.no_testcases_present = 0;

    nod.cur_num_of_cases = 0;
    nod.max_num_of_cases = 0;

    nod.cur_num_of_out_edges = 0;
    nod.max_num_of_out_edges = 0;

    nod.type_of_branch = 0;
    nod.cmp_instruction_code = 0;
    nod.cmp_compare_predicate = 0 ;

    nod.random_walk_score = 0;

    return nod;
}

int  nodes_not_solved( uint32_t node, uint32_t val)
{
    auto it = Nodes.find( node );
    if( it == Nodes.end() ) return -1;

    return 
        it->second.set_of_cases.find( val ) != it->second.set_of_cases.end() && 
        it->second.set_of_solved_cases.find( val ) == it->second.set_of_solved_cases.end() ; 
    
} 


void nodes_add_new_node( uint32_t node )
{
    if( Nodes.find( node ) != Nodes.end() ) return;

    Nodes.insert( {node, nodes_init_empty_node()} );
}

bool nodes_exist(uint32_t node)
{
    return Nodes.find(node) != Nodes.end();
}

    
bool nodes_is_solved(uint32_t node)
{
 
    auto it = Nodes.find(node);
    if (it == Nodes.end() || it->second.cur_num_of_cases >= it->second.max_num_of_cases )
        return true;

    return false;
}

void nodes_add_number_of_edges(uint32_t node, int new_cur_num_edges, int new_max_num_edges)
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) {
        nodes_add_new_node( node );
        it = Nodes.find(node);
    }

    if( new_cur_num_edges >= 0) it->second.cur_num_of_out_edges = new_cur_num_edges;
    if( new_max_num_edges >= 0) it->second.max_num_of_out_edges = new_max_num_edges;
}

void nodes_add_outcoming_node(uint32_t node, uint32_t outcome_node)
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) {
        nodes_add_new_node( node );
        it = Nodes.find(node);
    }

    it->second.set_of_outcoming_nodes.insert( outcome_node );
    it->second.cur_num_of_out_edges = it->second.set_of_outcoming_nodes.size();
}

void nodes_add_if(uint32_t node, uint32_t cmp_instruction_code, uint32_t cmp_compare_predicate)
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) {
        nodes_add_new_node( node );
        it = Nodes.find(node);
    }

    it->second.type_of_branch = 1;
    it->second.cmp_instruction_code = cmp_instruction_code;
    it->second.cmp_compare_predicate = cmp_compare_predicate;

    it->second.set_of_cases.insert( 0 );
    it->second.set_of_cases.insert( 1 );

    it->second.max_num_of_cases = 2;
}


void nodes_add_switch_case(uint32_t node, uint32_t sw_case)
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) {
        nodes_add_new_node( node );
        it = Nodes.find(node);
    }

    it->second.type_of_branch = 2;
    it->second.set_of_cases.insert( sw_case );

    it->second.max_num_of_cases = it->second.set_of_cases.size() + 1;
}

uint32_t nodes_get_number_all_outcoming_nodes()
{
    uint32_t s= 0;
    for( auto it=Nodes.begin(); it!= Nodes.end(); it++)
        s+= it->second.set_of_outcoming_nodes.size();

    return s;
}


uint32_t nodes_get_number_all_solved()
{
    uint32_t s= 0;
    for( auto it=Nodes.begin(); it!= Nodes.end(); it++)
        s+= it->second.set_of_solved_cases.size();

    return s;
}



set<uint32_t> nodes_get_outcome_nodes(  uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) set<uint32_t>();
    return it->second.set_of_outcoming_nodes;
}

int nodes_get_number_outcome_nodes(  uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) return -1;
    return it->second.set_of_outcoming_nodes.size();
}


int nodes_get_max_out_edges( uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) return -1;
    return it->second.max_num_of_out_edges;

}


int nodes_get_number_solved(  uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) return -1;
    return it->second.cur_num_of_cases;
}

int nodes_get_number_max(  uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) return -1;
    return it->second.max_num_of_cases;
}


void nodes_add_type(uint32_t node, int type_of_branch)
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) {
        nodes_add_new_node( node );
        it = Nodes.find(node);
    }

    it->second.type_of_branch = type_of_branch;
}

int nodes_get_testcase_count( uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) return -1;

    return  it->second.no_testcases_present ;

}

void nodes_increment_testcase_count(uint32_t node)
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) {
        nodes_add_new_node( node );
        it = Nodes.find(node);
    }

    it->second.no_testcases_present ++ ;

}

void nodes_decrement_testcases_count(uint32_t node ){

    auto it = Nodes.find(node);
    if( it == Nodes.end() || it->second.no_testcases_present  <= 1 ) 
        return;

    it->second.no_testcases_present -- ;
}

void nodes_clear_all_testcase_count()
{
    for(auto it=Nodes.begin(); it!= Nodes.end(); it++)
        it->second.no_testcases_present = 0 ;
}

uint32_t nodes_get_number_of_important( int branch_type  )
{
    uint32_t s = 0;
    for(auto it=Nodes.begin(); it!= Nodes.end(); it++){    
        if( it->second.cur_num_of_cases > 0 && (branch_type & it->second.type_of_branch) )
            s ++;
    }
    return s;
}


uint32_t nodes_get_number_of_important_unsolved(int branch_type )
{
    uint32_t s = 0;
    for(auto it=Nodes.begin(); it!= Nodes.end(); it++)
        if( it->second.cur_num_of_cases > 0 && (branch_type & it->second.type_of_branch) )
            s += it->second.cur_num_of_cases < it->second.max_num_of_cases;
    return s;
}


uint32_t nodes_get_num_cur_or_max(int branch_type , int cur_or_max)
{
    uint32_t s1 = 0, s2=0 ;
    for(auto it=Nodes.begin(); it!= Nodes.end(); it++)
        if( it->second.cur_num_of_cases > 0 && (branch_type & it->second.type_of_branch) ){
            s1 += it->second.cur_num_of_cases;
            s2 += it->second.max_num_of_cases;
        }
    return (0 == cur_or_max) ? s1 : s2;
}


uint32_t nodes_get_number_of_present_in_testcases()
{
    uint32_t s = 0;
    for(auto it=Nodes.begin(); it!= Nodes.end(); it++)
        if( it->second.no_testcases_present > 0 )
            s++;
    return s;
}


void fill_nodes_from_edgecoverage_file( string filename )
{
    ifstream myfile;
    myfile.open( filename );
    uint32_t node, count, b ;
    uint32_t prev_node = 0;
    while( myfile >> hex >> node >> count >> b ){
        if( 0 == prev_node){
            prev_node = node;
            continue;
        }
        nodes_add_outcoming_node( prev_node , node );

        prev_node = node;
    }
    myfile.close();
}


void nodes_fill_ifs( string bp )
{
    string fname = get_only_filename_from_filepath( bp );
    if( fname.size() == 0) return;
    string ifs_filename = bp.substr( 0, bp.size() - fname.size() ) + LLVM_IF_PREFIX + fname ;

    if( !file_exists(ifs_filename)){
        printf( KERR "Cannot find the ifs file %s \n" KNRM, ifs_filename.c_str() ); 
        return;
    }

    ifstream myfile;
    myfile.open( ifs_filename );
    uint32_t node, cmp_instruction_code, cmp_compare_predicate;
    uint32_t tot_add = 0;

    string line;
    while (getline(myfile, line))
    {
        stringstream ss;
        ss.str(line);
        ss >> hex >> node >> cmp_instruction_code >> cmp_compare_predicate ;
        nodes_add_if( node, cmp_instruction_code , cmp_compare_predicate );
        tot_add ++;
    }
    myfile.close();

    printf( KINFO "Added if nodes : %d\n", tot_add );

}


void nodes_fill_switch_cases( string bp )
{
    string fname = get_only_filename_from_filepath( bp );
    if( fname.size() == 0) return;
    string switch_filename = bp.substr( 0, bp.size() - fname.size() ) + LLVM_SWITCH_PREFIX + fname ;

    if( !file_exists(switch_filename)){
        printf( KERR "Cannot find the switch cases file %s \n" KNRM, switch_filename.c_str() ); 
        return;
    }

    int swiches_added = 0;
    ifstream myfile;
    myfile.open( switch_filename );
    uint32_t node,sw_successors, sw_cases, sw_one_case;
    uint32_t tot_add = 0;

    string line;
    while (getline(myfile, line))
    {
        stringstream ss;
        ss.str(line);
        ss >> hex >> node >> sw_successors>> sw_cases ;
        swiches_added ++;
        for( int i=0; i<sw_cases; i++){
            ss >> sw_one_case;
            nodes_add_switch_case( node, sw_one_case );
            tot_add ++;
            if( ss.eof() ) break;
        } 
    }
    myfile.close();

    printf( KINFO "Added switch nodes & switch cases: %d  %d\n", swiches_added, tot_add );
}

void nodes_add_solved_value( uint32_t node, uint32_t sw_value )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() ) return;

    if( it->second.type_of_branch == 1){
        if( it->second.set_of_cases.size() != 2 ){
            it->second.set_of_cases.clear();
            it->second.set_of_cases.insert( 0 );
            it->second.set_of_cases.insert( 1 );
            it->second.max_num_of_cases = 2;
        }
        it->second.set_of_solved_cases.insert( sw_value );
        it->second.cur_num_of_cases = it->second.set_of_solved_cases.size();

        return;
    }

    if( it->second.set_of_cases.find( sw_value ) != it->second.set_of_cases.end() ){
        it->second.set_of_solved_cases.insert( sw_value );
        it->second.cur_num_of_cases = it->second.set_of_solved_cases.size() + 1;
    }

}

int nodes_get_type_of_branch( uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() )  return -1;

    return it->second.type_of_branch;

}

uint32_t nodes_choose_random_unsolved_switch( uint32_t node )
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() )  return 0;

    vector <uint32_t> targets;
    for( auto zz=it->second.set_of_cases.begin(); zz!=it->second.set_of_cases.end(); zz++ )
        if( it->second.set_of_solved_cases.find( *zz ) == it->second.set_of_solved_cases.end() )
            targets.push_back( *zz );

    if( targets.size() == 0) targets.push_back( mtr.randInt() );

    return targets[ mtr.randInt() % targets.size() ];
}



/*
 * CFG for random walk 
 */

map < uint32_t , vector <uint32_t> > cfgs;

void nodes_fill_cfg( string bp )
{
    string fname = get_only_filename_from_filepath( bp );
    if( fname.size() == 0) return;
    string cfg_filename = bp.substr( 0, bp.size() - fname.size() ) + LLVM_CFG_PREFIX + fname ;

    if( !file_exists(cfg_filename)){
        printf( KERR "Cannot find the cfg file %s \n" KNRM, cfg_filename.c_str() ); 
        return;
    }

    int nexts_added = 0;
    ifstream myfile;
    myfile.open( cfg_filename );
    uint32_t node,no_successors, one_successor;
    uint32_t tot_add = 0;
    string line;
    while (getline(myfile, line))
    {
        stringstream ss;
        ss.str(line);
        ss >> hex >> node >> no_successors ;
        if( no_successors  > 1000)  continue;
        vector <uint32_t> onenode;
        for( int i=0; i<no_successors; i++){
            ss >> one_successor;
            onenode.push_back( one_successor);
            nexts_added ++;
            if( ss.eof() ) break;
        }
        cfgs.insert ( {node, onenode} );
    }
    myfile.close();


    printf( KINFO "Added CFG nodes & successors : %ld  %d\n", cfgs.size(), nexts_added ); fflush(stdout);

}

int random_walk_on_cfg( uint32_t start_node, uint32_t steps)
{
    int found_new_nodes = 0;

    set <uint32_t> fnodes;

    uint32_t current_node = start_node;
    for( int step=0; step<steps; step++){

        auto zz = Nodes.find( current_node );
        if( zz != Nodes.end() ){
            if ( fnodes.find( current_node) == fnodes.end() &&  
                 zz->second.type_of_branch > 0 && 
                 zz->second.set_of_solved_cases.size() == 0 )
                    found_new_nodes ++;

        }

        fnodes.insert( current_node );

        auto it = cfgs.find( current_node );
        if( it == cfgs.end() )
            break;

        if( it->second.size() == 0) break;

        current_node = it->second[ mtr.randInt() % it->second.size() ];
    }

    return found_new_nodes;
}


void do_random_walk_on_all_nodes( int steps )
{
    for( auto it= Nodes.begin(); it!= Nodes.end(); it++)
        it->second.random_walk_score = random_walk_on_cfg( it->first, steps );
}

int nodes_get_random_walk_score(uint32_t node)
{
    auto it = Nodes.find(node);
    if( it == Nodes.end() )  return 0;

    return it->second.random_walk_score;

}