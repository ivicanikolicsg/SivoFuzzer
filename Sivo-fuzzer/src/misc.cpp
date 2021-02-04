/*
Copyright (c) 2021, Ivica Nikolic <cube444@gmail.com> and Radu Mantu <radu.mantu@upb.ro>

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



#include "misc.h"
#include "MersenneTwister.h"


//
// Miscellaneous functions about everything, including
// execution, files/folders, randomization, etc...
//


static int tmp_fd = -1;     /* permanently open fd for TMP_INPUT */
MTRand mtr;
string waited_result;
float write_speed = 0;
uint64_t write_length = 0;
uint64_t tot_writes_to_file = 0;


//
//
//
// Execution stuff
//
//
//

void timer_handler (int signum) 
{
    printf ("Timed out!\n");
}


void exec_store_in_waited(string scmd)
{
    scmd += " 2>&1";
    const char *cmd = scmd.c_str();
    array<char, 128> buffer;
    waited_result="";
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        waited_result += buffer.data();
    }
}

void simple_exec( string scmd )
{
    scmd += " 1>/dev/null";
    int ans = system(scmd.c_str() );
    if (ans)
        printf("\033[31;1m[ERR] system(\"%s\") call failed!\033[0m\n",
            scmd.c_str());
}







//
//
// Files & folders stuff
//
//


bool copy_binary_file( string source, string dest)
{
    std::ifstream  src(source, std::ios::binary);
    std::ofstream  dst(dest,   std::ios::binary);

    dst << src.rdbuf();    

}

int file_size(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg(); 
}

bool file_exists (const std::string& name) 
{
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

bool dir_exists( char* mydir )
{
    DIR* dir = opendir(mydir);
    if (dir) {
        closedir(dir);
        return true;
    } 
    else 
        return false;
}


uint64_t hash_file(string filename)
{
    std::ifstream file(filename, std::ifstream::binary);
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    char buf[1024 * 16];
    while (file.good()) {
        file.read(buf, sizeof(buf));
        MD5_Update(&md5Context, buf, file.gcount());
    }
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);
    uint64_t h = 0;
    for(int i=0; i<MD5_DIGEST_LENGTH; i++){
        h <<= 8;
        h += result[i];
    }

    return h;
}



unsigned char* read_binary_file(string filename, int &bytes_read)
{
    bytes_read = 0;
    FILE *file = fopen(filename.c_str(), "rb");
    if (file == NULL){
        printf( KERR "Cannot open binary filename:%s\n" KNRM, filename.c_str());
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long int nsize = ftell(file);
    fclose(file);
    // Reading data to array of unsigned chars
    file = fopen(filename.c_str(), "rb");
    if( NULL == file) return NULL;
    unsigned char * in = (unsigned char *) malloc(nsize+1);
    bytes_read = fread(in, sizeof(unsigned char), nsize, file);
    fclose(file);
    return in;
}

uint64_t * read_binary_file_uint64_t(string filename, int &qword_read)
{
    qword_read = 0;
    FILE *file = fopen(filename.c_str(), "rb");
    if (file == NULL){
        printf( KERR "Cannot open binary filename:%s\n" KNRM, filename.c_str());
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long int nsize = ftell(file);
    fclose(file);
    // Reading data to array of unsigned chars
    file = fopen(filename.c_str(), "rb");
    if( NULL == file) return NULL;
    uint64_t * in = (uint64_t *) malloc( (nsize+8)/8 * sizeof(uint64_t) );
    qword_read = fread(in, sizeof(uint64_t), nsize, file);
    fclose(file);
    return in;
}

/*
 * write_common - common write subroutine
 *      @fd    : file descriptor
 *      @data  : data buffer
 *      @dsize : buffer length
 */
static void write_common(int fd, unsigned char* data, int dsize)
{
    int ans;            /* answer (return value) */
    int dsz_cp = dsize; /* dsize copy            */

    START_TIMER(begin_time);

    /* write data to file */
    while (dsize) {
        ans = write(fd, data, dsize);
        if (ans == -1) {
            fprintf(stderr, "ERROR: unable to write to file (%d)\n", errno);
            exit(-1);
        }

        data  += ans;
        dsize -= ans;
    }

    /* truncate file to current size (if previously larger) */
    ans = ftruncate(fd, dsz_cp);
    if (ans == -1) {
        fprintf(stderr, "ERROR: unable to write to file (%d)\n", errno);
        exit(-1);
    }

    /* update stats */
    write_speed        += TIME_DIFFERENCE_SECONDS(begin_time);
    write_length       += dsz_cp;
    tot_writes_to_file += 1;
}

/*
 * write_bytes_to_binary_file - old interface, now used only for TMP_INPUT
 *      @filename : TMP_INPUT (we can remove this at some point)
 *      @data     : data buffer
 *      @dsize    : buffer size
 *
 * This function will open a file descriptor for TMP_INPUT only once and reuse
 * it across calls. Reduce number of open() and close() syscalls.
 */
void write_bytes_to_binary_file(string filename, unsigned char *data, int dsize)
{
    /* first time call; open file */
    if (__builtin_expect((tmp_fd == -1), 0)) {
        tmp_fd = open(filename.c_str(), O_WRONLY|O_CREAT, 0666);
        if (tmp_fd == -1) {
            fprintf(stderr, "ERROR: unable to open file (%d)\n", errno);
            exit(-1);
        }
    }

    /* seek start of file for subsequent writes */
    int ans = lseek(tmp_fd, 0, SEEK_SET);
    if (ans == (off_t) -1) {
        fprintf(stderr, "ERROR: unable to lseek start of file (%d)\n", errno);
        exit(-1);
    }

    /* pass control to common function */
    write_common(tmp_fd, data, dsize);
}

/*
 * write_bytes_to_binary_file_non_permanent - used in minimization
 *      @filename : not TMP_INPUT
 *      @data     : data buffer
 *      @dsize    : buffer size
 *
 * This function will open and close a file descriptor.
 */
void write_bytes_to_binary_file_non_permanent(string filename, unsigned char *data, int dsize)
{
    /* open one-off file */
    int fd = open(filename.c_str(), O_WRONLY|O_CREAT, 0666);
    if (fd == -1) {
        fprintf(stderr, "ERROR: unable to open file (%d)\n", errno);
        exit(-1);
    }

    /* pass control to common function */
    write_common(fd, data, dsize);

    /* close file */
    int ans = close(fd);
    if (ans == -1) {
        fprintf(stderr, "ERROR: unable to close file (%d)\n", errno);
        exit(-1);
    }
}


string get_only_filename_from_filepath( string filepath)
{
    int pos = filepath.size() - 2;
    while( pos >= 0 && filepath[pos] != '/' ) pos--;
    if( pos < 0 ) return filepath;

    return filepath.substr(pos+1);
}

string get_current_dir()
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
       return string(cwd);
    } else {
       return string("");
    }
}

bool prepare_tmp_input(string original_input)
{
    if( !file_exists( original_input )){ 
        printf( KERR "Cannot read the original input file:%s\n" KNRM, original_input.c_str());
        exit(0);
        return false;
    }

    // copy the original input
    copy_binary_file( original_input, TMP_INPUT);

    // expand to min bytes (if needed)
    if (EXPAND_TESTCASE_SIZE){
        int tmplenx;
        unsigned char * tmpx = read_binary_file( TMP_INPUT , tmplenx );
        if( tmplenx < MIN_EXPANDED_TESTCASE_SIZE ){
            tmpx = (unsigned char*)realloc(tmpx, MIN_EXPANDED_TESTCASE_SIZE);
            memset(tmpx + tmplenx , 0 , MIN_EXPANDED_TESTCASE_SIZE - tmplenx );
            write_bytes_to_binary_file(TMP_INPUT, tmpx, MIN_EXPANDED_TESTCASE_SIZE);
        }
        free(tmpx);
    }

    return true;

}

//
//
// Randomization stuff
//
//

void init_random()
{
    srand(time(0));
    mtr.seed(time(NULL));
}

unsigned char one_rand_byte()
{
    return mtr.randInt() % 256;
}

uint8_t adv_mutate_byte(uint8_t x)
{
    if( mtr.randInt() % 2 )
        return x ^ one_nonzero_rand_nibble();
    else{
        uint8_t cand = x - 4 + (mtr.randInt() % 8);
        while( cand == x) 
            cand = x - 4 + (mtr.randInt() % 8);
        return cand ; 
    }
}


int discrete_distribution_sample( vector <float> &v )
{
    random_device rd;
    mt19937 gen(rd());

    discrete_distribution<> d(v.begin(), v.end());

    return d(gen);
}


uint8_t one_nonzero_uniformly_random_byte()
{
    return 1 + (mtr.randInt() % 255 );
}

uint8_t one_nonzero_rand_nibble()
{
    if( mtr.randInt() % 3 )
        return 1 + (mtr.randInt() % 255 );
    else
        return 1<<(mtr.randInt() % 8);
}


//
//
// Everything else
//
//


uint32_t extract_32bit_val_from_double( double v)
{
    if( 0 == v ) return 0;
    uint32_t extracted_32bit_v = 0;
    if( v < 0) extracted_32bit_v = 1;
    char sv[1024];
    int writn = snprintf(sv,1024, "%f",v);
    for( int pos=0; pos < writn; pos++){
        if( sv[pos] >= '0' && sv[pos] <= '9'){
            extracted_32bit_v *=3;
            extracted_32bit_v ^=  sv[pos] + 7 * pos;
        }
        else if( '.' == sv[pos] ){
            extracted_32bit_v += 113;
            extracted_32bit_v *= 13;
        }
    }

    return extracted_32bit_v;

}

float mmin( float a, float b)
{
    return a<b ? a : b;
}

float mmax( float a, float b)
{
    return a>b ? a : b;
}


const uint64_t m1  = 0x5555555555555555; 
const uint64_t m2  = 0x3333333333333333; 
const uint64_t m4  = 0x0f0f0f0f0f0f0f0f; 
const uint64_t m8  = 0x00ff00ff00ff00ff; 
const uint64_t m16 = 0x0000ffff0000ffff; 
const uint64_t m32 = 0x00000000ffffffff; 

int hamming_weight64(uint64_t x)
{
    x -= (x >> 1) & m1;             
    x = (x & m2) + ((x >> 2) & m2); 
    x = (x + (x >> 4)) & m4;        
    x += x >>  8;  
    x += x >> 16;  
    x += x >> 32;  
    return x & 0x7f;
}


#define JJ_SIZE (1<<15)
uint8_t jj[ JJ_SIZE ];

bool jumping_jindices( int count)
{
    if( count < JJ_SIZE) return jj[ count % JJ_SIZE];

    return jj[ 1000 + ( count % (JJ_SIZE-1000) ) ];
    
}

void reinit_jumping_jindices()
{
    for( int count=0; count<JJ_SIZE; count++){

        if( count <= 128)
        { 
            jj[count] = 1;
            continue;
        }

        float cprob = log(count+2)/log(2);
        if( count > 10000)
            cprob *= 100;
        else if( count > 1000)
            cprob *= 10;
        else if( count > 100)
            cprob *= 5;
        jj[count] = !( mtr.randInt() % int(cprob) );
    }
}


string inst_no_to_str(uint32_t no, int t)
{
    if( 0 == t ){
        switch(no){
            case 51: return "cmp";
            default: return "ukn";
        }
    }
    if (1 == t ){
        switch(no){
            case 32: return "je"; 
            case 33: return "jne"; 
            case 34: return "ja"; 
            case 35: return "jae"; 
            case 36: return "jb"; 
            case 37: return "jbe"; 
            case 38: return "jg"; 
            case 39: return "jge";
            case 40: return "jb";           // this should be singed "jl" !!!!!
            case 41: return "jle"; 

            default: return "ukn";
        }
    }

    return "unt";
}

