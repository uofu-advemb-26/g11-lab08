add_executable(mytest test.c unity_config.c)

add_library(unity ${UNITY_PATH}/src/unity.c)
target_include_directories(unity PUBLIC ${UNITY_PATH}/src)

target_link_libraries(mytest PRIVATE pico_stdlib pico_async_context_freertos
                                     FreeRTOS-Kernel-Heap4 unity)

# The CYW43 is the Wifi/Bluetooth module. If the board is set to pico_w, this
# variable will be set and the wireless libraries added.
if(PICO_CYW43_SUPPORTED)
  target_link_libraries(mytest PRIVATE pico_cyw43_arch_none)
endif()

# Modify the below lines to enable/disable output over UART/USB. These are
# mutually exclusive, only set one.
pico_enable_stdio_uart(mytest 0)
pico_enable_stdio_usb(mytest 1)

# This will speed up the simulation, since data won't be copied from the flash
# controller. It does make the binary non-persistent on a real microcontroller.
# pico_set_binary_type(mytest no_flash)

find_program(OPENOCD openocd)
find_program(PICOTOOL picotool)

add_custom_target(
  flash_test
  ${PICOTOOL} load -f $<TARGET_FILE:mytest>
  DEPENDS mytest)
add_custom_target(
  openocd_test
  ${OPENOCD}
  -s
  ${OPENOCD_PATH}
  -f
  interface/cmsis-dap.cfg
  -f
  target/rp2040.cfg
  -c
  'adapter speed 5000; program "$<TARGET_FILE:mytest>" verify reset exit'
  DEPENDS mytest)
