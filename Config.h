#pragma once

//#define DEBUG
#define NUKI_REST_BRIDGE_VERSION "0.01"
#define NUKI_REST_BRIDGE_WIFI_VERSION "2.1"
#define NUKI_REST_BRIDGE_HW_ID "9C198B38"
#define MAX_LOG_FILE_SIZE 500
#define MAX_LOG_MESSAGE_LEN 80
#define LOG_FILENAME "/bridge.log"
#define CONNECT_OVER_LAN 0  // SET TO 0 FOR WIFI
#define DHCP 1          // Set To 0 to define static IP
#define SERVER_PORT 8080  // Server Port

#if !CONNECT_OVER_LAN
const char *SSID = "SSID";
const char *PWD = "PWD";
#endif

#if !DHCP
// Set your Static IP address
IPAddress localIP(192, 168, 1, 184);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);    //optional
IPAddress secondaryDNS(8, 8, 4, 4);  //optional
#endif
