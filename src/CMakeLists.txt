set(PROGRAM_NAME thermostat)

execute_process(COMMAND
        perl ${PROJECT_SOURCE_DIR}/tools/makefsdata
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        ECHO_OUTPUT_VARIABLE
        ECHO_ERROR_VARIABLE
        )
file(RENAME fsdata.c new_fsdata.c)

add_executable(${PROGRAM_NAME}
    main.c
    bm.c
    dhcpserver/dhcpserver.c
    dnsserver/dnsserver.c
    onboard_temperature.c
    QR-Code-generator/qrcodegen.c
    UC8151c/UC8151c.c
)

pico_enable_stdio_usb(${PROGRAM_NAME} 1)
pico_enable_stdio_uart(${PROGRAM_NAME} 1)

pico_add_extra_outputs(${PROGRAM_NAME})

target_compile_definitions(${PROGRAM_NAME} PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)

target_include_directories(${PROGRAM_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    dhcpserver
    dnsserver
    QR-Code-generator
    UC8151c
)

target_link_libraries(${PROGRAM_NAME}
    hardware_adc
    hardware_gpio
    hardware_rtc
    hardware_spi
    hardware_watchdog
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_http
    pico_lwip_mdns
    pico_lwip_sntp
    pico_stdlib
)