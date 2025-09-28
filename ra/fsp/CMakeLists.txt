set(SOURCE
    src/bsp/cmsis/Device/RENESAS/Source/startup.c
    src/bsp/cmsis/Device/RENESAS/Source/system.c
    src/bsp/mcu/all/bsp_clocks.c
    src/bsp/mcu/all/bsp_common.c
    src/bsp/mcu/all/bsp_delay.c
    src/bsp/mcu/all/bsp_group_irq.c
    src/bsp/mcu/all/bsp_guard.c
    src/bsp/mcu/all/bsp_io.c
    src/bsp/mcu/all/bsp_ipc.c
    src/bsp/mcu/all/bsp_irq.c
    src/bsp/mcu/all/bsp_macl.c
    src/bsp/mcu/all/bsp_register_protection.c
    src/bsp/mcu/all/bsp_sbrk.c
    src/bsp/mcu/all/bsp_sdram.c
    src/bsp/mcu/all/bsp_security.c
    src/bsp/mcu/ra4m1/bsp_linker.c
    src/r_ioport/r_ioport.c
    src/r_iic_master/r_iic_master.c)

add_library(fsp ${SOURCE})

target_include_directories(fsp PUBLIC
    inc
    inc/api
    inc/instances
    src/bsp/cmsis/Device/RENESAS/Include/)

target_link_libraries(fsp PUBLIC FSP_CONFIG CMSIS_6)