#include <stdio.h>
#include <stdlib.h>

#include "../src/xtb.h"

int
main(void)
{
    
    EitherXTB_Client e = xtb_client_new();

    
    if(e.is_value == true)
    {    
        XTB_Client * client = e.value.left;

        if(xtb_client_login(client, "13587481", "73KR19SPn", demo) == true)
            printf("connected\n");
        else
            printf("connection failure\n");

        xtb_client_delete(client);
        return EXIT_SUCCESS;
    }
    else  
        printf("Runtime exception XTB_Client construction: %d\n", e.value.right);
    
    return EXIT_FAILURE;
}
