# oc-utils
OpenCAPI Utils, abbreviated as oc-utils, is a scripts repository to program the flash, reset card, reload flash and debug.

# Contributing
This is an open-source project. We greatly appreciate your contributions and collaboration.
Before contributing to this project, please read and agree to the rules in
* [CONTRIBUTING.md](CONTRIBUTING.md)

# Usage
A typical use of oc-utils will follow this pattern:

1. **make:** make command will compile all the source codes.
2. **./oc-flash-script.sh primary.bin secondary.bin:** Write binary files to FLASH. You can select which card will be flashed according to the OpenCAPI card connection node.
3. **./oc-reload.sh:** Reload the image from FLASH to FPGA core which is usually the next step of oc-flash-script
4. **./oc-reset.sh:** Reset one CARD as you select (Sent reset signal to FPGA). You should be aware that this is only a reset operation for a specific OpenCAPI card.
4. **./oc-list-cards.sh:** List the card programming information. 

Add "-h" to get more options for above scripts.

# Example:
After `git clone` this repository, 

```
make
sudo ./oc-flash-script.sh primary.bin secondary.bin
sudo ./oc-reload.sh
```

# Note: 
Have been verified for following FPGA cards in OC-Accel supported list:
* AD9V3
* AD9H3

On System FP5290G2, IC922, S924 with specific firmware (skiboot) and OS kernels. 
Contact your technical support team for more information.


