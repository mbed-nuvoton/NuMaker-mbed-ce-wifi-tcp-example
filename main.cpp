#include <algorithm>
#include "mbed.h"
#include "mbed_stats.h"
#include "TCPSocket.h"

#define MBED_HEAP_STATS_ENABLED 1
//#define LOCAL_LAN

#define ETHERNET        1
#define WIFI            2
#define MESH_LOWPAN_ND  3
#define MESH_THREAD     4

#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI
#define ESP8266_AT_ONBOARD      1   // On-board ESP8266
#define ESP8266_AT_EXTERN       2   // External ESP8266 through UNO D1/D0

#ifndef ESP8266_AT_SEL
#error("ESP8266_AT_SEL missing. Select ESP8266 on-board/external.")
#endif

#include "ESP8266Interface.h"
#   if ESP8266_AT_SEL == ESP8266_AT_ONBOARD
#       if TARGET_NUMAKER_IOT_M487
DigitalOut esp_rst(PH_3, 0);        // Simulate reset button pressed
ESP8266Interface esp(PH_8, PH_9);
#       elif TARGET_NUMAKER_PFM_M2351
DigitalIn esp_gpio0(PD_6);          // Go boot mode by default
                                    // User can change to F/W update mode by short'ing ESP8266 GPIO0/GND
                                    // before power-on
DigitalOut esp_pwr_off(PD_7, 1);    // Disable power to on-board ESP8266
ESP8266Interface esp(PD_1, PD_0);
#       elif TARGET_NUMAKER_IOT_M263A
ESP8266Interface esp(PC_7, PC_6, false, PE_13, PC_8, PE_12);
//ESP8266Interface esp(PC_7, PC_6, false, PE_13, PC_8, NC);
//ESP8266Interface esp(PC_7, PC_6);
#       endif
#   elif ESP8266_AT_SEL == ESP8266_AT_EXTERN
ESP8266Interface esp(D1, D0);
#   endif
#elif MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
#include "EthernetInterface.h"
EthernetInterface eth;
#elif MBED_CONF_APP_NETWORK_INTERFACE == MESH_LOWPAN_ND
#define MESH
#include "NanostackInterface.h"
LoWPANNDInterface mesh;
#elif MBED_CONF_APP_NETWORK_INTERFACE == MESH_THREAD
#define MESH
#include "NanostackInterface.h"
ThreadInterface mesh;
#endif

namespace {
    // Test connection information
#ifndef LOCAL_LAN
const char *HTTP_SERVER_NAME = "www.ifconfig.io"; 
#else
const char *HTTP_SERVER_NAME = "pt22_winserver2.nuvoton.com";
#endif

#ifndef LOCAL_LAN
const char *HTTP_SERVER_FILE_PATH = "/method";
const int HTTP_SERVER_PORT = 80;
#else
const char *HTTP_SERVER_FILE_PATH = "/examples/arm_mbed/method.txt";
const int HTTP_SERVER_PORT = 8080;
#endif


    const int RECV_BUFFER_SIZE = 512;

    // Test related data
    const char *HTTP_OK_STR = "200 OK";
    const char *HTTP_EXPECT_STR = "GET";

    // Test buffers
    char buffer[RECV_BUFFER_SIZE] = {0};
}

bool find_substring(const char *first, const char *last, const char *s_first, const char *s_last) {
    const char *f = std::search(first, last, s_first, s_last);
    return (f != last);
}

int main() {
#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI
#   if ESP8266_AT_SEL == ESP8266_AT_ONBOARD
#       if TARGET_NUMAKER_IOT_M487
    wait_ms(5);
    esp_rst = 1;                    // Simulate reset button released
    wait_ms(5);
#       elif TARGET_NUMAKER_PFM_M2351
    wait_ms(50);
    esp_pwr_off = 0;                // Turn on on-board ESP8266
    wait_ms(50);
#       endif
#   endif
#endif

#if MBED_HEAP_STATS_ENABLED
    mbed_stats_heap_t heap_stats;
#endif

    printf("Start WiFi test \r\n");
     
    bool result = true;
    int rc = 0;

    printf("Start Connection ... \r\n");


    NetworkInterface *network_interface = 0;
   
#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI
    printf("\n\rUsing WiFi \r\n");
    printf("\n\rConnecting to WiFi..\r\n");
    rc = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    network_interface = &esp;
#elif MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
    printf("Using Ethernet\r\n");
    rc = eth.connect();
    network_interface = &eth;
#endif
#ifdef MESH
    printf("Using Mesh\r\n");
    printf("\n\rConnecting to Mesh..\r\n");
    rc = mesh.connect();
    network_interface = &mesh;
#endif

    if(rc == 0) {
        printf("\n\rConnected to Network successfully\r\n");
    } else {
        printf("\n\rConnection to Network Failed %d! Exiting application....\r\n", rc);
        return 0;
    }    
        
    printf("TCP client IP Address is %s\r\n", network_interface->get_ip_address());

    TCPSocket sock(network_interface);
    printf(" HTTP Connection ... \r\n");
    if (sock.connect(HTTP_SERVER_NAME, HTTP_SERVER_PORT) == 0) {
        printf("HTTP: Connected to %s:%d\r\n", HTTP_SERVER_NAME, HTTP_SERVER_PORT);

        // We are constructing GET command like this:
#ifndef USE_HTTP_1_1
        // GET http://www.ifconfig.io/method HTTP/1.0\n\n
        strcpy(buffer, "GET http://");
        strcat(buffer, HTTP_SERVER_NAME);
        strcat(buffer, HTTP_SERVER_FILE_PATH);
        strcat(buffer, " HTTP/1.0\n\n");       
#else
       // GET /method HTTP/1.1\r\nHost: ifconfig.io\r\nConnection: close\r\n\r\n"
        strcpy(buffer, "GET ");
        strcat(buffer, HTTP_SERVER_FILE_PATH);   
        strcat(buffer, " HTTP/1.1\r\nHost: ");
        strcat(buffer, HTTP_SERVER_NAME);
        strcat(buffer, "\r\nConnection: close\r\n\r\n");
#endif
        
        // Send GET command
        sock.send(buffer, strlen(buffer));

        // Server will respond with HTTP GET's success code
        const int ret = sock.recv(buffer, sizeof(buffer) - 1);
        buffer[ret] = '\0';
        
        // Find 200 OK HTTP status in reply
        bool found_200_ok = find_substring(buffer, buffer + ret, HTTP_OK_STR, HTTP_OK_STR + strlen(HTTP_OK_STR));
        // Find "deny" string in reply
        bool found_expect = find_substring(buffer, buffer + ret, HTTP_EXPECT_STR, HTTP_EXPECT_STR + strlen(HTTP_EXPECT_STR));

        if (!found_200_ok) result = false;
        if (!found_expect) result = false;

        printf("HTTP: Received %d chars from server\r\n", ret);
        printf("HTTP: Received 200 OK status ... %s\r\n", found_200_ok ? "[OK]" : "[FAIL]");
        printf("HTTP: Received '%s' status ... %s\r\n", HTTP_EXPECT_STR, found_expect ? "[OK]" : "[FAIL]");
        printf("HTTP: Received massage:\r\n\r\n");
        printf("%s", buffer);
    }

#if MBED_HEAP_STATS_ENABLED
    mbed_stats_heap_get(&heap_stats);
    printf("Current heap: %lu\r\n", heap_stats.current_size);
    printf("Max heap size: %lu\r\n", heap_stats.max_size);
#endif
 
    printf(" Close socket & disconnect ... \r\n");
    sock.close();
    
#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI
    esp.disconnect();
#elif MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
    eth.disconnect();
#elif MBED_CONF_APP_NETWORK_INTERFACE == MESH_LOWPAN_ND
    mesh.disconnect();
#elif MBED_CONF_APP_NETWORK_INTERFACE == MESH_THREAD
    mesh.disconnect();
#endif
    printf(" End \r\n");
}
