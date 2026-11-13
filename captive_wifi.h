/*
 * captive-wifi — Wi-Fi station management with captive-portal fallback for
 * ESP-IDF 5.x. Created by Aram Vartanyan, (C) 2026. MIT licence.
 *
 * Boot: a network stored by the Wi-Fi driver (NVS) → STA with reconnect; none, or too many failed
 * attempts → open SoftAP with a captive portal (DNS catch-all + DHCP option
 * 114) serving a page that scans networks and stores the chosen credentials.
 * Optional: the portal closes and the radio goes off after a time without a client when no network
 * is stored (CONFIG_CAPTIVE_WIFI_PORTAL_IDLE_MIN); an access-point-only mode for the application's
 * own web interface (captive_wifi_start_ap(), no portal, no station), with client events and an
 * idle time-out.
 * Follows the idea of tonyp7/esp32-wifi-manager; structure from the ESP-IDF
 * captive_portal example. Uses driver-independent ESP-IDF APIs only.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAPTIVE_WIFI_VERSION "0.4.0"

typedef enum {
    CAPTIVE_WIFI_EVT_STA_CONNECTED = 0,   /* got IP */
    CAPTIVE_WIFI_EVT_STA_DISCONNECTED,
    CAPTIVE_WIFI_EVT_PORTAL_STARTED,      /* SoftAP + portal up */
    CAPTIVE_WIFI_EVT_PORTAL_STOPPED,      /* sent before STA_CONNECTED */
    CAPTIVE_WIFI_EVT_PORTAL_CONNECT_FAILED, /* network chosen on the page did not connect */
    /* 0.4.0 */
    CAPTIVE_WIFI_EVT_PORTAL_TIMEOUT,      /* no network stored, no client for PORTAL_IDLE_MIN: the
                                             portal closed (PORTAL_STOPPED before it), radio off */
    CAPTIVE_WIFI_EVT_AP_STARTED,          /* captive_wifi_start_ap(): the access point is up */
    CAPTIVE_WIFI_EVT_AP_STOPPED,          /* the access point is down (radio off) */
    CAPTIVE_WIFI_EVT_AP_TIMEOUT,          /* sent after AP_STOPPED when the cause was the idle time */
    CAPTIVE_WIFI_EVT_AP_CLIENT_CONNECTED, /* a station joined the access point */
    CAPTIVE_WIFI_EVT_AP_CLIENT_DISCONNECTED, /* a station left it */
} captive_wifi_event_t;

typedef void (*captive_wifi_cb_t)(captive_wifi_event_t evt, void *ctx);

/* esp_netif, default event loop, Wi-Fi driver, STA netif with `hostname`. */
esp_err_t captive_wifi_init(const char *hostname, captive_wifi_cb_t cb, void *ctx);

/* STA when credentials exist, otherwise the portal. Non-blocking. */
esp_err_t captive_wifi_start(void);

/* Stop Wi-Fi entirely (used when the Wi-Fi setting is switched off); ends the access point too. */
esp_err_t captive_wifi_stop(void);

/* Access point only (0.4.0): the SoftAP "<prefix>-xxxxxx" at 192.168.4.1 (open or WPA2 with the
 * Kconfig password), no station, no portal, no DNS catch-all; DHCP tells the phones that there is no
 * captive portal (RFC 8910), so no sign-in window opens — the application serves its own pages on
 * the access point. Ends the station / the portal first (STA_DISCONNECTED, PORTAL_STOPPED). With
 * `idle_s` > 0 the access point goes down after `idle_s` without any client — counted from the start
 * and from the last client leaving — and AP_STOPPED, AP_TIMEOUT are sent (radio off; the application
 * may call captive_wifi_start() then, from its own task). Non-blocking. captive_wifi_start() or
 * captive_wifi_stop() end it as well. */
esp_err_t captive_wifi_start_ap(uint32_t idle_s);

bool        captive_wifi_has_credentials(void);
esp_err_t   captive_wifi_clear_credentials(void);   /* esp_wifi_restore(); next start → portal */
bool        captive_wifi_is_connected(void);
bool        captive_wifi_portal_active(void);
bool        captive_wifi_ap_active(void);           /* captive_wifi_start_ap() is running */
int         captive_wifi_ap_clients(void);          /* stations on the access point / the portal */
const char *captive_wifi_ap_ssid(void);             /* "<prefix>-xxxxxx" */
const char *captive_wifi_portal_ip(void);           /* "192.168.4.1" */
esp_err_t   captive_wifi_get_ip(char *buf, size_t len);
int         captive_wifi_rssi(void);                /* dBm, 0 when not connected */

#ifdef __cplusplus
}
#endif
