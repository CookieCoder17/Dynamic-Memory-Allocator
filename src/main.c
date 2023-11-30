#include <stdio.h>
#include <string.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    double* ptr = sf_malloc(4032);
    sf_show_heap();
    double *ptr2 = sf_malloc(4078);
    sf_show_heap();
    // printf("%f\n",sf_fragmentation());
    // printf("%f\n",sf_utilization());
    sf_free(ptr);
        sf_show_heap();
    sf_free(ptr2);
    sf_show_heap();
    return EXIT_SUCCESS;
}
