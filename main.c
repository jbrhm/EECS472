#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

char const* const shortopts = "v:h";
int const MAX_LINE_LENGTH = 1000;

int main(int argc, char** argv){
    char const* file_name = NULL;
    while(true){
        int c;
        int idx;
        static struct option longopts[] = {
            {"vcd",     required_argument,  0, 'v'},
            {"help",    0,                  0, 'h'},
            {0,         0,                  0, 0}
        };

        c = getopt_long(argc, argv, shortopts, longopts, &idx);

        if(c == -1)
            break;

        switch(c){
            case 'v':
                printf("VCD file %s\n", optarg);
                file_name = optarg;
                break;
            case 'h':
                printf("Usage: ./vcd-analysis [--help | --vcd=path/to/vcd/file]");
                exit(0);
                break;
            default:
                printf("Unknown command line argument\n");
                exit(1);
                break;
        }
    }

    if(!file_name){
        printf("No VCD file provided\n");
        exit(1);
    }

    FILE* file_ptr = fopen(file_name, "r");

    if(!file_ptr){
        perror("Failed to open VCD file");
        exit(1);
    }

    char* line_buf = malloc(MAX_LINE_LENGTH);

    if(!line_buf){
        fclose(file_ptr);
        perror("Failed to allocate line buffer");
        exit(1);
    }

    while(fgets(line_buf, MAX_LINE_LENGTH, file_ptr)){
        printf("Got line %s", line_buf);
    }

    free(line_buf);
    fclose(file_ptr);
}
