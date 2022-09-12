#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "cmd_system.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_console.h"


static bool s_reconnect = true;
static const char *TAG_ANCHOR = "gtec-ftm-anchor";

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;

const wifi_bandwidth_t  CURRENT_BW = WIFI_BW_HT20; //enumerator; WIFI_BW_HT20 = 1
const uint8_t  CURRENT_CHANNEL = 1; //a cross-platform type that is used to create an exact amount of 8 bits  with or without sign, no matter which platform the program runs on.


static void wifi_connected_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;

    ESP_LOGI(TAG_ANCHOR, "Connected to %s (BSSID: "MACSTR", Channel: %d)", event->ssid,
             MAC2STR(event->bssid), event->channel);

    xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (s_reconnect) {
        ESP_LOGI(TAG_ANCHOR, "sta disconnect, s_reconnect...");
        esp_wifi_connect();
    } else {
        ESP_LOGI(TAG_ANCHOR, "sta disconnect");
    }
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
}


void initialise_wifi(void)
{
    static bool initialized = false;

    if (initialized) {
        return;
    }
  
    /* ESP_ERROR_CHECK macro serves similar purpose as assert, except that it checks esp_err_t value rather than a bool condition.
    If the argument of ESP_ERROR_CHECK is not equal ESP_OK, then an error message is printed on the console, and abort() is called. 
    There are other variants like ESP_ERROR_CHECK_WITHOUT_ABORT, ESP_RETURN_ON_ERROR */
    
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_CONNECTED, // WIFI_MODE_STA is the station mode, which is the standard client mode
                    &wifi_connected_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_DISCONNECTED,
                    &disconnect_handler,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL) ); // The null mode or the WIFI_MODE_OFF is basically the OFF mode 

    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(ESP_IF_WIFI_AP, CURRENT_BW)); // WIFI_MODE_AP is the Access Point Mode where the clients connect to the ESP32; set_bandwidth is used to set the current bandwidth value to the access point
    ESP_ERROR_CHECK(esp_wifi_start() ); /*Start WiFi according to current configuration (Here, it is the anchor mode, WIFI_MODE_AP).
          If mode is WIFI_MODE_STA, it create station control block and start station. 
          If mode is WIFI_MODE_AP, it create soft-AP control block and start soft-AP.
          If mode is WIFI_MODE_APSTA, it create soft-AP and station control block and start soft-AP and station.*/
    initialized = true;
}


static bool start_wifi_ap(const char* ssid, const char* pass)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .max_connection = 4,
            .password = "",
            .authmode = WIFI_AUTH_WPA2_PSK,
            .channel = CURRENT_CHANNEL

        },
    };

    s_reconnect = false;
    strlcpy((char*) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    strlcpy((char*) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));


    if (strlen(pass) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    return true;
}


void app_main(void)
{
    uint8_t mac[6]; // no. of tags? 
    char mac_add[17]; // referring to the physical mac address of the tag?

    wifi_bandwidth_t bw; //a specific datatype (basically an enumerator that has two ? WIFI_BW_20 = 1 and WIFI_BW_40)

    esp_err_t ret = nvs_flash_init(); //
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    initialise_wifi();

    printf("\n ==========================================================\n");
    printf(" |                      ESP32 S2 FTM ANCHOR                 |\n");
    printf(" ==========================================================\n\n");

    //GET MAC ADDRESS
    ESP_ERROR_CHECK(esp_base_mac_addr_get(&mac[0]));
    sprintf(&mac_add[0],"ftm_%02X%02X%02X%02X%02X%02X",(unsigned char)mac[0], (unsigned char)mac[1], (unsigned char)mac[2], (unsigned char)mac[3], (unsigned char)mac[4],(unsigned char)mac[5]);


    //Start AP
    start_wifi_ap(mac_add, "ftmftmftmftm");

    ESP_ERROR_CHECK(esp_wifi_get_bandwidth(ESP_IF_WIFI_AP, &bw));

    if (bw==WIFI_BW_HT20){
        printf("BW = 20Mhz\n");
    } else {
        printf("BW = 40Mhz\n");
    }

    printf("Starting SoftAP with FTM Responder support, SSID - %s\n", mac_add);




}
