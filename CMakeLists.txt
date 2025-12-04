# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
  set(USERHOME $ENV{USERPROFILE})
else()
  set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
  include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Use CYW43 + lwIP with FreeRTOS (NO_SYS=0)
set(PICO_CYW43_ARCH_FREERTOS 1)
set(PICO_CYW43_ARCH_LWIP_SYS_FREERTOS 1)

# Path to your FreeRTOS kernel
set(FREERTOS_KERNEL_PATH /Users/mykytasiryk/university/real-time-operating-systems/FreeRTOS-Kernel)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(/Users/mykytasiryk/university/real-time-operating-systems/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(personal-project C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# ------------------------------------------------------------------------------------
# Make lwipopts.h visible to the SDK's lwIP sources and to your target.
# Place your config header at: ${CMAKE_CURRENT_LIST_DIR}/config/lwipopts.h
add_library(pico_lwipopts INTERFACE)
target_include_directories(pico_lwipopts INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/config
)
# Alias legacy FreeRTOS macro used by lwIP port to the modern name.
target_compile_definitions(pico_lwipopts INTERFACE
    portTICK_RATE_MS=portTICK_PERIOD_MS
)
# Also add a *global* definition so all SDK-built lwIP sources (e.g., sys_arch.c) see it:
add_compile_definitions(
    portTICK_RATE_MS=portTICK_PERIOD_MS
)
# ------------------------------------------------------------------------------------

# Add Subdirectory
add_subdirectory(./lib/Config)
add_subdirectory(./lib/OLED)
add_subdirectory(./lib/Fonts)
add_subdirectory(./lib/GUI)
add_subdirectory(./lib/SHTC3)
add_subdirectory(./lib/SGP40)
add_subdirectory(./lib/QMI8658)

# Include Directories (project-wide)
include_directories(./lib/Config)
include_directories(./lib/OLED)
include_directories(./lib/Fonts)
include_directories(./lib/GUI)
include_directories(./lib/SHTC3)
include_directories(./lib/SGP40)
include_directories(./lib/QMI8658)

# Add executable. Default name is the project name, version 0.1
add_executable(personal-project personal-project.c )

pico_set_program_name(personal-project "personal-project")
pico_set_program_version(personal-project "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(personal-project 0)
pico_enable_stdio_usb(personal-project 1)

# Add the standard include files to the build (app target)
target_include_directories(personal-project PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/config       # <= ensure your app also sees lwipopts.h
    ${FREERTOS_KERNEL_PATH}/include 
    ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040 
)

# Link libraries (single consolidated call)
target_link_libraries(personal-project
  pico_stdlib
  hardware_spi
  hardware_i2c
  hardware_pwm
  hardware_adc

  # FreeRTOS
  FreeRTOS-Kernel
  FreeRTOS-Kernel-Heap4

  # Your libs
  Config
  GUI
  Fonts
  OLED
  SGP40
  SHTC3
  QMI8658

  # CYW43 + lwIP arch for FreeRTOS (NO_SYS=0)
  pico_cyw43_arch_lwip_sys_freertos
)

pico_add_extra_outputs(personal-project)
