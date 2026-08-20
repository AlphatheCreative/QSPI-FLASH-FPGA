/*
 * Alternative approach: writing to QSPI flash via a Vitis baremetal
 * application using the PS Quad-SPI controller, instead of Vivado's
 * "Program Configuration Memory Device" flow (which was used as the
 * primary method for this project - see README).
 *
 * Useful if the FPGA design itself needs to decide what to write to
 * flash at runtime, rather than loading a fixed pre-made data file.
 */

#include "xparameters.h"
#include "xqspips.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "sleep.h"
#include <string.h>

#define FLASH_ADDR   0x600000
#define PAGE_SIZE    256

#define WRITE_ENABLE_CMD   0x06
#define SECTOR_ERASE_CMD   0xD8
#define PAGE_PROGRAM_CMD   0x02
#define READ_STATUS_CMD    0x05

static XQspiPs QspiInstance;
static u8 WriteBuffer[PAGE_SIZE + 4];
static u8 ReadBuffer[PAGE_SIZE + 4];

void WaitForFlashReady(void) {
    u8 status;
    u8 cmd[2];
    u8 rbuf[2];
    do {
        cmd[0] = READ_STATUS_CMD;
        cmd[1] = 0x00;
        XQspiPs_PolledTransfer(&QspiInstance, cmd, rbuf, 2);
        status = rbuf[1];
    } while (status & 0x01);
}

void FlashWriteEnable(void) {
    u8 cmd = WRITE_ENABLE_CMD;
    XQspiPs_PolledTransfer(&QspiInstance, &cmd, NULL, 1);
}

void FlashEraseSector(u32 addr) {
    u8 cmd[4];
    FlashWriteEnable();
    cmd[0] = SECTOR_ERASE_CMD;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;
    XQspiPs_PolledTransfer(&QspiInstance, cmd, NULL, 4);
    WaitForFlashReady();
}

void FlashWritePage(u32 addr, u8 *data, u32 len) {
    FlashWriteEnable();
    WriteBuffer[0] = PAGE_PROGRAM_CMD;
    WriteBuffer[1] = (addr >> 16) & 0xFF;
    WriteBuffer[2] = (addr >> 8) & 0xFF;
    WriteBuffer[3] = addr & 0xFF;
    memcpy(&WriteBuffer[4], data, len);
    XQspiPs_PolledTransfer(&QspiInstance, WriteBuffer, NULL, len + 4);
    WaitForFlashReady();
}

void FlashReadData(u32 addr, u8 *data, u32 len) {
    WriteBuffer[0] = 0x03;
    WriteBuffer[1] = (addr >> 16) & 0xFF;
    WriteBuffer[2] = (addr >> 8) & 0xFF;
    WriteBuffer[3] = addr & 0xFF;
    memset(&WriteBuffer[4], 0, len);
    XQspiPs_PolledTransfer(&QspiInstance, WriteBuffer, ReadBuffer, len + 4);
    memcpy(data, &ReadBuffer[4], len);
}

int main() {
    XQspiPs_Config *QspiConfig;
    int Status;

    xil_printf("QSPI Flash Write/Read Test\r\n");

    QspiConfig = XQspiPs_LookupConfig(XPAR_XQSPIPS_0_DEVICE_ID);
    Status = XQspiPs_CfgInitialize(&QspiInstance, QspiConfig, QspiConfig->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("QSPI Init Failed\r\n");
        return XST_FAILURE;
    }

    XQspiPs_SetOptions(&QspiInstance, XQSPIPS_MANUAL_START_OPTION | XQSPIPS_FORCE_SSELECT_OPTION);
    XQspiPs_SetClkPrescaler(&QspiInstance, XQSPIPS_CLK_PRESCALE_8);

    u8 test_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x11, 0x22, 0x33, 0x44};
    u8 read_data[8] = {0};

    xil_printf("Erasing sector at 0x%08X...\r\n", FLASH_ADDR);
    FlashEraseSector(FLASH_ADDR);

    xil_printf("Writing test data...\r\n");
    FlashWritePage(FLASH_ADDR, test_data, 8);

    xil_printf("Reading back data...\r\n");
    FlashReadData(FLASH_ADDR, read_data, 8);

    xil_printf("Read data: ");
    int i;
    for (i = 0; i < 8; i++) {
        xil_printf("%02X ", read_data[i]);
    }
    xil_printf("\r\n");

    if (memcmp(test_data, read_data, 8) == 0) {
        xil_printf("SUCCESS: Data matches!\r\n");
    } else {
        xil_printf("FAILURE: Data mismatch!\r\n");
    }

    return 0;
}
