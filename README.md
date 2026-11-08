# captive-wifi

Wi-Fi station management with a captive-portal fallback for **ESP-IDF 5.x**.

> **Credits.** This component is built on the idea of
> **[esp32-wifi-manager](https://github.com/tonyp7/esp32-wifi-manager)** by **Tony Pottier**
> (MIT licence): an ESP32 that falls back to an access point with a web page where the user
> picks a network and enters the password. `captive-wifi` follows that concept and workflow and
> takes its structure from the official ESP-IDF example
> [`examples/protocols/http_server/captive_portal`](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/captive_portal)
> (DNS catch-all, DHCP option 114). The code is a new implementation written against current,
> driver-independent ESP-IDF 5.x APIs. Thank you, Tony, for the original project.

Created by Aram Vartanyan, (C) 2026. MIT licence (see [LICENSE](LICENSE)).

**Version:** 0.2.1 — tested on hardware: ESP32-S3 (ESP-IDF 5.5.4) in solar-heater-hap; 0.2.0 on
ESP32-S2 (ESP-IDF 5.5.4) in [mppt-1k](https://github.com/AramVartanyan/mppt-1k).

## Features

- Credentials stored in NVS (namespace `cwifi`); STA with automatic reconnect.
- **The stored network is never given up**: failed attempts come in bursts with a pause in between
  (default 5 attempts, then 1 min), so the device reconnects by itself as soon as the router is back
  (e.g. after a power cut, when the router starts slower than the device).
- No stored network, or the stored one never answers since boot → SoftAP with a captive portal:
  - DNS server answering every name with `192.168.4.1`;
  - DHCP option 114 (captive-portal URI) — the sign-in page opens by itself on iOS, Android and Windows;
  - web page: scanned networks with signal strength, password field, connect button, status.
- The chosen network is tried **while the portal stays up**; credentials are saved **only after
  the network gives an IP**, so a typo never replaces a working network.
- After a successful connection the portal stops (10 s later) and the device stays in STA — **no reboot**.
- Event callback for the application (LED, display, starting HomeKit, ...).
- No dependencies beyond ESP-IDF components: `esp_wifi`, `esp_netif`, `esp_event`, `nvs_flash`,
  `esp_http_server`, `lwip`.

## Behaviour

1. `captive_wifi_start()`: credentials in NVS → STA, connect. Failed attempts come in bursts of
   `CONFIG_CAPTIVE_WIFI_RETRY_BURST`, with `CONFIG_CAPTIVE_WIFI_RETRY_PAUSE_S` between bursts — forever.
2. No credentials, or `CONFIG_CAPTIVE_WIFI_MAX_RETRIES` failed attempts without ever connecting since
   boot → SoftAP `<prefix>-xxxxxx` (prefix from Kconfig, last 3 bytes of the MAC),
   open or WPA2 with the Kconfig password.
   While the portal is open and a network is stored, the stored network is **tried in the background**
   once per pause (APSTA mode); as soon as it connects, the portal closes by itself. The device never
   stays in the portal without trying the network it already knows. During such an attempt the
   SoftAP may follow the station to another channel for a moment, so a phone on the portal can drop
   briefly (single 2.4 GHz radio — the same on ESP32, -S2, -S3, -C3, -C6).
3. Portal endpoints: `/` (page), `/scan`, `/connect`, `/status`; any other URL redirects to the page.
4. The device tries the chosen network; the page polls `/status`. A failed attempt (3 disconnects
   or 30 s) is reported on the page and with `CAPTIVE_WIFI_EVT_PORTAL_CONNECT_FAILED`.
5. On success: credentials saved, `CAPTIVE_WIFI_EVT_PORTAL_STOPPED` is sent **before**
   `CAPTIVE_WIFI_EVT_STA_CONNECTED`, so nothing started on the connection meets the portal's web server.
6. `captive_wifi_clear_credentials()` + `captive_wifi_start()` → back to the portal (e.g. "Reset Wi-Fi").

## API

```c
#include "captive_wifi.h"

esp_err_t   captive_wifi_init(const char *hostname, captive_wifi_cb_t cb, void *ctx);
esp_err_t   captive_wifi_start(void);              /* STA or portal, non-blocking */
esp_err_t   captive_wifi_stop(void);               /* stop Wi-Fi entirely */

bool        captive_wifi_has_credentials(void);
esp_err_t   captive_wifi_clear_credentials(void);  /* next start → portal */
bool        captive_wifi_is_connected(void);
bool        captive_wifi_portal_active(void);
const char *captive_wifi_ap_ssid(void);
const char *captive_wifi_portal_ip(void);          /* "192.168.4.1" */
esp_err_t   captive_wifi_get_ip(char *buf, size_t len);
int         captive_wifi_rssi(void);               /* dBm, 0 when not connected */
```

Events (`captive_wifi_event_t`): `STA_CONNECTED` (got IP), `STA_DISCONNECTED`, `PORTAL_STARTED`,
`PORTAL_STOPPED`, `PORTAL_CONNECT_FAILED`.

```c
static void wifi_event(captive_wifi_event_t evt, void *ctx)
{
    switch (evt) {
    case CAPTIVE_WIFI_EVT_STA_CONNECTED:    /* start mDNS / HomeKit, LED on */      break;
    case CAPTIVE_WIFI_EVT_STA_DISCONNECTED: /* LED off */                           break;
    case CAPTIVE_WIFI_EVT_PORTAL_STARTED:   /* show captive_wifi_ap_ssid() */       break;
    default: break;
    }
}

captive_wifi_init("my-device", wifi_event, NULL);
captive_wifi_start();
```

## Configuration (Kconfig — "Captive Wi-Fi")

| Option | Default | Description |
|---|---|---|
| `CONFIG_CAPTIVE_WIFI_AP_SSID_PREFIX` | `ESP` | Short device name for the SoftAP: `<prefix>-XXXXXX` (max 24 characters) |
| `CONFIG_CAPTIVE_WIFI_AP_PASSWORD` | empty | SoftAP password (empty = open network) |
| `CONFIG_CAPTIVE_WIFI_MAX_RETRIES` | 10 | Failed attempts (never connected since boot) before the portal starts |
| `CONFIG_CAPTIVE_WIFI_RETRY_BURST` | 5 | Connection attempts in a row before a pause |
| `CONFIG_CAPTIVE_WIFI_RETRY_PAUSE_S` | 60 | Pause between bursts, and between background attempts while the portal is open (10 – 120 s) |

## Using it in a project

The repository root is an ESP-IDF component. Either:

- add it as a git submodule in a components folder of the project
  (e.g. `project/common/captive-wifi`) listed in `EXTRA_COMPONENT_DIRS`, and add `captive-wifi`
  to `REQUIRES` / `PRIV_REQUIRES` of `main`; or
- clone it next to the project and point `EXTRA_COMPONENT_DIRS` to it.

### Coexistence with HomeKit (esp-homekit-sdk)

The HomeKit SDK runs its own `esp_http_server` on port 80. `captive-wifi` runs its web server
**only while the portal is active** (AP mode, before `hap_start()`) and stops it before the
connection is reported, so the two servers never listen on port 80 at the same time.

## History

See [Changelog.md](Changelog.md).
