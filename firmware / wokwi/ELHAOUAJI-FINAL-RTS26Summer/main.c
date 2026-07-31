/*
 * Application 5 — Dual-core IPC pipeline
 *
 * Scaffold level: ~65% (the pipeline logic is yours; the infrastructure is provided).
 *
 * Scaffold Code - AI useage:
 *   Addition of the USE_WEBSERVER compile-time switch and a working serial
 *     monitor task on Core 0, plus per-task heartbeat counters
 *   Logic to allow for switching between a serial monitor and the (student-built)
 *     web monitor, so the pipeline runs in Wokwi with no Wi-Fi by default
 *   Commenting of code including human readable summaries
 *
 * The three IPC primitives are CREATED and wired; the pipeline LOGIC is yours:
 *   Queue             — fixed-size FIFO between producer & consumer
 *   Task notification — fast 1-to-1 signal (ISR or task → specific task)
 *   Event group       — N-way rendezvous (wait for a set of bits)
 * Required by the assignment: at least one use of each, EACH defended.
 *
 * What this scaffold gives you:
 *   - Producer / consumer / coordinator / responder task skeletons on Core 1.
 *   - The event-group rendezvous + coordinator→responder notification, wired.
 *   - A button ISR → responder direct-notification path.
 *   - Per-task heartbeat counters and a Core-0 monitor that prints queue depth,
 *     event-group bits, and heartbeats once a second (USE_WEBSERVER=0).
 *
 * What you do:
 *   1. Producer body — themed data items, queue send with a back-pressure policy.
 *   2. Consumer body — receive with timeout, themed processing.
 *   3. Web monitor — port App 1's HTTP code into webmonitor_task (USE_WEBSERVER=1).
 *   4. Size the queue (depth + item size) and defend it.
 *   5. Theme-rename (YOURTHEME): task names, log strings, the meaning of a data item.
 *
 * What you DON'T need to change:
 *   - The event-group / notification plumbing, the button ISR, or the monitor.
 *   - The heartbeat counters (already incremented at the end of each task loop).
 *
 * ============================================================
 *  RUN MODE  (serial monitor vs. web monitor)
 * ============================================================
 *
 * USE_WEBSERVER selects the Core-0 observability plane. The Core-1 pipeline is
 * identical in both modes.
 *
 *   USE_WEBSERVER = 0  -> Serial monitor (provided, working). Prints queue depth,
 *                         event bits, and heartbeats once a second. No Wi-Fi, so
 *                         the pipeline runs in Wokwi out of the box.
 *   USE_WEBSERVER = 1  -> Web monitor. Runs webmonitor_task instead — the stub you
 *                         fill in with App 1's HTTP server (deliverable #3). Needs
 *                         the Wi-Fi REQUIRES already in this folder's CMakeLists.
 *
 * Start on USE_WEBSERVER=0 to get the pipeline moving in the simulator, then flip
 * to 1 once you have implemented the web monitor.
 *
 * ============================================================
 * Theme: AVIONICS
 * ============================================================
 */

#ifndef USE_WEBSERVER
#define USE_WEBSERVER 0
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_random.h"

#if USE_WEBSERVER
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#endif

#define BUTTON_GPIO GPIO_NUM_18

#define CONFIG_LOG_DEFAULT_LEVEL_INFO 1
//#define CONFIG_LOG_MAXIMUM_LEVEL  5

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""
#define HTTP_PORT 80

#define PROCESS_TIME 20 /* 
Process time is time consumer uses to process data from producer
0 = <= producer time
1 = 1% chance of process taking 2x longer than producer
2 = 2% chance of process taking 2x longer than producer
3 = 3% chance of process taking 2x longer than producer
...
100 = 100% chance of process taking 2x longer than producer*/
 
static const char *TAG = "app5";

/* ---------- IPC objects (created in app_main, used everywhere) ---------- */
static QueueHandle_t      data_q;        /* TODO: choose depth + item size for YOUR pipeline */
static EventGroupHandle_t evt_group;
static TaskHandle_t       responder_handle;

/* Event-group bit definitions */
#define EV_BIT_DATA_PRODUCED  (1 << 0)
#define EV_BIT_DATA_PROCESSED (1 << 1)

/* Per-task heartbeats — proof of life for the monitor. Single 32-bit reads are
 * atomic on Xtensa, so the monitor can read these without a lock (App 6's topic). */
static volatile uint32_t hb_prod, hb_cons, hb_coord, hb_resp, dropped_items;

/* ---------- Producer task (Core 1) ----------
 * TODO(YOU): generate themed data. Push it into data_q. Set EV_BIT_DATA_PRODUCED.
 */

typedef struct {
    uint32_t timestamp_ms;
    int value;
} data_item_t;

static void producer_task(void *arg)
{
    int tick = 0;
    for (;;) {
        data_item_t item;

        item.timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* Example themed data: simulated attitude angle */
        item.value = (tick % 21) - 10;   /* cycles from -10 to +10 */

        /*
         * Wait up to 10 ms for queue space.
         * If the queue is still full, drop the new sample.
         */
        /*printf("[producer] tick=%d, sending value=%d\n",
        tick, item.value);*/
        if (xQueueSend(data_q, &item, pdMS_TO_TICKS(10)) == pdPASS) {

            xEventGroupSetBits(evt_group, EV_BIT_DATA_PRODUCED);
        } else {
            /* Queue full: sample was dropped */
            dropped_items++;
            printf("[producer] queue full, dropped value %d\n", item.value);
        }

        tick++;
        hb_prod++;

        vTaskDelay(pdMS_TO_TICKS(50));   /* 20 Hz producer */
    }
}

/* ---------- Consumer task (Core 1) ----------
 * TODO(YOU): pop from queue, process, set EV_BIT_DATA_PROCESSED. */
static void consumer_task(void *arg)
{
    data_item_t item;
    for (;;) {
        /* TODO(YOU): replace this placeholder wait with a real
         *   xQueueReceive(data_q, &item, timeout) plus themed processing.
         * The delay below only keeps the scaffold from busy-spinning (and
         * starving Core 1's idle task) until you wire the queue up — a real
         * xQueueReceive blocks on its own, so delete this line when you add it. */
        //vTaskDelay(pdMS_TO_TICKS(5000));
        /* TODO: process the item (themed work). */
        if (xQueueReceive(data_q, &item, pdMS_TO_TICKS(10)) == pdPASS) {

            float chance = 0.0f;

            if (PROCESS_TIME >= 0 && PROCESS_TIME <= 100) {
                chance = PROCESS_TIME / 100.0f;
            }

            /* Random value from 0.00 through 0.99 */
            float process_factor =
                (esp_random() % 100) / 100.0f;

            if (process_factor < chance) {
                vTaskDelay(pdMS_TO_TICKS(100));
            } else {
                vTaskDelay(pdMS_TO_TICKS(40));
            }

            xEventGroupSetBits(evt_group, EV_BIT_DATA_PROCESSED);
            hb_cons++;
        }
    }
}

/* ---------- Coordinator task (Core 1) ----------
 * Waits for BOTH event bits to be set, then signals the responder via direct
 * task notification.
 */
static void coordinator_task(void *arg)
{
    const EventBits_t wait_mask = EV_BIT_DATA_PRODUCED | EV_BIT_DATA_PROCESSED;
    for (;;) {
        EventBits_t got = xEventGroupWaitBits(evt_group, wait_mask,
                                              pdTRUE,   /* clear on exit */
                                              pdTRUE,   /* wait for ALL */
                                              portMAX_DELAY);
        if ((got & wait_mask) == wait_mask) {
            /* TODO: do whatever the "cycle complete" event means for your theme.
             * Then notify the responder. */
            // Just getting started and want to see your button presses?
            // Comment out the line below. -mb 
            xTaskNotifyGive(responder_handle);
            hb_coord++;
        }
    }
}

/* ---------- Responder task (Core 1) ----------
 * Wakes via direct task notification from coordinator OR from button ISR.
 */
static void responder_task(void *arg)
{
    for (;;) {
        uint32_t n = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (n == 0) continue;
        hb_resp++;
        //ESP_LOGI(TAG, "[responder] notified (count=%d)", (int)hb_resp);
        /* TODO: themed action. */
    }
}

/* ---------- Button ISR — notify responder directly ---------- */
static volatile int64_t last_edge_us;
static void IRAM_ATTR button_isr(void *arg)
{
    int64_t now = esp_timer_get_time();
    if (now - last_edge_us < 200) return;
    last_edge_us = now;

    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(responder_handle, &woken);
    portYIELD_FROM_ISR(woken);
}

#if USE_WEBSERVER
/* ---------- Web monitor task (Core 0)  [USE_WEBSERVER = 1] ----------
 * TODO(YOU) — deliverable #3. Port the HTTP server from App 1/2 here:
 *   - nvs_flash_init(); esp_netif_init(); esp_event_loop_create_default();
 *   - wifi_init_sta();  (STA connect — serial prints "Got IP: 10.13.37.x")
 *   - start_webserver(); register a root handler that renders:
 *       * uxQueueMessagesWaiting(data_q)     — live queue depth
 *       * xEventGroupGetBits(evt_group)      — event-group bit state
 *       * hb_prod / hb_cons / hb_coord / hb_resp  — per-task heartbeats
 * The Wi-Fi headers are already included above under USE_WEBSERVER, and the
 * Wi-Fi/HTTP components are in this folder's CMakeLists REQUIRES.
 * Auto-refresh ~1 Hz; faster and your handler shows up in latency measurements.
 */
static esp_err_t handle_state(httpd_req_t *req)
{
    char buf[384];

    UBaseType_t queue_depth = uxQueueMessagesWaiting(data_q);
    EventBits_t event_bits = xEventGroupGetBits(evt_group);

    uint32_t produced = hb_prod;
    uint32_t dropped = dropped_items;

    float drop_percentage = 0.0f;

    if (produced > 0) {
        drop_percentage =
            ((float)dropped / (float)produced) * 100.0f;
    }

    int n = snprintf(
        buf,
        sizeof(buf),
        "{"
            "\"queue_depth\":%u,"
            "\"event_bits\":%u,"
            "\"data_produced\":%s,"
            "\"data_processed\":%s,"
            "\"hb_prod\":%lu,"
            "\"hb_cons\":%lu,"
            "\"hb_coord\":%lu,"
            "\"hb_resp\":%lu,"
            "\"dropped_items\":%lu,"
            "\"drop_percentage\":%.2f"
        "}",
        (unsigned)queue_depth,
        (unsigned)event_bits,
        (event_bits & EV_BIT_DATA_PRODUCED) ? "true" : "false",
        (event_bits & EV_BIT_DATA_PROCESSED) ? "true" : "false",
        (unsigned long)produced,
        (unsigned long)hb_cons,
        (unsigned long)hb_coord,
        (unsigned long)hb_resp,
        (unsigned long)dropped,
        (double)drop_percentage
    );

    if (n < 0 || n >= sizeof(buf)) {
        httpd_resp_send_err(
            req,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Failed to generate state"
        );
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, buf, n);

    return ESP_OK;
}

/*
 static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}
*/

static esp_err_t handle_root(httpd_req_t *req)
{
    static const char html[] =
        "<!DOCTYPE html>"
        "<html lang=\"en\">"
        "<head>"
        "  <meta charset=\"utf-8\">"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "  <title>Avionics IPC Monitor</title>"

        "  <style>"
        "    body {"
        "      font-family: Arial, sans-serif;"
        "      background: #f4f6f8;"
        "      color: #1a1a1a;"
        "      padding: 2rem;"
        "      margin: 0;"
        "    }"

        "    h1 {"
        "      color: #334a66;"
        "      margin-bottom: 0.3rem;"
        "    }"

        "    .subtitle {"
        "      color: #667788;"
        "      margin-bottom: 2rem;"
        "    }"

        "    .grid {"
        "      display: grid;"
        "      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));"
        "      gap: 1rem;"
        "    }"

        "    .card {"
        "      background: white;"
        "      border-radius: 8px;"
        "      padding: 1rem;"
        "      box-shadow: 0 2px 8px rgba(0,0,0,0.1);"
        "    }"

        "    .label {"
        "      color: #667788;"
        "      font-size: 0.9rem;"
        "    }"

        "    .value {"
        "      font-size: 2rem;"
        "      font-weight: bold;"
        "      margin-top: 0.4rem;"
        "    }"

        "    .active { color: #16833b; }"
        "    .inactive { color: #b3212d; }"

        "    #connection {"
        "      margin-top: 1.5rem;"
        "      color: #667788;"
        "    }"
        "  </style>"
        "</head>"

        "<body>"
        "  <h1>UAV-01 IPC Pipeline Monitor</h1>"
        "  <p class=\"subtitle\">Live FreeRTOS queue, event-group, and task status</p>"

        "  <div class=\"grid\">"

        "    <div class=\"card\">"
        "      <div class=\"label\">Queue depth</div>"
        "      <div id=\"queue_depth\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Dropped attitude packets</div>"
        "      <div id=\"dropped_items\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Drop percentage</div>"
        "      <div id=\"drop_percentage\" class=\"value\">--</div>"
        "    </div>"
        "    <div class=\"card\">"
        "      <div class=\"label\">Event bits</div>"
        "      <div id=\"event_bits\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Data produced bit</div>"
        "      <div id=\"produced_bit\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Data processed bit</div>"
        "      <div id=\"processed_bit\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Producer heartbeat</div>"
        "      <div id=\"hb_prod\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Consumer heartbeat</div>"
        "      <div id=\"hb_cons\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Coordinator heartbeat</div>"
        "      <div id=\"hb_coord\" class=\"value\">--</div>"
        "    </div>"

        "    <div class=\"card\">"
        "      <div class=\"label\">Responder heartbeat</div>"
        "      <div id=\"hb_resp\" class=\"value\">--</div>"
        "    </div>"

        "  </div>"

        "  <p id=\"connection\">Connecting...</p>"

        "  <script>"
        "    function showBit(elementId, isSet) {"
        "      const element = document.getElementById(elementId);"
        "      element.textContent = isSet ? 'SET' : 'CLEAR';"
        "      element.className = isSet ? 'value active' : 'value inactive';"
        "    }"

        "    async function pollState() {"
        "      try {"
        "        const response = await fetch('/state', { cache: 'no-store' });"

        "        if (!response.ok) {"
        "          throw new Error('HTTP error');"
        "        }"

        "        const state = await response.json();"

        "        document.getElementById('queue_depth').textContent ="
        "          state.queue_depth;"

        "        document.getElementById('dropped_items').textContent ="
        "          state.dropped_items;"

        "        document.getElementById('drop_percentage').textContent ="
        "          state.drop_percentage.toFixed(2) + '%';"

        "        document.getElementById('event_bits').textContent ="
        "          '0x' + state.event_bits.toString(16).padStart(2, '0');"

        "        showBit('produced_bit', state.data_produced);"
        "        showBit('processed_bit', state.data_processed);"

        "        document.getElementById('hb_prod').textContent = state.hb_prod;"
        "        document.getElementById('hb_cons').textContent = state.hb_cons;"
        "        document.getElementById('hb_coord').textContent = state.hb_coord;"
        "        document.getElementById('hb_resp').textContent = state.hb_resp;"

        "        document.getElementById('connection').textContent ="
        "          'Connected — updated once per second';"
        "      } catch (error) {"
        "        document.getElementById('connection').textContent ="
        "          'Connection lost — retrying...';"
        "      }"
        "    }"

        "    setInterval(pollState, 1000);"
        "    pollState();"
        "  </script>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();

    cfg.server_port = HTTP_PORT;
    cfg.core_id = 0;
    cfg.task_priority = 5;
    cfg.stack_size = 8192;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &cfg) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = handle_root,
            .user_ctx = NULL
        };

        httpd_uri_t state = {
            .uri = "/state",
            .method = HTTP_GET,
            .handler = handle_state,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &state);

        ESP_LOGI(TAG, "HTTP server started on port %d", HTTP_PORT);
    } else {
        ESP_LOGE(TAG, "HTTP server failed to start");
    }

    return server;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected; reconnecting...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        start_webserver();
    }
}

static void webmonitor_task(void *arg)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            wifi_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            wifi_event_handler,
            NULL
        )
    );

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_cfg
        )
    );

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization complete");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#else
/* ---------- Serial monitor task (Core 0)  [USE_WEBSERVER = 0] ----------
 * Provided and working. Prints the same state the web monitor will show, so the
 * pipeline is observable in Wokwi with no Wi-Fi. This is your baseline; the web
 * monitor (USE_WEBSERVER=1) renders the identical fields over HTTP.
 */
static void serial_monitor_task(void *arg)
{
    for (;;) {
        UBaseType_t depth = uxQueueMessagesWaiting(data_q);
        EventBits_t bits  = xEventGroupGetBits(evt_group);
        ESP_LOGI(TAG,
                 "[UAV-01 monitor] q_depth=%u  evt=0x%02x  hb: prod=%lu cons=%lu coord=%lu resp=%lu",
                 (unsigned)depth, (unsigned)bits,
                 (unsigned long)hb_prod, (unsigned long)hb_cons,
                 (unsigned long)hb_coord, (unsigned long)hb_resp);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif /* USE_WEBSERVER */

/* ---------- app_main ---------- */
void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "==== App 5 UAV-01 starting — IPC pipeline ====");

#if USE_WEBSERVER
    ESP_LOGI(TAG, "Monitor: WEB (USE_WEBSERVER=1) — implement webmonitor_task (Core 0)");
#else
    ESP_LOGI(TAG, "Monitor: SERIAL (USE_WEBSERVER=0) — Core-0 summary once/sec, no Wi-Fi");
#endif

    /* TODO: pick queue length + item size. Defend in README.
     * Hint: producer at 20 Hz, consumer at unknown rate — what burst size? */
    data_q = xQueueCreate(/*depth=*/ 10, /*item size=*/ sizeof(data_item_t));

    evt_group = xEventGroupCreate();

    /* Tasks on Core 1 (real-time plane). 4096-byte stacks: any task that calls
     * ESP_LOGI needs headroom for the vprintf formatting (2048 overflows). */
    xTaskCreatePinnedToCore(producer_task,    "prod",   4096, NULL,  8, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(consumer_task,    "cons",   4096, NULL,  8, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(coordinator_task, "coord",  4096, NULL,  9, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(responder_task,   "resp",   4096, NULL, 12, &responder_handle, APP_CPU_NUM);

    /* Observability plane on Core 0 (networking plane) */
#if USE_WEBSERVER
    xTaskCreatePinnedToCore(webmonitor_task,    "webmon",  4096, NULL, 4, NULL, PRO_CPU_NUM);
#else
    xTaskCreatePinnedToCore(serial_monitor_task, "monitor", 4096, NULL, 4, NULL, PRO_CPU_NUM);
#endif

    /* Button ISR */
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL);
}
