# 🦊 Luckfox Pico – SPI Configuration and Build Guide

This guide explains how to **set up**, **modify**, **build**, and **upload** firmware for the **Luckfox Pico** board.  
Before building, make sure to configure the correct **SPI controller** and install the required dependencies.

---

## ⚙️ Prerequisites

The **Luckfox Pico-SDK** is primarily developed and tested on **Ubuntu LTS**, with official support for **Ubuntu 22.04**.  
If you're using Ubuntu 22.04, simply install the required dependencies below.

> 💡 For other Ubuntu versions, a preconfigured **Docker environment** is available to ensure compatibility.  
> 📘 More information: [Luckfox Pico SDK Compilation Wiki](https://wiki.luckfox.com/Luckfox-Pico-Pro-Max/SDK-Image-Compilation)

---

## 🧩 Build Dependencies

Run the following commands to update your system and install all necessary packages:

```bash
sudo apt update
```

```bash
sudo apt-get install -y   git ssh make gcc gcc-multilib g++-multilib module-assistant expect   g++ gawk texinfo libssl-dev bison flex fakeroot cmake unzip gperf   autoconf device-tree-compiler libncurses5-dev pkg-config bc   python-is-python3 passwd openssl openssh-server openssh-client   vim file cpio rsync curl
```

---

## 🔧 SPI Configuration

Before building the SDK, you must **change the active SPI controller**.

### 1️⃣ Initialize the Build Configuration

Before building, run the **lunch** setup command to select your board configuration:

```bash
cd luckfox-pico
./build.sh lunch
```

> 💡 This will prompt you to select your specific **board variant** (e.g., Luckfox Pico, Pico Pro, or Pico Max).  
> Make sure you select the correct target before proceeding.


### Locate the Device Tree File
Within the directory:

```
luckfox-pico/config
```

you’ll find a file named `dts_config`, which is a **symlink** to the actual `.dts` device tree file.

### Modify the SPI Node
Open the linked `.dts` file and locate the section for **`&spi0`**.  
Modify it as follows (or ensure it matches this configuration):

```dts
/********** SPI Configuration **********/
&spi0 {
    status = "okay";

    // Optional pin configuration:
    // pinctrl-names = "default";
    // pinctrl-0 = <&spi0m0_clk &spi0m0_mosi &spi0m0_miso &spi0m0_cs0>;

    spidev@0 {
        compatible = "rockchip,spidev";
        reg = <0>;                      // CS0
        spi-max-frequency = <10000000>; // Safe for nRF24 modules
        status = "okay";
    };
};
```

> ⚠️ Make sure the correct SPI interface (`&spi0`, `&spi1`, etc.) is enabled for your hardware setup.

---

## 🏗️ Building the SDK

Once your SPI configuration is set, follow the steps below to build the SDK:

### 1️⃣ Initialize the Build Configuration
If not done before, run the **lunch** setup command to select your board configuration:

```bash
cd luckfox-pico
./build.sh lunch
```

> 💡 This will prompt you to select your specific **board variant** (e.g., Luckfox Pico, Pico Pro, or Pico Max).  
> Make sure you select the correct target before proceeding.

### 2️⃣ Compile the SDK

After selecting the target, build the SDK:

```bash
./build.sh
```

The script will compile the SDK and generate the necessary firmware image.

---

## 🚀 Uploading to the Board

1. **Enter Bootloader Mode**
   - Press and hold the **BOOT** button on your Luckfox Pico board.
   - While holding **BOOT**, **reset** the board to enter bootloader mode.

2. **Upload the Firmware**

```bash
sudo ./rkflash.sh update
```

> 💡 If the board is not detected, ensure you have the correct USB permissions or try a different USB cable.

---

## ✅ Summary

| Step | Description |
|------|--------------|
| 1️⃣ | Install dependencies |
| 2️⃣ | Run `./build.sh lunch` and select your board |
| 3️⃣ | Edit SPI configuration in `.dts` |
| 4️⃣ | Run `./build.sh` to compile |
| 5️⃣ | Set board to bootloader mode |
| 6️⃣ | Flash with `sudo ./rkflash.sh update` |

---

### ✨ Notes

- The provided SPI configuration works well with **nRF24** and similar SPI devices.  
- You can experiment with higher SPI frequencies if your peripheral supports it.  
- For more advanced configuration (GPIO remapping, multiple SPI devices, etc.), consult the official [Luckfox Wiki](https://wiki.luckfox.com/).