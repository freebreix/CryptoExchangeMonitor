#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>

const char GeckoCert[] PROGMEM = R"CERT(
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ
RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD
VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX
DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y
ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy
VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr
mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr
IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK
mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu
XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy
dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye
jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1
BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3
DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92
9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx
jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0
Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz
ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS
R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp
-----END CERTIFICATE-----
)CERT";

X509List cert(GeckoCert);

const char* ssid = "lmao";
const char* password = "rofl1234";

const char* host = "api.coingecko.com";
// ssl fingerprint to api.coingecko.com
const char* fingerprint = "8925605d5044fcc0852b98d7d3665228684de6e2";

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(9600);
  Serial.printf("\nConnecting to my hotspot \"%s\"", ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    //digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    Serial.print(".");
    //digitalWrite(LED_BUILTIN, HIGH);
  }
  
  Serial.print("\nConnected, IP address: ");
  Serial.println(WiFi.localIP());

  syncTime(true);
}
	
void loop() {
  if (Serial.available()) {
    String msg;
    while (Serial.available()) {
      msg += (char)Serial.read();
      delay(50);
    }
    Serial.write("Received request: ");
    Serial.print(msg);
    String cmd = getValue(msg, '|', 0);
    String coin = getValue(msg, '|', 1);
    Serial.printf("Command: %s, Coin: %s\n", cmd, coin);
    
    if (cmd.equals("getTime")) {
      Serial.printf("{\"unixTimestamp\":%d}\n", syncTime(false));
    } else if (cmd.equals("getCoins"))
      fetchCoins();
    else if (cmd.equals("getCoinData"))
      fetchCoinData(coin);
    else if (cmd.equals("getCoinPrice"))
      fetchCoinPrice(coin);
    else if (cmd.equals("getMarketData"))
      fetchMarketData(coin, getValue(msg, '|', 2), getValue(msg, '|', 3));
    digitalWrite(LED_BUILTIN, LOW);
  }
}

// Modified version of: https://arduino.stackexchange.com/a/1237
String getValue(String data, char separator, int index) {
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;
    
    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    String value = found > index ? data.substring(strIndex[0], strIndex[1]) : "";
    value.trim();
    return value;
}

long syncTime(boolean debug) {
  // Set time via NTP, as required for x.509 validation
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  if (debug)
    Serial.print("Waiting for NTP time sync");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    if (debug)
      Serial.print(".");
    now = time(nullptr);
  }
  
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  if (debug)
    Serial.printf("\nTime succesfully synced!\nCurrent date & time: %d.%d.%d %d:%d:%d\n", day(now), month(now), year(now), hour(now), minute(now), second(now));
  return now;
}

void fetchCoins() {
  downloadAndSendStringData("https://api.coingecko.com/api/v3/coins/list");
}

void fetchCoinPrice(String coin) {
  digitalWrite(LED_BUILTIN, HIGH);
  downloadAndSendStringData("https://api.coingecko.com/api/v3/simple/price?ids=" + coin + "&vs_currencies=usd");
}

void fetchCoinData(String coin) {
  downloadAndSendStringData("https://api.coingecko.com/api/v3/coins/" + coin + "?tickers=false&market_data=false&community_data=false&developer_data=false&sparkline=false");
}

void fetchMarketData(String coin, String from, String to) {
  downloadAndSendStringData("https://api.coingecko.com/api/v3/coins/" + coin + "/market_chart/range?vs_currency=usd&from=" + from + "&to=" + to);
}

void downloadAndSendStringData(String url) {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Requesting url: ");
    Serial.println(url);
    
    WiFiClientSecure client;
    HTTPClient https;
    //client.setInsecure();
    client.setFingerprint(fingerprint);
    client.setTrustAnchors(&cert);
    
    if (client.connect(host, 443)) {
      https.begin(client, url);
      
      int code = https.GET();
      if (code > 0)
          Serial.println(https.getString());
      else Serial.printf("GET Request failed with code: %d\n", code);
    } else Serial.printf("Couldn't connect to %s\n", host);
  }
}
