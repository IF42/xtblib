/**
 * @file xtb.h
 * @author Petr Horáček
 *
 * @brief Library for communication with XTB web api. Library is based on official Python
 * library: https://github.com/federico123579/XTBApi/blob/master/XTBApi/api.py
 * 
 * Documentation for the xtb web api is available: http://developers.xstore.pro/documentation
 */


#ifndef __XTB_H__
#define __XTB_H__

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <json.h>

#define DEBUG_ENABLE


#define XTB_LIB_VERSION 1.0.0


typedef enum {
    AccountMode_Demo
    , AccountMode_Real
}AccountMode;


typedef enum {
    CLIENT_NOT_LOGGED
    , CLIENT_LOGGED
}XTB_Client_Status;


typedef enum {
    TransMode_BUY
    , TransMode_SELL
    , TransMode_BUY_LIMIT 
    , TransMode_SELL_LIMIT
    , TransMode_BUY_STOP
    , TransMode_SELL_STOP
    , TransMode_BALANCE
    , TransMode_CREDIT
}TransMode;


typedef enum {
    TransType_OPEN
    , TransType_PENDING 
    , TransType_CLOSE
    , TransType_MODIFY
    , TransType_DELETE
}TransType;


typedef enum {
    PERIOD_M1    = 60
    , PERIOD_M5  = 300
    , PERIOD_M15 = 900
    , PERIOD_M30 = 1800
    , PERIOD_H1  = 3600
    , PERIOD_H4  = 14400
    , PERIOD_D1  = 86400
    , PERIOD_W1  = 604800
    , PERIOD_MN  = 2592000
}Period;


typedef struct {
    XTB_Client_Status status;

    /**
     * @brief Each streaming command takes as an argument streamSessionId 
     * which is sent in response message for login command performed in 
     * main connection. streamSessionId token allows to identify user in 
     * streaming connection. In one streaming connection multiple commands 
     * with different streamSessionId can be invoked. It will cause sending 
     * streaming data for multiple login sessions in one streaming connection. 
     * streamSessionId is valid until logout command is performed on main 
     * connection or main connection is disconnected.
     *
     * <a href="http://developers.xstore.pro/documentation/#available-streaming-commands">Available streaming commands</a> 
     */
    char * stream_session_id;
    char * id;
    char * password;
    AccountMode mode;

    SSL_CTX * ctx;
    SSL     * ssl;
    BIO     * bio;
}XTB_Client;


/**
 * @brief 
 */
XTB_Client xtb_client_init(AccountMode mode, char * id, char * password);


/**
 * @brief
 */
bool xtb_client_refresh_connection(XTB_Client * self);


/**
 * @brief
 */
bool xtb_client_ping(XTB_Client * self);


/**
 * @brief
 */
Json * xtb_client_get_all_symbols(XTB_Client * self);


/*
 * @brief
 */
Json * xtb_client_get_calendar(XTB_Client * self);


/**
 * @brief
 */
Json * xtb_client_get_chart_last_request(
    XTB_Client * self, char * symbol, Period period, time_t start);


/**
 * @brief
 */
Json * xtb_client_get_chart_range_request(
    XTB_Client * self, char * symbol, Period period, time_t start, time_t end, int32_t tick);


/**
 * @brief
 */
Json * xtb_client_get_lastn_candle_history(
    XTB_Client * self, char * symbol, Period period, size_t number);


/*
 * @brief
 */
Json * xtb_client_get_commision(XTB_Client * self, char * symbol, float volume);


/*
 * @brief
 */
Json * xtb_client_get_commision_def(XTB_Client * self, char * symbol, float volume);


/*
 * @brief
 */
Json * xtb_client_get_margin_level(XTB_Client * self);


/*
 * @brief
 */
Json * xtb_client_get_margin_trade(XTB_Client * self, char * symbol, float volume);


/*
 * @brief
 */
Json * xtb_client_get_profit_calculation(
    XTB_Client * self, float close_price, TransMode mode, float open_price, char * symbol, float volume);


/*
 * @brief
 */
Json * xtb_client_get_server_time(XTB_Client * self);


/*
 * @brief
 */
Json * xtb_client_get_symbol(XTB_Client * self, char * symbol);


/*
 * @brief
 */
Json * xtb_client_get_tick_prices(
    XTB_Client * self, size_t size, char ** symbols, int price_level, time_t timestamp);


/*
 * @brief
 */
Json * xtb_client_get_news(XTB_Client * self, time_t start, time_t end);


/*
 * @brief
 */
Json * xtb_client_get_version(XTB_Client * self);


/*
 * @brief
 */
Json * xtb_client_get_trades(XTB_Client * self, bool opened_only);


/*
 * @brief 
 */
Json * xtb_client_get_trade_records(XTB_Client * self, size_t size, char ** orders);


/*
 * @brief
 */
Json * xtb_client_get_trade_history(XTB_Client * self, time_t start, time_t end);


/*
 * @brief
 */
Json * xtb_client_trade_transaction_status(XTB_Client * self, unsigned long order);


/*
 * @brief 
 */
Json * xtb_client_get_user_data(XTB_Client * self);


/*
 * @brief
 */
Json * xtb_client_get_trading_hours(XTB_Client * self, size_t size, char ** symbols);


/*
 * @brief
 */
Json * xtb_client_check_if_market_open(XTB_Client * self, size_t size, char ** symbols);


/*
 * @brief
 */
Json * xtb_client_get_step_rules(XTB_Client * self);


/*
 * @brief
 */
Json * xtb_client_trade_transaction(
    XTB_Client * self
    , char * symbol
    , char * custom_comment
    , TransMode mode
    , time_t expriration
    , int offset
    , char * order
    , float price
    , float tp
    , float sl
    , TransType type
    , float volume);


/*
 * @brief
 */
Json * xtb_client_open_trade(XTB_Client * self, char * symbol, TransMode mode, float volume, float tp, float sl);


/*
 * @brief
 */
Json * xtb_client_close_trade(XTB_Client * self, char * symbol, char * order, TransMode mode, float price, float volume);


/**
 * @brief
 */
void xtb_client_delete(XTB_Client * self);



#endif
