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

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


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


/**
** @brief 
*/
Client * 
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
