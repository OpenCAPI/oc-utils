# oc-utils
OpenCAPI Utils, abbreviated as oc-utils, is a scripts repository to program the flash, reset card, reload flash and debug.

# Contributing
This is an open-source project. We greatly appreciate your contributions and collaboration.
Before contributing to this project, please read and agree to the rules in
* [CONTRIBUTING.md](CONTRIBUTING.md)

# Usage
A typical use of oc-utils will follow this pattern:

1. **make:** make command will compile all the source codes.
2. **./oc-flash-script.sh primary.bin secondary.bin:** this command will write binary files to FLASH. You can select which card will be flashed according to the OpenCAPI card connection node.
3. **./oc-reload.sh:** this command will reload the image from FLASH to FPGA core which is usually a next step for oc-flash-script
4. **./oc-reset.sh:** this command will reset one CARD as you select. You should be aware that this is a only reset operation for a specific OpenCAPI card.
