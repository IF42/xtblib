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
#include <openssl/bio.h>
#include <json.h>

#define DEBUG_ENABLE


#define XTB_LIB_VERSION 1.2.0


/**
 * @brief
 */
typedef enum {
    XTB_AccountMode_Demo
    , XTB_AccountMode_Real
}XTB_AccountMode;


/**
 * @brief
 */
typedef enum {
    XTB_TransMode_BUY
    , XTB_TransMode_SELL
    , XTB_TransMode_BUY_LIMIT 
    , XTB_TransMode_SELL_LIMIT
    , XTB_TransMode_BUY_STOP
    , XTB_TransMode_SELL_STOP
    , XTB_TransMode_BALANCE
    , XTB_TransMode_CREDIT
}XTB_TransMode;


/**
 * @brief
 */
typedef enum {
    XTB_TransType_OPEN
    , XTB_TransType_PENDING 
    , XTB_TransType_CLOSE
    , XTB_TransType_MODIFY
    , XTB_TransType_DELETE
}XTB_TransType;


/**
 * @brief
 */
typedef enum {
    XTB_PERIOD_M1    = 60
    , XTB_PERIOD_M5  = 300
    , XTB_PERIOD_M15 = 900
    , XTB_PERIOD_M30 = 1800
    , XTB_PERIOD_H1  = 3600
    , XTB_PERIOD_H4  = 14400
    , XTB_PERIOD_D1  = 86400
    , XTB_PERIOD_W1  = 604800
    , XTB_PERIOD_MN  = 2592000
}XTB_Period;


/**
 * @brief
 */
typedef struct XTB_Client XTB_Client;


/**
 * @brief 
 */
XTB_Client * xtb_client_new(XTB_AccountMode mode, char * id, char * password);


/**
 * @brief
 */
bool xtb_client_logged(XTB_Client * self);


/**
 * @brief
 */
size_t xtb_client_stream_session_size(XTB_Client * self);


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
    XTB_Client * self, char * symbol, XTB_Period period, time_t start);


/**
 * @brief
 */
Json * xtb_client_get_chart_range_request(
    XTB_Client * self, char * symbol, XTB_Period period, time_t start, time_t end, int32_t tick);


/**
 * @brief
 */
Json * xtb_client_get_lastn_candle_history(
    XTB_Client * self, char * symbol, XTB_Period period, size_t number);


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
    XTB_Client * self, float close_price, XTB_TransMode mode, float open_price, char * symbol, float volume);


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
    , XTB_TransMode mode
    , time_t expriration
    , int offset
    , char * order
    , float price
    , float tp
    , float sl
    , XTB_TransType type
    , float volume);


/*
 * @brief
 */
Json * xtb_client_open_trade(
        XTB_Client * self, char * symbol, XTB_TransMode mode, float volume, float tp, float sl);


/*
 * @brief
 */
Json * xtb_client_close_trade(
        XTB_Client * self, char * symbol, char * order, XTB_TransMode mode, float price, float volume);


/**
 * @brief
 */
void xtb_client_delete(XTB_Client * self);


/*
 * @brief Streaming client is connection into XTB server for automatic reading of reacent data
 * This is the fastest way for getting the newst data.
 *
 * Every StreamClient command takes as argument streaming_session_id received from loggin to main port. 
 *
 * <a href="http://developers.xstore.pro/documentation/#available-streaming-commands">Available streaming commands</a> 
 *
 * Main problem of subscribing and unsubcribing of streaming commands is that, there is not 
 * confimed and only thing that confirm subcription is receiving of given streaming data, but
 * if the command is wrong, it is hard to detect
 */
typedef struct XTB_StreamClient XTB_StreamClient;


/*
 * @brief
 */
typedef void (*StreamCallback)(void *, Json *);


/**
 * @brief
 */
typedef struct {
    StreamCallback balance;
    StreamCallback news;
    StreamCallback candle;
    StreamCallback keep_alive;
    StreamCallback profit;
    StreamCallback tick_prices;
    StreamCallback trades;
    StreamCallback trade_status;
} StreamClientCallback;


/**
 * @brief
 */
XTB_StreamClient * xtb_stream_client_new(
        XTB_Client * self, StreamClientCallback * callback, void * param);


/**
 * @brief
 */
void xtb_stream_client_process(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_news(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_news(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_balance(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_balance(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_candles(XTB_StreamClient * self, char * symbol);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_candles(XTB_StreamClient * self, char * symbol);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_keep_alive(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_keep_alive(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_profits(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_profits(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_tick_prices(
        XTB_StreamClient * self, char * symbol, time_t min_arrive_time, int max_level);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_tick_price(XTB_StreamClient * self, char * symbol);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_trades(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_trades(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_subscribe_trade_status(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_unsubscribe_trade_status(XTB_StreamClient * self);


/**
 * @brief
 */
bool xtb_stream_client_ping(XTB_StreamClient * self);


/**
 * @brief
 */
void xtb_stream_client_delete(XTB_StreamClient * self);


#endif





