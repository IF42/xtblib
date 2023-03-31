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
#include <vector.h>
#include <ndarray.h>
#include <time.h>


#define LOGIN_TIMEOUT 120
#define MAX_TIME_INTERVAL 0.200


typedef enum
{
    demo
    , real
}AccountMode;


typedef enum
{
    CLIENT_NOT_LOGGED
    , CLIENT_LOGGED
}Client_Status;


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
struct XTB_Client;


/**
** @brief
*/
typedef struct XTB_Client XTB_Client;


typedef enum
{
   XTB_Client_AllocationError
   , XTB_Client_SSLInitError
   , XTB_Client_NetworkError
}XTB_Client_Status;


typedef enum
{
    Either_Left
   , Either_Right
}Either_ID;

typedef struct
{
    Either_ID id;

    union
    {
        XTB_Client_Status right;
        XTB_Client * left;    
    }value;
}E_XTB_Client;


/**
** @brief 
*/
E_XTB_Client 
xtb_client_new(AccountMode mode);


/**
** @brief
*/
bool
xtb_client_login(
    XTB_Client * self
    , char * username
    , char * password);


bool
xtb_client_connected(XTB_Client * self);


/**
**
*/
void
xtb_client_logout(XTB_Client * self);


typedef struct
{
    bool is_value;
    int32_t value;
}O_Int;


typedef struct
{
    bool is_value;
    time_t value;
}O_Time;


typedef struct
{
    double ask;
	double bid;
	char categoryName[64];
	int32_t contractSize;
	char currency[16];
	bool currencyPair;
	char currencyProfit[16];
	char description[16];
	O_Time expiration;
	char groupName[16];
	double high;
	int32_t initialMargin;
	int32_t instantMaxVolume;
	double leverage;
	bool longOnly;
	double lotMax;
	double lotMin;
	double lotStep;
	double low;
	int32_t marginHedged;
	bool marginHedgedStrong;
	O_Int marginMaintenance;
	int32_t marginMode;
	double percentage;
    int32_t pipsPrecision;
	int32_t precision;
	int32_t profitMode;
	int32_t quoteId;
	bool shortSelling;
	double spreadRaw;
	double spreadTable;
	O_Time starting;
	int32_t stepRuleId;
	int32_t stopsLevel;
	int32_t swap_rollover3days;
	bool swapEnable;
	double swapLong;
	double swapShort;
	int32_t swapType;
	char symbol[36];
	double tickSize;
	double tickValue;
	time_t time;
	char timeString[64];
	bool trailingEnabled;
	int32_t type;
}SymbolRecord;


/**
** @brief 
*/
Vector(SymbolRecord) *
xtb_client_get_all_symbols(XTB_Client * self);


typedef struct
{
    double close_price;
    /* TODO: code */
}TradeRecord;


Vector(TradeRecord) *
xtb_client_get_trades(XTB_Client * self);


/**
** @brief
*/
void
xtb_client_delete(XTB_Client * self);



#endif
