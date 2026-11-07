# Changelog

All notable changes to this component are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- `README.md` — credits to the original project (tonyp7/esp32-wifi-manager by Tony Pottier) and
  the ESP-IDF captive_portal example, features, behaviour, API, Kconfig, integration notes.
- `Changelog.md` — this file, with the history from the mppt-1k project.

## [0.2.1] — in development (not tested)

### Added
- `CONFIG_CAPTIVE_WIFI_AP_SSID_PREFIX` — short device name for the setup SoftAP
  (`<prefix>-XXXXXX`, default `ESP`, max 24 characters). Replaces the fixed `MPPT-` prefix,
  so the component can be shared between projects (mppt-1k: `MPPT`).
- The stored network is never given up: failed attempts come in bursts
  (`CONFIG_CAPTIVE_WIFI_RETRY_BURST`, default 5) with a pause in between
  (`CONFIG_CAPTIVE_WIFI_RETRY_PAUSE_S`, default 60 s, max 120 s), forever.
- While the portal is open and a network is stored, the stored network is tried in the background
  once per pause (APSTA); when it connects, the portal closes by itself (`PORTAL_STOPPED`, then
  `STA_CONNECTED`). A connection started from the page takes priority over the background attempt.

### Removed
- `CONFIG_CAPTIVE_WIFI_PORTAL_TIMEOUT_MIN` — replaced by the background attempts above. With the old
  default (0) the device stayed in the portal forever after a power cut in which the router started
  slower than the device.

### Fixed
- `captive_wifi_stop()`: the disconnect caused by stopping Wi-Fi no longer calls `esp_wifi_connect()`.

## [0.2.0] — 2026-10-31 (developed in mppt-1k, tested on hardware)

### Added
- Captive portal: SoftAP (open, or WPA2 with the Kconfig password) with
  - a DNS server answering every name with `192.168.4.1` and DHCP option 114, so phones open
    the sign-in page by themselves;
  - a web page on port 80: scanned networks with signal, password field, connect button and
    status (`/`, `/scan`, `/connect`, `/status`; any other URL redirects to the page).
- The chosen network is tried while the portal stays up; credentials are saved only after it
  gives an IP; 10 s later the portal stops and the device stays in STA without a reboot.
- Event `CAPTIVE_WIFI_EVT_PORTAL_CONNECT_FAILED`; `PORTAL_STOPPED` is sent before `STA_CONNECTED`.

### Fixed
- `portal_start()` only logged that the portal was not implemented but still reported it as
  started.

## [0.1.0] — 2026-09-22 (developed in mppt-1k)

### Added
- STA part: credentials in NVS (namespace `cwifi`), connect with retries, reconnect,
  events to the application (`STA_CONNECTED`, `STA_DISCONNECTED`, `PORTAL_STARTED`, `PORTAL_STOPPED`).
- Public API (`captive_wifi.h`), Kconfig options (AP password, max retries, portal timeout).
- Replaces `app_wifi` from the HomeKit SDK template: kept `esp_netif_init`, default event loop,
  STA netif with hostname, `WIFI_INIT_CONFIG_DEFAULT`, reconnect on disconnect, IPv6 link-local;
  dropped `wifi_provisioning` (deprecated in ESP-IDF 5.x), QR code, WAC and hard-coded credentials.

