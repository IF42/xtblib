#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#include "../src/xtb.h"


#define throw(FORMAT, ...)                                                      \
    do                                                                          \
    {                                                                           \
        fprintf(stderr, "%s:%d:"FORMAT, __FILE__, __LINE__, ##__VA_ARGS__);     \
        raise(SIGABRT);                                                         \
    }while(0)


int
main(int argc, char ** argv)
{
    if(argc < 3)
        throw("No login ID and password!\n");

    E_XTB_Client e;

    if((e = xtb_client(demo, argv[1], argv[2])).id == Either_Right)
        throw("Runtime exception XTB_Client construction: %d\n", e.value.right);
    else
    {
        XTB_Client client = e.value.left;

        printf("Logged in\n");

        Vector(SymbolRecord) * symbols = xtb_client_get_all_symbols(&client);
        
        if(symbols == NULL)
            throw("Reading symbols error\n");

        printf("symbols: %ld\n", VECTOR(symbols)->length);
        
        for(size_t i = 0; i < VECTOR(symbols)->length; i++)
        {   
                printf(
                    "{description: %s, ask: %f, bid: %f}\n"
                    , symbols[i].symbol
                    , symbols[i].ask
                    , symbols[i].bid);
        }
        
        vector_delete(VECTOR(symbols));
        xtb_client_close(&client);
    }

    printf("Program exit\n");

    return EXIT_SUCCESS;

}
