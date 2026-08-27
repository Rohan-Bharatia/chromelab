# Installation Process

This guide walks you through installing **Chromelab** on a fresh Alpine Linux system.

## 1. Prepare the Alpine Linux Installer

1. Download the **Alpine Linux Extended ISO** from the [Alpine Linux website](https://alpinelinux.org/downloads/).
2. Write the downloaded ISO image to a USB drive using your preferred disk-imaging tool.
3. Insert the USB drive into the target machine.
4. Enter the machine's BIOS/UEFI settings and configure the USB drive as the first boot device.
5. Save your changes and reboot the machine.
6. Boot into the Alpine Linux installer.

## 2. Install Alpine Linux

Once the Alpine Linux installer has booted, run:

```sh
setup-alpine
```

Follow the prompts to configure and install Alpine Linux.

When the installation is complete, reboot the machine:

```sh
reboot
```

Remove the installation USB when prompted or before the machine boots again, then boot into the newly installed Alpine Linux system.

## 3. Install Chromelab

Log in and switch to the root account:

```sh
su -
```

Install `curl`:

```sh
apk add curl
```

Download the Chromelab installation script:

```sh
curl -fsSL https://raw.githubusercontent.com/Rohan-Bharatia/chromelab/refs/heads/main/install.sh -o install.sh
```

Run the installer:

```sh
sh install.sh
```

> The installer will install the required dependencies, configure the system, build Chromelab, install `labd` and `labctl`, configure the web dashboard, and set up `labd` as an OpenRC service.

## 4. Finish the installation

Once the installer reports that installation is complete, reboot the system:

```sh
reboot
```

After the system starts again, labd should start automatically through OpenRC.

You can check its status with:

```sh
rc-service labd status
```

The web dashboard is available at:

```
http://<machine-ip>:8080
```

You can find the machine's IP address with:

```sh
hostname -i
```

The Chromelab configuration is located at:

```
/etc/chromelab/labd.conf
```

and the command-line client can be used with:

```sh
labctl status
```
