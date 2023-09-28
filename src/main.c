#include <stdio.h>
#include <time.h>

#include "hardware/flash.h"
#include "hardware/rtc.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/sntp.h"
#include "lwipopts.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "UC8151c.h"
#include "bm.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "fonts/font.xbm"
#include "onboard_temperature.h"

#define NAME "Thermostat"
#define PASSWORD "password"

#define WIFI_TIMEOUT (7000)
#define WATCHDOG_TIMEOUT (8000)

// We're going to erase and reprogram a region 256k from the start of flash.
// Once done, we can access this at XIP_BASE + 256k.
#define FLASH_TARGET_OFFSET (256 * 1024)

#define CONFIG_MAGIC (0x0c0ffe5)

#define NTP_DELTA (2208988800) // seconds between 1 Jan 1900 and 1 Jan 1970

#define BTNA (12)
#define BTNB (13)
#define BTNC (14)

#define LEVEL_LOW (0x1)
#define LEVEL_HIGH (0x2)
#define EDGE_FALL (0x4)
#define EDGE_RISE (0x8)

typedef enum {
    ST_START = 0,
    ST_SETUP,
    ST_WAIT,
    ST_INIT,
    ST_RUN,
    ST_RESET
} state_t;

typedef union {
    struct {
        uint32_t magic;
        char ssid[33];
        char pass[65];
        int tz;
        char therm;
        char mode;
        uint32_t heattimer;
        uint32_t watertimer;
    } data;
    uint8_t padding[FLASH_PAGE_SIZE];
} config_t;

typedef struct {
    state_t state;
    bool save;
    char name[sizeof(NAME) + 9];
    char addr[17];
    char time[10];
    float temp;
    char humid;
    bool heat;
    bool water;
    char heatboost;
    char waterboost;
    bool heatext;
    bool waterext;
    uint32_t btna;
    uint32_t btnat;
    uint32_t btnb;
    uint32_t btnbt;
    uint32_t btnc;
    uint32_t btnct;
} status_t;

extern cyw43_t cyw43_state;

const uint8_t* flash_target_contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);

static config_t config = {
    .data = {
        .magic = CONFIG_MAGIC,
        .ssid = WIFI_SSID,
        .pass = WIFI_PASSWORD,
        .tz = 0,
        .therm = 0,
        .mode = 0,
        .heattimer = 0,
        .watertimer = 0,
    }
};

static status_t status = {
    .state = ST_START,
    .save = false,
    .name = NAME,
    .addr = "192.168.4.1",
    .time = "",
    .temp = 0.0,
    .humid = 0,
    .heat = false,
    .water = false,
    .heatboost = false,
    .waterboost = false,
    .heatext = false,
    .waterext = false,
    .btna = 0,
    .btnat = 0,
    .btnb = 0,
    .btnbt = 0,
    .btnc = 0,
    .btnct = 0,
};

// max length of the tags defaults to be 8 chars
// LWIP_HTTPD_MAX_TAG_NAME_LEN
static const char* __not_in_flash("httpd") ssi_tags[] = {
    "name",
    "addr",
    "time",
    "temp",
    "humid",
    "heat",
    "water",
    "setup",
    "therm",
    "mode",
    "heatbst",
    "waterbst",
    "heattmr",
    "watertmr",
    "heatext",
    "waterext",
};

static void config_save(config_t* config)
{
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)config, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

static void config_load(config_t* config)
{
    if (CONFIG_MAGIC == ((config_t*)flash_target_contents)->data.magic) {
        memcpy(config, flash_target_contents, sizeof(config->data));
        printf("loaded cofiguration\n");
    }
}

static void gpio_callback(uint gpio, uint32_t events)
{
    if (BTNA == gpio) {
        status.btna = events;
    } else if (BTNB == gpio) {
        status.btnb = events;
    } else if (BTNC == gpio) {
        status.btnc = events;
    }
}

static u16_t __time_critical_func(ssi_handler)(int iIndex, char* pcInsert, int iInsertLen)
{
    size_t printed;
    switch (iIndex) {
    case 0:
        printed = snprintf(pcInsert, iInsertLen, "%s", status.name);
        break;
    case 1:
        printed = snprintf(pcInsert, iInsertLen, "%s", status.addr);
        break;
    case 2:
        printed = snprintf(pcInsert, iInsertLen, "%s", status.time);
        break;
    case 3:
        printed = snprintf(pcInsert, iInsertLen, "%.02f", status.temp);
        break;
    case 4:
        printed = snprintf(pcInsert, iInsertLen, "%d", status.humid);
        break;
    case 5:
        printed = snprintf(pcInsert, iInsertLen, status.heat ? "true" : "false");
        break;
    case 6:
        printed = snprintf(pcInsert, iInsertLen, status.water ? "true" : "false");
        break;
    case 7:
        printed = snprintf(pcInsert, iInsertLen, ST_RUN == status.state ? "false" : "true");
        break;
    case 8:
        printed = snprintf(pcInsert, iInsertLen, "%d", config.data.therm);
        break;
    case 9:
        printed = snprintf(pcInsert, iInsertLen, "%d", config.data.mode);
        break;
    case 10:
        printed = snprintf(pcInsert, iInsertLen, status.heatboost ? "true" : "false");
        break;
    case 11:
        printed = snprintf(pcInsert, iInsertLen, status.waterboost ? "true" : "false");
        break;
    case 12:
        printed = snprintf(pcInsert, iInsertLen, "%d", config.data.heattimer);
        break;
    case 13:
        printed = snprintf(pcInsert, iInsertLen, "%d", config.data.watertimer);
        break;
    case 14:
        printed = snprintf(pcInsert, iInsertLen, status.heatext ? "true" : "false");
        break;
    case 15:
        printed = snprintf(pcInsert, iInsertLen, status.waterext ? "true" : "false");
        break;
    }
}

/* cgi-handler triggered by a request  */
static const char* cgi_handler_basic(int iIndex, int iNumParams, char* pcParam[], char* pcValue[])
{
    printf("cgi_handler_basic called with index %d and %d params\n", iIndex, iNumParams);

    for (int i = 0; i < iNumParams; i++) {
        /* check if parameter is "led" */
        if (strcmp(pcParam[i], "ssid") == 0) {
            strncpy(config.data.ssid, pcValue[i], sizeof(config.data.ssid));
            status.state = ST_RESET;
            status.save = true;
        } else if (strcmp(pcParam[i], "pass") == 0) {
            strncpy(config.data.pass, pcValue[i], sizeof(config.data.pass));
            status.state = ST_RESET;
            status.save = true;
        } else if (strcmp(pcParam[i], "tz") == 0) {
            config.data.tz = atoi(pcValue[i]);
            status.save = true;
        } else if (strcmp(pcParam[i], "time") == 0) {
            time_t now = atoi(pcValue[i]);
            struct tm* time = localtime(&now);

            datetime_t datetime = {
                .year = (int16_t)(1900 + time->tm_year),
                .month = (int8_t)(time->tm_mon + 1),
                .day = (int8_t)time->tm_mday,
                .hour = (int8_t)time->tm_hour,
                .min = (int8_t)time->tm_min,
                .sec = (int8_t)time->tm_sec,
                .dotw = (int8_t)time->tm_wday,
            };

            if (!rtc_set_datetime(&datetime)) {
                printf("datetime error\n");
            }
        } else if (strcmp(pcParam[i], "therm") == 0) {
            config.data.therm = atoi(pcValue[i]);
            status.save = true;
        } else if (strcmp(pcParam[i], "mode") == 0) {
            config.data.mode = atoi(pcValue[i]);
            status.save = true;
        } else if (strcmp(pcParam[i], "heatbst") == 0) {
            if (status.heatboost) {
                status.heatboost = 0;
            } else {
                status.heatboost = 60;
            }
        } else if (strcmp(pcParam[i], "waterbst") == 0) {
            if (status.waterboost) {
                status.waterboost = 0;
            } else {
                status.waterboost = 60;
            }
        } else if (strcmp(pcParam[i], "heattmr") == 0) {
            config.data.heattimer = atoi(pcValue[i]);
            status.save = true;
        } else if (strcmp(pcParam[i], "watertmr") == 0) {
            config.data.watertimer = atoi(pcValue[i]);
            status.save = true;
        } else if (strcmp(pcParam[i], "heatext") == 0) {
            status.heatext = strcmp(pcValue[i], "true");
        } else if (strcmp(pcParam[i], "waterext") == 0) {
            status.waterext = strcmp(pcValue[i], "true");
        }
    }

    /* Our response to the "SUBMIT" is to simply send the same page again*/
    return "/index.html";
}

static const tCGI cgi_handlers[] = {
    { "/", cgi_handler_basic },
};

static void srv_txt(struct mdns_service* service, void* txt_userdata)
{
    err_t res;
    LWIP_UNUSED_ARG(txt_userdata);

    res = mdns_resp_add_service_txtitem(service, "path=/", 6);
    LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
}

static void mdns_report(struct netif* netif, u8_t result, s8_t service)
{
    LWIP_PLATFORM_DIAG(("mdns status[netif %d][service %d]: %d\n", netif->num, service, result));
}

/* Provide your function definition */
void sntp_set_system_time_us(unsigned long sec, unsigned long us)
{
    struct timeval tv = {
        .tv_sec = sec - NTP_DELTA - (config.data.tz * 60),
        .tv_usec = us
    };

    settimeofday(&tv, NULL);

    struct tm* time = localtime(&tv.tv_sec);

    datetime_t datetime = {
        .year = (int16_t)(1900 + time->tm_year),
        .month = (int8_t)(time->tm_mon + 1),
        .day = (int8_t)time->tm_mday,
        .hour = (int8_t)time->tm_hour,
        .min = (int8_t)time->tm_min,
        .sec = (int8_t)time->tm_sec,
        .dotw = (int8_t)time->tm_wday,
    };

    if (!rtc_set_datetime(&datetime)) {
        printf("datetime error\n");
    }
}

int main()
{
    // Set progtram description
    bi_decl(bi_program_description(NAME));

    // needed to get the RP2040 chip talking with the wireless module
    stdio_init_all();

    // enable watchdog
    watchdog_enable(WATCHDOG_TIMEOUT, 1);

    // Load configuration from flash
    config_load(&config);

    gpio_set_irq_enabled_with_callback(BTNA, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BTNB, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BTNC, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // Initiliaze the RTC
    rtc_init();

    onboard_temperature_init();

    // Initialise the LCD
    uc8151_init();

    // Clear the LCD
    uc8151_clear();

    while (true) {
        switch (status.state) {
        case ST_START:
            // attempt connect to wifi access point
            if (cyw43_arch_init()) {
                printf("Wi-Fi init failed");
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                status.state = ST_RESET;
                break;
            }

            // we'll be connecting to an access point, not creating one
            cyw43_arch_enable_sta_mode();

            int err = cyw43_arch_wifi_connect_timeout_ms(config.data.ssid, config.data.pass, CYW43_AUTH_WPA2_AES_PSK, WIFI_TIMEOUT);

            watchdog_update();

            uint8_t* mac = cyw43_state.mac;
            snprintf(status.name, sizeof(status.name), NAME "%d", mac[5] + (mac[4] << 8) + (mac[3] << 16));
            printf("name: %s\n", status.name);

            // WiFi credentials are taken from cmake/credentials.cmake
            if (0 == err) {
                status.state = ST_INIT;
                break;
            }

        case ST_SETUP:
            // access point not found, create access point for user to setup wifi
            printf("Cannot find wifi, fallback to ap mode\n");
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

            cyw43_arch_enable_ap_mode(status.name, PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

            ip_addr_t gw;
            ip4_addr_t mask;
            IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
            IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

            // Start the dhcp server
            dhcp_server_t dhcp_server;
            dhcp_server_init(&dhcp_server, &gw, &mask);

            // Start the dns server
            dns_server_t dns_server;
            dns_server_init(&dns_server, &gw);

            for (int i = 0; i < LWIP_ARRAYSIZE(ssi_tags); i++) {
                LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN", strlen(ssi_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
            }
            // let lwIP do it's magic
            httpd_init();
            http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
            http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));

            bm_qr_printf(80, 32, "WIFI:S:%s;T:WPA;P:%s;;", status.name, PASSWORD);
            bmp_printf("/monospace.bmp", 64, 0, "Setup");
            uc8151_refresh();

            status.state = ST_WAIT;
        case ST_WAIT:
            // wait for wifi settings
            watchdog_update();
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(10);

            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(990);
            break;
        case ST_INIT:
            // access point found, start webserver and ntp
            printf("Connected.\n");
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

            uint32_t ip_addr = cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr;
            snprintf(status.addr, sizeof(status.addr), "%lu.%lu.%lu.%lu", ip_addr & 0xFF, (ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF, ip_addr >> 24);
            printf("IP Address: %s\n", status.addr);

            /* Configure and start the SNTP client */
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            // sntp_setservername(0, "pool.ntp.org");
            sntp_init();

            for (int i = 0; i < LWIP_ARRAYSIZE(ssi_tags); i++) {
                LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN", strlen(ssi_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
            }
            // let lwIP do it's magic
            httpd_init();
            http_set_ssi_handler(ssi_handler, ssi_tags, LWIP_ARRAYSIZE(ssi_tags));
            http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));

            mdns_resp_register_name_result_cb(mdns_report);
            mdns_resp_init();
            mdns_resp_add_netif(netif_default, status.name);
            mdns_resp_add_service(netif_default, NAME, "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
            mdns_resp_announce(netif_default);

            bm_printf(font_bits, font_height, font_width, 0, UC8151_HEIGHT - 8, "http://%s", status.addr);
            bm_qr_printf(0, 32, "http://%s", status.addr);
            // bmp_printf("/monospace.bmp", 64, 0, NAME);
            uc8151_refresh();

            // Clear the Display
            uc8151_clear();
            bm_printf(font_bits, font_height, font_width, 0, UC8151_HEIGHT - 8, "http://%s", status.addr);
            bm_qr_printf(0, 32, "http://%s", status.addr);
            // bmp_printf("/monospace.bmp", 64, 0, NAME);

            status.state = ST_RUN;
        case ST_RUN:
            // normal operation poll every second
            watchdog_update();

            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

            // detect btn preses
            if (status.btna & EDGE_RISE) {
                status.btnat++;

                if (status.btnat > 2) {
                    config.data.therm++;
                    status.save = true;
                    uc8151_clear();
                    bmp_printf("/monospace.bmp", 64, 32, "%.02fC", config.data.therm);
                    uc8151_refresh();
                }
            }

            if (status.btna & EDGE_FALL) {
                status.btna = 0;

                if (status.btnat < 2) {
                    if (status.heatboost) {
                        status.heatboost = 0;
                    } else {
                        status.heatboost = 60;
                    }
                }

                status.btnat = 0;
            }

            if (status.btnb & EDGE_FALL) {
                status.btnb = 0;
                config.data.mode = (config.data.mode + 1) % 3;
                status.save = true;
            }

            if (status.btnc & EDGE_RISE) {
                status.btnct++;

                if (status.btnct > 2) {
                    config.data.therm--;
                    status.save = true;
                    uc8151_clear();
                    bmp_printf("/monospace.bmp", 64, 32, "%.02fC", config.data.therm);
                    uc8151_refresh();
                }
            }

            if (status.btnc & EDGE_FALL) {
                status.btnc = 0;

                if (status.btnct < 2) {
                    if (status.waterboost) {
                        status.waterboost = 0;
                    } else {
                        status.waterboost = 60;
                    }
                }

                status.btnct = 0;
            }

            // update every 1 min
            datetime_t datetime;
            if (rtc_get_datetime(&datetime)) {
                static int8_t min = 100;
                if (datetime.min != min) {
                    min = datetime.min;

                    if (status.save) {
                        status.save = false;
                        config_save(&config);
                    }

                    if (status.heatboost) {
                        status.heatboost--;
                        bmp_draw("/rocket.bmp", UC8151_WIDTH - 32, 8);
                    } else {
                        uc8151_fill_rectangle(UC8151_WIDTH - 32, 8, UC8151_WIDTH, 40, 0xff);
                    }

                    if (status.waterboost) {
                        status.waterboost--;
                        bmp_draw("/rocket.bmp", UC8151_WIDTH - 32, 88);
                    } else {
                        uc8151_fill_rectangle(UC8151_WIDTH - 32, 88, UC8151_WIDTH, 120, 0xff);
                    }

                    switch (config.data.mode) {
                    case 0:
                        status.heat = (status.heatboost) && ((config.data.therm < status.temp) || status.heatext);
                        status.water = status.waterext || status.waterboost;
                        bmp_draw("/no_sign.bmp", UC8151_WIDTH - 32, 48);
                        break;
                    case 1:
                        status.heat = (status.heatboost || ((config.data.heattimer >> datetime.hour) & 1)) && ((config.data.therm < status.temp) || status.heatext);
                        status.water = status.waterext || status.waterboost || ((config.data.watertimer >> datetime.hour) & 1);
                        bmp_draw("/clock.bmp", UC8151_WIDTH - 32, 48);
                        break;
                    case 2:
                        status.heat = (config.data.therm < status.temp) || status.heatext;
                        status.water = true;
                        bmp_draw("/radio_on.bmp", UC8151_WIDTH - 32, 48);
                        break;
                    default:
                        printf("Invalid mode\n");
                        status.state = ST_RESET;
                    }

                    bmp_printf("/monospace.bmp", 64, 64, "%02d:%02d", datetime.hour, datetime.min);
                    snprintf(status.time, sizeof(status.time), "%02d:%02d", datetime.hour, datetime.min);

                    status.temp = onboard_temperature_read(TEMPERATURE_UNITS);
                    printf("Onboard temperature = %.02f %c\n", status.temp, TEMPERATURE_UNITS);
                    bmp_printf("/monospace.bmp", 64, 32, "%.02fC", status.temp);
                    uc8151_refresh();
                }
            }

            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(1000);
            break;
        case ST_RESET:
            // trip watchdog to reset
            if (status.save) {
                config_save(&config);
            }
            printf("Waiting for reset\n");
            sleep_ms(1000);
        default:
            printf("Invalid state\n");
            status.state = ST_RESET;
        }
    }

    // dns_server_deinit(&dns_server);
    // dhcp_server_deinit(&dhcp_server);

    uc8151_sleep();

    cyw43_arch_deinit();

    return 0;
}
