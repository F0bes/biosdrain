#define OHCI_REG_BASE 0xBF801600

int usbInitialize(void);

int usbGetHardwareInfo(t_PS2DBUSBHardwareInfo *devinfo);
