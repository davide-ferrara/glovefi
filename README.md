# glovefi

**glovefi** is a lightweight command-line utility written in C designed to automate and simplify the firmware update process for the **Glove80** split keyboard on Linux systems.

The program monitors the system event bus (via `libsystemd`) to automatically detect when units are connected, handling mounting, safe `.uf2` firmware copying, and clean unmounting, while ensuring the correct update order is respected (Right side first, then Left side).

## Features

* **Automatic Detection:** Monitors USB hotplug events for devices in bootloader mode without manual configuration.
* **Guided Flow:** Enforces the correct installation order (Right -> Left) to avoid synchronization issues between the two halves.
* **Safe Operations:** Automatically executes mount, copy with `sync` to disk, and clean unmount to prevent flash memory corruption.
* **Lightweight:** Written in pure C, utilizing native Linux APIs (`sd-device`) for maximum efficiency.

## Prerequisites

To compile the project, you need `gcc`, `make`, and the systemd development libraries (`libsystemd` / `libudev`).

**Debian / Ubuntu / Pop!_OS:**
```bash
sudo apt install build-essential libsystemd-dev pkg-config
````

**Fedora:**

```bash
sudo dnf install systemd-devel
```

**Arch Linux:**

```bash
sudo pacman -S base-devel systemd
```

## Installation

1.  Compile the source code:

    ```bash
    make
    ```

2.  Install the binary to the system (copies to `/usr/bin`):

    ```bash
    sudo make install
    ```

## Usage

Since the program needs to mount volumes and access block devices, it must be run with root privileges (`sudo`).

The syntax is:

```bash
sudo glovefi <path_to_firmware.uf2>
```

### Example

```bash
sudo glovefi "5fc13697-a440-4091-99ea-0a2efd7b084d_v25.11_DaveKey v1.0.uf2"
```

*(Note: wrap the filename in quotes if it contains spaces)*

### Operational Flow

Once the command is launched:

1.  The program enters a waiting state.
2.  Connect the **Right Half** in bootloader mode. The firmware will be installed.
3.  Wait for the confirmation message and disconnect the Right Half.
4.  Connect the **Left Half** in bootloader mode. The firmware will be installed.
5.  The program terminates automatically upon completion.

## Uninstallation and Cleanup

To remove the program from the system:

```bash
sudo make uninstall
```

To remove object files generated during local compilation:

```bash
make clean
```

-----

**Disclaimer:** This software is an independent tool and is not affiliated with MoErgo. Use at your own risk. Always ensure you have a backup of your configuration.

