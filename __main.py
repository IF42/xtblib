import XTBApi.api as api
import numpy as np
import time

client = api.Client()
client.login("15713459", "4xl74fx0.H", api.AccountMode.demo)

#candles = client.get_lastn_candle_history("BITCOIN", 5*60, 10)
#print(candles);
#prices = np.array(list(map(lambda candle: candle["close"], candles)))
#print(prices)

#calendar = client.get_calendar()
#print(calendar)

candles = client.get_chart_range_request("BITCOIN", api.PERIOD.M5.value, time.time()-(3* api.PERIOD.M5.value), time.time(), 0)
print(len(candles));
print(candles)
