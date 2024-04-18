#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <throw.h>
#include <vector.h>


#include "../src/xtblib.h"


typedef struct {
    size_t size;

    struct tuple {
        float open;
        float close;
    } array[];
} MarketData;


void
__read_candle_history(XTB_Client * client) {
    Json * history = xtb_client_get_lastn_candle_history(client, "BITCOIN", XTB_PERIOD_M1, 1000);

    if(history != NULL) {
        json_show(history, stdout);
        json_delete(history);
    }
}   


void
__read_candles(XTB_Client * client) {
    const int num = XTB_PERIOD_M5 * 1000;
    Json * candles    = xtb_client_get_chart_last_request(client, "BITCOIN", XTB_PERIOD_M5, time(NULL)-num);

    if(candles != NULL) {
        json_show(candles, stdout);
        json_delete(candles);
    }
    else {
        printf("cant load pricess\n");
    }
}


void __read_calendar(XTB_Client * client) {
    Json * calendar = xtb_client_get_calendar(client);

    if(calendar != NULL) {
        json_show(calendar, stdout);
        json_delete(calendar);
    } else {
        printf("cant read calendar\n");
    }
}


void __read_range_chart(XTB_Client * client) {
    time_t start = time(NULL) - XTB_PERIOD_M5 * 5;
    time_t end = time(NULL);
    Json *chart = xtb_client_get_chart_range_request(client, "BITCOIN", XTB_PERIOD_M5, start, end, 2);

    if(chart != NULL) {
        if(json_is_type(json_lookup(chart, "rateInfos"), JsonArray) == true)
            printf("size: %ld\n", json_lookup(chart, "rateInfos")->array.size);
        else
            printf("it is not array\n");

        json_show(chart, stdout);
        json_delete(chart);
    } else {
        printf("Chart cant be load\n");
    }
}


void __get_commision(XTB_Client * client) {
    Json *commision = xtb_client_get_commision(client, "BITCOIN", 1);

    if(commision != NULL) {
        json_show(commision, stdout);
        json_delete(commision);
    } else {
        printf("cant get comision\n");
    }

}


void __get_margin_level(XTB_Client * client) {
    Json * margin = xtb_client_get_margin_level(client);

    if(margin != NULL) {
        json_show(margin, stdout);
        json_delete(margin);
    } else {
        printf("Cant get margin level\n");
    }
}


void __get_margin_trade(XTB_Client * client) {
    Json * margin = xtb_client_get_margin_trade(client, "BITCOIN", 1);

    if(margin != NULL) {
        json_show(margin, stdout);
        json_delete(margin);
    } else {
        printf("Cant get margin trade\n");
    }
}

void __get_server_time(XTB_Client * client) {
    Json * time = xtb_client_get_server_time(client);

    if(time != NULL) {
        json_show(time, stdout);
        json_delete(time);
    } else {
        printf("Cant get server time\n");
    }
}


void __get_symbol(XTB_Client * client) {
    Json * symbol = xtb_client_get_symbol(client, "BITCOIN");

    if(symbol != NULL) {
        json_show(symbol, stdout);
        json_delete(symbol);
    } else {
        printf("Cant load symbol\n");
    }
}


void __get_news(XTB_Client * client) {
    Json * news = xtb_client_get_news(client, time(NULL)-(XTB_PERIOD_D1*2), 0);

    if(news != NULL) {
        json_show(news, stdout);
        json_delete(news);
    } else {
        printf("Cant load news\n");
    }
}


void __get_version(XTB_Client * client) {
    Json * version = xtb_client_get_version(client);

    if(version != NULL) {
        json_show(version, stdout);
        json_delete(version);
    } else {
        printf("Cant get version\n");
    }
}   


void __get_trades(XTB_Client * client) {
    Json * trades = xtb_client_get_trades(client, true);

    if(trades != NULL) {
        json_show(trades, stdout);
        json_delete(trades);
    } else {
        printf("Trades cant be loaded\n");
    }
}


void __get_tick_prices(XTB_Client * client) {
    Json * prices = xtb_client_get_tick_prices(client, 1, (char * []) {"BITCOIN"}, 0, time(NULL)-(XTB_PERIOD_M5*60));

    if(prices != NULL) {
        json_show(prices, stdout);
        printf("%ld\n", json_lookup(prices, "quotations")->array.size);
        json_delete(prices);
    } else {
        printf("Prices cant be loaded\n");
    }
}


void __get_trade_records(XTB_Client * client) {
    Json * prices = xtb_client_get_trade_records(client, 1, (char * []) {"594037518"});

    if(prices != NULL) {
        json_show(prices, stdout);
        json_delete(prices);
    } else {
        printf("Prices cant be loaded\n");
    }
}


void __get_trade_history(XTB_Client * client) {
    Json * history = xtb_client_get_trade_history(client, time(NULL) - (XTB_PERIOD_D1 * 10), 0);

    if(history != NULL) {
        //json_show(history, stdout);
        for(size_t i = 0; i < history->array.size; i++) {
            printf(
                "%s | %s: %s\n"
                , json_lookup(history->array.value[i], "order")->string
                , json_lookup(history->array.value[i], "symbol")->string
                , json_lookup(history->array.value[i], "profit")->string);
        }

        json_delete(history);
    } else {
        printf("history cant be loaded\n");
    }
}


void __trade_transaction_status(XTB_Client * client) {
    Json * status = xtb_client_trade_transaction_status(client, 594037518);

    if(status != NULL) {
        json_show(status, stdout);
        json_delete(status);
    } else {
        printf("Cant load status\n");
    }
}


void __get_user_data(XTB_Client * client) {
    Json * status = xtb_client_get_user_data(client);

    if(status != NULL) {
        json_show(status, stdout);
        json_delete(status);
    } else {
        printf("Cant get user data\n");
    }
}


void __get_tradin_hours(XTB_Client * client) {
    Json * hours = xtb_client_get_trading_hours(client, 1, (char * []) {"BITCOIN"});

    if(hours != NULL) {
        json_show(hours, stdout);
        json_delete(hours);
    } else {
        printf("Hours cant be loaded\n");
    }
}


void __check_if_market_open(XTB_Client * client) {
    Json * status = xtb_client_check_if_market_open(client, 1, (char * []) {"BITCOIN"});

    if(status != NULL) {
        json_show(status, stdout);
        json_delete(status);
    } else {
        printf("Cant get market status\n");
    }
}


void __find_symbol(XTB_Client * client, char * name) {
    Json * symbols = xtb_client_get_all_symbols(client);

    if(symbols != NULL) {
        for(size_t i = 0; i < symbols->array.size; i++) {
            if(strncmp(json_lookup(symbols->array.value[i], "symbol")->string, name, strlen(name)) == 0) 
                json_show(symbols->array.value[i], stdout);
        }

        json_delete(symbols);
    } else {
        printf("cant load symbols\n");
    }
}


void __trade_transaction(XTB_Client * client) {
    Json * symbol = xtb_client_get_symbol(client, "BITCOIN");
    //Json * server_time = xtb_client_get_server_time(client);
    Json * result = xtb_client_trade_transaction(client, "BITCOIN", NULL, XTB_TransMode_BUY, 0, 0, "595490628", 51794.80, 0, 0, XTB_TransType_CLOSE, 0.01);

    if(result != NULL) {
        
        json_show(result, stdout);
        json_delete(result);
        json_delete(symbol);
    } else {
        printf("cant execute command\n");
    }
}


void __get_step_rules(XTB_Client * client) {
    Json * step_rules = xtb_client_get_step_rules(client);

    if(step_rules != NULL) {
        json_show(step_rules, stdout);
        json_delete(step_rules);
    } else {
        printf("cant get step rules\n");
    }
}


void __get_commision_def(XTB_Client * client) {
    Json * commision = xtb_client_get_commision_def(client, "BITCOIN", 1);

    if(commision != NULL) {
        json_show(commision, stdout);
        json_delete(commision);
    } else {
        printf("Cant get commision\n");
    }
}


void __open_trade(XTB_Client * client) {
    Json * result = xtb_client_open_trade(client, "BITCOIN", XTB_TransMode_BUY, 0.01, 0, 0);

    if(result != NULL) {
        json_show(result, stdout);
        json_delete(result);
    } else {
        printf("Trade can't be open\n");
    }
}   


typedef struct {
    char * symbol;
    char * order; 
    XTB_TransMode mode; 
    float price;
    float volume;
}TradeRecord;


TradeRecord * trade_record_new_from_json(Json * trade_record) {
    TradeRecord * trade = malloc(sizeof(TradeRecord));

    *trade = (TradeRecord) {
        .symbol = strdup(json_lookup(trade_record, "symbol")->string)
        , .order = strdup(json_lookup(trade_record, "order")->string)
        , .mode = atoi(json_lookup(trade_record, "cmd")->string)
        , .price = atof(json_lookup(trade_record, "open_price")->string)
        , .volume = atof(json_lookup(trade_record, "volume")->string)
    };

    return trade;
}

void trade_record_delete(TradeRecord * self) {
    if(self != NULL) {
        if(self->symbol != NULL)
            free(self->symbol);

        if(self->order != NULL)
            free(self->order);

        free(self);
    }
}


TradeRecord * __find_trade_by_order(Json * trades, char * order) {
    for(size_t i = 0; i < trades->array.size; i++) {
        if(strcmp(order, json_lookup(trades->array.value[i], "order")->string) == 0)
            return trade_record_new_from_json(trades->array.value[i]);
    }

    return NULL;
} 


void __close_trade(XTB_Client * client) {
    Json * trades = xtb_client_get_trades(client, true);
    
    TradeRecord * trade_record = __find_trade_by_order(trades, "595797079");

    if(trade_record != NULL) {
        printf("%s %s %d %f %f\n", trade_record->symbol, trade_record->order, trade_record->mode, trade_record->price, trade_record->volume);

        Json * result = xtb_client_close_trade(client, trade_record->symbol, trade_record->order, trade_record->mode, trade_record->price, trade_record->volume);

        trade_record_delete(trade_record);

        if(result != NULL) {
            json_show(result, stdout);
            json_delete(result);
        } else {
            printf("order cant be close\n");
        }
    }

    json_delete(trades);
}


void __close_all_trade(XTB_Client * client) {
    Json * trades = xtb_client_get_trades(client, true);

    for(size_t i = 0; i < trades->array.size; i++) {
        TradeRecord * trade_record = trade_record_new_from_json(trades->array.value[i]);
        Json * result = xtb_client_close_trade(client, trade_record->symbol, trade_record->order, trade_record->mode, trade_record->price, trade_record->volume);

        trade_record_delete(trade_record);

        if(result != NULL) {
            json_show(result, stdout);
            json_delete(result);
        } else {
            printf("order cant be close\n");
        }
    }

    json_delete(trades);
}


void
client_run(XTB_Client * client) {
    if(xtb_client_ping(client) == false)
        printf("ping false\n");
    
    /*
    Json * all_symbols = xtb_client_get_all_symbols(client);

    json_show(all_symbols, stdout);
    json_delete(all_symbols);
      */
   
   /*
    MarketData * open_price = __read_open_price(client, "BITCOIN");

    
    printf("[");
    for(size_t i = 0; i < open_price->size; i++)
        printf(i == 0 ? "(%f, %f)" : ", (%f, %f)", open_price->array[i].open, open_price->array[i].close);
    printf("]\n");   
    
    free(open_price);
   */

    //__read_candles(client);
    //__read_candle_history(client);
    //__read_calendar(client);
    //__read_range_chart(client);
    //__get_commision(client);
    //__get_margin_level(client);
    //__get_margin_trade(client);
    //__get_server_time(client);
    //__get_symbol(client);
    //__get_news(client);
    //__get_version(client);
    //__get_trades(client);
    //__get_tick_prices(client);
    //__get_trade_records(client);
    //__get_trade_history(client);
    //__trade_transaction_status(client);
    //__get_user_data(client);
    //__get_tradin_hours(client);
    //__check_if_market_open(client);
    //__trade_transaction(client);
    //__find_symbol(client, "EURUSD");
    //__get_step_rules(client);
    //__get_commision_def(client);
    //__open_trade(client);
    //__close_trade(client);
    //__close_all_trade(client);
    //EURUSD
}


#include <time.h>


typedef struct {
    size_t size;
	size_t capacity;
	float * bidPrices; // Pole s nejlepšími 5 bid cenami
    float * bidVolumes; // Pole s odpovídajícími objemy bid cen
    float * askPrices; // Pole s nejlepšími 5 ask cenami
    float * askVolumes; // Pole s odpovídajícími objemy ask cen 

	float trend;

	bool open;
	float profit;
} Predictor;


Predictor predictor_new(size_t size) {
	return (Predictor) {
		.size = size
		, .bidPrices = memset(malloc(sizeof(float) * size), 0, sizeof(float) * size)
		, .bidVolumes = memset(malloc(sizeof(float) * size), 0, sizeof(float) * size)
		, .askPrices = memset(malloc(sizeof(float) * size), 0, sizeof(float) * size)
		, .askVolumes = memset(malloc(sizeof(float) * size), 0, sizeof(float) * size)
	};
}


void predictor_delete(Predictor * self) {	
	free(self->bidPrices);
	free(self->bidVolumes);
	free(self->askPrices);
	free(self->askVolumes);
}


float simpleMovingAverage(float *prices, int n) {
    float sum = 0;
    for (int i = 0; i < n; i++) {
        sum += prices[i];
    }
    return sum / n;
}


void process_tick_price(void * param, Json * tick) {
    Predictor * self = param;
	float bid_price = atof(json_lookup(tick, "bid")->string);
	float bid_volume = atof(json_lookup(tick, "bidVolume")->string);
	float ask_price = atof(json_lookup(tick, "ask")->string);
	float ask_volume = atof(json_lookup(tick, "askVolume")->string);

    if(self->capacity < self->size) {
        self->bidVolumes[self->capacity] = bid_volume;
        self->bidPrices[self->capacity] = bid_price;
        self->askVolumes[self->capacity] = ask_volume;
        self->askPrices[self->capacity] = ask_price;
        self->capacity ++;
    } else {
        for(size_t i = 0; i < self->size - 1; i++) {
            self->bidVolumes[i] = self->bidVolumes[i+1];
            self->bidPrices[i] = self->bidPrices[i+1];
            self->askVolumes[i] = self->askVolumes[i+1];
            self->askPrices[i] = self->askPrices[i+1]; 
        }

        self->bidVolumes[self->size-1] = bid_volume;
        self->bidPrices[self->size-1] = bid_price;
        self->askVolumes[self->size-1] = ask_volume;
        self->askPrices[self->size-1] = ask_price;
    }

    
    float m1 = (self->bidPrices[0] + self->askPrices[0]) / 2;
    float m2 = (self->bidPrices[self->size-1] + self->askPrices[self->size -1]) / 2;

    self->trend = (m2 - m1) / self->size;

    printf("%f - %f / %ld = %f\n", m2, m1, self->size, self->trend);

    json_show(tick, stdout);
}


void process_news(void * param, Json * news) {
    (void) param;
    json_show(news, stdout);
}


void process_balance(void * param, Json * balance) {
    (void) param;
    json_show(balance, stdout);
}


void process_candle(void * param, Json * candle) {
    (void) param;

    json_show(candle, stdout);
}


void process_keep_alive(void * param, Json * keep_alive) {
    (void) param;
    json_show(keep_alive, stdout);
}


void process_profit(void * param, Json * profit) {
	Predictor * self = param;
	float profit_value = atof(json_lookup(profit, "profit")->string);
	self->profit = profit_value;
    json_show(profit, stdout);
}


void process_trades(void * param, Json * trades) {
    (void) param;
    json_show(trades, stdout);
}


void process_trade_status(void * param, Json * trade_status) {
    (void) param;
    json_show(trade_status, stdout);
}


void scalping(XTB_Client * client) {
    StreamClientCallback callback = {
        .balance = process_balance
        , .tick_prices = process_tick_price
        , .news = process_news
        , .candle = process_candle
        , .keep_alive = process_keep_alive
        , .profit = process_profit
        , .trades = process_trades
        , .trade_status = process_trade_status
    };  
    
	time_t start, end;
    double cpu_time_used;
    
	start = time(NULL);

	char * symbol = "ETHEREUM";
	Predictor predictor = predictor_new(5);
    XTB_StreamClient * stream_client = xtb_stream_client_new(client, &callback, &predictor);

    xtb_stream_client_subscribe_tick_prices(stream_client, symbol, 0, 0);
    xtb_stream_client_subscribe_balance(stream_client);
    xtb_stream_client_subscribe_profits(stream_client);
    xtb_stream_client_subscribe_trade_status(stream_client);

    for(size_t i = 0; i < 1000; i++) {
        xtb_stream_client_process(stream_client);

		end = time(NULL);

		cpu_time_used = difftime(end, start) * 1000;

		if(cpu_time_used > 200 && predictor.capacity >= predictor.size) {
			if(predictor.open == false) {
				if(predictor.trend > 0.1) {
					start = time(NULL);

					predictor.open = true;
					Json * result = xtb_client_open_trade(client, symbol, XTB_TransMode_BUY, 0.1, 0, 0);
					json_show(result, stdout);
					json_delete(result);
				} else if(predictor.trend < -0.1) {
					start = time(NULL);

					predictor.open = true;
					Json * result = xtb_client_open_trade(client, symbol, XTB_TransMode_SELL, 0.1, 0, 0);
					json_show(result, stdout);
					json_delete(result);
				}
			} else {
				if(predictor.profit > 0.1 || predictor.profit < -40) {
					predictor.open = false;
					start = time(NULL);
					__close_all_trade(client);
				}
			}
		}
    }

	predictor_delete(&predictor);
}




#define ID       "15713459"
#define PASSWORD "4xl74fx0.H"


int main(void) {
    XTB_Client * client = xtb_client_new(XTB_AccountMode_Demo, ID, PASSWORD);

    if(client != NULL && xtb_client_logged(client) == true) {
        //client_run(client);
		scalping(client);
        xtb_client_delete(client);
    } else {
        printf("Can't login to XTB\n");
    }

    printf("Program exit\n");

    return EXIT_SUCCESS;
}




