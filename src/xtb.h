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
#include <time.h>

#include <openssl/ssl.h>
#include <openssl/err.h>


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
    ONE_MINUTE        = 1
    , FIVE_MINUTES    = 5
    , FIFTEEN_MINUTES = 15
    , THIRTY_MINUTES  = 30
    , ONE_HOUR        = 60
    , FOUR_HOURS      = 240
    , ONE_DAY         = 1440
    , ONE_WEEK        = 10080
    , ONE_MONTH       = 43200
}Period;


typedef enum
{
   XTB_Client_SSLInitError
   , XTB_Client_NetworkError
   , XTB_Client_LoginError
}XTB_Client_Status;


typedef struct 
{
    Client_Status status;

    AccountMode mode;
    char    * id;
    char    * password;
        
    SSL_CTX * ctx;
    SSL     * ssl;
    BIO     * bio;
}XTB_Client;


typedef enum
{
    Either_Left
   , Either_Right
}E_ID;


typedef struct
{
    E_ID id;

    union
    {
        XTB_Client_Status right;
        XTB_Client left;    
    }value;
}E_XTB_Client;


/**
** @brief 
*/
E_XTB_Client 
xtb_client(
    AccountMode mode
    , char * id
    , char * password);


/**
**
*/
void
xtb_client_close(XTB_Client * self);


typedef struct
{
    double ask;
	double bid;
	char categoryName[256];
	int32_t contractSize;
	char currency[256];
	bool currencyPair;
	char currencyProfit[256];
	char description[256];

    struct
    {
	    bool is_value;
        time_t value;
    }expiration;

	char groupName[256];
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
	
    struct
    {
        bool is_value;
        int32_t value;
    }marginMaintenance;

	int32_t marginMode;
	double percentage;
    int32_t pipsPrecision;
	int32_t precision;
	int32_t profitMode;
	int32_t quoteId;
	bool shortSelling;
	double spreadRaw;
	double spreadTable;

	struct
    {
        bool is_value;
        time_t value;   
    }starting;

	int32_t stepRuleId;
	int32_t stopsLevel;
	int32_t swap_rollover3days;
	bool swapEnable;
	double swapLong;
	double swapShort;
	int32_t swapType;
	char symbol[256];
	double tickSize;
	double tickValue;
	time_t time;
	char timeString[256];
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



#endif
