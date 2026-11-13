# Changelog

All notable changes to this component are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.4.0] — not yet tested on hardware

### Added
- Portal idle limit `CONFIG_CAPTIVE_WIFI_PORTAL_IDLE_MIN` (0 = never, the default — as before):
  with **no stored network**, when no phone is on the portal for that long (from the start and from
  the last client leaving, not during a page attempt) the portal closes, the radio goes off and
  `CAPTIVE_WIFI_EVT_PORTAL_TIMEOUT` is sent (after `PORTAL_STOPPED`); `captive_wifi_start()` starts
  again. With a stored network the portal stays and keeps trying it, as before.
- Access point only (`CONFIG_CAPTIVE_WIFI_AP_ONLY`, off by default — a firmware without its own web
  interface keeps the portal only; then `captive_wifi_start_ap()` returns `ESP_ERR_NOT_SUPPORTED`):
  `captive_wifi_start_ap(idle_s)` — the SoftAP `<prefix>-xxxxxx` at 192.168.4.1
  without the portal, the station and the DNS catch-all; DHCP option 114 says "no captive portal"
  (`urn:ietf:params:capport:unrestricted`, RFC 8910), so phones open no sign-in window and the
  application serves its own pages on port 80. Events `AP_STARTED`, `AP_STOPPED`,
  `AP_CLIENT_CONNECTED`, `AP_CLIENT_DISCONNECTED`; with `idle_s` > 0 the access point goes down
  after `idle_s` without a client (`AP_STOPPED`, then `AP_TIMEOUT`, radio off).
  `captive_wifi_start()` and `captive_wifi_stop()` end it too. `captive_wifi_ap_active()`,
  `captive_wifi_ap_clients()`.
- The new events are added at the end of `captive_wifi_event_t`; the old values do not change.

## [0.3.0] — 2026-11-08, tested on hardware

Tested on ESP32-S3 (ESP-IDF 5.5.4) in solar-heater-hap, starting from erased flash: no stored
network → portal → network chosen → connected, directly connected after a reboot; a wrong password
for another network while the stored one was out of reach → red "Wrong password" (reason 15), the
stored network was put back and connected by itself from the next background attempt, portal closed;
`captive_wifi_clear_credentials()` → restart → portal; erasing the whole NVS partition → portal.

### Changed
- The network is kept by the Wi-Fi driver (station config in NVS, `WIFI_STORAGE_FLASH` — the
  ESP-IDF default) instead of an own NVS namespace: `esp_wifi_get_config()` shows the stored
  network to any other code, `esp_wifi_restore()` clears it. The network chosen on the page is
  stored when the attempt starts; if it does not connect, the previously stored network is put back
  (a typo still never replaces a working network). Background attempts no longer set the config
  again (no NVS write per attempt).
- `captive_wifi_clear_credentials()` calls `esp_wifi_restore()` (needs `captive_wifi_init()` first).
- Portal: clearer messages when the page's network does not connect, shown in red: wrong password,
  network not found, signal too weak, security type not supported, router refused the connection,
  no answer; any other case shows the reason code. Reason 205 (`WIFI_REASON_CONNECTION_FAIL`) now
  counts as a wrong password too — seen on hardware (ESP-IDF 5.5.4) with a wrong password on a
  mesh network, where the page used to show only "Not connected: connection failed".
- Log: the failed page attempt names the cause next to the reason code.

## [0.2.2] — tested on hardware

Tested on ESP32-S3 (ESP-IDF 5.5.4) in solar-heater-hap: a network chosen on the page 3.4 s after a
background attempt started — the driver went on with the new config by itself, associated 110 ms
after the click, no driver error or warning, portal closed without a reboot.

### Fixed
- Portal: a network chosen on the page while a background attempt of the stored network was
  running — `esp_wifi_connect()` right after `esp_wifi_disconnect()` was refused by the driver
  (`E wifi: sta is connecting`). On hardware the driver goes on by itself with the new config
  (auth ~90 ms after the click), so no call is made at that moment any more. As a safety net,
  if within 5 s there is neither an association nor a failed attempt, the worker task starts the
  attempt once. (An intermediate version retried every 0.5 s; on hardware that only produced
  errors and, once connected, a "sta is connected, disconnect before connecting" warning.)
- Portal: choosing another network within the 10 s after a successful connection no longer lets
  the pending portal stop cut the new attempt short.
- Portal: `esp_wifi_set_config()` right after `esp_wifi_disconnect()` can fail with
  `ESP_ERR_WIFI_STATE` (the disconnect is asynchronous; the driver behaviour differs between
  ESP-IDF versions). The result was ignored, so the interrupted attempt could go on with the old
  (stored) network and its credentials were then saved under the page's network. Now a refused
  config is set again by the worker task as soon as the driver accepts it, before the attempt is
  started; events of the interrupted attempt do not count as tries of the page's network; and a
  connection to a different SSID than the chosen one is dropped and the chosen one is used.

## [0.2.1] — tested on hardware

Tested on ESP32-S3 (Waveshare ESP32-S3-Zero, ESP-IDF 5.5.4) in solar-heater-hap:
SoftAP `SOLAR-XXXXXX` from the Kconfig prefix; portal page, connection to the chosen network and
portal closing without a reboot; reconnection to the stored network after a reboot; router off
while connected → bursts of 5 attempts with 60 s pauses, no portal, reconnection when the router
is back; boot with the router off → portal after 10 attempts, the stored network tried in the
background every 60 s, a network chosen on the page takes priority over a running background
attempt; the SoftAP follows the station channel (1 → 6).

### Added
- `README.md` — credits to the original project (tonyp7/esp32-wifi-manager by Tony Pottier) and
  the ESP-IDF captive_portal example, features, behaviour, API, Kconfig, integration notes.
- `Changelog.md` — this file, with the history from the mppt-1k project.
- `CONFIG_CAPTIVE_WIFI_AP_SSID_PREFIX` — short device name for the setup SoftAP
  (`<prefix>-XXXXXX`, default `ESP`, max 24 characters). Replaces the fixed `MPPT-` prefix,
  so the component can be shared between projects (mppt-1k: `MPPT`).
- The stored network is never given up: failed attempts come in bursts
  (`CONFIG_CAPTIVE_WIFI_RETRY_BURST`, default 5) with a pause in between
  (`CONFIG_CAPTIVE_WIFI_RETRY_PAUSE_S`, default 60 s, max 120 s), forever.
- While the portal is open and a network is stored, the stored network is tried in the background
  once per pause (APSTA); when it connects, the portal closes by itself (`PORTAL_STOPPED`, then
  `STA_CONNECTED`). A connection started from the page takes priority over the background attempt.

### Changed
- Portal page restyled after the iOS Settings look: grouped grey background (`#F2F1F6`), white
  inset groups with 24 px corners and inset separators, capsule buttons — "Connect" in green
  (`#66C468`), "Scan again" in blue (`#3F88F2`), the chosen network in blue with a checkmark,
  system font; the device name and "Wi-Fi setup" on separate lines in the title.

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

