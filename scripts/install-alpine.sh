#!/usr/bin/env sh

# Setup controls:
# - Hostname: chromelab (whatever you want)
# - Root Password: ************* (whatever you want)
# - Username: lab
# - User Password: ************* (whatever you want)
#
# Unless you know what your doing, use the default option for everything else
setup-alpine

# Verify that the SSH daemon (OpenSSH) is running
rc-service sshd status

reboot
