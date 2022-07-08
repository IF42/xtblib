#include <stdio.h>
#include <stdlib.h>

#include "../src/xtb.h"

int
main(int argc, char ** argv)
{
    printf("%d\n", argc);
    if(argc >= 3)
    {
        EitherXTB_Client e = xtb_client_new();

        if(e.is_value == true)
        {    
            XTB_Client * client = e.value.left;

            if(xtb_client_login(client, argv[1], argv[2], demo) == true)
                printf("connected\n");
            else
                printf("connection failure\n");

            xtb_client_delete(client);
            return EXIT_SUCCESS;
        }
        else  
            printf("Runtime exception XTB_Client construction: %d\n", e.value.right);
    }
    else
        printf("no login ID and password!\n");

    return EXIT_FAILURE;
}
