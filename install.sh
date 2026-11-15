#!/bin/bash
echo "Building OS-Pulse Pro v2.0..."
gcc -std=c17 os_pulse_pro_v2.c -o os_pulse
echo "Installing to /usr/local/bin/ospulse..."
sudo cp os_pulse /usr/local/bin/ospulse
echo "Installation complete! You can now run 'ospulse' from anywhere."

