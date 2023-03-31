#include <stdio.h>
#include <stdlib.h>

#include "../src/xtb.h"

int
main(int argc, char ** argv)
{
    if(argc < 3)
    {
        printf("no login ID and password!\n");
        return EXIT_FAILURE;
    }

    E_XTB_Client e = xtb_client_new();

    if(e.id == Either_Right)
    {    
        printf("Runtime exception XTB_Client construction: %d\n", e.value.right);
        return EXIT_FAILURE;
    }

    XTB_Client * client = e.value.left;

    if(xtb_client_login(client, argv[1], argv[2], demo) == true)
        printf("connected\n");
    else
        printf("connection failure\n");

    Vector(SymbolRecord) * symbols = xtb_client_get_all_symbols(client);

    for(size_t i = 0; i < VECTOR(symbols)->length; i++)
        printf("%s\n", symbols[i].symbol);

    vector_delete(VECTOR(symbols));
    xtb_client_logout(client);

    xtb_client_delete(client);

    return EXIT_SUCCESS;

}
