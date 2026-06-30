# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Build a single driver module (run from the driver's subdirectory):
make -C $(AGENT_SDK_KERNEL_PATH) M=$(pwd) ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- modules

# The per-module Makefile also supports building via the included buildmod.mk:
# export AGENT_PLATFORM_SOC_PATH=...  (needed by individual Makefiles)
# export AGENT_SDK_KERNEL_PATH=...    (path to compiled kernel source)
# make -C <module_dir>
```

The build system is Kbuild-based. Each numbered module directory has its own `Makefile` that sets `obj-m := <driver>.o` and includes the top-level `buildmod.mk`. The cross-compiler is `arm-none-linux-gnueabihf-`, targeting `ARCH=arm`.

## Architecture

This is a collection of **Linux kernel module drivers** for the NXP i.MX6ULL (ARM Cortex-A7) platform. Each numbered directory (`01.led` through `23.usb`, plus `x1.relay` through `x4.platform`) teaches one kernel subsystem through a working driver implementation.

### Driver patterns

Two bus/driver models are used throughout:

1. **Platform driver** (`platform_driver_register`) — for SoC-internal peripherals (LED, ADC, PWM, RNG, etc.). Matches against `compatible` strings in the device tree via `of_match_table`. The `probe` function: allocates a private data struct, initializes hardware (GPIO, IOMEM, clocks), creates a char device, and registers with the relevant kernel subsystem framework.

2. **I2C driver** (`i2c_add_driver`) — for I2C-attached devices (AP3216C sensor, touchscreen controllers, RTC). Matches against `compatible` strings on I2C child nodes.

### Character device creation pattern

Nearly every driver exposes a `/dev/<name>` node using this sequence:
```
alloc_chrdev_region → cdev_init + cdev_add → class_create → device_create
```
on `probe`, and the reverse on `remove`. File operations (`open`, `read`, `write`, `release`, `unlocked_ioctl`) are wired into a `struct file_operations`.

### Hardware access conventions

- **GPIO**: `devm_gpiod_get()` with named GPIOs from device tree; `gpiod_set_value_cansleep()`/`gpiod_get_value_cansleep()`
- **MMIO registers**: `of_iomap()` or `devm_platform_ioremap_resource()` with `readl()`/`writel()`
- **I2C**: `i2c_transfer()` with `struct i2c_msg` for raw reads/writes, or `regmap` API for register-based devices
- **Pin control**: `devm_pinctrl_get()` + `pinctrl_lookup_state()` for runtime pinctrl switching

### Device tree

Custom driver `compatible` strings use the `"rmk,..."` prefix (e.g., `"rmk,usr-led"`, `"rmk,ap3216"`). Device tree binding snippets are documented in comments at the top of each driver's `.c` file.

### Directory structure

- **`NN.<topic>/`** — kernel driver module (`.c` + `Makefile`). May contain subdirectories showing alternative implementations (e.g., `bus_i2c` vs `regmap_i2c`, `hw_rngc` vs `sf_rngc`).
- **`NN.<topic>/test/`** — user-space test programs with their own `Makefile`, run on the target ARM board.
- **`x1.relay/`**, **`x2.param/`**, **`x3.example/`**, **`x4.platform/`** — advanced/cross-cutting topics (multi-file modules, module parameters, platform device/driver model).

### Key subsystems covered

| # | Subsystem | Kernel API used |
|---|---|---|
| 01–03, 06 | GPIO / LED / input keys | `gpiod_*`, `led_classdev`, `input_dev` |
| 04–05 | I2C / SPI | `i2c_driver`, `spi_driver`, `regmap` |
| 07–08 | IIO (ADC, load cell) | `iio_dev`, `iio_trigger` |
| 09 | RTC | `rtc_device`, `regmap` |
| 10 | HW random number | `hwrng` |
| 11 | NVMEM (eFuse/OTP) | `nvmem_config` |
| 12 | Serial / UART | `uart_driver` |
| 13 | Block device | `request_queue`, `gendisk` |
| 14 | Touchscreen | Input subsystem over I2C |
| 15 | CAN / network | Socket CAN, `flexcan` platform driver |
| 16 | Thermal | `thermal_zone_device`, `thermal_cooling_device` |
| 17, 22 | PWM | `pwm_chip`, `pwm_bl` (backlight) |
| 18 | Watchdog | `watchdog_device` |
| 19 | DRM / framebuffer | DRM/KMS helpers, SPI display |
| 20 | CSI camera capture | V4L2 capture |
| 21 | Joystick | Input subsystem over ADC |
| 23 | USB | USB host serial |
