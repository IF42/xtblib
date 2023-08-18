#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

#include "../src/xtb.h"

int
main(int argc, char ** argv)
{
    
    struct timeval start, end;
    gettimeofday (&start, NULL);

    if(argc < 3)
    {
        printf("no login ID and password!\n");
        return EXIT_FAILURE;
    }

    E_XTB_Client e = xtb_client_new(demo);

    if(e.id == Either_Right)
    {    
        printf("Runtime exception XTB_Client construction: %d\n", e.value.right);
        return EXIT_FAILURE;
    }
    else
    {
        printf("client created\n");
        fflush(stdout);
    }

    XTB_Client * client = e.value.left;

    if(xtb_client_login(client, argv[1], argv[2]) == true)
        printf("connected\n");
    else
    {
        printf("connection failure\n");
        xtb_client_delete(client);
        return EXIT_FAILURE;
    }

    Vector(SymbolRecord) * symbols = xtb_client_get_all_symbols(client);

 //   for(size_t i = 0; i < VECTOR(symbols)->length; i++)
 //       printf("%s\n", symbols[i].symbol);
    
    vector_delete(VECTOR(symbols));

    xtb_client_logout(client);
    xtb_client_delete(client);

    gettimeofday (&end, NULL);
    
    uint64_t timeElapsed = (((end.tv_sec * 1000000) + end.tv_usec) -
           ((start.tv_sec * 1000000) + start.tv_usec))/1000;

    printf("Elapsed time: %lldms\n", timeElapsed);

    return EXIT_SUCCESS;

}
