INCLUDE_DIRECTORIES(
    ${SMEDIA_INC_DIRS}
    ${PROJECT_SOURCE_DIR}/core
    ${PROJECT_SOURCE_DIR}/.repos
)

IF(${SMEDIA_PLATFORM} STREQUAL freertos)
    ADD_EXECUTABLE(jedi
        main_freertos.c
    )
ENDIF()

ADD_EXECUTABLE(jedi
    crc32.h
    crc32.c
    decompress.h
    decompress.c
    main.c
)

IF(${CONFIG_HAVE_USBD})
    ADD_EXECUTABLE(jedi
        it_hw_grabber.cpp
        it_usbd_pcgrabber.cpp
    )
ENDIF()

IF(${SYSTEM_FAT_TYPE} STREQUAL HAVE_FAT)
    SET(SMEDIA_FAT_LIB
        fat
        msc
    )
ENDIF()

IF(${COMPONENT_DEV} STREQUAL ENABLE)
    SET(COMPONENT_LIB
        cat9883
    )
ENDIF()

IF(${COMPOSITE_DEV} STREQUAL ENABLE)
    SET(COMPOSITE_LIB
        adv7180
    )
ENDIF()

IF(${HDMI_LOOPTHROUGH} STREQUAL ENABLE)
    SET(HDMITX_LIB
        hdmitx
    )
ENDIF()

IF(${CONFIG_HAVE_NTFS})
    SET(LIBNTFS
        libsxa_blkdev_usb
        libsxa_ntfs3g
    )
ENDIF()

TARGET_LINK_LIBRARIES(jedi
    ir
    uart
    core
    polarssl
    nor
    spi
    tso
    eagle_modulator
    dpu
    usb
    watchdog
    dma
    intr
    spdif
    pal
    mmp
    exrtc
    hdmirx
    ${SMEDIA_FAT_LIB}
    ${COMPONENT_LIB}
    ${COMPOSITE_LIB}
    ${HDMITX_LIB}
    ${LIBNTFS}
    freertos
)

ADD_CUSTOM_COMMAND(
    OUTPUT ${LIBRARY_OUTPUT_PATH}/default.ld
    COMMAND ${CMAKE_C_PREPROCESSOR}
        -I${PROJECT_SOURCE_DIR}/sdk/include/aud
        ${JEDI_DEFS}
        -E
        -P
        -o ${LIBRARY_OUTPUT_PATH}/default.ld
        ${CMAKE_CURRENT_SOURCE_DIR}/../../freertos/src/startup/default.lds
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../freertos/src/startup/default.lds
)
ADD_CUSTOM_TARGET(
    default.ld ALL
    DEPENDS ${LIBRARY_OUTPUT_PATH}/default.ld
)
ADD_CUSTOM_COMMAND(
    TARGET jedi
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
        -O binary ${CMAKE_CURRENT_BINARY_DIR}/jedi ${CMAKE_CURRENT_BINARY_DIR}/jedi.bin
)
ADD_CUSTOM_COMMAND(
    TARGET jedi
    POST_BUILD
    COMMAND mkrom -z -b 512K
        ${PROJECT_SOURCE_DIR}/tool/target/${JEDI_ROM_INIT_SCRIPT} ${CMAKE_CURRENT_BINARY_DIR}/jedi.bin ${CMAKE_CURRENT_BINARY_DIR}/jedi.rom
    COMMAND mkrom -z -b 512K
        ${PROJECT_SOURCE_DIR}/tool/target/${JEDI_ROM_INIT_SCRIPT} ${CMAKE_CURRENT_BINARY_DIR}/jedi.bin ${CMAKE_CURRENT_BINARY_DIR}/kproc.sys
)
ADD_CUSTOM_COMMAND(
    TARGET jedi
    POST_BUILD
    COMMAND sm32-elf-readelf
        -a ${CMAKE_CURRENT_BINARY_DIR}/jedi > ${CMAKE_CURRENT_BINARY_DIR}/jedi.symbol
)
