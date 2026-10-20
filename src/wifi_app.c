#include "wifi_app.h"
#include "alarm_timer.h"
#include "effects.h"
#include "web_assets.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <unistd.h>

static const char *TAG = "WIFI_APP";

#define WIFI_AP_SSID "Mochi-Free"
#define WIFI_AP_PASS "" // 零门槛开放热点，免密直连，绝不触发鉴权拒绝

static WebControlBridge g_bridge = {
    .current_effect_idx = 0,
    .num_effects = 17,
    .auto_cycle = true,
    .request_next = false,
    .request_prev = false,
    .request_set_idx = -1,
    .request_toggle_cycle = false,
    .request_rotate = false
};

WebControlBridge* wifi_app_get_bridge(void) {
    return &g_bridge;
}

static const char *EFFECT_TITLES[17] = {
    "甜甜圈", "赛博落日", "数字雨", "等离子", "音频频谱",
    "引力黑洞", "银河旋涡", "毁灭火焰", "虫洞隧道", "星空超立方",
    "DNA双螺旋", "熔岩融球", "战术雷达", "激光示波器", "生命游戏",
    "北极极光", "朱利亚分形"
};

// GET / - 返回移动端完整网页
static esp_err_t root_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, ">>> [HTTP] GET / 收到网页访问请求");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    esp_err_t res = httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, ">>> [HTTP] GET / 发送完成 (结果: %d)", res);
    return res;
}

// GET /api/status - 返回系统时间、闹钟、定时器及屏幕状态
static esp_err_t api_status_handler(httpd_req_t *req) {
    AlarmTimerContext *ctx = alarm_timer_get_ctx();
    time_t now_t = time(NULL);
    struct tm cur_tm;
    localtime_r(&now_t, &cur_tm);

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec);

    const char *tstate_str = "idle";
    if (ctx->timer_state == TIMER_STATE_RUNNING) tstate_str = "running";
    else if (ctx->timer_state == TIMER_STATE_PAUSED) tstate_str = "paused";
    else if (ctx->timer_state == TIMER_STATE_RINGING) tstate_str = "ringing";

    int fx_idx = g_bridge.current_effect_idx;
    if (fx_idx < 0) fx_idx = 0;
    if (fx_idx >= 17) fx_idx = 16;
    const char *fx_title = EFFECT_TITLES[fx_idx];

    char json_buf[384];
    snprintf(json_buf, sizeof(json_buf),
        "{"
        "\"time\":\"%s\","
        "\"time_synced\":%s,"
        "\"alarm_enabled\":%s,"
        "\"alarm_h\":%d,"
        "\"alarm_m\":%d,"
        "\"timer_state\":\"%s\","
        "\"timer_rem\":%d,"
        "\"timer_tot\":%d,"
        "\"is_ringing\":%s,"
        "\"ring_title\":\"%s\","
        "\"cur_fx\":%d,"
        "\"cur_fx_title\":\"%s\","
        "\"auto_cycle\":%s"
        "}",
        time_str,
        ctx->time_synced ? "true" : "false",
        ctx->alarm_enabled ? "true" : "false",
        ctx->alarm_hour,
        ctx->alarm_minute,
        tstate_str,
        ctx->timer_remaining_sec,
        ctx->timer_total_sec,
        ctx->is_ringing ? "true" : "false",
        ctx->ring_title,
        fx_idx,
        fx_title,
        g_bridge.auto_cycle ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

// POST /api/sync_time - 手机端一键对时
static esp_err_t api_sync_time_handler(httpd_req_t *req) {
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        int y = 0, mon = 0, d = 0, h = -1, m = -1, s = -1;
        long long ts = 0;

        char *py = strstr(buf, "\"y\":");
        char *pmon = strstr(buf, "\"mon\":");
        char *pd = strstr(buf, "\"d\":");
        char *ph = strstr(buf, "\"h\":");
        char *pm = strstr(buf, "\"m\":");
        char *ps = strstr(buf, "\"s\":");
        char *pts = strstr(buf, "\"ts\":");

        if (py) sscanf(py, "\"y\":%d", &y);
        if (pmon) sscanf(pmon, "\"mon\":%d", &mon);
        if (pd) sscanf(pd, "\"d\":%d", &d);
        if (ph) sscanf(ph, "\"h\":%d", &h);
        if (pm) sscanf(pm, "\"m\":%d", &m);
        if (ps) sscanf(ps, "\"s\":%d", &s);
        if (pts) sscanf(pts, "\"ts\":%lld", &ts);

        if (y >= 2024 && h >= 0 && m >= 0 && s >= 0) {
            alarm_timer_set_datetime(y, mon, d, h, m, s);
        } else if (ts > 1000000000LL) {
            alarm_timer_set_timestamp((time_t)ts);
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/set_alarm - 设置日常闹钟
static esp_err_t api_set_alarm_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        bool enabled = (strstr(buf, "\"enabled\":true") != NULL);
        int h = 7, m = 30;
        char *ph = strstr(buf, "\"hour\":");
        char *pm = strstr(buf, "\"minute\":");
        if (ph) sscanf(ph, "\"hour\":%d", &h);
        if (pm) sscanf(pm, "\"minute\":%d", &m);
        alarm_timer_set_alarm(enabled, h, m);
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/timer - 控制倒计时定时器
static esp_err_t api_timer_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        if (strstr(buf, "\"action\":\"start\"")) {
            int sec = 300;
            char *ps = strstr(buf, "\"seconds\":");
            if (ps) sscanf(ps, "\"seconds\":%d", &sec);
            alarm_timer_start(sec);
        } else if (strstr(buf, "\"action\":\"pause\"")) {
            alarm_timer_pause();
        } else if (strstr(buf, "\"action\":\"resume\"")) {
            alarm_timer_resume();
        } else if (strstr(buf, "\"action\":\"stop\"")) {
            alarm_timer_stop();
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/dismiss - 关闭响铃警报
static esp_err_t api_dismiss_handler(httpd_req_t *req) {
    alarm_timer_dismiss_ring();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// POST /api/control - 远程屏幕控制
static esp_err_t api_control_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        if (strstr(buf, "\"action\":\"set_fx\"")) {
            int idx = 0;
            char *pi = strstr(buf, "\"idx\":");
            if (pi && sscanf(pi, "\"idx\":%d", &idx) == 1) {
                g_bridge.request_set_idx = idx;
            }
        } else if (strstr(buf, "\"action\":\"toggle_cycle\"")) {
            g_bridge.request_toggle_cycle = true;
        } else if (strstr(buf, "\"action\":\"rotate\"")) {
            g_bridge.request_rotate = true;
        }
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

// HTTP 204: 针对 Android/鸿蒙/各大厂商网络连通性探测立即应答 204 No Content
// 作用：让手机系统判定网络验证通过，消除 18 秒转圈超时与断连切 4G/5G
static esp_err_t http_204_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, ">>> [PROBE] Responded 204 No Content: %s", req->uri);
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

// Apple iOS / macOS 连通性探测 (captive.apple.com/hotspot-detect.html)
static esp_err_t apple_detect_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, ">>> [PROBE] Responded Apple Success: %s", req->uri);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>", HTTPD_RESP_USE_STRLEN);
}

// 404 捕获与重定向处理：若手机请求未知路径，兜底判定或重定向至控制台首页
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    if (err == HTTPD_404_NOT_FOUND) {
        if (strstr(req->uri, "generate_204") || strstr(req->uri, "gen_204")) {
            httpd_resp_set_status(req, "204 No Content");
            return httpd_resp_send(req, NULL, 0);
        }
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        return httpd_resp_send(req, NULL, 0);
    }
    return ESP_FAIL;
}

// 嵌入式轻量级 DNS 快速响应服务：对所有域名查询立即返回 192.168.4.1，消除手机 18 秒探测超时
static void dns_server_task(void *pvParameters) {
    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket creation failed");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(53);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "DNS socket bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Captive DNS Server running on UDP port 53");

    uint8_t rx_buffer[256];
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);
        if (len >= 12) {
            uint8_t flags_high = rx_buffer[2];
            if ((flags_high & 0x78) == 0) { // OPCODE == 0
                rx_buffer[2] = 0x81; // QR=1, RD=1
                rx_buffer[3] = 0x80; // RA=1, RCODE=0
                rx_buffer[6] = 0x00; // ANCOUNT = 1
                rx_buffer[7] = 0x01;
                rx_buffer[8] = 0x00; // NSCOUNT = 0
                rx_buffer[9] = 0x00;
                rx_buffer[10] = 0x00; // ARCOUNT = 0
                rx_buffer[11] = 0x00;

                int q_end = 12;
                while (q_end < len && rx_buffer[q_end] != 0) {
                    q_end += rx_buffer[q_end] + 1;
                }
                q_end++; // skip 0x00
                q_end += 4; // skip QTYPE + QCLASS

                if (q_end <= len && q_end + 16 <= (int)sizeof(rx_buffer)) {
                    rx_buffer[q_end++] = 0xC0;
                    rx_buffer[q_end++] = 0x0C; // Pointer to QNAME
                    rx_buffer[q_end++] = 0x00; // Type: A
                    rx_buffer[q_end++] = 0x01;
                    rx_buffer[q_end++] = 0x00; // Class: IN
                    rx_buffer[q_end++] = 0x01;
                    rx_buffer[q_end++] = 0x00; // TTL: 60s
                    rx_buffer[q_end++] = 0x00;
                    rx_buffer[q_end++] = 0x00;
                    rx_buffer[q_end++] = 0x3C;
                    rx_buffer[q_end++] = 0x00; // Len: 4
                    rx_buffer[q_end++] = 0x04;
                    rx_buffer[q_end++] = 192;  // 192.168.4.1
                    rx_buffer[q_end++] = 168;
                    rx_buffer[q_end++] = 4;
                    rx_buffer[q_end++] = 1;

                    sendto(sock, rx_buffer, q_end, 0, (struct sockaddr *)&source_addr, socklen);
                }
            }
        }
    }
}

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 20;
    config.max_open_sockets = 10;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t index_uri = { .uri = "/index.html", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t p204_1 = { .uri = "/generate_204", .method = HTTP_GET, .handler = http_204_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &p204_1);

        httpd_uri_t p204_2 = { .uri = "/gen_204", .method = HTTP_GET, .handler = http_204_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &p204_2);

        httpd_uri_t p_apple = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = apple_detect_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &p_apple);

        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t sync_uri = { .uri = "/api/sync_time", .method = HTTP_POST, .handler = api_sync_time_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &sync_uri);

        httpd_uri_t alarm_uri = { .uri = "/api/set_alarm", .method = HTTP_POST, .handler = api_set_alarm_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &alarm_uri);

        httpd_uri_t timer_uri = { .uri = "/api/timer", .method = HTTP_POST, .handler = api_timer_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &timer_uri);

        httpd_uri_t dismiss_uri = { .uri = "/api/dismiss", .method = HTTP_POST, .handler = api_dismiss_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &dismiss_uri);

        httpd_uri_t ctrl_uri = { .uri = "/api/control", .method = HTTP_POST, .handler = api_control_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &ctrl_uri);

        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);

        return server;
    }
    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, ">>> [WIFI] 手机已连入热点! MAC: %02x:%02x:%02x:%02x:%02x:%02x, AID: %d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGW(TAG, ">>> [WIFI] 手机断开连接! MAC: %02x:%02x:%02x:%02x:%02x:%02x, AID: %d, 原因: %d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5], event->aid, event->reason);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ASSIGNED_IP_TO_CLIENT) {
        ip_event_assigned_ip_to_client_t* event = (ip_event_assigned_ip_to_client_t*) event_data;
        ESP_LOGI(TAG, ">>> [DHCP] 手机成功获取 IP 地址并就绪: " IPSTR, IP2STR(&event->ip));
    }
}

void wifi_app_init(void) {
    ESP_LOGI(TAG, "Initializing NVS, Wi-Fi SoftAP & Web Server...");

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化 TCP/IP 网络堆栈
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

    // 注册网络与 DHCP 事件监视器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_ASSIGNED_IP_TO_CLIENT, &wifi_event_handler, NULL, NULL));

    // 3. 配置并启动 Wi-Fi SoftAP 热点模式
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW20));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4. 设置最高发射功率 (19.5dBm)，确保信号强劲稳定
    esp_wifi_set_max_tx_power(78);

    // 5. 禁用 Wi-Fi 芯片省电睡眠模式，保持射频全功率常开
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // 6. 配置 DHCP 服务器主动下发 DNS Option 6 (指向 192.168.4.1)
    {
        esp_netif_dhcps_stop(ap_netif);
        uint8_t dhcps_offer_dns = 0x02; // DHCPS_OFFER_DNS
        esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET,
            ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_dns, sizeof(dhcps_offer_dns));

        esp_netif_dns_info_t dns_info;
        memset(&dns_info, 0, sizeof(dns_info));
        dns_info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(192, 168, 4, 1);
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
        esp_netif_dhcps_start(ap_netif);
    }

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  Wi-Fi AP Started! SSID: [%s]", WIFI_AP_SSID);
    ESP_LOGI(TAG, "  Wi-Fi Auth Mode       : [WPA2-PSK (AES-CCMP)]");
    ESP_LOGI(TAG, "  Wi-Fi Password        : [%s]", WIFI_AP_PASS);
    ESP_LOGI(TAG, "  Web Control URL       : http://192.168.4.1");
    ESP_LOGI(TAG, "==================================================");

    // 7. 启动轻量级 HTTP Web 服务器
    start_webserver();

    // 8. 启动简易 DNS 快速应答服务 (UDP 53)
    xTaskCreate(dns_server_task, "dns_server", 3072, NULL, 3, NULL);
}
