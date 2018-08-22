#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>


const char *ssid = "safety-danger";
const char *pass = "anarres myriad janus";

WiFiUDP Udp;

struct JacketInfo {
  char ca;
  char fe;
  unsigned long now;
  unsigned char pattern;
  unsigned long color;
} jacket_info;

ESP8266WebServer server(80);

extern "C" {
#include <uart.h>
#include <uart_register.h>
#include <user_interface.h>

}
#define UartWaitBetweenUpdatesMicros 500
#define UartBaud 3200000
#define UART1 1
#define UART1_INV_MASK (0x3f << 19)

#define ledCount 24

int max_brightness = 50;

const char webPage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body style="font-size:64pt">welcome to the<br><b>love and light brigade</b><br>
<a href="?s=0">RED/BLUE</a><br>
<a href="?s=1">Spectrum</a><br>
<a href="?s=2">GREEN </a><br>
<a href="?s=3">RED</a><br>
<a href="?s=4">BLUE</a><br>
<a href="?s=5">WHITE</a><br>
</body></html>
)=====";


void handleWebRoot() {
  server.send(200, "text/html", webPage);
  String s = server.arg("s");
  if(s) {
    Serial.println(s);
    if(s == "0") {
      jacket_info.pattern = 0;
    } else if(s == "1") {
      jacket_info.pattern = 1;
    } else if(s == "2"){
      jacket_info.pattern = 2;
    } else if(s == "3"){
      jacket_info.pattern = 3;
    } else if(s == "4"){
      jacket_info.pattern = 4;
    } else if(s == "5"){
      jacket_info.pattern = 5;
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("hello from baby vestlight");
  
  Serial1.begin(UartBaud, SERIAL_6N1, SERIAL_TX_ONLY);
  CLEAR_PERI_REG_MASK(UART_CONF0(UART1), UART1_INV_MASK);
  SET_PERI_REG_MASK(UART_CONF0(UART1), (BIT(22)));

  WiFi.softAP(ssid,pass);
  
  IPAddress myIP = WiFi.softAPIP();  
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // TODO: eeprom?
  jacket_info.pattern = 0;
  jacket_info.ca = 0xCA;
  jacket_info.fe = 0xFE;

  server.on("/", handleWebRoot);
  server.begin();

  Udp.begin(5199);
}

long HSV_to_RGB( float h, float s, float v ) {
  /* modified from Alvy Ray Smith's site: http://www.alvyray.com/Papers/hsv2rgb.htm */
  // H is given on [0, 6]. S and V are given on [0, 1].
  // RGB is returned as a 24-bit long #rrggbb
  int i;
  float m, n, f;
 
  // not very elegant way of dealing with out of range: return black
  if ((s<0.0) || (s>1.0) || (v<0.0) || (v>1.0)) {
    return 0L;
  }
 
  if ((h < 0.0) || (h > 6.0)) {
    return long( v * 255 ) + long( v * 255 ) * 256 + long( v * 255 ) * 65536;
  }
  i = floor(h);
  f = h - i;
  if ( !(i&1) ) {
    f = 1 - f; // if i is even
  }
  m = v * (1 - s);
  n = v * (1 - s * f);
  switch (i) {
  case 6:
  case 0:
    return long(v * 255 ) * 65536 + long( n * 255 ) * 256 + long( m * 255);
  case 1:
    return long(n * 255 ) * 65536 + long( v * 255 ) * 256 + long( m * 255);
  case 2:
    return long(m * 255 ) * 65536 + long( v * 255 ) * 256 + long( n * 255);
  case 3:
    return long(m * 255 ) * 65536 + long( n * 255 ) * 256 + long( v * 255);
  case 4:
    return long(n * 255 ) * 65536 + long( m * 255 ) * 256 + long( v * 255);
  case 5:
    return long(v * 255 ) * 65536 + long( m * 255 ) * 256 + long( n * 255);
  }
}


// From Makuna's NeoPixelBus
static const uint8_t uartBitPatterns[4] = {
  0b110111, // On wire: 1 000 100 0 [Neopixel reads 00]
  0b000111, // On wire: 1 000 111 0 [Neopixel reads 01]
  0b110100, // On wire: 1 110 100 0 [Neopixel reads 10]
  0b000100, // On wire: 1 110 111 0 [NeoPixel reads 11]
};

static inline uint8_t getTxLengthForUART1()
{
  return (U1S >> USTXC) & 0xff;
}


static inline void writeByteToUART1(uint8_t byte)
{
  U1F = byte;
}


byte colorValue(int i, long int now) {
  int led = i / 3;
  int color = i % 3;

  byte col = 0;
  //return max_brightness;

  if(jacket_info.pattern == 0) {
    static float the_sin, the_cos;
    static long int the_now;
    if(the_now != now) {
      the_now = now;
      the_sin = sin(2 * 3.1415 * ((float)(now % 15000) / 15000));
      the_cos = cos(2 * 3.1415 * ((float)(now % 15000) / 15000));
    }
    if(color == 1) { // RED
      col = max_brightness * abs(the_sin);
    } else if(color == 2) { // BLUE
      col = max_brightness * abs(the_cos);
    }
  } else if(jacket_info.pattern == 1) {
    unsigned long rgb = HSV_to_RGB(float(now % 30000) * 6 / 30000.0, 1, float(max_brightness) / 255);
    if(color == 1) { // red
      col = rgb & 0xFF;
    } else if(color == 2) { //blue
      col = rgb >> 16;
    } else { // green
      col = (rgb >> 8) & 0xFF;
    }
  } else if(jacket_info.pattern == 2) {
    if(color == 0) { // green
      // TODO digital rain
      col = max_brightness;
    }
  } else if(jacket_info.pattern == 3) {
    if(color == 1) { // red
      // TODO fire
      col = max_brightness;
    }
  } else if(jacket_info.pattern == 4) {
    if(color == 2) { // blue
      // TODO fire
      col = max_brightness;
    }
  } else if(jacket_info.pattern == 5) {
    // white
    col = max_brightness;
  }

  return col;
}


void fillUART() {
  unsigned long nowMillis = millis();
  
  for(int i = 0; i < ledCount * 3; i++) {
    uint8_t availableFifoSpace = 0;
    while(availableFifoSpace < 4) {
      availableFifoSpace = (UART_TX_FIFO_SIZE - getTxLengthForUART1());
    }

    byte val = colorValue(i, nowMillis);
    writeByteToUART1(uartBitPatterns[(val >> 6) & 0x3]);
    writeByteToUART1(uartBitPatterns[(val >> 4) & 0x3]);
    writeByteToUART1(uartBitPatterns[(val >> 2) & 0x3]);
    writeByteToUART1(uartBitPatterns[ val       & 0x3]);
  }
}

void writeUART() {
  static bool pausing = false;
  static unsigned long pauseUntilMicros = 0;

  if (getTxLengthForUART1() == 0) {
    if(!pausing) {
      pausing = true;
      pauseUntilMicros = micros() + UartWaitBetweenUpdatesMicros;
    } else {
      if(pauseUntilMicros <= micros()) {
        pausing = false;
        fillUART();
      }
    }
  }
}


long int lastUdp = -750;
void writeUDP() {
  //Serial.println(millis());
  //Serial.println(lastUdp);
  
  if(lastUdp + 1000 <= millis()) { // once a second
    //Serial.println("iterating over clients");
    struct station_info *stat_info;
    stat_info = wifi_softap_get_station_info();

    while(stat_info) {
       //Serial.print("client IP address: ");
       //Serial.println(stat_info->ip.addr);
       Udp.beginPacket(stat_info->ip.addr, 5199);
       
       jacket_info.now = millis();
       Udp.write((char *)&jacket_info, sizeof(JacketInfo));
       Udp.endPacket();
       stat_info = STAILQ_NEXT(stat_info, next);
    }
    lastUdp = millis();
  }
}

void loop() {
  writeUART();
  writeUDP();
  server.handleClient();
}
