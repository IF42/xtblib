/**
** @file xtb.c
** @author Petr Horáček
** @brief 
*/

#include "xtb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__)
#include <sys/socket.h>
#include <arpa/inet.h>
#elif defined(_WIN32)
#include<winsock2.h>
#endif

#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#define SOCKET_PORT_DEMO 5124
#define SOCKET_PORT_REAL 5112

#define XTB_API_ADDRESS "xapi.xtb.com"


typedef struct
{
    int socket;
    int fd;
}Connection;


struct XTB_Client
{
    Status status;

    SSL_CTX *ctx;
    SSL *ssl;

    Connection connection;
};


typedef struct
{
    bool is_value;
    Connection just;
}MaybeConnection;


#define MaybeConnectionJust(T) (MaybeConnection) {.is_value = true, .just = T}
#define MaybeConnectionNothing (MaybeConnection) {.is_value = false}


static int 
_xtb_client_receive(
    int hSocket
    , char* Rsp
    , short RvcSize);


static int 
_xtb_client_send(
    int hSocket
    , char* Rqst
    , short lenRqst);


MaybeConnection
_xtb_client_create_connection(
    const char * address
    , uint16_t port);

static SSL_CTX * 
_xtb_client_init_CTX(void);


EitherXTB_Client 
xtb_client_new()
{
    XTB_Client * self = malloc(sizeof(XTB_Client));

    if(self == NULL)
        return EitherXTB_ClientRight(XTB_ClientAllocationError);

    self->status = NOT_LOGGED;
    
    SSL_library_init();
    self->ctx = _xtb_client_init_CTX();

    if(self->ctx == NULL)
        return EitherXTB_ClientRight(XTB_ClientSSLInitError);

    self->ssl = SSL_new(self->ctx);
    self->connection = (Connection) {0};

    return EitherXTB_ClientLeft(self);
}

MaybeConnection
_xtb_client_create_connection(
    const char * address
    , uint16_t port)
{
    Connection connection;

#if defined (_WIN32)
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        
        return MaybeConnectionNothing;
    }
#endif

    printf("connection start\n");
    if ((connection.socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        printf("Could not create socket : %d\n" , WSAGetLastError());
        return MaybeConnectionNothing;
    }

    //u_long mode = 1;  // 1 to enable non-blocking socket
    //ioctlsocket(connection.socket, FIONBIO, &mode);

    printf("connection to: %s\n", address);

    struct hostent *he;

    if ( (he = gethostbyname(address) ) == NULL)
    {
        printf("Cant convert host name to ip address!\n");
        return MaybeConnectionNothing;
    }

    struct in_addr **addr_list = (struct in_addr **) he->h_addr_list;

    for(int i = 0; addr_list[i] != NULL; i++)
    {
        printf("%s\n", inet_ntoa(*addr_list[i]) );

        struct sockaddr_in serv_addr = 
        {
            .sin_family = AF_INET
            , .sin_port = htons(port)
            , .sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[i]))
        };

        if ((connection.fd = 
                connect(
                    connection.socket
                    , (struct sockaddr*) &serv_addr
                    , sizeof(serv_addr))) >= 0) 
        {
            return MaybeConnectionJust(connection);
        }
    }

    printf("not connected: %d\n", connection.fd);

    return MaybeConnectionNothing;
}


 SSL_CTX * 
_xtb_client_init_CTX(void)
{
    OpenSSL_add_all_algorithms(); 
    SSL_load_error_strings();   

    const SSL_METHOD * method = TLS_client_method();  
    SSL_CTX *ctx = SSL_CTX_new(method);   

    return ctx;
}


bool
xtb_client_login(
    XTB_Client * self
    , char * username
    , char * password
    , AccountMode mode)
{
    if(self->connection.fd <= 0)
    {
        MaybeConnection m = 
            _xtb_client_create_connection(
                XTB_API_ADDRESS
                , mode == real ? SOCKET_PORT_REAL : SOCKET_PORT_DEMO);

        if(m.is_value == false)
        {
            printf("connection was not established\n");

            return false;
        }

        self->connection = m.just;
    }

    SSL_set_fd(self->ssl, self->connection.socket);    

    if (SSL_connect(self->ssl) <= 0)
        return false;

    char login_str[128];

    snprintf(login_str, 127, "{\"command\": \"login\", \"arguments\": {\"userId\": \"%s\", \"password\": \"%s\"}}", username, password);

    printf("%s\n", login_str);

    char resp[512];    

    if(SSL_write(self->ssl, login_str, strlen(login_str))<= 0)
    {
        printf("cant send\n");
        return false;
    }

    if(SSL_read(self->ssl, resp, 511) <= 0)
    {
        printf("Cant received nothing\n");
        return false;
    }

    printf("received:\n%s\n", resp);

    return true;
}


void
xtb_client_logout(XTB_Client * self)
{
    (void) self;
}


void
xtb_client_delete(XTB_Client * self)
{
    if(self != NULL)
    {
        if(self->status == LOGGED)
            xtb_client_logout(self);

        if(self->ssl != NULL)
             SSL_free(self->ssl); 

        if(self->connection.fd > 0)
            close(self->connection.fd);

        if(self->ctx != NULL)
            SSL_CTX_free(self->ctx);

        free(self);
    }    
}


static int 
_xtb_client_send(
    int hSocket
    , char* Rqst
    , short lenRqst)
{
    int shortRetval = -1;
    struct timeval tv =
    {
        .tv_sec = 20  /* 2 Secs Timeout */
        , .tv_usec = 0
    };

    if(setsockopt(hSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv)) < 0)
        return -1;

    printf("sending...\n");
    shortRetval = send(hSocket, Rqst, lenRqst, 0);

    return shortRetval;
}

static int 
_xtb_client_receive(
    int hSocket
    , char* Rsp
    , short RvcSize)
{
    int shortRetval = -1;
    struct timeval tv = 
    {
        .tv_sec = 20  /* 2 Secs Timeout */
        , .tv_usec = 0
    };

    if(setsockopt(hSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) < 0)
        return -1;

    printf("receiving...\n");
    shortRetval = recv(hSocket, Rsp, RvcSize, 0);

    return shortRetval;
}
