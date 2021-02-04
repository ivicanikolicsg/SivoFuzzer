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


#include "branches.h"
#include "executions.h"
#include "nodes.h"



//
//
// Branch addresses, their multiplicity, and all functions related to them.
// E.g. their dependency on input bytes, successors, types of branches,... 
//
// Branch information is stored across different iterations (of seeds) in  the map *Branches*
//
//

map < branch_address_pair , branch_address_type > Branches;


void branch_clear_all_branches()
{
    Branches.clear();
}

unsigned long branch_current_size()
{
    return Branches.size();
}


branch_address_pair get_ba(  uint32_t ad_bas, uint32_t ad_cou  )
{
    return make_pair( ad_bas, ad_cou) ; 
}



branch_address_type branch_insert_empty_branch(branch_address_pair b)
{
    auto iz = Branches.find(b);
    if( iz != Branches.end() )
        return branch_address_type();

    branch_address_type br;
    br.dependency.clear();
    br.dependency_slow.clear();
    br.dependency_fast.clear();
    br.solutions.clear();
    br.edges.clear();
    br.found_at_iter = 0;
    br.times_fuzzed = 0;
    br.branch_type = 0;
    br.max_out_edges = 0;
    br.cannot_infer_constraints = false;
    br.is_solved_constraint = false;
    br.is_independent_constraint = false;
    br.is_multidependent_constraint = false;

    br.fill_rand_byte_values = false;
    br.used_detailed_dependency_inference = false;

    Branches.insert({b,br});

    return br;
}

void import_branches(VM &all_branch_addresses, int iteration_no)
{

    for(int i=0; i<all_branch_addresses.size(); i++){
        
        branch_address_pair b = get_ba( all_branch_addresses[i][0],all_branch_addresses[i][1]  );
        int branch_type = all_branch_addresses[i][3];
        int output_edges = 2;
        if( 2 == branch_type){ // if switch, then 
            // read the actual number of cases
            output_edges= all_branch_addresses[i][5];
        }

        // if not condbranch or switch
        if( !branch_type ) 
            continue;

        // Add to nodes
        nodes_add_type( all_branch_addresses[i][0] , branch_type );
        nodes_add_number_of_edges( all_branch_addresses[i][0] , -1, output_edges );
        nodes_add_solved_value( all_branch_addresses[i][0], all_branch_addresses[i][7] );

        auto it = Branches.find( b ); 
        if( it == Branches.end() ){
            branch_insert_empty_branch(b);
            it = Branches.find( b );
            it->second.found_at_iter = iteration_no;
            it->second.branch_type = branch_type;
            it->second.max_out_edges = output_edges;
        }

        if( i < all_branch_addresses.size() -1 &&   it != Branches.end() ){
            uint32_t next_node = all_branch_addresses[i+1][0]; 
            if( (it->second).edges.find(next_node) == (it->second).edges.end()  )
                (it->second).edges.insert( next_node );
        }

    }

}

void print_all_branches()
{
    for( auto it=Branches.begin(); it!=Branches.end(); it++){
        branch_address_pair b = it->first;
        cout << b.first<<" "<<b.second <<" ";
        cout << endl;
    }
}

bool branch_exist(branch_address_pair bp)
{
    return Branches.find(bp) != Branches.end();
}

    
bool branch_is_solved(branch_address_pair bp)
{
    return false;
}

void branch_add_dependency(branch_address_pair bp, vector<int> found_vars, bool slow)
{
    auto it = Branches.find(bp);
    if( it == Branches.end() ) return;

    for( int i=0; i<found_vars.size(); i++)
        if( slow ){
            if( find( it->second.dependency_slow.begin() , it->second.dependency_slow.end() , found_vars[i] ) == it->second.dependency_slow.end() )
                    it->second.dependency_slow.push_back( found_vars[i] );
        }
        else{
            if( find( it->second.dependency_fast.begin() , it->second.dependency_fast.end() , found_vars[i] ) == it->second.dependency_fast.end() )
                    it->second.dependency_fast.push_back( found_vars[i] );
        }


    while( it->second.dependency_slow.size() > MAX_DEPENDENCIES_PER_BRANCH )
        it->second.dependency_slow.erase (it->second.dependency_slow.begin()+ (mtr.randInt() % it->second.dependency_slow.size()) );

    while( it->second.dependency_fast.size() > MAX_DEPENDENCIES_PER_BRANCH - it->second.dependency_slow.size()  )
        it->second.dependency_fast.erase (it->second.dependency_fast.begin()+ (mtr.randInt() % it->second.dependency_fast.size()) );

    it->second.dependency.clear();
    for( int i=0; i<it->second.dependency_slow.size() ; i++)
        it->second.dependency.push_back( it->second.dependency_slow[i] );
    for( int i=0; i<it->second.dependency_fast.size() ; i++)
        it->second.dependency.push_back( it->second.dependency_fast[i] );

}

void branch_replace_dependency_fast(branch_address_pair bp, vector<int> found_vars)
{
    auto it = Branches.find(bp);
    if( it == Branches.end() ) return;

    if( it->second.dependency_fast == found_vars) return;

    // if new dependency then start random guess from beginning
    it->second.dependency_fast = found_vars;
    for( int i=0; i<256; i++){
        it->second.rand_byte_values[ 0 ].insert( i );
        it->second.rand_byte_values[ 1 ].insert( i );
    }
}



vector<int> get_dependency( branch_address_pair bp)
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return vector<int>();
    return it->second.dependency;
}

set<uint32_t> branch_get_edges(  branch_address_pair bp )
{
    auto it = Branches.find(bp);
    if( it == Branches.end() ) set<uint32_t>();
    return it->second.edges;
}


uint64_t get_sol_hash(vector<int> varz)
{
    uint64_t h = 0;
    for( int i=0; i<varz.size(); i++){
        h *= 3;
        h ^= varz[i] + 7;
    }
    return h;
}

void branch_insert_solution( branch_address_pair bp, vector<int> varz, vector<int> sols)
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return;
    uint64_t h = get_sol_hash(varz);
    if ( it->second.solutions.find(h) == it->second.solutions.end() ){
        vector<vector<int>> dummy;
        it->second.solutions.insert({h,dummy});
    }
    auto i2 = it->second.solutions.find(h);
    i2->second.push_back( sols );

    while(i2->second.size() >= MAX_SOLUTIONS_PER_BRANCH_ADDRESS ){
        i2->second.erase(i2->second.begin() );
    }
}

vector<int> get_previous_solution( branch_address_pair bp, vector<int> varz )
{
    vector<int> sol;
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return sol;

    uint64_t h = get_sol_hash(varz);
    auto i2 = it->second.solutions.find(h);
    if( i2 == it->second.solutions.end() ) return sol; 

    sol = i2->second[ mtr.randInt() %  i2->second.size() ];
    return sol;

}

void branch_add_edge( branch_address_pair bp, uint32_t next_branch_address)
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ){
        printf( KERR "(add_edge)Branch does not exist: %08x %4d\n" KNRM, bp.first,bp.second);
        return; 
    }
    if ( it->second.edges.find( next_branch_address ) != it->second.edges.end() ){ 
        printf( KINFO "(add_edge)Such next branch exists already: %08x :" KNRM, next_branch_address);
        for( auto i2 = it->second.edges.begin(); i2 != it->second.edges.end(); i2++)
            printf("%08x ", (*i2));
        printf("\n");
        return; 
    }
    it->second.edges.insert( next_branch_address );
    printf( KGRN "Add new edge: %08x %4d  ->  %08x\n" KNRM, bp.first,bp.second, next_branch_address ) ;
}



void print_branches_with_dependency( vector <branch_address_pair> branches_with_dependency  )
{
    printf("===========================================================================================\n");
    printf("Branches with dependency:\n");
    printf("\n");
    for(int i=0; i<branches_with_dependency.size(); i++){ 
        //branch_address_pair ba = get_ba( branch_addresses[i][0], branch_addresses[i][1] );
        branch_address_pair ba = branches_with_dependency[i];
        if ( branch_exist(ba)){
            vector <int> fv = get_dependency( ba );
            if( fv.size() > 0){
                printf("%08x %4d # solv %ld  # vars :", ba.first, ba.second, branch_get_edges(ba).size() );
                for( int j=0; j<fv.size(); j++)
                    printf("%d ", fv[j]);
                printf("\n");
            }
        }
    }
    printf("===========================================================================================\n");
}

int branch_get_solving_candidates( VM branch_addresses )
{
    int candidates = 0;
    for(int i=0; i<branch_addresses.size(); i++){ 
        branch_address_pair ba = get_ba( branch_addresses[i][0], branch_addresses[i][1] );
        
        if ( !branch_exist(ba) ) continue;
        if ( branch_is_solved(ba) ) continue;
        if ( get_dependency( ba ).size() == 0 ) continue;

        candidates ++;

    }

    return candidates;

}


int branch_get_number_of_not_solved(bool with_failed)
{
    int not_solved= 0;
    for(auto it=Branches.begin(); it != Branches.end(); it++){
        not_solved += it->second.edges.size() < 2 ;

    }

    return not_solved;
}


void branch_add_constraints(branch_address_pair bp, map <int,int> true_deps, vector <uint32_t> constraints, bool inverted)
{
    auto it = Branches.find(bp);
    if( it == Branches.end() ) return;
    it->second.true_deps = true_deps;
    it->second.constraints = constraints;
    it->second.inverted = inverted;

    it->second.is_solved_constraint = false;
    it->second.is_independent_constraint = false;
    it->second.is_multidependent_constraint = false;
}

bool branch_has_constraints(branch_address_pair bp )
{
    auto it = Branches.find(bp);
    if( it == Branches.end() ) return false;
    return it->second.true_deps.size() > 0;
}

map <int,int> branch_get_true_dep( branch_address_pair bp)
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return map<int,int>();
    return it->second.true_deps;
}


vector <uint32_t> branch_get_constraints( branch_address_pair bp)
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return vector <uint32_t> {0,0,0,0,0};
    return it->second.constraints;
}

bool branch_get_inverted( branch_address_pair bp )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return false;
    return it->second.inverted;
}



bool branch_get_cannot_infer_constraints( branch_address_pair bp)
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return false;
    return it->second.cannot_infer_constraints;
}


bool branch_is_multi_byte_constraint( branch_address_pair bp )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return false;
    return it->second.true_deps.size() >= 1;
}


bool branch_is_solved_constraint( branch_address_pair bp )
{

    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return true;
    return it->second.is_solved_constraint;
}

void branch_set_solved_constraint( branch_address_pair bp, bool set_solved )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return ;
    it->second.is_solved_constraint = set_solved;
    if ( set_solved == false ){
        it->second.is_independent_constraint = false;
        it->second.is_multidependent_constraint = false;
    }
}





set<uint8_t> branch_get_rand_byte_values( branch_address_pair bp , int truth_value  )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return set<uint8_t>();

    // first fill
    if( ! it->second.fill_rand_byte_values ){
        it->second.fill_rand_byte_values = true;
        for( int i=0; i<256; i++){
            it->second.rand_byte_values[  truth_value].insert( i );
            it->second.rand_byte_values[1-truth_value].insert( i );
        }
    }

    return it->second.rand_byte_values[ truth_value ];
}

void branch_set_rand_byte_values( branch_address_pair bp , int truth_value, set<uint8_t> byte_values  )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return;

    it->second.rand_byte_values[ truth_value] = byte_values;
}

vector <int> branch_get_correct_dependency( branch_address_pair bp )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return vector<int>();

    auto true_dependencyZ    = it->second.true_deps;
    auto simple_dependencyZ  = it->second.dependency;
    vector <int> tmp_depsZ;
    if( true_dependencyZ.size() > 0){
        for( auto iz=true_dependencyZ.begin(); iz!= true_dependencyZ.end(); iz++){
            tmp_depsZ.push_back( iz->first );
        }
    } 
    else tmp_depsZ = simple_dependencyZ;

    return tmp_depsZ;

}



int branch_get_found_at_iteration( branch_address_pair bp ) 
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return 100000;

    return it->second.found_at_iter;
}


void branch_set_constraint_interval( branch_address_pair bp, vector < pair<uint64_t,uint64_t> > interval ) 
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return;

    it->second.interval = interval;
    it->second.is_solved_constraint = true;
    it->second.is_multidependent_constraint = true;
}

vector < pair<uint64_t,uint64_t> > branch_get_constraint_interval( branch_address_pair bp )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return vector< pair<uint64_t,uint64_t> >();
    return it->second.interval;
}


bool branch_is_constraint_independent( branch_address_pair bp )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return false;
    return it->second.is_independent_constraint;    
}

bool branch_is_constraint_multibyte_dependent( branch_address_pair bp )
{
    auto it = Branches.find( bp );
    if ( it == Branches.end() ) return false;
    return it->second.is_multidependent_constraint;    
}


void branch_print_info( branch_address_pair ba , int code )
{
    printf( " %08x %4d : Instr ", ba.first, ba.second );
    auto con = branch_get_constraints(ba);
    if( con.size() == 5)
        printf("%s %d  %s %d  %08x", 
            inst_no_to_str(con[2],0).c_str(), con[3],   
            inst_no_to_str(con[0],1).c_str(), con[1], con[4] );    

    printf(" : is_multi  %d %d ", branch_is_multi_byte_constraint( ba ), branch_is_constraint_independent(ba) );
    printf(" : dep  ");
    auto dtmp = get_dependency( ba );
    for( int i=0; i<dtmp.size(); i++) printf("%d ", dtmp[i] );
    printf(" : adep  ");
    auto dit = branch_get_true_dep( ba );
    for( auto it=dit.begin(); it!= dit.end(); it++) printf("%d ", it->second );
    int inv = branch_get_inverted( ba ) ? 1:0;
    printf(" : inverted %d ", inv);



}

bool branch_get_used_detailed_dependency_inference( branch_address_pair ba  )
{
    auto it = Branches.find( ba );
    if ( it == Branches.end() ) return false;

    return it->second.used_detailed_dependency_inference;
}

void branch_set_used_detailed_dependency_inference( branch_address_pair ba, bool val  )
{
    auto it = Branches.find( ba );
    if ( it == Branches.end() ) return ;

    it->second.used_detailed_dependency_inference = val;
}


void branch_random_sieve()
{

    printf("Removing randomly branches: start with %ld\n", Branches.size() );
    if( Branches.size() < 1000){
        printf("Not enough branches to remove\n");
        return;
    }

    set<branch_address_pair> to_remove; 
    for( auto it=Branches.begin(); it!= Branches.end(); it++)
        if( !jumping_jindices( it->first.second  ) )
            to_remove.insert( it->first );

    for( auto it=to_remove.begin(); it!=to_remove.end(); it++)
        Branches.erase(*it);

    printf("Left: %ld\n", Branches.size() );

}

bool branch_good_for_iteration(  branch_address_pair ba )
{

    auto it = Branches.find( ba );
    if ( it == Branches.end() ) return false;

    return jumping_jindices( it->first.second );

}


void branch_remove_all_dependencies()
{
    for( auto it= Branches.begin(); it != Branches.end(); it++){
        it->second.dependency = vector <int> ();
        it->second.dependency_slow = vector <int> ();
        it->second.dependency_fast = vector <int> ();
        it->second.true_deps = map <int,int>();
        it->second.constraints = vector <uint32_t>();
        it->second.inverted = false;

        it->second.is_solved_constraint = false;
        it->second.is_independent_constraint = false;
        it->second.is_multidependent_constraint = false;

    }
}

int branch_get_total_number()
{
    return Branches.size();
}

int branch_get_times_fuzzed(  branch_address_pair ba   )
{
    auto it = Branches.find( ba );
    if ( it == Branches.end() ) return false;

    return it->second.times_fuzzed;
}

void branch_increase_times_fuzzed( branch_address_pair ba   )
{
    auto it = Branches.find( ba );
    if ( it == Branches.end() ) return ;

    it->second.times_fuzzed ++;
}

int branch_get_branch_type(  branch_address_pair ba   )
{
    auto it = Branches.find( ba );
    if ( it == Branches.end() ) return 0;

    return it->second.branch_type;
}

int branch_get_max_out_edges(  branch_address_pair ba   )
{
    auto it = Branches.find( ba );
    if ( it == Branches.end() ) return 0;

    return it->second.max_out_edges;
}
