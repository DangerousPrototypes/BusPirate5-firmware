#include <stdio.h>
#include <stdint.h>

#define REG_VENDOR_ID 0
#define REG_PRODUCT_ID 2
#define REG_DEVICE_ID 4
#define REG_USBTYPEC_REV 6
#define REG_USBPD_REV_VER 8
#define REG_PD_INTERFACE_REV 0xa
#define REG_ALERT 0x10
#define REG_ALERT_MASK 0x12
#define REG_POWER_STATUS_MASK 0x14
#define REG_FAULT_STATUS_MASK 0x15
#define REG_EXTENDED_STATUS_MASK 0x16
#define REG_ALERT_EXTENDED_MASK 0x17
#define REG_CONFIG_STANDARD_OUTPUT 0x18
#define REG_TCPC_CONTROL 0x19
#define REG_ROLE_CONTROL 0x1a
#define REG_FAULT_CONTROL 0x1b
#define REG_POWER_CONTROL 0x1c
#define REG_CC_STATUS 0x1d
#define REG_POWER_STATUS 0x1e
#define REG_FAULT_STATUS 0x1f
#define REG_EXTENDED_STATUS 0x20
#define REG_ALERT_EXTENDED 0x21
#define REG_COMMAND 0x23
#define REG_DEVICE_CAPABILITIES_1 0x24
#define REG_DEVICE_CAPABILITIES_2 0x26
#define REG_STANDARD_INPUT_CAPABILITIES 0x28
#define REG_STANDARD_OUTPUT_CAPABILITIES 0x29
#define REG_CONFIGURE_EXTENDED1 0x2a
#define REG_GENERIC_TIMER 0x2c
#define REG_MESSAGE_HEADER_INFO 0x2e
#define REG_RECEIVE_DETECT 0x2f
#define REG_RX_BUF_BYTE_x 0x30
#define REG_TRANSMIT 0x50
#define REG_TX_BUF_BYTE_x 0x51
#define REG_VBUS_VOLTAGE 0x70
#define REG_VBUS_SINK_DISCONNECT_THRESHOLD 0x72
#define REG_VBUS_STOP_DISCHARGE_THRESHOLD 0x74
#define REG_VBUS_VOLTAGE_ALARM_HI_CFG 0x76
#define REG_VBUS_VOLTAGE_ALARM_LO_CFG 0x78
#define REG_VBUS_HW_TARGET 0x7a
#define REG_EXT_STATUS 0x90
#define REG_VCONN_CONFIG 0x9c


static uint32_t reg_read8(uint32_t reg) {
  return 0;
}

static uint32_t reg_read16(uint32_t reg) {
  return 0;
}

static void reg_dump(void) {
  printf("VEND  0x%04x\r\n", reg_read16(REG_VENDOR_ID));
  printf("PROD  0x%04x\r\n", reg_read16(REG_PRODUCT_ID));
  printf("DEV   0x%04x\r\n", reg_read16(REG_DEVICE_ID));
  printf("CREV  0x%04x\r\n", reg_read16(REG_USBTYPEC_REV));
  printf("PDREV 0x%04x\r\n", reg_read16(REG_USBPD_REV_VER));
  printf("IFREV 0x%04x\r\n", reg_read16(REG_PD_INTERFACE_REV));
  printf("ALERT 0x%04x\r\r", reg_read16(REG_ALERT));
  printf("ALMSK 0x%04x\r\r", reg_read16(REG_ALERT_MASK));
  printf("PSMSK 0x%04x\r\r", reg_read16(REG_POWER_STATUS_MASK));
  printf("FSMSK 0x%02x\r\r", reg_read8(REG_FAULT_STATUS_MASK));
  printf("ESMSK 0x%02x\r\r", reg_read8(REG_EXTENDED_STATUS_MASK));
  printf("AEMSK 0x%02x\r\r", reg_read8(REG_ALERT_EXTENDED_MASK));
  printf("CFGSO 0x%02x\r\r", reg_read8(REG_CONFIG_STANDARD_OUTPUT));
  printf("TCCTL 0x%02x\r\r", reg_read8(REG_TCPC_CONTROL));
  printf("RLCTL 0x%02x\r\r", reg_read8(REG_ROLE_CONTROL));
  printf("FTCTL 0x%02x\r\r", reg_read8(REG_FAULT_CONTROL));
  printf("PWCTL 0x%02x\r\r", reg_read8(REG_POWER_CONTROL));
  printf("CCSTA 0x%02x\r\r", reg_read8(REG_CC_STATUS));
  printf("PWSTA 0x%02x\r\r", reg_read8(REG_POWER_STATUS));
  printf("FTSTA 0x%02x\r\r", reg_read8(REG_FAULT_STATUS));
  printf("EXSTA 0x%02x\r\r", reg_read8(REG_EXTENDED_STATUS));
  printf("ALEXT 0x%02x\r\r", reg_read8(REG_ALERT_EXTENDED));
  // REG_COMMAND is WRITE ONLY
  printf("DEVC1 0x%04x\r\r", reg_read16(REG_DEVICE_CAPABILITIES_1));
  printf("DEVC2 0x%04x\r\r", reg_read16(REG_DEVICE_CAPABILITIES_2));
  printf("STDIC 0x%02x\r\r", reg_read8(REG_STANDARD_INPUT_CAPABILITIES));
  printf("STDOC 0x%02x\r\r", reg_read8(REG_STANDARD_OUTPUT_CAPABILITIES));
  printf("CFGE1 0x%02x\r\r", reg_read8(REG_CONFIGURE_EXTENDED1));
  // REG_GENERIC_TIMER is WRITE ONLY
  printf("MSGHI 0x%02x\r\r", reg_read8(REG_MESSAGE_HEADER_INFO));
  printf("MSGHI 0x%02x\r\r", reg_read8(REG_RECEIVE_DETECT));
  // change on READ printf("RXCNT 0x%02x\r\r", reg_read8(REG_READABLE_BYTE_COUNT));
  printf("XMIT 0x%02x\r\r", reg_read8(REG_TRANSMIT));
  // REG_TX_BUF_BYTE_x is WRITE ONLY
  printf("VBUSV 0x%04x\r\r", reg_read16(REG_VBUS_VOLTAGE));
  // REG_VBUS_SINK_DISCONNECT_THRESHOLD
  // REG_VBUS_STOP_DISCHARGE_THRESHOLD
  // REG_VBUS_VOLTAGE_ALARM_HI_CFG
  // REG_VBUS_VOLTAGE_ALARM_LO_CFG 
  printf("HVTGT 0x%04x\r\r", reg_read16(REG_VBUS_HW_TARGET));
  printf("XTSTA 0x%04x\r\r", reg_read16(REG_EXT_STATUS));
  printf("VCCFG 0x%02x\r\r", reg_read8(REG_VCONN_CONFIG));
}
