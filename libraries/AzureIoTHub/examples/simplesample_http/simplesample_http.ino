// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.


// Use Arduino IDE 1.6.8 or later.

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <SPI.h>
#ifdef ARDUINO_ARCH_ESP8266
// for ESP8266
#include <ESPWiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#elif ARDUINO_ARCH_ESP32
// for ESP32
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#elif ARDUINO_SAMD_FEATHER_M0
// for Adafruit WINC1500
#include <Adafruit_WINC1500.h>
#include <Adafruit_WINC1500SSLClient.h>
#include <Adafruit_WINC1500Udp.h>
#include <NTPClient.h>
#else
#include <WiFi101.h>
#include <WiFiSSLClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#endif

#ifdef ARDUINO_SAMD_FEATHER_M0
// For the Adafruit WINC1500 we need to create our own WiFi instance
// Define the WINC1500 board connections below.
#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2     // or, tie EN to VCC
// Setup the WINC1500 connection with the pins above and the default hardware SPI.
Adafruit_WINC1500 WiFi(WINC_CS, WINC_IRQ, WINC_RST);
#endif

#include <AzureIoTHub.h>
#include <AzureIoTUtility.h>
#include <AzureIoTProtocol_HTTP.h>
#include <AzureIoTProtocol_MQTT.h>

#include "simplesample_http.h"

static char ssid[] = "wifi-11";     // your network SSID (name)
static char pass[] = "sumof1+1=2";    // your network password (use for WPA, or use as key for WEP)

// In the next line we decide each client ssl we'll use.
#ifdef ARDUINO_ARCH_ESP8266
static WiFiClientSecure sslClient; // for ESP8266 
#elif  ARDUINO_ARCH_ESP32
static WiFiClientSecure sslClient; // for ESP32 
#elif ARDUINO_SAMD_FEATHER_M0
static Adafruit_WINC1500SSLClient sslClient; // for Adafruit WINC1500
#else
static WiFiSSLClient sslClient;
#endif

static AzureIoTHubClient iotHubClient;

void setup() {
    initSerial();
    initWifi();
    initTime();

    iotHubClient.begin(sslClient);
}

void loop() {
    simplesample_http_run();
}

void initSerial() {
    // Start serial and initialize stdout
    Serial.begin(115200);
    Serial.setDebugOutput(true);
}

void initWifi() {
#ifdef ARDUINO_SAMD_FEATHER_M0
    // for the Adafruit WINC1500 we need to enable the chip
    pinMode(WINC_EN, OUTPUT);
    digitalWrite(WINC_EN, HIGH);
#endif

    // check for the presence of the shield :
  //  if (WiFi.status() == WL_NO_SHIELD) {
  //      Serial.println("WiFi shield not present");
  //      // don't continue:
  //      while (true);
  //  }

    // attempt to connect to Wifi network:
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass);

    Serial.println("Waiting for Wifi connection.");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }

    Serial.println("Connected to wifi");
}

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
static void obtain_time(void)
{
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
        delay(2000);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

static void initialize_sntp(void)
{
    printf("Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

void initTime() {
#if defined(ARDUINO_SAMD_ZERO) || defined(ARDUINO_SAMD_MKR1000) || defined(ARDUINO_SAMD_FEATHER_M0)
#ifdef ARDUINO_SAMD_FEATHER_M0
    Adafruit_WINC1500UDP ntpUdp; // for Adafruit WINC1500
#else
   // WiFiUDP ntpUdp;
#endif
   // NTPClient timeClient(ntpUDP);

   

#elif ARDUINO_ARCH_ESP8266 
    time_t epochTime;

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    while (true) {
        epochTime = time(NULL);

        if (epochTime == 0) {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        } else {
            Serial.print("Fetched NTP epoch time is: ");
            Serial.println(epochTime);
            break;
        }
    }

#elif ARDUINO_ARCH_ESP32
    Serial.print("----------------------\n");
#if 0
    timeClient.begin();

    while (!timeClient.update()) {
        Serial.println("Fetching NTP epoch time failed! Waiting 5 seconds to retry.");
        delay(5000);
    }
    Serial.println(timeClient.getFormattedTime());

    timeClient.end();

    unsigned long epochTime = timeClient.getEpochTime();

    Serial.print("Fetched NTP epoch time is: ");
    Serial.println(epochTime);

    iotHubClient.setEpochTime(epochTime);
#endif
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        Serial.print( "Time is not set yet. Connecting to WiFi and getting time over NTP.\n");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];

    setenv("TZ", "CST-8CDT-9,M4.2.0/2,M9.2.0/3", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c\n", &timeinfo);
    printf("The current date/time in Shanghai is: %s\n", strftime_buf);
#if 0
    time_t epochTime;

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    while (true) {
        epochTime = time(NULL);

        if (epochTime == 0) {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        } else {
            Serial.print("Fetched NTP epoch time is: ");
            Serial.println(epochTime);
            break;
        }
    }
#endif
#endif
}
