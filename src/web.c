#include "esp_http_server.h"
#include "esp_log.h"

#include "config.h"

static const char *TAG = "badcast_web";

extern const char html_start[] asm("_binary_web_html_start");
extern const char html_end[] asm("_binary_web_html_end");

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = html_end - html_start;

    ESP_LOGI(TAG, "get root");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_start, root_len);

    return ESP_FAIL; // Make sure the conenction is closed.
}

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "http://"CAPTIVE_PORTAL_DOMAIN"/");
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN); // For iOS.
    ESP_LOGI(TAG, "redirect to root");
    return ESP_OK;
}

void start_web_task(void)
{
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        ESP_LOGI(TAG, "web server started, port: %d", config.server_port);
    }
}