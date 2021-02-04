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


#include <set>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include "files_etc.h"
#include "solutions.h"
#include "config.h"     

//
// Create required folder structure, discover master/slaves, 
// get seeds from the master/slave folders, put them in some temporary sets
//  


static set<string> all_files;
static set<string> untested_files;
static set<string> dir_queues;

void init_folders()
{
    simple_exec( "rm -rf "+OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME);
    simple_exec( "mkdir -p "+OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME +"/queue" );
    simple_exec( "mkdir -p "+OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME +"/queue_full" );
    simple_exec( "mkdir -p "+OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME +"/crashes" );
    simple_exec( "mkdir -p "+OUTPUT_FOLDER+"/"+FUZZER_OUTPUT_FOLDER_NAME +"/hangs" );
    simple_exec( "rm -rf " + FOLDER_AUX );
    simple_exec( "mkdir -p " + FOLDER_AUX );
}

void init_tmp_inputs()
{
    TMP_INPUT = OUTPUT_FOLDER + "/" + FUZZER_OUTPUT_FOLDER_NAME + "/tmp_input.txt";
}

void get_testcases_from_folder(string sfolder, bool external)
{
    const char *folder = sfolder.c_str();
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (folder)) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            const char *file_name = ent->d_name;
            if (strlen(file_name) < 3 ) continue; 
            string full_file_name = sfolder + "/" + file_name;

            if ( all_files.find(full_file_name) == all_files.end() && 
                 file_size(full_file_name.c_str()) < MAX_TESTCASE_SIZE){

                    all_files.insert( full_file_name );
                    // if external, then copy to local folder
                    if( external ){
                        string new_full_file_name = create_id( full_file_name, true, SOL_TYPE_IMPORTED, "" );
                        if( new_full_file_name.size() > 0){
                            untested_files.insert( new_full_file_name );
                            solution_add_found_types( 0 );
                        }
                    }
                    else{
                        untested_files.insert( full_file_name );
                    }
            }
        }
        closedir (dir);
    } 
    else {
        printf(KRED "Cannot open folder %s\n" KNRM, folder);
        return ;
    }
}

string get_untested_file()
{
    if ( untested_files.size() == 0) return "";
    int choice = mtr.randInt() % untested_files.size();
    auto it = untested_files.begin();
    for(  ; choice > 0; it++) choice--;
    string one = *it;
    untested_files.erase(it);

    return one;
}

void add_untested_file(string filename )
{
    untested_files.insert( filename );
}

void set_tested_file(string filename)
{
    auto it = untested_files.find( filename );
    if (it != untested_files.end() )
        untested_files.erase( it );
}


void find_all_afl_instance_folders()
{

    DIR *dir = opendir(OUTPUT_FOLDER.c_str());
    struct dirent *entry = readdir(dir);
    while (entry != NULL)
    {
        if (entry->d_type == DT_DIR &&
            strcmp(entry->d_name,".") && 
            strcmp(entry->d_name,"..") && 
            strcmp(entry->d_name,FUZZER_OUTPUT_FOLDER_NAME.c_str() )
            ){
                //printf(KINFO "Discovered dir:%s\n" KNRM, entry->d_name);
                fflush(stdout);
                dir_queues.insert( OUTPUT_FOLDER+"/"+entry->d_name );
        }

        entry = readdir(dir);
    }

    closedir(dir);

}

void retrieve_testcase_files_from_afl_instances()
{
    for( auto it=dir_queues.begin(); it!= dir_queues.end(); it++){
        get_testcases_from_folder(*it+"/queue", true );
    }
}

void print_all_untested_testcases()
{
    printf("Untested files:%lu\n", untested_files.size());
    for( auto it=untested_files.begin(); it!= untested_files.end(); it++)
        printf(":%s:\n", (*it).c_str() );
}
