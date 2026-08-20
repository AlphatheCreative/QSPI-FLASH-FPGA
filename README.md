# Persistent Data Storage via QSPI Flash on FPGA

Writing data to the onboard QSPI flash on a Xilinx Zynq-7020 FPGA so it survives power cycles, the FPGA reads the data back correctly after a full power-off/power-on, proving true non-volatile storage rather than relying on volatile block RAM or registers that reset on reboot.

**Board:** Alinx AC7020C (Xilinx Zynq-7020, XC7Z020-2CLG400I)
**Flash:** Micron MT25QL256 (32MB QSPI serial NOR flash), single, x4 mode

## Overview

The Zynq PS (Processing System) has a dedicated hard QSPI controller wired directly to the board's onboard flash chip, this is fixed hardware wiring, not reachable from general-purpose PL (fabric) I/O pins. That means writing to this flash requires going through the PS, either via Vivado's built-in flash programming tool or via software running on the ARM processor.

This project uses **Vivado's Hardware Manager flash programming flow** as the primary method, no custom software required, no risk of accidentally corrupting the boot image through manual low-level flash commands.

## Repository structure

```
├── data/
│   └── testdata.bin       # Test payload: "HELLO THIS IS TESTING DATA"
├── src/
│   └── main.c              # Alternative: Vitis baremetal QSPI driver approach
└── README.md
```

## Method used: Vivado Hardware Manager (primary)

1. Prepare a binary data file (see `data/testdata.bin`)
2. Open **Hardware Manager** → Auto Connect → right-click the device → **Add Configuration Memory Device**
3. Select the correct flash part: `mt25ql256-qspi-x4-single`
4. Right-click the device → **Program Configuration Memory Device**
5. Set the configuration file to the `.bin` data file
6. **Address offset: `0x600000`**  deliberately chosen well past the start of flash to avoid overwriting the FPGA's boot bitstream image, which typically resides at the beginning of flash
7. Enable Erase, Program, and Verify → Program
8. Power-cycle the board completely
9. Use Hardware Manager's readback/verify function to confirm the data is still present after the power cycle

⚠️ **Do not write to address `0x0`** or low offsets without first confirming your board's boot configuration — this region likely holds the bitstream image needed for the FPGA to auto-configure at power-on.

## Alternative method: Vitis baremetal application

`src/main.c` implements the same erase/write/read cycle in C, using the `XQspiPs` driver directly. This approach is useful if the FPGA design itself needs to decide *what* to write to flash dynamically at runtime, rather than loading a fixed pre-made file. Requires a Zynq PS block design with QSPI enabled and an FSBL for JTAG-based flash programming support.

## Results

- Successfully wrote and verified `HELLO THIS IS TESTING DATA` (26 bytes) to flash address `0x600000`
- Confirmed data integrity after a full power cycle — data was correctly read back, unchanged
- No impact to the board's boot configuration (safe address offset used throughout)

## License

MIT — see [LICENSE](LICENSE)
