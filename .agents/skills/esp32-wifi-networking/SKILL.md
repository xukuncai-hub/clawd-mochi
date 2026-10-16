---
name: esp32-wifi-networking
description: Complete engineering guide and diagnostic runbook for ESP32 Wi-Fi networking (SoftAP, Station, Captive Portal, HTTP WebServer). Use when debugging connection rejection (拒绝接入), authentication failures, PMF/SAE mismatch, DHCP leases, or mobile web controller integration.
---

# ESP32 Wi-Fi & Embedded Networking Skill

This skill contains field-tested patterns and diagnostic procedures for establishing rock-solid Wi-Fi SoftAP hotspots, Station clients, and captive web portals across iOS, Android, macOS, and Windows.

---

## 1. SoftAP Architecture & The "拒绝接入" (Connection Rejected) Prevention Protocol

Modern smartphones (iOS 15+, Android 12+) have aggressive Wi-Fi security heuristics. To guarantee 100% instant association without "拒绝接入" or "无法加入网络":

### Rule 1: Always Enforce RAM-Only Storage
By default, ESP-IDF writes Wi-Fi configurations into NVS Flash (`WIFI_STORAGE_FLASH`). When firmware changes security types or passwords, stale flash entries cause corrupt 4-way handshakes and auth rejections.
**Solution**:
```c
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));
// Crucial: isolate Wi-Fi state from dirty NVS flash
ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
```

### Rule 2: Pure WPA2-PSK (AES-CCMP) Without TKIP
- NEVER use `WIFI_AUTH_WPA_WPA2_PSK` in SoftAP mode. WPA1 uses broken TKIP encryption, which modern mobile operating systems actively block with "拒绝接入".
- Always configure:
  ```c
  .authmode = WIFI_AUTH_WPA2_PSK,
  .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP,
  ```

### Rule 3: Disable PMF (Protected Management Frames) for SoftAP
- If `pmf_cfg.capable = true`, mobile devices negotiate 802.11w Protected Management Frames. Any mismatch in timing, SAE, or key derivation results in `wifi:m f auth` and immediate client disassociation.
- Set PMF to completely disabled:
  ```c
  .pmf_cfg = {
      .capable = false,
      .required = false,
  },
  ```

### Rule 4: Disable SoftAP SAE Support in sdkconfig
- In `sdkconfig`, set:
  ```ini
  # CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT is not set
  ```
- This prevents the Wi-Fi driver from advertising WPA3-SAE AKMs on the SoftAP beacon, ensuring consistent WPA2-PSK 4-way handshake behavior.

### Rule 5: Disable Wi-Fi Modem Power Saving
- Modern smartphones drop connections if the AP goes to sleep or misses DTIM beacons:
  ```c
  esp_wifi_set_ps(WIFI_PS_NONE);
  ```

---

## 2. Captive Portal & Auto-Popup Architecture

When a smartphone connects to a SoftAP without upstream internet, it probes connectivity check endpoints (e.g. `captive.apple.com`, `connectivitycheck.gstatic.com`, `/generate_204`). If queries time out, the phone will automatically drop the Wi-Fi and fallback to 4G/5G cellular.

### Implementation Checklist
1. **DNS Hijacking (UDP Port 53)**:
   - Run a lightweight FreeRTOS task listening on UDP 53.
   - For every incoming DNS query, return answer `192.168.4.1` with TTL 60s.
2. **HTTP 302 Redirect Handlers**:
   - Register endpoints: `/generate_204`, `/gen_204`, `/hotspot-detect.html`, `/library/test/success.html`.
   - Send HTTP 302 redirect: `Location: http://192.168.4.1/`.
3. **Catch-All 404 Fallback**:
   - Register `httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler)` to redirect all unexpected probes directly to `http://192.168.4.1/`.

---

## 3. Wi-Fi Diagnostic Runbook

| Log Message | Root Cause | Solution |
| :--- | :--- | :--- |
| `W (...) wifi:m f auth` | Authentication Management Frame rejected by AP or STA | Check authmode, disable PMF (`pmf_cfg.capable=false`), ensure password is $\ge 8$ chars, set `WIFI_STORAGE_RAM`. |
| `W (...) wifi:m f deauth reason:2` | Previous authentication no longer valid | Phone is using cached credentials from a previous network of the same SSID. Tell user to "Forget Network" or change SSID. |
| `W (...) wifi:m f deauth reason:15` | 4-Way Handshake timeout | Poor RF signal or high CPU load during handshake. Lower render task priority during handshake. |
| `DHCP server: lease fail` | Client IP address pool exhausted | In `esp_netif_create_default_wifi_ap()`, ensure max connections matches DHCP lease pool size. |
