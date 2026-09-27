    set(SOURCES_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp/src/led.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp/src/ble.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp/src/ble_hid_adv.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp/src/ble_hid.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp/src/sle_measure_dis.c
        ${CMAKE_CURRENT_SOURCE_DIR}/driver/src/uhal_spi.c
    )

    set(PUBLIC_HEADER_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/driver/inc
    )

    set(SOURCES "${SOURCES_LIST}" PARENT_SCOPE)
    set(PUBLIC_HEADER "${PUBLIC_HEADER_LIST}" PARENT_SCOPE)
