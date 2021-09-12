#include <ArduinoJson.h>
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <IRremote.h>
#include <math.h>

#define DEBUG false

SoftwareSerial esp(3,2);
LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
IRrecv remote(6);

// currently loaded prices on the chart
float prices[12];
short priceUpdatePos = 0, coinIndex = 1, totalHours = 1;

float updateInterval = 300000;
long lastUpdated;
boolean displayOn = true;

const char* coins[][2] = {
  { "ADA", "cardano" },
  { "BTC", "bitcoin" },
  { "DOGE", "dogecoin" }
};

void setup() {
  esp.begin(9600);
  Serial.begin(9600);
  remote.enableIRIn();
  lcd.begin(16, 2);
  pinMode(5, OUTPUT);
  createBars();
  loadSelectedCoin();
}

void loop() {
  // Handle remote signals
  if (remote.decode()) {
    switch (remote.decodedIRData.command) {
      case 67: // switch foward between coins
        if (coinIndex < sizeof(coins)-1)
          coinIndex++;
        else coinIndex = 1;
        loadSelectedCoin();
        break;
      case 68: // switch backwards between coins
        if (coinIndex > 0)
          coinIndex--;
        else coinIndex = sizeof(coins)-1;
        loadSelectedCoin();
        break;
      case 70: // scale up one hour
        totalHours++;
        loadSelectedCoin();
        break;
      case 21: // scale down one hour
        if (coinIndex > 0) {
          totalHours--;
          loadSelectedCoin();
        } break;
      case 64: // reload chart
        loadSelectedCoin();
        break;
      case 69: // turn display on/off
        displayOn = !displayOn;
        digitalWrite(5, displayOn? LOW : HIGH);
        if (displayOn)
          drawChart();
        delay(500);
        break;
      default:
        Serial.println(remote.decodedIRData.command);
        break;
    }
    remote.resume(); // receive the next value
  }
}

void loadSelectedCoin() {
  Serial.printf("Loading %s (%s)...\n", coins[coinIndex][1], coins[coinIndex][0]);
  loadCoinCandles();
  drawCoinInfo();
  lastUpdated = millis();
}

void loadCoinCandles() {
  // 1. get current time
  esp.println("getTime");
  long unixTimestamp = deserializeResponse(collectResponse(), "").as<JsonObject>()["unixTimestamp"].as<long>();
  Serial.printf("Current timestamp: %ld\n", unixTimestamp);
  if (unixTimestamp == 0) return;
  
  // 2. get prices
  esp.printf("getMarketData|%s|%ld|%ld", coins[coinIndex][1], unixTimestamp-totalHours*3600, unixTimestamp);
  int timeOut = millis()+10000, i = 0, c = 1;
  
  while (millis() < timeOut) {
    if (esp.available()) {
      if (DEBUG)
        Serial.print((char)esp.read());
      else {
        esp.find("\"prices\":[");
        do {
            DynamicJsonDocument json(128);
            deserializeJson(json, esp);
            float price = json.as<JsonArray>()[1].as<float>();
            if (c%totalHours == 0
                && price > 0 && !isinf(price))
                  prices[i++] = price;
            c++;
            if (i == 12) break;
        } while (esp.findUntil(",","]"));
      }
    }
  }
  
  // 3. redraw
  drawChart();
}

String collectResponse() {
  int timeOut = millis() + 10000;
  String PROGMEM json;
  while (millis() < timeOut) {
    if (esp.available()) {
      // read fully
      while (esp.available()) {
        char c = esp.read();
        if (DEBUG)
          Serial.print(c);
        if (c == '{' || json[0] == '{')
          json += c;
        if (c == '}') // otherwise repeats till timeout
          return json;
      }
    }
  }
  Serial.println("\nFailed to obtain any response data!");
}

DynamicJsonDocument deserializeResponse(String response, String filter) {
  DynamicJsonDocument json(128);
  DeserializationError fail;
  if (filter.length() > 0) {
    StaticJsonDocument<64> filterJson;
    filterJson[filter] = true;
    fail = deserializeJson(json, response, DeserializationOption::Filter(filterJson));
  } else fail = deserializeJson(json, response);
  
  if (fail)
    Serial.printf("Failed to parse the response: %s\n", fail.c_str());
  return json;
}

void drawChart() {
  // calc min/max
  float topPrice, botPrice = prices[0];
  for (int i = 0; i < 12; i++) {
    Serial.println(prices[i]);
    if (prices[i] > topPrice)
      topPrice = prices[i];
    else if (prices[i] < botPrice)
      botPrice = prices[i];
  }
  
  // draw current prices scaled
  for (int i = 0; i < 12; i++)
    drawBar(i, round(map(prices[i], botPrice, topPrice, 1, 16)));
}

void drawCoinInfo() {
  lcd.setCursor(12, 0);
  lcd.write(coins[coinIndex][0]);
  
  if (priceUpdatePos > 0) {
    double priceDiff = prices[priceUpdatePos]-prices[priceUpdatePos-1];
    double percentage = prices[priceUpdatePos-1]/100;
    float prcChange = priceDiff/percentage;
    String prcFormatted = String(prcChange, 2);

    Serial.println(priceDiff);
    Serial.println(percentage);
    
    if (prcChange < 1 && prcChange > 0)
      prcFormatted.remove(0, 1);
    else if (prcChange > -1 && prcChange < 0)
      prcFormatted.remove(1, 1);

    //lcd.setCursor(15, 0);
    lcd.print((char)(prcChange > 0 ? 94 : prcChange < 0 ? 118 : 45));
    
    lcd.setCursor(12, 1);
    if (prcChange < 100 && prcChange >= 1)
      lcd.write("+");
    lcd.print(prcFormatted);
    lcd.write("%");
  }
}

// draw a bar with a given height in given position
void drawBar(int pos, int height) {
  lcd.setCursor(pos, 1);
  lcd.write(height > 8 ? 8 : height);

  if (height > 8) {
    lcd.setCursor(pos, 0);
    lcd.write(height-8);
  }
}
 
// Bytes for a block starting from the bottom with a defined height
void createBars() {
  for (int height = 1; height <= 8; height++) {
    byte* block = new byte[8];
    for (int i = 0; i < 8; i++)
      block[i] = (8 - i <= height) ? B11111 : B00000;
    lcd.createChar(height, block);
  }
}
