/**
 * @file xtblib.c
 * @author Petr Horáček
 * @brief 
 * TODO: 
 * * prepare streaming commands interface
 * * rozdělit api na stream a command-response, navenek nebude rozdíl, ale uvnitř bude pro stremované api jiná struktura než pro command-response api
 * * zvýšit výkon knihovny tak, že se bude sdílet jeden buffer pro sestavení výstupního json příkazu
 */
#include "xtblib.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <throw.h>


#if defined(DEBUG_ENABLE)
#define __assert(...) debug(__VA_ARGS__)
#else
#define __assert()
#endif


typedef struct {
    SSL_CTX * ctx;
    SSL     * ssl;
    BIO     * bio;
}XTB_Api;


static bool xtb_api_connect(XTB_Api * self, const char * url) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    if((self->ctx = SSL_CTX_new(TLS_client_method())) == NULL) {
        return false;
	}

	if((self->bio = BIO_new_ssl_connect(self->ctx)) == NULL)  {
        return false;
	}

    BIO_set_conn_hostname(self->bio, url);
	BIO_set_conn_mode(self->bio, BIO_SOCK_NODELAY);

	if (BIO_do_connect(self->bio) <= 0) { 
		SSL_CTX_free(self->ctx); 
	    BIO_free_all(self->bio);

        return false;
  	}

	BIO_get_ssl(self->bio, &self->ssl);
	SSL_set_mode(self->ssl, SSL_MODE_AUTO_RETRY);

    return true;
}


static void xtb_api_close(XTB_Api * self) {
    SSL_CTX_free(self->ctx); 
    BIO_free_all(self->bio);
}


static char * xtb_api_receive(XTB_Api * self) {
    size_t length = 0;
    size_t rcv_len;
    char * resp = malloc(1024);

    while((rcv_len = SSL_read(self->ssl, resp + length, 1024)) > 0) {
        length += rcv_len;

        if((resp[length - 2] == '\n' && resp[length - 1] == '\n')) {
            break;
        } else {
            resp = realloc(resp, length + 1024);
        }
    }

    resp[length] = '\0';

    if(length == 0) {
        __assert("receive error\n");
        free(resp);
        return NULL;
    } else {
        return resp;
    }
}


static inline bool xtb_api_send(XTB_Api * self, const char * msg) {
    printf("%s\n", msg);

    if(SSL_write(self->ssl, msg, strlen(msg)) <= 0) {
        __assert("write command error\n");
        return false;
    } else {
        return true;
    }
}


//aktuálně zabírá knihovna 32KB a spustitelný soubor 77KB
static Json * xtb_api_transaction(XTB_Api * self, const char * cmd) {
    if(xtb_api_send(self, cmd) == false)
        return NULL;

    char * resp = xtb_api_receive(self);

    if(resp != NULL) {
        Json * json_resp = json_parse(resp);
        free(resp);
        return json_resp;
    } else {
        return NULL;
    }
}


static bool read_status(Json * json) {
    if(json == NULL) {
        return false;
    } else {
        Json * json_status = json_lookup(json, "status");

        if(json_is_type(json_status, JsonBool) == false || strcmp(json_status->string, "true") != 0) {
            return false;
        }

        return true;
    }
}


static Json * extract_return_data(Json * json) {
    /*
     * cut off the returnData 
     */
    Json * return_data = json_lookup(json, "returnData");

    json_object_set(json, "returnData", NULL);
    json_delete(json);

    return return_data;
}


#define CMD_BUFFER_SIZE 1024


struct XTB_StreamClient {
    XTB_Api api;
    char * stream_session_id;

    StreamClientCallback callback;
    void * param;

    char cmd_buffer[CMD_BUFFER_SIZE];

    XTB_StreamClient * prev;
    XTB_StreamClient * next;
};


struct XTB_Client {
    XTB_Api api;
    XTB_AccountMode mode;

    char * id;
    char * password;
    char * stream_session_id;

    char cmd_buffer[CMD_BUFFER_SIZE];

    XTB_StreamClient * stream_client;
};


static void xtb_client_logout(XTB_Client * self) {
    if(self->stream_session_id != NULL) {
        Json * json_logout = xtb_api_transaction(&self->api, "{\"command\": \"logout\"}");
     
        if(read_status(json_logout) == false)
            __assert("logout error\n");

        json_delete(json_logout);   

        free(self->stream_session_id);
        self->stream_session_id = NULL;
    }
}


static inline Json * xtb_client_send_login(XTB_Client * self, char * id, char * password) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"login\", \"arguments\": {\"userId\": \"%s\", \"password\": \"%s\"}}"
        , id, password);

    return xtb_api_transaction(&self->api, self->cmd_buffer); 
}


static bool xtb_client_login(XTB_Client * self, char * id, char * password) {
    /*
     * attempt to login is done only if the status is not logged
     * otherwise is only returned true result, because user is already logged
     */
    if(self->stream_session_id == NULL) {
        Json * result = xtb_client_send_login(self, id, password);

        if(read_status(result) == false) {
            json_delete(result);
            return false;
        }

        if(json_is_type(json_lookup(result, "streamSessionId"), JsonString) == true) 
            self->stream_session_id = strdup(json_lookup(result, "streamSessionId")->string);

        json_delete(result);    
    }

    return true;
}


#define XTB_API_SOCKET_PORT_DEMO "5124"
#define XTB_API_SOCKET_PORT_REAL "5112"

#define XTB_API_SOCKET_PORT_DEMO_STREAM "5125"
#define XTB_API_SOCKET_PORT_REAL_STREAM "5113"

#define XTB_API_ADDRESS "xapi.xtb.com"


#define XTB_API_MAIN_URL(mode)                                                          \
    ((mode) == XTB_AccountMode_Real ? XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_REAL:     \
                                    XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_DEMO)


#define XTB_API_STREAM_URL(mode)                                                                \
    ((mode) == XTB_AccountMode_Real ? XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_REAL_STREAM:      \
                                    XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_DEMO_STREAM)


XTB_Client * xtb_client_new(XTB_AccountMode mode, char * id, char * password) {
	XTB_Client * self = malloc(sizeof(XTB_Client));
    const char * url = XTB_API_MAIN_URL(mode);

	/*
	 * initializing of OpenSSL library
	 */
    if(xtb_api_connect(&self->api, url) == false) {
        xtb_client_delete(self);
        return NULL;
    }

	if(xtb_client_login(self, id, password) == false) {
		__assert("Login was not successfull\n");
	}

    self->mode     = mode;
    self->id       = strdup(id);
    self->password = strdup(password);

    return self;
}


bool xtb_client_logged(XTB_Client * self) {
    return self->stream_session_id != NULL; 
}


size_t xtb_client_stream_session_size(XTB_Client * self) {
    XTB_StreamClient * stream_client = self->stream_client;
    size_t size = 0;
    
    while(stream_client != NULL) {
        stream_client = stream_client->next;
        size++;
    }

    return size;
}


bool xtb_client_ping(XTB_Client * self) {
    Json * result = xtb_api_transaction(&self->api, "{\"command\": \"ping\"}");
    
    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return false;
    }

    json_delete(result);

    return true;
}


Json * xtb_client_get_all_symbols(XTB_Client * self) {
    Json * result = xtb_api_transaction(&self->api, "{\"command\": \"getAllSymbols\"}");
    
    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }

    return extract_return_data(result);
}


Json * xtb_client_get_calendar(XTB_Client * self) {
    Json * result = xtb_api_transaction(&self->api, "{\"command\": \"getCalendar\"}");

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }

    return extract_return_data(result);
}


static Json * xtb_client_send_get_chart_last_request(
        XTB_Client * self, char * symbol, XTB_Period period, time_t start) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getChartLastRequest\", \"arguments\":"
          "{\"info\": {\"period\": %d, \"start\": %ld, \"symbol\": \"%s\"}}}"
        , period
        , start
        , symbol);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_chart_last_request(XTB_Client * self, char * symbol, XTB_Period period, time_t start) {
    Json * result = xtb_client_send_get_chart_last_request(self, symbol, period / 60, start * 1000);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_chart_range_request(
        XTB_Client * self, char * symbol, XTB_Period period, time_t start, time_t end, int32_t tick) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getChartRangeRequest\", \"arguments\":"
          "{\"info\": {\"end\": %ld, \"period\": %d, \"start\": %ld, \"symbol\": \"%s\", \"ticks\": %d}}}"
        , end, period, start, symbol, tick);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_chart_range_request(
        XTB_Client * self, char * symbol, XTB_Period period, time_t start, time_t end, int32_t tick) {
    Json * json_chart = xtb_client_send_get_chart_range_request(self, symbol, period / 60, start * 1000, end * 1000, tick);

    if(read_status(json_chart) == false) {
        __assert("command failed\n");
        json_delete(json_chart);
        return NULL;
    }
    
    return extract_return_data(json_chart);
}


static Json * build_candle_record(Json * json_record, int digits) {
    /*
     * getting record data
     */
    Json * json_open  = json_lookup(json_record, "open");
    Json * json_close = json_lookup(json_record, "close");
    Json * json_high  = json_lookup(json_record, "high");
    Json * json_low   = json_lookup(json_record, "low");
    Json * json_ctm   = json_lookup(json_record, "ctm");
    Json * json_vol   = json_lookup(json_record, "vol");

    /*
     * check runtime error values
     */
    if(json_is_type(json_open, JsonFrac) == false
            || json_is_type(json_close, JsonFrac) == false
            || json_is_type(json_high, JsonFrac) == false
            || json_is_type(json_low, JsonFrac) == false
            || json_is_type(json_vol, JsonFrac) == false
            || json_is_type(json_ctm, JsonInteger) == false) {
        return NULL;
    }

    /*
     * assembly the candle data record
     */
    float open_val  = atof(json_open->string);
    float close_val = atof(json_close->string);
    float high_val  = atof(json_high->string);
    float low_val   = atof(json_low->string);
    long timestamp  = strtol(json_ctm->string, NULL, 10);
    float vol       = atof(json_vol->string);

    Json * candle_record = json_object_new(6);

    json_object_set_record(candle_record, 0, "timestamp", json_integer_new(timestamp));
    json_object_set_record(candle_record, 1, "open", json_frac_new(open_val / pow(10, digits)));
    json_object_set_record(candle_record, 2, "close", json_frac_new((open_val + close_val) / pow(10, digits)));
    json_object_set_record(candle_record, 3, "high", json_frac_new((open_val + high_val) / pow( 10, digits)));
    json_object_set_record(candle_record, 4, "low", json_frac_new((open_val + low_val) / pow(10, digits)));
    json_object_set_record(candle_record, 5, "vol", json_frac_new(vol));

    return candle_record;
}


Json * xtb_client_get_lastn_candle_history(XTB_Client * self, char * symbol, XTB_Period period, size_t number) {
    Json * chart        = NULL;
    Json * chart_record = NULL;
    size_t size         = 0;
    size_t sec_prior    = period * number;

    /*
     * reading chart history, the history size is rounded by time period, so it is 
     * sometimes nececary to load a litle bit more and then just process the
     * required number of records
     */
    while(size < number) {
        json_delete(chart);
        chart = xtb_client_get_chart_last_request(self, symbol, period, time(NULL) - sec_prior);

        if(json_is_type(chart, JsonObject) == false) {
            __assert("command failed\n");
            json_delete(chart);
            return NULL;
        }

        chart_record = json_lookup(chart, "rateInfos");

        /*
         * getting array of candles 
         */
        if(json_is_type(chart_record, JsonArray) == false) {
            __assert("response format error\n");
            json_delete(chart);
            return NULL;
        }

        size       = chart_record->array.size;
        sec_prior *= 2;
    } 

    int digits     = atoi(json_lookup(chart, "digits")->string);
    Json * candles = json_array_new(number);

    /*
     * processing output values
     */
    for(size_t i = 0; i < number; i++) {
        Json * candle_record = build_candle_record(chart_record->array.value[i], digits);

        if(candle_record == NULL) {
            __assert("error build candle record\n");
            json_delete(candles);
            json_delete(chart);
            return NULL;
        }

        candles->array.value[i] = candle_record;
    }

    json_delete(chart);
    
    return candles;
}


static Json * xtb_client_send_get_commision(XTB_Client * self, char * symbol, float volume) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getCommissionDef\", \"arguments\": {\"symbol\": \"%s\", \"volume\": %f}}"
        , symbol
        , volume);
    
    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_commision(XTB_Client * self, char * symbol, float volume) {
    Json * json_commision = xtb_client_send_get_commision(self, symbol, volume);

    if(read_status(json_commision) == false) {
        __assert("command failed\n");
        json_delete(json_commision);
        return NULL;
    }
    
    return extract_return_data(json_commision);
}


static Json * xtb_client_send_get_commision_def(XTB_Client * self, char * symbol, float volume) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getCommissionDef\", \"arguments\": {\"symbol\": \"%s\", \"volume\": %f}}"
        , symbol
        , volume);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_commision_def(XTB_Client * self, char * symbol, float volume) {
    Json * json_commision = xtb_client_send_get_commision_def(self, symbol, volume);

    if(read_status(json_commision) == false) {
        __assert("command failed\n");
        json_delete(json_commision);
        return NULL;
    }
    
    return extract_return_data(json_commision);
}


Json * xtb_client_get_margin_level(XTB_Client * self) {
    Json * json_margin = xtb_api_transaction(&self->api, "{\"command\": \"getMarginLevel\"}");

    if(read_status(json_margin) == false) {
        __assert("command failed\n");
        json_delete(json_margin);
        return NULL;
    }
    
    return extract_return_data(json_margin);
}


static Json * xtb_client_send_get_margin_trade(XTB_Client * self, char * symbol, float volume) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getMarginTrade\", \"arguments\": {\"symbol\": \"%s\", \"volume\":%f}}"
        , symbol
        , volume);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_margin_trade(XTB_Client * self, char * symbol, float volume) {
    Json * result = xtb_client_send_get_margin_trade(self, symbol, volume);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_profit_calculation(
        XTB_Client * self, char * symbol, XTB_TransMode mode, float open_price, float close_price, float volume) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getProfitCalculation\", \"arguments\": "
          "{\"closePrice\", %f, \"cmd\": %d, \"openPrice\": %f, \"symbol\": %s, \"volume\": %f}}"
        , close_price
        , mode
        , open_price
        , symbol
        , volume);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_profit_calculation(
        XTB_Client * self, float close_price, XTB_TransMode mode, float open_price, char * symbol, float volume) {
    Json * result = xtb_client_send_get_profit_calculation(self, symbol, mode, open_price, close_price, volume);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


Json * xtb_client_get_server_time(XTB_Client * self) {
    Json * result = xtb_api_transaction(&self->api, "{\"command\": \"getServerTime\"}");

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_symbol(XTB_Client * self, char * symbol) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getSymbol\", \"arguments\": {\"symbol\": \"%s\"}}"
        , symbol);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_symbol(XTB_Client * self, char * symbol) {
    Json * result = xtb_client_send_get_symbol(self, symbol);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_tick_prices(
        XTB_Client * self, size_t size, char ** symbols, int price_level, time_t timestamp) {
    size_t pos = snprintf(
                    self->cmd_buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"getTickPrices\", \"arguments\": "
                      "{\"level\": %d, \"symbols\": ["
                    , price_level);

    for(size_t i = 0; i < size; i++) {
        pos += snprintf(self->cmd_buffer + pos, CMD_BUFFER_SIZE, i == 0 ? "\"%s\"" : ", \"%s\"", symbols[i]);
    }

    snprintf(self->cmd_buffer + pos, CMD_BUFFER_SIZE, "], \"timestamp\": %ld}}", timestamp);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_tick_prices(
        XTB_Client * self, size_t size, char ** symbols, int price_level, time_t timestamp) {
    Json * result = xtb_client_send_get_tick_prices(self, size, symbols, price_level, timestamp * 1000);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_news(XTB_Client * self, time_t start, time_t end) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getNews\", \"arguments\": {\"end\": %ld, \"start\": %ld}}"
        , end, start);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_news(XTB_Client * self, time_t start, time_t end) {
    Json * result = xtb_client_send_get_news(self, start, end);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


Json * xtb_client_get_version(XTB_Client * self) {
    Json * result = xtb_api_transaction(&self->api, "{\"command\": \"getVersion\"}");

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_trades(XTB_Client * self, bool opened_only) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getTrades\", \"arguments\": {\"openedOnly\": %s}}"
        , opened_only ? "true" : "false");

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_trades(XTB_Client * self, bool opened_only) {
    Json * result = xtb_client_send_get_trades(self, opened_only);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_trade_records(XTB_Client * self, size_t size, char ** orders) {
    size_t pos = snprintf(
                    self->cmd_buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"getTradeRecords\", \"arguments\": {\"orders\": [");

    for(size_t i = 0; i < size; i++) {
        pos += snprintf(self->cmd_buffer + pos, CMD_BUFFER_SIZE, i == 0 ? "%s" : ", %s", orders[i]);
    }

    snprintf(self->cmd_buffer + pos, CMD_BUFFER_SIZE, "]}}");

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_trade_records(XTB_Client * self, size_t size, char ** orders) {
    Json * result = xtb_client_send_get_trade_records(self, size, orders);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_trade_history(XTB_Client * self, time_t start, time_t end) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getTradesHistory\", \"arguments\": {\"end\": %ld, \"start\": %ld}}"
        , end
        , start);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_trade_history(XTB_Client * self, time_t start, time_t end) {
    Json * result = xtb_client_send_get_trade_history(self, start * 1000, end * 1000);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_trade_transaction_status(XTB_Client * self, unsigned long order) {
    snprintf(
        self->cmd_buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"tradeTransactionStatus\", \"arguments\": {\"order\": %ld}}"
        , order);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_trade_transaction_status(XTB_Client * self, unsigned long order) {
    Json * result = xtb_client_send_trade_transaction_status(self, order);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


Json * xtb_client_get_user_data(XTB_Client * self) {
    Json * result = xtb_api_transaction(&self->api, "{\"command\": \"getCurrentUserData\"}");

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


static Json * xtb_client_send_get_trading_hours(XTB_Client * self, size_t size, char ** symbols) {
    size_t pos = snprintf(
                    self->cmd_buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"getTradingHours\", \"arguments\": {\"symbols\": [");

    for(size_t i = 0; i < size; i ++) {
        pos += snprintf(self->cmd_buffer + pos, CMD_BUFFER_SIZE, i == 0 ? "\"%s\"" : ", \"%s\"", symbols[i]);
    }

    snprintf(self->cmd_buffer + pos, CMD_BUFFER_SIZE, "]}}");

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_get_trading_hours(XTB_Client * self, size_t size, char ** symbols) {
    Json * result = xtb_client_send_get_trading_hours(self, size, symbols);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


Json * filter_market_day(Json * trading, uint8_t day) {
    for(size_t i = 0; i < trading->array.size; i++) {
        Json * json_day = json_lookup(trading->array.value[i], "day");
        
        if(json_is_type(json_day, JsonInteger) == true && (atoi(json_day->string) % 7) == day) {
            return trading->array.value[i];
        }
    }

    return NULL;
}


static Json * check_market_status(Json * market_day, struct tm * current_time) {
    if(market_day != NULL) {
        Json * json_fromT = json_lookup(market_day, "fromT");
        Json * json_toT = json_lookup(market_day, "toT");

        if(json_is_type(json_fromT, JsonInteger) == true 
                && json_is_type(json_toT, JsonInteger) == true) {
            long fromT = strtol(json_fromT->string, NULL, 10);
            long toT   = strtol(json_toT->string, NULL, 10);
            long today = (current_time->tm_hour * 3600) + (current_time->tm_min * 60) + current_time->tm_sec * 1000;

            if(today >= fromT && today <= toT) {
                return json_bool_new(true);
            } else {
                return json_bool_new(false);
            }
        } 
    }

    return json_bool_new(false);
}


static Json * build_market_status(Json * market_record, struct tm * current_time) {
    Json * trading = json_lookup(market_record, "trading");

    if(json_is_type(trading, JsonArray) == false) {
        return NULL;
    }

    Json * json_symbol = json_lookup(market_record, "symbol");

    if(json_is_type(json_symbol, JsonString) == false) {
        return NULL;
    }

    Json * market_day = filter_market_day(trading, current_time->tm_wday);
    Json * market_status = json_object_new(1);
    json_object_set_record(market_status, 0, json_symbol->string , check_market_status(market_day, current_time));

    return market_status;
}


Json * xtb_client_check_if_market_open(XTB_Client * self, size_t size, char ** symbols) {
    Json * result = xtb_client_get_trading_hours(self, size, symbols);

    if(json_is_type(result, JsonArray) == false) {
        json_delete(result);
        return NULL;
    }

    time_t now = time(NULL);
    struct tm * current_time = localtime(&now);
    Json * trading_status = json_array_new(result->array.size);

    for(size_t i = 0; i < result->array.size; i++) {
        Json * market_status = build_market_status(result->array.value[i], current_time);

        if(market_status == NULL) {
            __assert("response format error\n");
            json_delete(market_status);
            json_delete(result);
            return NULL;
        }

        trading_status->array.value[i] = market_status;
    }
    
    json_delete(result);
    return trading_status;
}


static Json * xtb_client_send_trade_transaction(
        XTB_Client * self, char * symbol, XTB_TransType type, XTB_TransMode mode, float price, float volume, int offset
        , float sl, float tp, time_t expiration, char * order, char * custom_comment) {
    size_t pos = snprintf(
                    self->cmd_buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"tradeTransaction\", \"arguments\": {\"tradeTransInfo\": {\"cmd\": %d", mode);

    if(custom_comment != NULL) {
        pos += snprintf(self->cmd_buffer+pos, CMD_BUFFER_SIZE, ", \"customComment\": \"%s\"", custom_comment);
    }

    if(expiration != 0) {
        pos += snprintf(self->cmd_buffer+pos, CMD_BUFFER_SIZE, ", \"expiration\": %ld", expiration);
    }

    if(offset != 0) {
        pos += snprintf(self->cmd_buffer+pos, CMD_BUFFER_SIZE, ", \"offset\": %d", offset);
    }

    if(order != NULL) {
        pos += snprintf(self->cmd_buffer+pos, CMD_BUFFER_SIZE, ", \"order\": %s", order);
    }

    pos += snprintf(self->cmd_buffer+pos, CMD_BUFFER_SIZE, ", \"price\": %f", price);

    snprintf(
        self->cmd_buffer+pos
        , CMD_BUFFER_SIZE
        , ", \"sl\": %f, \"symbol\": \"%s\", \"tp\": %f, \"type\": %d, \"volume\": %f}}}"
        , sl, symbol, tp, type, volume);

    return xtb_api_transaction(&self->api, self->cmd_buffer);
}


Json * xtb_client_trade_transaction(
        XTB_Client * self, char * symbol, char * custom_comment, XTB_TransMode mode, time_t expiration, int offset
        , char * order, float price, float tp, float sl, XTB_TransType type, float volume) {
    Json * result = xtb_client_send_trade_transaction(
                        self, symbol, type, mode, price, volume, offset, sl, tp, expiration, order, custom_comment);

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}   


Json * xtb_client_open_trade(XTB_Client * self, char * symbol, XTB_TransMode mode, float volume, float tp, float sl) {
    if(mode != XTB_TransMode_BUY && mode != XTB_TransMode_SELL) {
        __assert("mode can be buy or sell\n");
        return NULL;
    }

    Json * candle = xtb_client_get_symbol(self, symbol);

    if(candle == NULL 
            || (mode == XTB_TransMode_BUY && json_is_type(json_lookup(candle, "ask"), JsonFrac) == false)
            || (mode == XTB_TransMode_SELL && json_is_type(json_lookup(candle, "bid"), JsonFrac) == false)) {
        __assert("response format error\n");
        json_delete(candle);
        return NULL;
    }

    float price = atof((mode == XTB_TransMode_BUY) ? json_lookup(candle, "ask")->string : json_lookup(candle, "bid")->string);

    json_delete(candle);

    Json * result = 
        xtb_client_trade_transaction(self, symbol, NULL, mode, 0, 0, NULL, price, tp, sl, XTB_TransType_OPEN, volume);

    return result;
}


Json * xtb_client_close_trade(
        XTB_Client * self, char * symbol, char * order, XTB_TransMode mode, float price, float volume) {
    return xtb_client_trade_transaction(self, symbol, NULL, mode, 0, 0, order, price, 0, 0, XTB_TransType_CLOSE, volume);
}


Json * xtb_client_get_step_rules(XTB_Client * self) {
    Json * result = xtb_api_transaction(&self->api, "{\"command\": \"getStepRules\"}");

    if(read_status(result) == false) {
        __assert("command failed\n");
        json_delete(result);
        return NULL;
    }
    
    return extract_return_data(result);
}


void xtb_client_delete(XTB_Client * self) {
    if(self != NULL) {
        if(self->stream_session_id != NULL)
            xtb_client_logout(self);

        free(self->stream_session_id);

        if(self->id != NULL)
            free(self->id);

        if(self->password != NULL)
            free(self->password);

        /*
         * close and release stream clients
         */
        while(self->stream_client != NULL) {
            XTB_StreamClient * next = self->stream_client->next;

            xtb_api_close(&self->stream_client->api);

            free(self->stream_client);
            self->stream_client = next;
        }

        xtb_api_close(&self->api);

        free(self);
    }
}


static XTB_StreamClient * xtb_stream_client_last(XTB_StreamClient * self) {
    if(self != NULL) {
        while(self->next != NULL) {
            self = self->next;
        }
    }

    return self;  
}


XTB_StreamClient * xtb_stream_client_new(XTB_Client * self, StreamClientCallback *callback, void * param) {
    XTB_Api api = {0};

    const char * url = XTB_API_STREAM_URL(self->mode);

    if(xtb_api_connect(&api, url) == true) {
        XTB_StreamClient * stream_client = malloc(sizeof(XTB_StreamClient));

        /*
         * get last stream client from linked list
         */
        XTB_StreamClient * last = xtb_stream_client_last(self->stream_client);
        
        *stream_client = (XTB_StreamClient) {
            .api = api
            , .stream_session_id = self->stream_session_id
            , .callback = *callback
            , .param = param
            , .prev = last
            , .next = NULL
        };

        if(last != NULL) {
            last->next = stream_client;
        }

        return stream_client;
    } else {
        return NULL;
    }
}


void xtb_stream_client_process(XTB_StreamClient * self) {
    char * rcv = NULL;

    if((rcv = xtb_api_receive(&self->api)) != NULL) {
        Json * result = json_parse(rcv);

        if(result != NULL) {
            Json * command = json_lookup(result, "command");

            if(json_is_type(command, JsonString) == true) {
                if(strcmp(command->string, "balance") == 0 && self->callback.balance != NULL) {
                    self->callback.balance(self->param, json_lookup(result, "data"));
                } else if(strcmp(command->string, "candle") == 0 && self->callback.candle != NULL) {
                    self->callback.candle(self->param, json_lookup(result, "data"));
                } else if(strcmp(command->string, "keepAlive") == 0 && self->callback.keep_alive != NULL) {
                    self->callback.keep_alive(self->param, json_lookup(result, "data"));
                } else if(strcmp(command->string, "news") == 0 && self->callback.news != NULL) {
                    self->callback.news(self->param, json_lookup(result, "data"));
                } else if(strcmp(command->string, "profit") == 0 && self->callback.profit != NULL) {
                    self->callback.profit(self->param, json_lookup(result, "data"));
                } else if(strcmp(command->string, "tickPrices") == 0 && self->callback.tick_prices != NULL) {
                    self->callback.tick_prices(self->param, json_lookup(result, "data"));
                } else if(strcmp(command->string, "trade") == 0 && self->callback.trades != NULL) {
                    self->callback.trades(self->param, json_lookup(result, "data"));
                } else if(strcmp(command->string, "tradeStatus") == 0 && self->callback.trade_status != NULL) {
                    self->callback.trade_status(self->param, json_lookup(result, "data"));
                } else {
                    // TODO: treat unknown response error
                }
            }

            json_delete(result);
        } else {
            // TODO: treat json format error
        }
    } else {
        // TODO: treat empty response error
    }
}


static bool xtb_stream_client_subscribe_command(XTB_StreamClient * self, char * command) {
    snprintf(self->cmd_buffer, CMD_BUFFER_SIZE, "{\"command\": \"%s\", \"streamSessionId\": \"%s\"}", command, self->stream_session_id);
    return xtb_api_send(&self->api, self->cmd_buffer);
}


static inline bool xtb_stream_client_unsubscribe_command(XTB_StreamClient * self, char * msg) {
    return xtb_api_send(&self->api, msg);
}


bool xtb_stream_client_subscribe_news(XTB_StreamClient * self) {
    if(self->callback.news != NULL) {
        return xtb_stream_client_subscribe_command(self, "getNews");
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_news(XTB_StreamClient * self) {
    return xtb_stream_client_unsubscribe_command(self, "{\"command\": \"stopNews\"}");
}


bool xtb_stream_client_subscribe_balance(XTB_StreamClient * self) {
    if(self->callback.balance != NULL) {
        return xtb_stream_client_subscribe_command(self, "getBalance");
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_balance(XTB_StreamClient * self) {
    return xtb_stream_client_unsubscribe_command(self, "{\"command\": \"stopBalance\"}");
}


bool xtb_stream_client_subscribe_candles(XTB_StreamClient * self, char * symbol) {
    if(self->callback.candle != NULL) {
        snprintf(self->cmd_buffer, CMD_BUFFER_SIZE
                , "{\"command\": \"getCandles\", \"streamSessionId\": \"%s\", \"symbol\": \"%s\"}"
                , self->stream_session_id, symbol);

        return xtb_api_send(&self->api, self->cmd_buffer);
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_candles(XTB_StreamClient * self, char * symbol) {
    snprintf(self->cmd_buffer, CMD_BUFFER_SIZE, "{\"command\": \"stopCandles\", \"symbol\": \"%s\"}", symbol);
    return xtb_api_send(&self->api, self->cmd_buffer);
}


bool xtb_stream_client_subscribe_keep_alive(XTB_StreamClient * self) {
    if(self->callback.keep_alive != NULL) {
        return xtb_stream_client_subscribe_command(self, "getKeepAlive");
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_keep_alive(XTB_StreamClient * self) {
    return xtb_stream_client_unsubscribe_command(self, "{\"command\": \"stopKeepAlive\"}");
}


bool xtb_stream_client_subscribe_profits(XTB_StreamClient * self) {
    if(self->callback.profit != NULL) {
        return xtb_stream_client_subscribe_command(self, "getProfits");
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_profits(XTB_StreamClient * self) {
    return xtb_stream_client_unsubscribe_command(self, "{\"command\": \"stopProfits\"}");
}


bool xtb_stream_client_subscribe_tick_prices(XTB_StreamClient * self, char * symbol, time_t min_arrive_time, int max_level) {
    if(self->callback.tick_prices != NULL) {
        snprintf(self->cmd_buffer, CMD_BUFFER_SIZE
                , "{\"command\": \"getTickPrices\", \"streamSessionId\": \"%s\", \"symbol\": \"%s\", \"minArrivalTime\": %ld, \"maxLevel\": %d}"
                , self->stream_session_id, symbol, min_arrive_time, max_level);

        return xtb_api_send(&self->api, self->cmd_buffer);
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_tick_price(XTB_StreamClient * self, char * symbol) {
    snprintf(self->cmd_buffer, CMD_BUFFER_SIZE, "{\"command\": \"stopTickPrices\", \"symbol\": \"%s\"}", symbol);
    return xtb_api_send(&self->api, self->cmd_buffer);
}


bool xtb_stream_client_subscribe_trades(XTB_StreamClient * self) {
    if(self->callback.trades != NULL) {
        return xtb_stream_client_subscribe_command(self, "getTrades");
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_trades(XTB_StreamClient * self) {
    return xtb_stream_client_unsubscribe_command(self, "{\"command\": \"stopTrades\"}");
}


bool xtb_stream_client_subscribe_trade_status(XTB_StreamClient * self) {
    if(self->callback.trade_status != NULL) {   
        return xtb_stream_client_subscribe_command(self, "getTradeStatus");
    } else {
        return false;
    }
}


bool xtb_stream_client_unsubscribe_trade_status(XTB_StreamClient * self) {
    return xtb_stream_client_unsubscribe_command(self, "{\"command\": \"stopTradeStatus\"}");
}


bool xtb_stream_client_ping(XTB_StreamClient * self) {
    return xtb_stream_client_subscribe_command(self, "ping");
}


void xtb_scream_client_delete(XTB_StreamClient * self) {
    if(self != NULL) {
        /*
         * unconnect stream client from linked list
         */
        if(self->prev != NULL) {
            self->prev->next = self->next;
        }

        xtb_api_close(&self->api);
        free(self);
    }
}







