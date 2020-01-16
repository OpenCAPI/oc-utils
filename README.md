# oc-utils
OpenCAPI Utils, abbreviated as oc-utils, is a scripts repository to program the flash, reset card, reload flash and debug.

# Contributing
This is an open-source project. We greatly appreciate your contributions and collaboration.
Before contributing to this project, please read and agree to the rules in
* [CONTRIBUTING.md](CONTRIBUTING.md)

# Usage
Compile:

* **make:** make command will compile all the source codes.
* **make install/make uninstall:** Copy/Remove the executable scripts in /bin

Scripts:

* **oc-flash-script.sh primary.bin secondary.bin:** Write binary files to FLASH. You can select which card will be flashed according to the OpenCAPI card connection node.
* **oc-reload.sh:** Reload the image from FLASH to FPGA core which is usually the next step of oc-flash-script
* **oc-reset.sh:** Reset one CARD as you select (Sent reset signal to FPGA). You should be aware that this is only a reset operation for a specific OpenCAPI card.
* **oc-list-cards.sh:** List the card programming information. 

Add "-h" to get more options for above scripts.

# Example:
After `git clone` this repository, call oc-flash-script and oc-reload to refresh the FPGA bitstream online. 

```
make
sudo ./oc-flash-script.sh primary.bin secondary.bin
sudo ./oc-reload.sh
```

For some systems, a cold reboot is required to get the new FPGA bitstream work:
```
sudo ./oc-flash-script.sh primary.bin secondary.bin
sudo reboot
```

# Note: 

Online updating had been verified on following FPGA cards with OC-Accel bitstreams:

* AD9V3
* AD9H3

on System FP5290G2, IC922, S924 with specific firmware (skiboot) and OS kernels. 

Contact your technical support team for more information.



