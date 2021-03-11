#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* external API wrapper */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

/* main - program entry point
 *  @argc : number of arguments
 *  @argv : starting from 1, location of input files (tested one at a time)
 *
 *  @return : 0 if everything ok
 */
int main(int argc, char *argv[])
{
    struct stat statbuf;
    uint8_t     *data = NULL;
    ssize_t     rb;
    size_t      tot_size = 0;
    int         fd, ans;;

    for (size_t i=1; i<argc; ++i) {
        /* open input file */
        fd = open(argv[i], O_RDONLY);    
        if (fd == -1) {
            printf("[ERROR] Unable to open input file %s (%d)\n",
                argv[i], errno);
            exit(-1);
        }

        /* get input file size */
        ans = fstat(fd, &statbuf);
        if (ans == -1) {
            printf("[ERROR] Unable to stat input file (%d)\n", errno);
            exit(-1);
        }

        /* allocate buffer */
        if (statbuf.st_size > tot_size) {
            data = (uint8_t *) realloc(data, statbuf.st_size);
            if (!data) {
                printf("[ERROR] Unable to allocate %lu bytes (%d)\n",
                    statbuf.st_size, errno);
                exit(-1);
            }

            tot_size = statbuf.st_size;
        }

        /* read data */
        rb = read(fd, data, statbuf.st_size);
        if (rb == -1) {
            printf("[ERROR] Unable to read input file (%d)\n", errno);
            exit(-1);
        }

        /* cleanup */
        ans = close(fd);
        if (ans == -1) {
            printf("[ERROR] Unable to close input file (%d)\n", errno);
            exit(-1);
        }

        /* call the wrapper */
        ans = LLVMFuzzerTestOneInput(data, statbuf.st_size);
        if (ans)
            printf("[WARNING] possible error in wrapper (ans=%d)\n", ans); 
    }

    /* more cleanup */
    free(data);

    return 0;
}

