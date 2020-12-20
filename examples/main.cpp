#include <Arduino.h>

/*
 * Anzeigetafel nach c't
 * 
 * Pinbelegung
 * EPD  Farbe   Bez   ESP Bez
 * 1    Rot     3,3V  1   3,3V
 * 2    Schw.   Gnd   36  Gnd
 * 3    DBlau   DIN   27  MOSI(23)
 * 4    Gelb    CLK   26  SCK(18)
 * 5    Orange  CS    25  SS(5)
 * 6    Grün    DC    23  17
 * 7    Weiß    RST   22  16
 * 8    HBlau   BUSY  21  4
 * 
 * Typische Konfiguration:
 * {
 *   "APSecret": ".....",
 *   "DeviceName": "Doorsign",
 *   "ImageAddress": "/index.php?debug=false&display=7.5&content=multi&scale=18",
 *   "ImageHost": "192.168.1.79",
 *   "ImageWait": "600",
 *   "ProductionMode": "false",
 *   "WifiConfigured": "True",
 *   "WifiEssid": "GDPWL3",
 *   "WifiPassword": "....."
 * }
 */

#include <Basecamp.hpp>
#include <rom/rtc.h>
#include <soc/rtc_cntl_reg.h>

Basecamp iot;

#include <GxEPD.h>

#if DISPLAY_TYPE == 29
#include <GxGDEH029A1/GxGDEH029A1.cpp>      // 2.9" b/w
#endif
#if DISPLAY_TYPE == 42
#include <GxGDEW042T2/GxGDEW042T2.cpp>      // 4.2" b/w
#endif
#if DISPLAY_TYPE == 75
#include <GxGDEW075T8/GxGDEW075T8.cpp>      // 7.5" b/w
#endif
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>
#include <Fonts/FreeMonoBold9pt7b.h>

GxIO_Class io(SPI, SS, 17, 16);
GxEPD_Class display(io, 16, 4);
bool connection = false;
bool production = false;

void wait_for_next(long ts, int bytecount) {
  int SleepTime = iot.configuration.get("ImageWait").toInt();
  #ifdef DEBUG
  SleepTime /= 10;
  #endif
  if (production) {
    if (ts > 0) {
      long period = ts / SleepTime + 1;
      SleepTime = period * SleepTime - ts;
    }
    Serial.print("Going to sleep now... ");
    Serial.print(SleepTime);
    Serial.println(" sec.");
    esp_sleep_enable_timer_wakeup(SleepTime * FactorSeconds);
    esp_deep_sleep_start();
  } else {
    Serial.print("Bytes read: ");
    Serial.println(bytecount);
    Serial.print("Setup: Not going to sleep. Use web config to setup. Waiting... ");
    Serial.print(SleepTime / 10);
    Serial.println(" sec.");
    if (SleepTime > 10 && SleepTime < 600) delay(SleepTime * 100);
    else delay(60000);
    if (WiFi.status() != WL_CONNECTED) connection = false;
    if (!connection) WiFi.reconnect();
    int retry = 0;
    while ((WiFi.status() != WL_CONNECTED) && (retry < 20)) {
      Serial.print(".");
      retry++;
      delay(500);
    }
    Serial.println();
    connection = (WiFi.status() == WL_CONNECTED);
  }
}

void setup() {
  bool configured;
  REG_WRITE(RTC_CNTL_BROWN_OUT_REG, 0);
  display.init();
  const GFXfont * f = & FreeMonoBold9pt7b;
  display.setTextColor(GxEPD_BLACK);
  RESET_REASON r = rtc_get_reset_reason(0);
  if (r == 15) {
    display.fillScreen(GxEPD_WHITE);
    display.setRotation(0);
    display.setFont(f);
    display.setCursor(0, 0);
    display.println();
    display.println("Brownout detected!");
    display.println("Please recharge Battery and ");
    display.println("press Reset after Power is re-established.");
    display.update();
    esp_deep_sleep_start();
  }
  REG_WRITE(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_RST_ENA | 
                                    RTC_CNTL_BROWN_OUT_ENA | 
									7 << RTC_CNTL_DBROWN_OUT_THRES_S | 
									RTC_CNTL_BROWN_OUT_PD_RF_ENA | 
									RTC_CNTL_BROWN_OUT_CLOSE_FLASH_ENA | 
									(0x3ff << RTC_CNTL_BROWN_OUT_RST_WAIT_S));
  iot.begin();

  iot.web.addInterfaceElement("ImageHost", "input",
    "Server to load image from (host name or IP address):",
    "#configform", "ImageHost");
  iot.web.addInterfaceElement("ImageAddress", "input",
    "Address to load image from (path on server, starting with / e.g.:"
    " /index.php/?debug=false&[...] ):", "#configform", "ImageAddress");
  iot.web.addInterfaceElement("ImageWait", "input",
    "Sleep time (to next update) in seconds:", "#configform", "ImageWait");
  iot.web.addInterfaceElement("ProductionMode", "input",
    "Production mode  (if set to 'true', deep sleep will be activated, "
    "this config page will be down.)", "#configform", "ProductionMode");

  configured = (iot.configuration.get("WifiConfigured").equalsIgnoreCase("TRUE"));

  #ifdef DEBUG
  Serial.println(configured ? "Configured" : "Not configured");
  #endif

  if (configured && iot.configuration.get("ProductionMode").equalsIgnoreCase("TRUE"))
    production = true;

  #ifdef DEBUG
  Serial.println(production ? "Production Mode found" : "Test Mode found");
  #endif

  if (iot.configuration.get("ImageWait").toInt() < 10) {
    iot.configuration.set("ImageWait", "60");
    iot.configuration.save();
  }

  if (!configured) {
    display.fillScreen(GxEPD_WHITE);
    display.setRotation(0);
    display.setFont(f);
    display.setCursor(0, 0);
    display.println();
    display.println("Wifi not configured!");
    display.println("Connect to hotspot 'ESP32' and open 192.168.4.1");
    display.update();
    connection = false;
  } else {

    int retry = 0;
    while ((WiFi.status() != WL_CONNECTED) && (retry < 20)) {
      Serial.print(".");
      retry++;
      delay(500);
    }
    Serial.println();

    bool connectfail = (WiFi.status() != WL_CONNECTED);
    bool missingHost = (iot.configuration.get("ImageHost").length() < 1) ||
      (iot.configuration.get("ImageAddress").length() < 1);

    if ((!connectfail) && (iot.configuration.get("FailCount").toInt() > 0)) {
      iot.configuration.set("FailCount", "0");
      iot.configuration.save();
    }

    #ifdef DEBUG
    Serial.println(connectfail ? "Failed to connect" : "Connection established");
    #endif

    connection = (!connectfail && !missingHost);
    if (!connection) {
      display.fillScreen(GxEPD_WHITE);
      display.setRotation(0);
      display.setFont(f);
      display.setCursor(0, 0);
      if (connectfail) {
        display.println();
        display.println("");
        display.println("Could not connect to " +
          iot.configuration.get("WifiEssid"));
      }
      if (missingHost) {
        display.println();
        display.println("");
        display.println("Image server not configured.");
        display.println("Open " + WiFi.localIP().toString() +
          " in your browser and set server address and path.");
        if (!connectfail) production = false;
      }
      if (connectfail) {
        int failcount = iot.configuration.get("FailCount").toInt() + 1;
        if (failcount < 144) {
          iot.configuration.set("FailCount", String(failcount));
          iot.configuration.save();
          display.printf("Fail-Counter: %d\n", failcount);
          display.update();
          wait_for_next(0L, 0);
        } else {
          display.println("Too many misconnects. Falling back to Non-Production!");
          display.println("Press Reset or change Header.");
          display.println("to reconfigure via Web.");
          iot.configuration.set("ProductionMode", "false");
          iot.configuration.save();
          display.update();
          esp_deep_sleep_start();
        }
      }
      display.update();
    }
  }
}

String hex_pad(char byte) {
  String res = "0" + String(byte, HEX);
  return (res.substring(res.length() - 2) + " ");
}

void loop() {

  int bytecount = 0;
  long ts = 0L;

  #ifdef DEBUG
  Serial.printf("Debug L1 - %s\n", 
    connection ? "Connection established" : "not connected");
  #endif

  if (connection) {
    String url = iot.configuration.get("ImageAddress");
    boolean currentLineIsBlank = true;
    WiFiClient client;
    delay(5000);

    const int httpPort = 80;
    const char * host = iot.configuration.get("ImageHost").c_str();

    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    } else Serial.println("get " + url + " Host: " +
      iot.configuration.get("ImageHost") + "\r\n");

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
      "Host: " + iot.configuration.get("ImageHost") + "\r\n" +
      "Connection: close\r\n\r\n");

    int x = 0;
    int y = 0;
    int start = 0;
    bool RLE = false;
    bool data = false;
    int value, count;

    String header;
    char buffer[16];

    display.eraseDisplay();

    while (client.available()) {
      unsigned char byte = client.read();
      bytecount++;

      if (data == false) {
        header += (char) byte;
      }

      if (byte == '\n' && currentLineIsBlank && data == false) {
        data = true;

        if (header.indexOf("X-productionMode: false") > 0)
          if (production) {
            Serial.println("Switching to Test-Mode.");
            production = false;
          }
        if (header.indexOf("X-productionMode: true") > 0)
          if (!production) {
            Serial.println("Switching to Production-Mode.");
            production = true;
          }
        int i = header.indexOf("Timestamp:");
        if (i >= 0) {
          int j = header.substring(i).indexOf("\n");
          if (j > 0) {
            ts = header.substring(i + 11, i + j - 1).toInt();
          }
        }
      }
      if (byte == '\n' && data == false) currentLineIsBlank = true;
      else if (byte != '\r' && data == false) currentLineIsBlank = false;

      if (data) {
        if (start < 1) {
          start++;
        } else {
          if (byte == 0xAA) {
            RLE = true;
            value = -1;
            continue;
          }
          if (RLE) {
            if (value < 0) {
              value = byte;
              continue;
            }
            count = (byte ? byte : 256);
            RLE = false;
          } else {
            count = 1;
            value = byte;
          }

          while (count--> 0 && y < GxEPD_HEIGHT) {
            for (int b = 7; b >= 0; b--) {
              int bit = bitRead((unsigned char) value, b);

              if (bit == 1) {
                display.drawPixel(x, y, GxEPD_BLACK);
              } else {
                display.drawPixel(x, y, GxEPD_WHITE);
              }
              x++;
              if (x == GxEPD_WIDTH) {
                y++;
                if (y >= GxEPD_HEIGHT) break;
                x = 0;
              }
            }
            if (y >= GxEPD_HEIGHT) break;
          }
        }
      }

    }
    display.update();
    if (!production) Serial.println("Image loaded.");
  }
  wait_for_next(ts, bytecount);
}