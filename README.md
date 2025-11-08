# Embedded RTOS Personal Project Hardware

This is a repository that contains a project's files, incuding header files, source files, makefiles, and other things.

## Project Structure

```text
embedded-rtos-personal-project-hardware/
├── config/              # Configuration files
│   ├── lwipopts.h       # LwIP network stack options
│   └── mbedtls_config.h # mbedTLS configuration
├── docs/                # Documentation and diagrams
│   └── diagrams/        # Architecture and communication diagrams
├── lib/                 # Hardware library components
│   ├── Config/          # Device configuration utilities
│   ├── Fonts/           # Font files for display
│   ├── GUI/             # GUI painting utilities
│   ├── OLED/            # OLED display driver (1.5 inch)
│   ├── QMI8658/         # IMU sensor driver
│   ├── SGP40/           # VOC (Volatile Organic Compounds) sensor driver
│   └── SHTC3/           # Temperature and humidity sensor driver
├── src/                 # Additional source files
│   └── mbedtls_time_alt.c # mbedTLS time alternative implementation
├── CMakeLists.txt       # Main CMake build configuration
├── FreeRTOSConfig.h     # FreeRTOS configuration
├── personal-project.c   # Main application source file
└── pico_sdk_import.cmake # Raspberry Pi Pico SDK import configuration
```

### Key Components

- **Main Application**: `personal-project.c` contains the primary embedded application code
- **Hardware Libraries**: The `lib/` directory contains drivers and utilities for various hardware components:
  - **Sensors**: QMI8658 (IMU), SGP40 (VOC), SHTC3 (Temperature/Humidity)
  - **Display**: OLED 1.5 inch display with GUI and font support
- **Configuration**: Network stack (LwIP) and TLS (mbedTLS) configurations in `config/`
- **Build System**: CMake-based build system for Raspberry Pi Pico W

## System Architecture

This section provides comprehensive visual documentation of the system architecture, communication flows, data processing, and API endpoints.

### Architecture Overview

The system consists of three main components: an Embedded System (Raspberry Pi Pico W), a Backend API (FastAPI), and a Frontend (Next.js). The embedded system collects sensor data and transmits it to the backend, which stores it in MongoDB. The frontend retrieves and visualizes this data for users.

![System Architecture](docs/diagrams/architecture-diagram.png)

### Communication Flow

The following sequence diagram illustrates the end-to-end communication between all system components, including initialization, sensor reading, data transmission, and frontend polling mechanisms.

![Communication Diagram](docs/diagrams/communication-daigram.png)
