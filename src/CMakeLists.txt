# This is the main binary. List your C files here.
add_executable(hello_freertos hello_freertos.c)

add_executable(can_node_a main.c)
add_executable(can_node_b main.c)
target_compile_definitions(can_node_a PRIVATE NODE_A)
add_executable(can_repeater repeater.c)

pico_set_program_name(hello_freertos "test")
pico_set_program_version(hello_freertos "0.1")

# Modify the below lines to enable/disable output over UART/USB. These are
# mutually exclusive, only set one.
pico_enable_stdio_uart(hello_freertos 0)
pico_enable_stdio_usb(hello_freertos 1)

pico_enable_stdio_uart(can_node_a 0)
pico_enable_stdio_usb(can_node_a 1)

pico_enable_stdio_uart(can_node_b 0)
pico_enable_stdio_usb(can_node_b 1)

pico_enable_stdio_uart(can_repeater 0)
pico_enable_stdio_usb(can_repeater 1)

# Add the libraries to the build
target_link_libraries(
  hello_freertos PRIVATE pico_stdlib pico_multicore pico_async_context_freertos
                         FreeRTOS-Kernel-Heap4)
target_link_libraries(
  can_node_a PRIVATE pico_stdlib pico_multicore pico_async_context_freertos
                     FreeRTOS-Kernel-Heap4 can2040)
target_link_libraries(
  can_node_b PRIVATE pico_stdlib pico_multicore pico_async_context_freertos
                     FreeRTOS-Kernel-Heap4 can2040)
target_link_libraries(
  can_repeater PRIVATE pico_stdlib pico_multicore pico_async_context_freertos
                       FreeRTOS-Kernel-Heap4 can2040)

# The CYW43 is the Wifi/Bluetooth module. If the board is set to pico_w, this
# variable will be set and the wireless libraries added.
if(PICO_CYW43_SUPPORTED)
  # For led support on pico_w
  target_link_libraries(hello_freertos PRIVATE pico_cyw43_arch_none)
  target_link_libraries(can_node_a PRIVATE pico_cyw43_arch_none)
  target_link_libraries(can_node_b PRIVATE pico_cyw43_arch_none)
  target_link_libraries(can_repeater PRIVATE pico_cyw43_arch_none)
endif()

# Another pico macro which generates the uf2 file in addition to the elf
pico_add_extra_outputs(hello_freertos)
pico_add_extra_outputs(can_node_a)
pico_add_extra_outputs(can_node_b)

find_program(OPENOCD openocd)
find_program(PICOTOOL picotool)

add_custom_target(
  flash
  ${PICOTOOL} load -f $<TARGET_FILE:hello_freertos>
  DEPENDS hello_freertos)

add_custom_target(
  openocd_flash
  ${OPENOCD}
  -s
  ${OPENOCD_PATH}
  -f
  interface/cmsis-dap.cfg
  -f
  target/rp2040.cfg
  -c
  "adapter speed 5000"
  -c
  "program $<TARGET_FILE:hello_freertos> verify reset exit"
  DEPENDS hello_freertos
  VERBATIM)

# activity 2 (babbling node). A = can_babbler, B = can_periodic
add_executable(can_babbler babble.c)
add_executable(can_periodic babble.c)
target_compile_definitions(can_babbler PRIVATE BABBLER)

pico_enable_stdio_uart(can_babbler 0)
pico_enable_stdio_usb(can_babbler 1)
pico_enable_stdio_uart(can_periodic 0)
pico_enable_stdio_usb(can_periodic 1)

target_link_libraries(can_babbler PRIVATE pico_stdlib FreeRTOS-Kernel-Heap4
                                          can2040)
target_link_libraries(can_periodic PRIVATE pico_stdlib FreeRTOS-Kernel-Heap4
                                           can2040)

pico_add_extra_outputs(can_babbler)
pico_add_extra_outputs(can_periodic)
