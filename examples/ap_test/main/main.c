/* captive-wifi 0.4.0 hardware test.
 *
 * - Start: a stored network → the router; none → the portal, which closes after 2 min without a
 *   phone (CONFIG_CAPTIVE_WIFI_PORTAL_IDLE_MIN=2): "PORTAL_TIMEOUT", radio off.
 * - BOOT button (GPIO0) short press: access point only for 2 min of idle time (no sign-in window on
 *   the phone; open http://192.168.4.1 — a page with the client count). Leaving it → after 2 min
 *   "AP_TIMEOUT" and back to captive_wifi_start(). A second press while the access point runs: back
 *   at once.
 * - BOOT held 3 s: forget the network (captive_wifi_clear_credentials()) and start again → portal.
 * Everything is in the log (115200). */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "captive_wifi.h"

static const char *TAG = "ap_test";
static QueueHandle_t s_q;
static httpd_handle_t s_web;

static const char *const NAMES[] = {
    "STA_CONNECTED", "STA_DISCONNECTED", "PORTAL_STARTED", "PORTAL_STOPPED", "PORTAL_CONNECT_FAILED",
    "PORTAL_TIMEOUT", "AP_STARTED", "AP_STOPPED", "AP_TIMEOUT", "AP_CLIENT_CONNECTED",
    "AP_CLIENT_DISCONNECTED",
};

static esp_err_t page(httpd_req_t *req)
{
    char html[200];
    snprintf(html, sizeof(html), "<!doctype html><meta name=viewport content='width=device-width'>"
             "<h2>captive-wifi %s</h2><p>Access point only, %d client(s).</p>", CAPTIVE_WIFI_VERSION,
             captive_wifi_ap_clients());
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, html);
}

static void web(bool on)
{
    if (on && !s_web) {
        httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
        if (httpd_start(&s_web, &cfg) == ESP_OK) {
            const httpd_uri_t u = { .uri = "/", .method = HTTP_GET, .handler = page };
            httpd_register_uri_handler(s_web, &u);
        }
    } else if (!on && s_web) {
        httpd_stop(s_web);
        s_web = NULL;
    }
}

/* runs in the component's task: hand over to app_main */
static void wifi_event(captive_wifi_event_t evt, void *ctx)
{
    (void)ctx;
    int e = (int)evt;
    xQueueSend(s_q, &e, 0);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    s_q = xQueueCreate(16, sizeof(int));
    gpio_config_t io = { .pin_bit_mask = 1ULL << GPIO_NUM_0, .mode = GPIO_MODE_INPUT,
                         .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&io);
    ESP_ERROR_CHECK(captive_wifi_init("cwtest", wifi_event, NULL));
    ESP_LOGI(TAG, "captive-wifi %s, AP %s; BOOT: access point only, held 3 s: forget the network",
             CAPTIVE_WIFI_VERSION, captive_wifi_ap_ssid());
    captive_wifi_start();

    int held = 0;
    for (;;) {
        int e;
        while (xQueueReceive(s_q, &e, 0) == pdTRUE) {
            ESP_LOGW(TAG, "event %s (AP only %d, clients %d)",
                     e < (int)(sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[e] : "?",
                     captive_wifi_ap_active(), captive_wifi_ap_clients());
            if (e == CAPTIVE_WIFI_EVT_AP_STARTED) {
                web(true);
            } else if (e == CAPTIVE_WIFI_EVT_AP_STOPPED) {
                web(false);
            } else if (e == CAPTIVE_WIFI_EVT_AP_TIMEOUT) {
                ESP_LOGI(TAG, "back to the router / the portal");
                captive_wifi_start();
            }
        }
        if (gpio_get_level(GPIO_NUM_0) == 0) {
            if (++held == 30) {
                ESP_LOGI(TAG, "forget the network");
                captive_wifi_stop();
                captive_wifi_clear_credentials();
                captive_wifi_start();
            }
        } else {
            if (held > 0 && held < 30) {
                if (captive_wifi_ap_active()) {
                    ESP_LOGI(TAG, "access point only: back now");
                    captive_wifi_start();
                } else {
                    ESP_LOGI(TAG, "access point only, 2 min idle limit");
                    captive_wifi_start_ap(120);
                }
            }
            held = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
