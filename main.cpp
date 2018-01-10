
#include <algorithm>
#include "mbed.h"
#include "TCPSocket.h"
#include "unity/unity.h"

#ifndef __CC_ARM 
#ifdef __GNUC__
extern "C" caddr_t _sbrk(int incr);
#endif
#endif


//#define LOCAL_LAN

#define ETHERNET        1
#define WIFI            2
#define MESH_LOWPAN_ND  3
#define MESH_THREAD     4

#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI
#include "ESP8266Interface.h"
ESP8266Interface esp(D1, D0);
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
const char *HTTP_SERVER_NAME = "os.mbed.com";
#else
const char *HTTP_SERVER_NAME = "pt22_winserver2.nuvoton.com";
#endif

#ifndef LOCAL_LAN
const char *HTTP_SERVER_FILE_PATH = "/media/uploads/mbed_official/hello.txt";
const int HTTP_SERVER_PORT = 80;
#else
const char *HTTP_SERVER_FILE_PATH = "/examples/arm_mbed/hello.txt";
const int HTTP_SERVER_PORT = 8080;
#endif


    const int RECV_BUFFER_SIZE = 512;

    // Test related data
    const char *HTTP_OK_STR = "200 OK";
    const char *HTTP_HELLO_STR = "Hello world!";

    // Test buffers
    char buffer[RECV_BUFFER_SIZE] = {0};
}

bool find_substring(const char *first, const char *last, const char *s_first, const char *s_last) {
    const char *f = std::search(first, last, s_first, s_last);
    return (f != last);
}


Serial output(USBTX, USBRX);

int main() {
    // Sets the console baud-rate
    output.baud(115200);
    output.printf(" Start WiFi test \r\n");
     
    bool result = true;
     int rc = 0;

#ifndef __CC_ARM 
#ifdef __GNUC__
    output.printf("sbrk=%x:\r\n", (unsigned int)_sbrk(0));
#endif
#endif 
    output.printf(" Start Connection ... \r\n");


    NetworkInterface *network_interface = 0;
   
#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI
    output.printf("\n\rUsing WiFi \r\n");
    output.printf("\n\rConnecting to WiFi..\r\n");
    rc = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD);
    network_interface = &esp;
#elif MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
    output.printf("Using Ethernet\r\n");
    rc = eth.connect();
    network_interface = &eth;
#endif
#ifdef MESH
    output.printf("Using Mesh\r\n");
    output.printf("\n\rConnecting to Mesh..\r\n");
    rc = mesh.connect();
    network_interface = &mesh;
#endif

    if(rc == 0) {
        output.printf("\n\rConnected to Network successfully\r\n");
    } else {
        output.printf("\n\rConnection to Network Failed %d! Exiting application....\r\n", rc);
        return 0;
    }    
        
    output.printf("TCP client IP Address is %s\r\n", network_interface->get_ip_address());

    TCPSocket sock(network_interface);
    output.printf(" HTTP Connection ... \r\n");
    if (sock.connect(HTTP_SERVER_NAME, HTTP_SERVER_PORT) == 0) {
        output.printf("HTTP: Connected to %s:%d\r\n", HTTP_SERVER_NAME, HTTP_SERVER_PORT);

        // We are constructing GET command like this:
        // GET http://developer.mbed.org/media/uploads/mbed_official/hello.txt HTTP/1.0\n\n
        strcpy(buffer, "GET http://");
        strcat(buffer, HTTP_SERVER_NAME);
        strcat(buffer, HTTP_SERVER_FILE_PATH);
        strcat(buffer, " HTTP/1.0\n\n");
        // Send GET command
        sock.send(buffer, strlen(buffer));

        // Server will respond with HTTP GET's success code
        const int ret = sock.recv(buffer, sizeof(buffer) - 1);
        buffer[ret] = '\0';

        // Find 200 OK HTTP status in reply
        bool found_200_ok = find_substring(buffer, buffer + ret, HTTP_OK_STR, HTTP_OK_STR + strlen(HTTP_OK_STR));
        // Find "Hello World!" string in reply
        bool found_hello = find_substring(buffer, buffer + ret, HTTP_HELLO_STR, HTTP_HELLO_STR + strlen(HTTP_HELLO_STR));

        TEST_ASSERT_TRUE(found_200_ok);
        TEST_ASSERT_TRUE(found_hello);

        if (!found_200_ok) result = false;
        if (!found_hello) result = false;

        output.printf("HTTP: Received %d chars from server\r\n", ret);
        output.printf("HTTP: Received 200 OK status ... %s\r\n", found_200_ok ? "[OK]" : "[FAIL]");
        output.printf("HTTP: Received '%s' status ... %s\r\n", HTTP_HELLO_STR, found_hello ? "[OK]" : "[FAIL]");
        output.printf("HTTP: Received massage:\r\n\r\n");
        output.printf("%s", buffer);
    }

#ifndef __CC_ARM 
#ifdef __GNUC__
    output.printf("sbrk=%x:\r\n", (unsigned int)_sbrk(0));
#endif
#endif 
    output.printf(" Close socket & disconnect ... \r\n");
    sock.close();
    
#if MBED_CONF_APP_NETWORK_INTERFACE == WIFI
    ((ESP8266Interface *)network_interface)->disconnect();
#elif MBED_CONF_APP_NETWORK_INTERFACE == ETHERNET
    ((EthernetInterface *)network_interface)->disconnect();
#elif MBED_CONF_APP_NETWORK_INTERFACE == MESH_LOWPAN_ND
    ((LoWPANNDInterface *)network_interface)->disconnect();
#elif MBED_CONF_APP_NETWORK_INTERFACE == MESH_THREAD
    ((ThreadInterface *)network_interface)->disconnect();
#endif
    output.printf(" End \r\n");
}
