/**
** @file xtb.h
** @author Petr Horáček
**
** @brief Library for communication with XTB web api. Library is based on official Python
** library: https://github.com/federico123579/XTBApi/blob/master/XTBApi/api.py
** 
** Documentation for the xtb web api is available: http://developers.xstore.pro/documentation
*/


#ifndef _XTB_H_
#define _XTB_H_

#include <stdbool.h>
#include <stdint.h>


#define LOGIN_TIMEOUT 120
#define MAX_TIME_INTERVAL 0.200


typedef enum
{
    NOT_LOGGED
    , LOGGED
}Status;


typedef enum
{
    BUY
    , SELL
    , BUY_LIMIT 
    , SELL_LIMIT
    , BUY_STOP
    , SELL_STOP
    , BALANCE
    , CREDIT
}Modes;


typedef enum
{
    OPEN
    , PENDING
    , CLOSE
    , MODIFY
    , DELETE
}TransType;


typedef enum
{
    ONE_MINUTE = 1
    , FIVE_MINUTES = 5
    , FIFTEEN_MINUTES = 15
    , THIRTY_MINUTES = 30
    , ONE_HOUR = 60
    , FOUR_HOURS = 240
    , ONE_DAY = 1440
    , ONE_WEEK = 10080
    , ONE_MONTH = 43200
}Period;


/**
** @brief
*/
typedef struct
{
    bool status;
    char streamSessionId[128];
}CMDLoginResp;


/**
** @brief
*/
struct Client;


/**
** @brief
*/
typedef struct Client Client;



typedef enum
{
   ClientAllocationError
   , ClientConnectionError
}ClientInitStatus;


typedef struct
{
    bool is_value;

    union
    {
        ClientInitStatus right;
        Client * left;    
    }value;
}EitherClient;


#define EitherClientRight(T) (EitherClient) {.is_value = false, .value.right = T}
#define EitherClientLeft(T) (EitherClient) {.is_value = true, .value.left = T}


/**
** @brief 
*/
EitherClient 
client_new();


/**
** @brief
*/
bool
client_login(
    Client * self
    , char * username
    , char * password);


/**
**
*/
void
client_logout(Client * self);



/**
** @brief
*/
void
client_delete(Client * self);



#endif
