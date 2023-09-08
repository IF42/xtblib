/**
** @file xtb.c
** @author Petr Horáček
** @brief 
*/

#include "xtb.h"
#include <json.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>

#define XTB_API_SOCKET_PORT_DEMO "5124"
#define XTB_API_SOCKET_PORT_REAL "5112"

#define XTB_API_ADDRESS "xapi.xtb.com"


static Vector(char) *
xtb_client_transaction(
    XTB_Client * self
    , char * cmd)
{
    if(SSL_write(self->ssl, cmd, strlen(cmd)) <= 0)
        return NULL;

    Vector(char) * response = vector(char, 1024);
    size_t readed_length    = 0; 

    /*
    ** reading input stream
    */
    while(true)
    {
        int16_t input_length = SSL_read(self->ssl, &response[readed_length], 1023);
        readed_length       += input_length;

        if(input_length <= 0 
            || (response[readed_length - 2] == '\n' 
                && response[readed_length - 1] == '\n'))
        {
            break;
        }
        else
             response = vector_resize(VECTOR(response), VECTOR(response)->length + 1024);
    }

    return response;
}


static char *
xtb_client_build_command(
    const char * format
    , ...)
{
    static char buffer[1024];
    va_list args;
    va_start(args, format);

    vsnprintf(buffer, 1023, format, args);

    va_end(args);

    return buffer;
}


static inline char *
xtb_client_login_command(
    char * id
    , char * password)
{
    return xtb_client_build_command(
            "{\"command\": \"login\", \"arguments\": {\"userId\": \"%s\", \"password\": \"%s\"}}"
            , id, password);
}


static inline char *
xtb_client_get_all_symbols_command(void)
{
    return  "{\"command\": \"getAllSymbols\"}";
}


static inline char *
xtb_client_get_logout_command(void)
{
    return "{\"command\": \"logout\"}";
}


static bool
xtb_client_login(
    XTB_Client * self
    , char * id
    , char * password)
{
    char * str_login_cmd      = xtb_client_login_command(id, password);
    Vector(char) * login_resp = xtb_client_transaction(self, str_login_cmd);

    if(login_resp == NULL)
        return false;

    Json * json = json_load(login_resp);

    if (json == NULL) 
    {
		vector_delete(VECTOR(login_resp));
        return false;
    }

    Json * status = json_lookup(json, "status");
	if(status == NULL || status->boolean == false)
	{
		vector_delete(VECTOR(login_resp));
		return false;
	}

    self->status = CLIENT_LOGGED;
    vector_delete(VECTOR(login_resp));
    json_delete(json);

    return true;
}


#define E_XTB_Client_Right(T) \
    (E_XTB_Client) {.id = Either_Right, .value.right = T}


#define E_XTB_Client_Left(T) \
    (E_XTB_Client) {.id = Either_Left, .value.left = T}


E_XTB_Client 
xtb_client(
    AccountMode mode
    , char * id
    , char * password)
{
	XTB_Client self;

	/*
	** initializing of OpenSSL library
	*/
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
	
    if((self.ctx = SSL_CTX_new(TLS_client_method())) == NULL)
	{
        ERR_print_errors_fp(stderr);
        return E_XTB_Client_Right(XTB_Client_SSLInitError);
	}

	 if((self.bio = BIO_new_ssl_connect(self.ctx)) == NULL)
		return E_XTB_Client_Right(XTB_Client_SSLInitError);

	BIO_get_ssl(self.bio, &self.ssl);
	SSL_set_mode(self.ssl, SSL_MODE_AUTO_RETRY);
	
	if(mode == real)
		BIO_set_conn_hostname(self.bio, XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_REAL);
	else
		BIO_set_conn_hostname(self.bio, XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_DEMO);

	if (BIO_do_connect(self.bio) <= 0) 
	{ 
		SSL_CTX_free(self.ctx); 
	    BIO_free_all(self.bio);

		return E_XTB_Client_Right(XTB_Client_NetworkError);
  	}

	if(xtb_client_login(&self, id, password) == false)
		return E_XTB_Client_Right(XTB_Client_LoginError);

    self.id       = strdup(id);
    self.password = strdup(password);

    return E_XTB_Client_Left(self);
}


Vector(SymbolRecord) *
xtb_client_get_all_symbols(XTB_Client * self)
{
    /*
    ** sending command to server
    */
    char * cmd              = xtb_client_get_all_symbols_command();
    Vector(char) * response = xtb_client_transaction(self, cmd);
    
    if(response == NULL)
        return NULL;

    Json * json = json_load(response);

    if(json == NULL)
    {
        vector_delete(VECTOR(response));
        return NULL;
    }

    Json * status = json_lookup(json, "status");

    if(status == NULL || status->boolean == false)
	{
		vector_delete(VECTOR(response));
		return NULL;
	}
    
    /*
    ** processing of input data
    */
    Json * return_data = json_lookup(json, "returnData");
    Vector(SymbolRecord) * symbols = NULL;

    for(size_t i = 0; i < VECTOR(return_data->array)->length; i++)
    {
        if(symbols == NULL)
            symbols = vector(SymbolRecord, VECTOR(return_data->array)->length);

        symbols[i].bid = json_lookup(return_data->array[i], "bid")->frac;
        symbols[i].ask = json_lookup(return_data->array[i], "ask")->frac;

        strncpy(symbols[i].symbol, json_lookup(return_data->array[i], "symbol")->string, 255);
        strncpy(symbols[i].description, json_lookup(return_data->array[i], "description")->string, 255);
    }
    
    vector_delete(VECTOR(response));

    return symbols;
}


void
xtb_client_close(XTB_Client * self)
{
    if(self->status == CLIENT_LOGGED)
    {
        char resp[512] = {0};
        const char * logout_str = xtb_client_get_logout_command();

        if(SSL_write(self->ssl, logout_str, strlen(logout_str)) <= 0)
            return;

        if(SSL_read(self->ssl, resp, 511) <= 0)
            return;
    }

    self->status = CLIENT_NOT_LOGGED;
    
    if(self->id != NULL)
        free(self->id);

    if(self->password != NULL)
        free(self->password);

    SSL_CTX_free(self->ctx); 
    BIO_free_all(self->bio);
}


    
