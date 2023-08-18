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
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#elif defined(_WIN32)
#include<winsock2.h>
#endif

#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <json.h>


#define XTB_NO_EXPORT static


#define SOCKET_PORT_DEMO 5124
#define SOCKET_PORT_REAL 5112


#define XTB_API_ADDRESS "xapi.xtb.com"


struct XTB_Client
{
    Client_Status status;

    int socket;
    int fd;

    SSL_CTX * ctx;
    SSL *     ssl;
};


XTB_NO_EXPORT SSL_CTX * 
init_CTX(void)
{
    OpenSSL_add_all_algorithms(); 
    SSL_load_error_strings();   

    const SSL_METHOD * method = TLS_client_method();  
    SSL_CTX *ctx = SSL_CTX_new(method);   

    return ctx;
}


#define E_XTB_Client_Right(T) \
    (E_XTB_Client) {.id = Either_Right, .value.right = T}


#define E_XTB_Client_Left(T) \
    (E_XTB_Client) {.id = Either_Left, .value.left = T}


E_XTB_Client 
xtb_client_new(AccountMode mode)
{
    XTB_Client * self = malloc(sizeof(XTB_Client));

    if(self == NULL)
        return E_XTB_Client_Right(XTB_Client_AllocationError);

    self->status = CLIENT_NOT_LOGGED;

    SSL_library_init();
    self->ctx = init_CTX();

    if(self->ctx == NULL)
        return E_XTB_Client_Right(XTB_Client_SSLInitError);

    self->ssl = SSL_new(self->ctx);

#if defined (_WIN32)
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        //TODO: release
    }
#endif

    if ((self->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        //TODO: release
    }

    /*
    ** translation of domain name into IP address
    */ 
    struct hostent *he;

    if ((he = gethostbyname(XTB_API_ADDRESS)) == NULL)
    {
        //TODO: release;
    }

    struct in_addr ** addr_list = (struct in_addr **) he->h_addr_list;

    for(int i = 0; addr_list[i] != NULL; i++)
    {
        struct sockaddr_in serv_addr = 
        {
            .sin_family = AF_INET
                , .sin_port = htons(mode == real ? SOCKET_PORT_REAL : SOCKET_PORT_DEMO)
                , .sin_addr.s_addr = inet_addr(inet_ntoa(*addr_list[i]))
        };

        if ((self->fd = connect(
                            self->socket
                            , (struct sockaddr*) &serv_addr
                            , sizeof(serv_addr))) >= 0) 
        {
            struct timeval timeout = 
                {
                    .tv_sec = 0
                    , .tv_usec = MAX_TIME_INTERVAL
                };

            setsockopt(self->fd, SOL_SOCKET, SO_SNDTIMEO, (void*) &timeout, sizeof(timeout));

            SSL_set_fd(self->ssl, self->socket);    
            SSL_connect(self->ssl);

            return E_XTB_Client_Left(self);
        }
    }

    return E_XTB_Client_Right(XTB_Client_NetworkError);
}


bool
xtb_client_login(
    XTB_Client * self
    , char * username
    , char * password)
{
    char login_str[128];

    snprintf(
        login_str, 127
        , "{\"command\": \"login\", \"arguments\": {\"userId\": \"%s\", \"password\": \"%s\"}}"
        , username
        , password);

    char resp[512] = {0};    

    if(SSL_write(self->ssl, login_str, strlen(login_str))<= 0)
        return false;

    if(SSL_read(self->ssl, resp, 511) <= 0)
        return false;

    O_Json o_json = json_load_string(resp);

    if(o_json.is_value == false)
        return false; 

    if(json_lookup(o_json.json, "status").is_value == false 
        || json_lookup(o_json.json, "status").json->boolean == false)
    {
        printf("status: %s\n", json_lookup(o_json.json, "status").json->boolean ? "true" : "false");
        json_delete(o_json.json);

        return false;
    }

    json_delete(o_json.json);
    self->status = CLIENT_LOGGED;    

    return true;
}


bool
xtb_client_connected(XTB_Client * self)
{
    return self->status == CLIENT_LOGGED;
}



xtb_client_get_all_symbols(XTB_Client * self)
{
    if (SSL_connect(self->ssl) <= 0)
        return NULL;

    /*
    ** sending command to server
    */
    const char * cmd = "{\"command\": \"getAllSymbols\"}";

    if(SSL_write(self->ssl, cmd, strlen(cmd)) <= 0)
        return NULL;

    Vector(char) * input_buffer = vector(char, 1024);
    size_t readed_length        = 0; 

    /*
    ** reading input stream
    */
    while(true)
    {
        int16_t input_length = SSL_read(self->ssl, &input_buffer[readed_length], 1023);
        readed_length       += input_length;

        if(input_length <= 0 
            || (input_buffer[readed_length-2] == '\n' 
                && input_buffer[readed_length-1] == '\n'))
        {
            break;
        }
        else
        {
             input_buffer = 
                 vector_resize(VECTOR(input_buffer), VECTOR(input_buffer)->length + 1024);
        }
    }

    /*
    ** processing of input data
    */
    Vector(SymbolRecord) * symbols = NULL;

#if 0
    struct json_object * status;
    struct json_object * returnData;
    struct json_object * record = json_tokener_parse(input_buffer);

    json_object_object_get_ex(record, "status", &status);
    
    if(strcmp(json_object_get_string(status), "true") == 0)
    {
        json_object_object_get_ex(record, "returnData", &returnData);

        symbols = vector(SymbolRecord, json_object_array_length(returnData), free);

        struct json_object * ask;
        struct json_object * bid;
        struct json_object * categoryName;
        struct json_object * contractSize;
        struct json_object * currency;
        struct json_object * currencyPair;
        struct json_object * currencyProfit;
        struct json_object * description;
        struct json_object * expiration;
        struct json_object * groupName;
        struct json_object * high;
        struct json_object * initialMargin;
        struct json_object * instantMaxVolume;
        struct json_object * leverage;
        struct json_object * longOnly;
        struct json_object * lotMax;
        struct json_object * lotMin;
        struct json_object * lotStep;
        struct json_object * low;
        struct json_object * marginHedged;
        struct json_object * marginHedgedStrong;
        struct json_object * marginMaintenance;
        struct json_object * marginMode;
        struct json_object * percentage;
        struct json_object * pipsPrecision;
        struct json_object * precision;
        struct json_object * profitMode;
        struct json_object * quoteId;
        struct json_object * shortSelling;
        struct json_object * spreadRaw;
        struct json_object * spreadTable;
        struct json_object * starting;
        struct json_object * stepRuleId;
        struct json_object * stopsLevel;
        struct json_object * swap_rollover3days;
        struct json_object * swapEnable;
        struct json_object * swapLong;
        struct json_object * swapShort;
        struct json_object * swapType;
        struct json_object * symbol;
        struct json_object * tickSize;
        struct json_object * tickValue;
        struct json_object * time;
        struct json_object * timeString;
        struct json_object * trailingEnabled;
        struct json_object * type;

        for(size_t i = 0; i < VECTOR(symbols)->length; i++)
        {
            struct json_object * symbol_record = 
                json_object_array_get_idx(returnData, i);

            json_object_object_get_ex(symbol_record, "ask", &ask);
            json_object_object_get_ex(symbol_record, "bid", &bid);
            json_object_object_get_ex(symbol_record, "categoryName", &categoryName);
            json_object_object_get_ex(symbol_record, "contractSize", &contractSize);
            json_object_object_get_ex(symbol_record, "currency", &currency);
            json_object_object_get_ex(symbol_record, "currencyPair", &currencyPair);
            json_object_object_get_ex(symbol_record, "currencyProfit", &currencyProfit);
            json_object_object_get_ex(symbol_record, "description", &description);
            json_object_object_get_ex(symbol_record, "expiration", &expiration);
            json_object_object_get_ex(symbol_record, "groupName", &groupName);
            json_object_object_get_ex(symbol_record, "high", &high);
            json_object_object_get_ex(symbol_record, "initialMargin", &initialMargin);
            json_object_object_get_ex(symbol_record, "instantMaxVolume", &instantMaxVolume);
            json_object_object_get_ex(symbol_record, "leverage", &leverage);
            json_object_object_get_ex(symbol_record, "longObly", &longOnly);
            json_object_object_get_ex(symbol_record, "lotMax", &lotMax);
            json_object_object_get_ex(symbol_record, "lotMin", &lotMin);
            json_object_object_get_ex(symbol_record, "lotStep", &lotStep);
            json_object_object_get_ex(symbol_record, "low", &low);
            json_object_object_get_ex(symbol_record, "marginHedged", &marginHedged);
            json_object_object_get_ex(symbol_record, "marginHedgedStrong", &marginHedgedStrong);
            json_object_object_get_ex(symbol_record, "marginMaintenance", &marginMaintenance);
            json_object_object_get_ex(symbol_record, "marginMode", &marginMode);
            json_object_object_get_ex(symbol_record, "percentage", &percentage);
            json_object_object_get_ex(symbol_record, "pipsPrecision", &pipsPrecision);
            json_object_object_get_ex(symbol_record, "precision", &precision);
            json_object_object_get_ex(symbol_record, "profitMode", &profitMode);
            json_object_object_get_ex(symbol_record, "quoteId", &quoteId);
            json_object_object_get_ex(symbol_record, "shortSelling", &shortSelling);
            json_object_object_get_ex(symbol_record, "spreadRaw", &spreadRaw);
            json_object_object_get_ex(symbol_record, "spreadTable", &spreadTable);
            json_object_object_get_ex(symbol_record, "starting", &starting);
            json_object_object_get_ex(symbol_record, "stepRuleId", &stepRuleId);
            json_object_object_get_ex(symbol_record, "stopsLevel", &stopsLevel);
            json_object_object_get_ex(symbol_record, "swap_rollover3days", &swap_rollover3days);
            json_object_object_get_ex(symbol_record, "swapEnable", &swapEnable);
            json_object_object_get_ex(symbol_record, "swapLong", &swapLong);
            json_object_object_get_ex(symbol_record, "swapShort", &swapShort);
            json_object_object_get_ex(symbol_record, "swapType", &swapType);
            json_object_object_get_ex(symbol_record, "symbol", &symbol);
            json_object_object_get_ex(symbol_record, "tickSize", &tickSize);
            json_object_object_get_ex(symbol_record, "tickValue", &tickValue);
            json_object_object_get_ex(symbol_record, "time", &time);
            json_object_object_get_ex(symbol_record, "timeString", &timeString);
            json_object_object_get_ex(symbol_record, "trailingEnabled", &trailingEnabled);
            json_object_object_get_ex(symbol_record, "type", &type);

            symbols[i].ask                = json_object_get_double(ask);
            symbols[i].bid                = json_object_get_double(bid);
            strcpy(symbols[i].categoryName, json_object_get_string(categoryName));
            symbols[i].contractSize       = json_object_get_int(contractSize);
            strcpy(symbols[i].currency, json_object_get_string(currency));
            symbols[i].currencyPair       = json_object_get_boolean(currencyPair);
            strcpy(symbols[i].currencyProfit, json_object_get_string(currencyProfit));
            strcpy(symbols[i].description, json_object_get_string(description));
            symbols[i].expiration.is_value = true;
            symbols[i].expiration.value   = json_object_get_int64(expiration);
            strcpy(symbols[i].groupName, json_object_get_string(groupName));
            symbols[i].high               = json_object_get_double(high);
            symbols[i].initialMargin      = json_object_get_int(initialMargin);
            symbols[i].instantMaxVolume   = json_object_get_int(instantMaxVolume);
            symbols[i].leverage           = json_object_get_double(leverage);
            symbols[i].longOnly           = json_object_get_boolean(longOnly);
            symbols[i].lotMax             = json_object_get_double(lotMax);
            symbols[i].lotMin             = json_object_get_double(lotMin);
            symbols[i].lotStep            = json_object_get_double(lotStep);
            symbols[i].low                = json_object_get_double(low);
            symbols[i].marginHedged       = json_object_get_int(marginHedged);
            symbols[i].marginHedgedStrong = json_object_get_boolean(marginHedgedStrong);
            symbols[i].marginMaintenance.is_value = true;
            symbols[i].marginMaintenance.value  = json_object_get_int(marginMaintenance);
            symbols[i].marginMode         = json_object_get_int(marginMode);
            symbols[i].percentage         = json_object_get_double(percentage);
            symbols[i].pipsPrecision      = json_object_get_int(pipsPrecision);
            symbols[i].precision          = json_object_get_int(precision);
            symbols[i].profitMode         = json_object_get_int(profitMode);
            symbols[i].quoteId            = json_object_get_int(quoteId);
            symbols[i].shortSelling       = json_object_get_boolean(shortSelling);
            symbols[i].spreadRaw          = json_object_get_double(spreadRaw);
            symbols[i].spreadTable        = json_object_get_double(spreadTable);
            symbols[i].starting.is_value  = true;
            symbols[i].starting.value     = json_object_get_int64(starting);
            symbols[i].stepRuleId         = json_object_get_int(stepRuleId);
            symbols[i].stopsLevel         = json_object_get_int(stopsLevel);
            symbols[i].swap_rollover3days = json_object_get_int(swap_rollover3days);
            symbols[i].swapEnable         = json_object_get_boolean(swapEnable);
            symbols[i].swapLong           = json_object_get_double(swapLong);
            symbols[i].swapShort          = json_object_get_double(swapShort);
            symbols[i].swapType           = json_object_get_int(swapType);
            strcpy(symbols[i].symbol, json_object_get_string(symbol));
            symbols[i].tickSize           = json_object_get_double(tickSize);
            symbols[i].tickValue          = json_object_get_double(tickValue);
            symbols[i].time               = json_object_get_int(time);
            strcpy(symbols[i].timeString, json_object_get_string(timeString));
            symbols[i].trailingEnabled    = json_object_get_boolean(trailingEnabled);
            symbols[i].type               = json_object_get_int(symbol_record);
        }
    }
#endif
    vector_delete(VECTOR(input_buffer));

    return symbols;
}


void
xtb_client_logout(XTB_Client * self)
{
    if (SSL_connect(self->ssl) <= 0)
        return;

    char resp[512] = {0};
    const char * logout_str = "{\"command\": \"logout\"}";

    if(SSL_write(self->ssl, logout_str, strlen(logout_str)) <= 0)
        return;

    if(SSL_read(self->ssl, resp, 511) <= 0)
        return;

    self->status = CLIENT_NOT_LOGGED;
}


void
xtb_client_delete(XTB_Client * self)
{
    if(self == NULL)
        return;

    if(self->status == CLIENT_LOGGED)
        xtb_client_logout(self);

    if(self->ssl != NULL)
         SSL_free(self->ssl); 

    if(self->fd > 0)
        close(self->fd);

    if(self->ctx != NULL)
        SSL_CTX_free(self->ctx);

    free(self); 
}


