/**
 * @file xtblib.c
 * @author Petr Horáček
 * @brief 
 * TODO: 
 * * make better result checker with debuging for shorter and cleaner code
 * * make rest of api functions
 * * prepare streaming commands interface
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


#if defined(DEBUG_ENABLE)
#define __assert(msg) \
    printf("%s:%s:%d: %s\n", __FILE__, (char*) __func__, __LINE__, msg), fflush(stdout)
#else
#define __assert(msg) (void) msg
#endif


typedef struct Cmd Cmd;
typedef const char * (*CmdToString)(Cmd *);


#define CMD_BUFFER_SIZE 1024


struct Cmd {
    char buffer[1024];
    CmdToString to_string;
};


static const char * cmd_to_string(Cmd * self) {
    return self->to_string(self);
}


static char * __send(XTB_Client * self, const char * msg) {
    if(SSL_write(self->ssl, msg, strlen(msg)) <= 0) {
        __assert("receive error");
        return NULL;
    }

    size_t length = 0;
    size_t rcv_len;
    char * resp = malloc(1024);

    while((rcv_len = SSL_read(self->ssl, resp + length, 1024)) > 0) {
        length += rcv_len;

        if((resp[length - 2] == '\n' && resp[length - 1] == '\n'))
            break;
        else 
            resp = realloc(resp, length + 1024);
    }

    return realloc(resp, length);
}


static bool __resp_status(Json * json) {
    if(json == NULL) 
        return false;

    Json * json_status = json_lookup(json, "status");

    if(json_is_type(json_status, JsonBool) == false || strcmp(json_status->string, "true") != 0) {
        return false;
    }

    return true;
}


static Json * __transaction(XTB_Client * self, Cmd * cmd) {
    const char * str_cmd = cmd_to_string(cmd);
    char * resp = __send(self, str_cmd);

    if(resp == NULL) 
        return NULL;

    Json * json_resp = json_parse(resp);
    free(resp);

    return json_resp;
}


typedef struct {
    Cmd super;
}Cmd_Logout;


static const char * __cmd_logout_to_string(Cmd_Logout * self) {
    return strncpy(self->super.buffer, "{\"command\": \"logout\"}", CMD_BUFFER_SIZE);
}


#define Cmd_Logout (Cmd_Logout) {.super.to_string = (CmdToString) __cmd_logout_to_string}


static void
__logout(XTB_Client * self) {
    if(self->status == CLIENT_LOGGED) {
        Json * json_logout = __transaction(self, (Cmd*) &Cmd_Logout);
     
        if(__resp_status(json_logout) == false)
            __assert("logout error");

        json_delete(json_logout);   
        self->status = CLIENT_NOT_LOGGED;
    }
}


typedef struct {
    Cmd super;
    char * id;
    char * password;
}Cmd_Login;


static const char * __cmd_login_to_string(Cmd_Login * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"login\", \"arguments\": {\"userId\": \"%s\", \"password\": \"%s\"}}"
        , self->id, self->password);

    return self->super.buffer;
}


#define Cmd_Login(_id, _password) \
    (Cmd_Login) {.super.to_string = (CmdToString) __cmd_login_to_string, .id = (_id), .password = (_password)}


static bool __login(XTB_Client * self, char * id, char * password) {
    /*
     * attempt to login is done only if the status is not logged
     * otherwise is only returned true result, because user is already logged
     */
    if(self->status != CLIENT_LOGGED) {
        Json * result = __transaction(self, (Cmd*) &Cmd_Login(id, password));
        
        if(__resp_status(result) == false) {
            json_delete(result);
            return false;
        }

        json_delete(result);    

        if(self->stream_session_id != NULL) 
            free(self->stream_session_id);

        if(json_is_type(json_lookup(result, "streamSessionId"), JsonString) == true) 
            self->stream_session_id = strdup(json_lookup(result, "streamSessionId")->string);

        self->status = CLIENT_LOGGED;
    }

    return true;
}


static Json * __transaction_with_check(XTB_Client * self, Cmd * cmd) {
    if(self->status != CLIENT_LOGGED) {
        if(__login(self, self->id, self->password) == false)
            return NULL;
    }

    Json * json_resp = __transaction(self, cmd);

    if(json_resp == NULL)
        __logout(self);

    return json_resp;
}


#define XTB_API_SOCKET_PORT_DEMO "5124"
#define XTB_API_SOCKET_PORT_REAL "5112"


#define XTB_API_ADDRESS "xapi.xtb.com"


XTB_Client xtb_client_init(AccountMode mode, char * id, char * password) {
	XTB_Client self = {0};

	/*
	 * initializing of OpenSSL library
	 */
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
	
    if((self.ctx = SSL_CTX_new(TLS_client_method())) == NULL) 
        __assert("SSL init error");

	 if((self.bio = BIO_new_ssl_connect(self.ctx)) == NULL) 
        __assert("SSL error");

	BIO_get_ssl(self.bio, &self.ssl);
	SSL_set_mode(self.ssl, SSL_MODE_AUTO_RETRY);
	
	if(mode == AccountMode_Real)
		BIO_set_conn_hostname(self.bio, XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_REAL);
	else
		BIO_set_conn_hostname(self.bio, XTB_API_ADDRESS ":" XTB_API_SOCKET_PORT_DEMO);

	if (BIO_do_connect(self.bio) <= 0) { 
		SSL_CTX_free(self.ctx); 
	    BIO_free_all(self.bio);
        __assert("Server connection error");
  	}

	if(__login(&self, id, password) == false) 
		__assert("Login was not successfull");

    self.id       = strdup(id);
    self.password = strdup(password);

    return self;
}


bool xtb_client_refresh_connection(XTB_Client * self) {
    __logout(self);
    return __login(self, self->id, self->password);
}


typedef struct {
    Cmd super;
}Cmd_Ping;


static const char * __cmd_ping_to_string(Cmd_Ping * self) {
    return strncpy(self->super.buffer, "{\"command\": \"ping\"}", CMD_BUFFER_SIZE);
}


#define Cmd_Ping() \
    (Cmd_Ping) {.super.to_string = (CmdToString) __cmd_ping_to_string}


bool xtb_client_ping(XTB_Client * self) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_Ping());
    
    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return false;
    }

    json_delete(result);

    return true;
}


static Json * __extract_return_data(Json * json) {
    /*
     * cut off the returnData 
     */
    Json * return_data = json_lookup(json, "returnData");

    json_object_set(json, "returnData", NULL);
    json_delete(json);

    return return_data;
}


typedef struct {
  Cmd super;  
}Cmd_GetAllSymbols;


static const char * __cmd_get_all_symbols_to_string(Cmd_Ping * self) {
    return strncpy(self->super.buffer, "{\"command\": \"getAllSymbols\"}", CMD_BUFFER_SIZE);
}


#define Cmd_GetAllSymbols()\
    (Cmd_GetAllSymbols) {.super.to_string=(CmdToString) __cmd_get_all_symbols_to_string}


Json * xtb_client_get_all_symbols(XTB_Client * self) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetAllSymbols());
    
    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }

    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
}Cmd_GetCalendar;


static const char * __cmd_get_calendar_to_string(Cmd_Ping * self) {
    return strncpy(self->super.buffer, "{\"command\": \"getCalendar\"}", CMD_BUFFER_SIZE);
}


#define Cmd_GetCalendar() \
    (Cmd_GetCalendar) {.super.to_string = (CmdToString) __cmd_get_calendar_to_string}


Json * xtb_client_get_calendar(XTB_Client * self) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetCalendar());

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }

    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    char * symbol;
    Period period;
    time_t start;
}Cmd_GetChartLastRequest;


static const char * __cmd_get_chart_last_request_to_string(Cmd_GetChartLastRequest * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getChartLastRequest\", \"arguments\":"
          "{\"info\": {\"period\": %d, \"start\": %ld, \"symbol\": \"%s\"}}}"
        , self->period
        , self->start
        , self->symbol);

    return self->super.buffer;
}


#define Cmd_GetChartLastRequest(_symbol, _period, _start)                           \
    (Cmd_GetChartLastRequest) {                                                     \
        .super.to_string = (CmdToString) __cmd_get_chart_last_request_to_string     \
        , .symbol = (_symbol)                                                       \
        , .period = (_period)                                                       \
        , .start = (_start)}


Json * xtb_client_get_chart_last_request(XTB_Client * self, char * symbol, Period period, time_t start) {
    Json * result = __transaction_with_check(self, (Cmd *) &Cmd_GetChartLastRequest(symbol, period/60, start * 1000));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    char * symbol;
    Period period;
    time_t start;
    time_t end;
    int32_t tick;
}Cmd_GetChartRangeRequest;


static const char * __cmd_get_chart_range_request_to_string(Cmd_GetChartRangeRequest * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getChartRangeRequest\", \"arguments\":"
          "{\"info\": {\"end\": %ld, \"period\": %d, \"start\": %ld, \"symbol\": \"%s\", \"ticks\": %d}}}"
        , self->end
        , self->period
        , self->start
        , self->symbol
        , self->tick);

    return self->super.buffer;
}


#define Cmd_GetChartRangeRequest(_symbol, _period, _start, _end, _tick)                 \
    (Cmd_GetChartRangeRequest) {                                                        \
        .super.to_string = (CmdToString) __cmd_get_chart_range_request_to_string        \
        , .symbol = (_symbol)                                                           \
        , .period = (_period)                                                           \
        , .start = (_start)                                                             \
        , .end = (_end)                                                                 \
        , .tick = (_tick)}


Json * xtb_client_get_chart_range_request(
        XTB_Client * self, char * symbol, Period period, time_t start, time_t end, int32_t tick) {
    Json * json_chart = __transaction_with_check(self, (Cmd*) &Cmd_GetChartRangeRequest(symbol, period / 60, start * 1000, end * 1000, tick));

    if(__resp_status(json_chart) == false) {
        __assert("command failed");
        json_delete(json_chart);
        return NULL;
    }
    
    return __extract_return_data(json_chart);
}


static Json *__build_candle_record(Json * json_record, int digits) {
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


Json * xtb_client_get_lastn_candle_history(XTB_Client * self, char * symbol, Period period, size_t number) {
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
            __assert("command failed");
            json_delete(chart);
            return NULL;
        }

        chart_record = json_lookup(chart, "rateInfos");

        /*
         * getting array of candles 
         */
        if(json_is_type(chart_record, JsonArray) == false) {
            __assert("response format error");
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
        Json * candle_record = __build_candle_record(chart_record->array.value[i], digits);

        if(candle_record == NULL) {
            __assert("error build candle record");
            json_delete(candles);
            json_delete(chart);
            return NULL;
        }

        candles->array.value[i] = candle_record;
    }

    json_delete(chart);
    
    return candles;
}

typedef struct {
    Cmd super;
    char * symbol;
    float volume;
}Cmd_GetCommision;


static const char * __cmd_get_commision(Cmd_GetCommision * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getCommissionDef\", \"arguments\": {\"symbol\": \"%s\", \"volume\": %f}}"
        , self->symbol
        , self->volume);
    
    return self->super.buffer;
}


#define Cmd_GetCommision(_symbol, _volume) \
    (Cmd_GetCommision) {.super.to_string=(CmdToString) __cmd_get_commision, .symbol = (_symbol), .volume = (_volume)}


Json * xtb_client_get_commision(XTB_Client * self, char * symbol, float volume) {
    Json * json_commision = __transaction_with_check(self, (Cmd*) &Cmd_GetCommision(symbol, volume));

    if(__resp_status(json_commision) == false) {
        __assert("command failed");
        json_delete(json_commision);
        return NULL;
    }
    
    return __extract_return_data(json_commision);
}


typedef struct {
    Cmd super;
    char * symbol;
    float volume;
}Cmd_GetCommisionDef;


static const char * __cmd_get_commision_def(Cmd_GetCommisionDef * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getCommissionDef\", \"arguments\": {\"symbol\": \"%s\", \"volume\": %f}}"
        , self->symbol
        , self->volume);

    return self->super.buffer;
}


#define Cmd_GetCommisionDef(_symbol, _volume)                       \
    (Cmd_GetCommisionDef) {                                         \
        .super.to_string = (CmdToString) __cmd_get_commision_def    \
        , .symbol = _symbol                                         \
        , .volume = _volume}


Json * xtb_client_get_commision_def(XTB_Client * self, char * symbol, float volume) {
    Json * json_commision = __transaction_with_check(self, (Cmd*) &Cmd_GetCommisionDef(symbol, volume));

    if(__resp_status(json_commision) == false) {
        __assert("command failed");
        json_delete(json_commision);
        return NULL;
    }
    
    return __extract_return_data(json_commision);
}


typedef struct {
    Cmd super;
}Cmd_GetMarginLevel;


static const char * __cmd_get_margin_level_to_string(Cmd_GetMarginLevel * self) {
    return strncpy(self->super.buffer, "{\"command\": \"getMarginLevel\"}", CMD_BUFFER_SIZE);
}


#define Cmd_GetMarginLevel (Cmd_GetMarginLevel) {.super.to_string = (CmdToString) __cmd_get_margin_level_to_string}


Json * xtb_client_get_margin_level(XTB_Client * self) {
    Json * json_margin = __transaction_with_check(self, (Cmd*) &Cmd_GetMarginLevel);

    if(__resp_status(json_margin) == false) {
        __assert("command failed");
        json_delete(json_margin);
        return NULL;
    }
    
    return __extract_return_data(json_margin);
}


typedef struct {
    Cmd super;
    char * symbol;
    float volume;
}Cmd_GetMarginTrade;


static const char * __cmd_get_margin_trade_to_string(Cmd_GetMarginTrade * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getMarginTrade\", \"arguments\": {\"symbol\": \"%s\", \"volume\":%f}}"
        , self->symbol
        , self->volume);

    return self->super.buffer;
}


#define Cmd_GetMarginTrade(_symbol, _volume)                                    \
    (Cmd_GetMarginTrade) {                                                      \
        .super.to_string = (CmdToString) __cmd_get_margin_trade_to_string       \
        , .symbol = (_symbol)                                                   \
        , .volume = (_volume)}


Json * xtb_client_get_margin_trade(XTB_Client * self, char * symbol, float volume) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetMarginTrade(symbol, volume));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    float close_price;
    TransMode mode;
    float open_price;
    char * symbol;
    float volume;
} Cmd_GetProfitCalculation;


static const char * __cmd_get_profit_calculation_to_string(Cmd_GetProfitCalculation * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getProfitCalculation\", \"arguments\": "
          "{\"closePrice\", %f, \"cmd\": %d, \"openPrice\": %f, \"symbol\": %s, \"volume\": %f}}"
        , self->close_price
        , self->mode
        , self->open_price
        , self->symbol
        , self->volume);

    return self->super.buffer;
}


#define Cmd_GetProfitCalculation(_close_price, _mode, _open_price, _symbol, _volume)    \
    (Cmd_GetProfitCalculation) {                                                        \
        .super.to_string = (CmdToString) __cmd_get_profit_calculation_to_string         \
        , .close_price = (_close_price)                                                 \
        , .mode = (_mode)                                                               \
        , .open_price = (_open_price)                                                   \
        , .symbol = (_symbol)                                                           \
        , .volume = (_volume)}


Json * xtb_client_get_profit_calculation(
        XTB_Client * self, float close_price, TransMode mode, float open_price, char * symbol, float volume) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetProfitCalculation(close_price, mode, open_price, symbol, volume));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
}Cmd_GetServerTime;


static const char * __cmd_get_server_time_to_string(Cmd_GetServerTime * self) {
    return strncpy(self->super.buffer, "{\"command\": \"getServerTime\"}", CMD_BUFFER_SIZE);
}


#define Cmd_GetServerTime() \
    (Cmd_GetServerTime) {.super.to_string = (CmdToString) __cmd_get_server_time_to_string}


Json * xtb_client_get_server_time(XTB_Client * self) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetServerTime());

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    char * symbol;
}Cmd_GetSymbol;


static const char * __cmd_get_symbol_to_string(Cmd_GetSymbol * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getSymbol\", \"arguments\": {\"symbol\": \"%s\"}}"
        , self->symbol);

    return self->super.buffer;
}


#define Cmd_GetSymbol(_symbol) \
    (Cmd_GetSymbol) {.super.to_string = (CmdToString) __cmd_get_symbol_to_string, .symbol = _symbol}


Json * xtb_client_get_symbol(XTB_Client * self, char * symbol) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetSymbol(symbol));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    size_t size;
    char ** symbols;
    int price_level;
    time_t timestamp;
}Cmd_GetTickPrices;


static const char * __cmd_get_tick_prices_to_string(Cmd_GetTickPrices * self) {
    size_t pos = snprintf(
                    self->super.buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"getTickPrices\", \"arguments\": "
                      "{\"level\": %d, \"symbols\": ["
                    , self->price_level);

    for(size_t i = 0; i < self->size; i++) 
        pos += snprintf(self->super.buffer + pos, CMD_BUFFER_SIZE, i == 0 ? "\"%s\"" : ", \"%s\"", self->symbols[i]);
    
    snprintf(self->super.buffer + pos, CMD_BUFFER_SIZE, "], \"timestamp\": %ld}}", self->timestamp);

    return self->super.buffer;
}


#define Cmd_GetTickPrices(_size, _symbols, _price_level, _timestamp)            \
    (Cmd_GetTickPrices) {                                                       \
        .super.to_string = (CmdToString) __cmd_get_tick_prices_to_string        \
            , .size = (_size)                                                   \
            , .symbols = (_symbols)                                             \
            , .price_level = (_price_level)                                     \
            , .timestamp = (_timestamp)} 


Json * xtb_client_get_tick_prices(
        XTB_Client * self, size_t size, char ** symbols, int price_level, time_t timestamp) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetTickPrices(size, symbols, price_level, timestamp * 1000));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    time_t start;
    time_t end;
}Cmd_GetNews;


static const char * __cmd_get_news_to_string(Cmd_GetNews * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getNews\", \"arguments\": {\"end\": %ld, \"start\": %ld}}"
        , self->end, self->start);

    return self->super.buffer;
}


#define Cmd_GetNews(_start, _end) \
    (Cmd_GetNews) {.super.to_string = (CmdToString) __cmd_get_news_to_string, .start = (_start), .end = (_end)}


Json * xtb_client_get_news(XTB_Client * self, time_t start, time_t end) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetNews(start, end));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
} Cmd_GetVersion;


static const char * __cmd_get_version_to_string(Cmd_GetVersion * self) {
    return strncpy(self->super.buffer, "{\"command\": \"getVersion\"}", CMD_BUFFER_SIZE);
}


#define Cmd_GetVersion \
    (Cmd_GetVersion) {.super.to_string=(CmdToString) __cmd_get_version_to_string}


Json * xtb_client_get_version(XTB_Client * self) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetVersion);

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    bool opened_only;
}Cmd_GetTrades;


static const char * __cmd_get_trades_to_string(Cmd_GetTrades * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getTrades\", \"arguments\": {\"openedOnly\": %s}}"
        , self->opened_only ? "true" : "false");

    return self->super.buffer;
}


#define Cmd_GetTrades(_opened_only) \
    (Cmd_GetTrades) {.super.to_string = (CmdToString) __cmd_get_trades_to_string, .opened_only = (_opened_only)}


Json * xtb_client_get_trades(XTB_Client * self, bool opened_only) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetTrades(opened_only));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    size_t size;
    char ** orders;
}Cmd_GetTradeRecords;


static const char * __cmd_get_trade_records_to_string(Cmd_GetTradeRecords * self) {
    size_t pos = snprintf(
                    self->super.buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"getTradeRecords\", \"arguments\": {\"orders\": [");

    for(size_t i = 0; i < self->size; i++) 
        pos += snprintf(self->super.buffer + pos, CMD_BUFFER_SIZE, i == 0 ? "%s" : ", %s", self->orders[i]);
    
    snprintf(self->super.buffer + pos, CMD_BUFFER_SIZE, "]}}");

    return self->super.buffer;
}


#define Cmd_GetTradeRecords(_size, _orders)                                     \
    (Cmd_GetTradeRecords) {                                                     \
        .super.to_string = (CmdToString) __cmd_get_trade_records_to_string      \
        , .size = _size                                                         \
        , .orders = _orders}


Json * xtb_client_get_trade_records(XTB_Client * self, size_t size, char ** orders) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetTradeRecords(size, orders));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    time_t start;
    time_t end;
}Cmd_GetTradeHistory;


static const char * __cmd_get_trade_history_to_string(Cmd_GetTradeHistory * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"getTradesHistory\", \"arguments\": {\"end\": %ld, \"start\": %ld}}"
        , self->end
        , self->start);

    return self->super.buffer;
}


#define Cmd_GetTradeHistory(_start, _end)                                       \
    (Cmd_GetTradeHistory) {                                                     \
        .super.to_string = (CmdToString) __cmd_get_trade_history_to_string      \
        , .start = _start                                                       \
        , .end = _end}


Json * xtb_client_get_trade_history(XTB_Client * self, time_t start, time_t end) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetTradeHistory(start * 1000, end * 1000));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    unsigned long order;
}Cmd_TradeTransactionStatus;


static const char * __cmd_trade_transaction_status_to_string(Cmd_TradeTransactionStatus * self) {
    snprintf(
        self->super.buffer
        , CMD_BUFFER_SIZE
        , "{\"command\": \"tradeTransactionStatus\", \"arguments\": {\"order\": %ld}}"
        , self->order);

    return self->super.buffer;
}


#define Cmd_TradeTransactionStatus(_order)                                              \
    (Cmd_TradeTransactionStatus){                                                       \
        .super.to_string = (CmdToString) __cmd_trade_transaction_status_to_string       \
        , .order = _order}


Json * xtb_client_trade_transaction_status(XTB_Client * self, unsigned long order) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_TradeTransactionStatus(order));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
}Cmd_GetUserData;


static const char * __cmd_get_user_data(Cmd_GetUserData * self) {
    return strncpy(self->super.buffer, "{\"command\": \"getCurrentUserData\"}", CMD_BUFFER_SIZE);
}


#define Cmd_GetUserData() (Cmd_GetUserData){.super.to_string = (CmdToString) __cmd_get_user_data}


Json * xtb_client_get_user_data(XTB_Client * self) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetUserData());

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


typedef struct {
    Cmd super;
    size_t size;
    char ** symbols;
}Cmd_GetTradingHours;


static const char * __cmd_get_trading_hours_to_string(Cmd_GetTradingHours * self) {
    size_t pos = snprintf(
                    self->super.buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"getTradingHours\", \"arguments\": {\"symbols\": [");

    for(size_t i = 0; i < self->size; i ++) 
        pos += snprintf(self->super.buffer + pos, CMD_BUFFER_SIZE, i == 0 ? "\"%s\"" : ", \"%s\"", self->symbols[i]);

    snprintf(self->super.buffer + pos, CMD_BUFFER_SIZE, "]}}");

    return self->super.buffer;
}


#define Cmd_GetTradingHours(_size, _symbols)                                    \
    (Cmd_GetTradingHours) {                                                     \
        .super.to_string = (CmdToString) __cmd_get_trading_hours_to_string      \
            , .size = _size                                                     \
            , .symbols = _symbols}


Json * xtb_client_get_trading_hours(XTB_Client * self, size_t size, char ** symbols) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetTradingHours(size, symbols));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


Json * __filter_market_day(Json * trading, uint8_t day) {
    for(size_t i = 0; i < trading->array.size; i++) {
        Json * json_day = json_lookup(trading->array.value[i], "day");
        
        if(json_is_type(json_day, JsonInteger) == true && (atoi(json_day->string) % 7) == day) 
            return trading->array.value[i];
    }

    return NULL;
}


static Json * __check_market_status(Json * market_day, struct tm * current_time) {
    if(market_day != NULL) {
        Json * json_fromT = json_lookup(market_day, "fromT");
        Json * json_toT = json_lookup(market_day, "toT");

        if(json_is_type(json_fromT, JsonInteger) == true 
                && json_is_type(json_toT, JsonInteger) == true) {
            long fromT = strtol(json_fromT->string, NULL, 10);
            long toT   = strtol(json_toT->string, NULL, 10);
            long today = (current_time->tm_hour * 3600) + (current_time->tm_min * 60) + current_time->tm_sec * 1000;

            if(today >= fromT && today <= toT) 
                return json_bool_new(true);
            else
                return json_bool_new(false);
        } 
    }

    return json_bool_new(false);
}


static Json * __build_market_status(Json * market_record, struct tm * current_time) {
    Json * trading = json_lookup(market_record, "trading");

    if(json_is_type(trading, JsonArray) == false)
        return NULL;

    Json * json_symbol = json_lookup(market_record, "symbol");

    if(json_is_type(json_symbol, JsonString) == false)
        return NULL;

    Json * market_day = __filter_market_day(trading, current_time->tm_wday);
    Json * market_status = json_object_new(1);
    json_object_set_record(market_status, 0, json_symbol->string , __check_market_status(market_day, current_time));

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
        Json * market_status = __build_market_status(result->array.value[i], current_time);

        if(market_status == NULL) {
            __assert("response format error");
            json_delete(market_status);
            json_delete(result);
            return NULL;
        }

        trading_status->array.value[i] = market_status;
    }
    
    json_delete(result);
    return trading_status;
}


typedef struct {
    Cmd super;
    char * custom_comment;
    TransMode mode;
    time_t expiration;
    int offset;
    char * order;
    float price;
    float sl;
    char * symbol;
    float tp;
    TransType type;
    float volume;
}Cmd_TradeTransaction;


static const char * __cmd_trade_transaction_to_string(Cmd_TradeTransaction * self) {
    size_t pos = snprintf(
                    self->super.buffer
                    , CMD_BUFFER_SIZE
                    , "{\"command\": \"tradeTransaction\", \"arguments\": {\"tradeTransInfo\": {\"cmd\": %d", self->mode);

    if(self->custom_comment != NULL)
        pos += snprintf(self->super.buffer+pos, CMD_BUFFER_SIZE, ", \"customComment\": \"%s\"", self->custom_comment == NULL ? "" : self->custom_comment);

    if(self->expiration != 0)
        pos += snprintf(self->super.buffer+pos, CMD_BUFFER_SIZE, ", \"expiration\": %ld", self->expiration);

    if(self->offset != 0)
        pos += snprintf(self->super.buffer+pos, CMD_BUFFER_SIZE, ", \"offset\": %d", self->offset);

    if(self->order != NULL)
        pos += snprintf(self->super.buffer+pos, CMD_BUFFER_SIZE, ", \"order\": %s", self->order);

    pos += snprintf(self->super.buffer+pos, CMD_BUFFER_SIZE, ", \"price\": %f", self->price);

    snprintf(
        self->super.buffer+pos
        , CMD_BUFFER_SIZE
        , ", \"sl\": %f, \"symbol\": \"%s\", \"tp\": %f, \"type\": %d, \"volume\": %f}}}"
        , self->sl
        , self->symbol
        , self->tp
        , self->type
        , self->volume);

    return self->super.buffer;
}


#define Cmd_TradeTransaction(                                               \
         _custom_comment                                                    \
        , _mode                                                             \
        , _expiration                                                       \
        , _offset                                                           \
        , _order                                                            \
        , _price                                                            \
        , _sl                                                               \
        , _symbol                                                           \
        , _tp                                                               \
        , _type                                                             \
        , _volume)                                                          \
    (Cmd_TradeTransaction) {                                                \
        .super.to_string=(CmdToString) __cmd_trade_transaction_to_string    \
        , .custom_comment = _custom_comment                                 \
        , .mode = _mode                                                     \
        , .expiration = _expiration                                         \
        , .offset = _offset                                                 \
        , .order = _order                                                   \
        , .price = _price                                                   \
        , .sl = _sl                                                         \
        , .symbol = _symbol                                                 \
        , .tp = _tp                                                         \
        , .type = _type                                                     \
        , .volume = _volume}


Json * xtb_client_trade_transaction(
        XTB_Client * self
        , char * symbol
        , char * custom_comment
        , TransMode mode
        , time_t expiration
        , int offset
        , char * order
        , float price
        , float tp
        , float sl
        , TransType type
        , float volume) {
    Json * result = __transaction_with_check(
                        self
                        , (Cmd*) &Cmd_TradeTransaction(custom_comment, mode, expiration, offset, order, price, sl, symbol, tp, type, volume));

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}   


Json * xtb_client_open_trade(XTB_Client * self, char * symbol, TransMode mode, float volume, float tp, float sl) {
    if(mode != TransMode_BUY && mode != TransMode_SELL) {
        __assert("mode can be buy or sell");
        return NULL;
    }

    Json * candle = xtb_client_get_symbol(self, symbol);

    if(candle == NULL 
        || (mode == TransMode_BUY && json_is_type(json_lookup(candle, "ask"), JsonFrac) == false)
        || (mode == TransMode_SELL && json_is_type(json_lookup(candle, "bid"), JsonFrac) == false)) {
        __assert("response format error");
        json_delete(candle);
        return NULL;
    }

    float price = atof((mode == TransMode_BUY) ? json_lookup(candle, "ask")->string : json_lookup(candle, "bid")->string);

    json_delete(candle);

    Json * result = 
        xtb_client_trade_transaction(
            self
            , symbol
            , NULL
            , mode
            , 0, 0 
            , NULL
            , price
            , tp
            , sl
            , TransType_OPEN
            , volume);

    return result;
}


Json * xtb_client_close_trade(XTB_Client * self, char * symbol, char * order, TransMode mode, float price, float volume) {
    return xtb_client_trade_transaction(self, symbol, NULL, mode, 0, 0, order, price, 0, 0, TransType_CLOSE, volume);
}


typedef struct {
    Cmd super;
}Cmd_GetStepRules;


static const char * __cmd_get_step_rules_to_string(Cmd_GetStepRules * self) {
    return strncpy(self->super.buffer, "{\"command\": \"getStepRules\"}", CMD_BUFFER_SIZE);
} 


#define Cmd_GetStepRules()\
    (Cmd_GetStepRules) {.super.to_string=(CmdToString) __cmd_get_step_rules_to_string}


Json * xtb_client_get_step_rules(XTB_Client * self) {
    Json * result = __transaction_with_check(self, (Cmd*) &Cmd_GetStepRules());

    if(__resp_status(result) == false) {
        __assert("command failed");
        json_delete(result);
        return NULL;
    }
    
    return __extract_return_data(result);
}


void xtb_client_delete(XTB_Client * self) {
    if(self != NULL) {
        if(self->status == CLIENT_LOGGED)
            __logout(self);

        if(self->stream_session_id != NULL)
            free(self->stream_session_id);

        if(self->id != NULL)
            free(self->id);

        if(self->password != NULL)
            free(self->password);

        SSL_CTX_free(self->ctx); 
        BIO_free_all(self->bio);
    }
}


    
