/**
** @file xtb.c
** @author Petr Horáček
** @brief 
*/

#include "xtb.h"

#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


#define WEB_SOCKET_PORT 443

typedef struct
{
    int socket;
    int fd;
}Connection;


struct Client
{
    Status status;
    Connection connection;
};


typedef struct
{
    bool is_value;
    Connection just;
}MaybeConnection;


#define MaybeConnectionJust(T) (MaybeConnection) {.is_value = true, .just = T}
#define MaybeConnectionNothing (MaybeConnection) {.is_value = false}


MaybeConnection
_client_create_connection(const char * address);


EitherClient 
client_new(const char * broker_address)
{
    MaybeConnection m = _client_create_connection(broker_address);
    
    if(m.is_value == false)
        return EitherClientRight(ClientConnectionError);

    Client * self = malloc(sizeof(Client));

    if(self == NULL)
        return EitherClientRight(ClientAllocationError);

    self->status = NOT_LOGGED;
    self->connection = m.just;

    return EitherClientLeft(self);
}


MaybeConnection
_client_create_connection(const char * address)
{
    Connection connection;
    struct sockaddr_in serv_addr;

    if ((connection.socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        return MaybeConnectionNothing;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(WEB_SOCKET_PORT);

    if (inet_pton(AF_INET, address, &serv_addr.sin_addr) <= 0) 
        return MaybeConnectionNothing;

    if ((connection.fd = 
            connect(
                connection.socket
                , (struct sockaddr*) &serv_addr
                , sizeof(serv_addr))) < 0) 
    {
        return MaybeConnectionNothing;
    }

    return MaybeConnectionJust(connection);
}



void
client_delete(Client * self)
{
    if(self != NULL)
    {
        //TODO: logout

        if(self->connection.fd > 0)
            close(self->connection.fd);

        free(self);
    }    
}
