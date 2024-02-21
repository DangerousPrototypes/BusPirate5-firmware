enum DEV_I2C_LIST{
    DEV_I2C_LIST_NONE=0,
	DEV_I2C_LIST_0,
	DEV_I2C_LIST_1,
	DEV_I2C_LIST_2,
	DEV_I2C_LIST_3,
	DEV_I2C_LIST_4,
	DEV_I2C_LIST_5,
	DEV_I2C_LIST_6,
	DEV_I2C_LIST_7,
	DEV_I2C_LIST_8,
	DEV_I2C_LIST_9,
	DEV_I2C_LIST_10,
	DEV_I2C_LIST_11,
	DEV_I2C_LIST_12,
	DEV_I2C_LIST_13,
	DEV_I2C_LIST_14,
	DEV_I2C_LIST_15,
	DEV_I2C_LIST_16,
	DEV_I2C_LIST_17,
	DEV_I2C_LIST_18,
	DEV_I2C_LIST_19,
	DEV_I2C_LIST_20,
	DEV_I2C_LIST_21,
	DEV_I2C_LIST_22,
	DEV_I2C_LIST_23,
	DEV_I2C_LIST_24,
	DEV_I2C_LIST_25,
	DEV_I2C_LIST_26,
	DEV_I2C_LIST_27,
	DEV_I2C_LIST_28,
	DEV_I2C_LIST_29,
	DEV_I2C_LIST_30,
	DEV_I2C_LIST_31,
	DEV_I2C_LIST_32,
	DEV_I2C_LIST_33,
	DEV_I2C_LIST_34,
	DEV_I2C_LIST_35,
	DEV_I2C_LIST_36,
	DEV_I2C_LIST_37,
	DEV_I2C_LIST_38,
	DEV_I2C_LIST_39,
	DEV_I2C_LIST_40,
	DEV_I2C_LIST_41,
	DEV_I2C_LIST_42,
	DEV_I2C_LIST_43,
	DEV_I2C_LIST_44,
	DEV_I2C_LIST_45,
	DEV_I2C_LIST_46,
	DEV_I2C_LIST_47,
	DEV_I2C_LIST_48,
	DEV_I2C_LIST_49,
	DEV_I2C_LIST_50,
	DEV_I2C_LIST_51,
	DEV_I2C_LIST_52,
	DEV_I2C_LIST_53,
	DEV_I2C_LIST_54,
	DEV_I2C_LIST_55,
	DEV_I2C_LIST_56,
	DEV_I2C_LIST_57,
	DEV_I2C_LIST_58,
	DEV_I2C_LIST_59,
	DEV_I2C_LIST_60,
	DEV_I2C_LIST_61,
	DEV_I2C_LIST_62,
	DEV_I2C_LIST_63,
	DEV_I2C_LIST_64,
	DEV_I2C_LIST_65,
	DEV_I2C_LIST_66,
	DEV_I2C_LIST_67,
	DEV_I2C_LIST_68,
	DEV_I2C_LIST_69,
	DEV_I2C_LIST_70,
	DEV_I2C_LIST_71,
	DEV_I2C_LIST_72,
	DEV_I2C_LIST_73,
	DEV_I2C_LIST_74,
	DEV_I2C_LIST_75,
	DEV_I2C_LIST_76,
	DEV_I2C_LIST_77,
	DEV_I2C_LIST_78,

};

static char const * const dev_i2c_addresses_text[]={
    [DEV_I2C_LIST_NONE]="",
	[DEV_I2C_LIST_0]="Reserved",
	[DEV_I2C_LIST_1]="Reserved for CBUS compatibility",
	[DEV_I2C_LIST_2]="Reserved for I2C-compatible bus variants",
	[DEV_I2C_LIST_3]="Reserved for future use",
	[DEV_I2C_LIST_4]="Reserved for HS-mode controller",
	[DEV_I2C_LIST_5]="LC709203F Fuel Gauge and Battery Monitor",
	[DEV_I2C_LIST_6]="MLX90393 3-Axis Magnetometer",
	[DEV_I2C_LIST_7]="MAG3110 3-Axis Magnetometer\r\nMLX90393 3-Axis Magnetometer",
	[DEV_I2C_LIST_8]="VEML6075 UV Sensor\r\nVEML7700 Ambient Light Sensor",
	[DEV_I2C_LIST_9]="Si4713 FM Transmitter with RDS",
	[DEV_I2C_LIST_10]="PMSA0031 Gas Sensor",
	[DEV_I2C_LIST_11]="VCNL40x0 Proximity Sensor",
	[DEV_I2C_LIST_12]="MCP9808 Temperature Sensor\r\nMPRLS Pressure Sensor\r\nLIS331 3-Axis Accelerometer\r\nLIS3DH 3-Axis Accelerometer",
	[DEV_I2C_LIST_13]="MCP9808 Temperature Sensor\r\nLIS331 3-Axis Accelerometer\r\nLIS3DH 3-Axis Accelerometer\r\nLSM303 Accelerometer/Magnetometer",
	[DEV_I2C_LIST_14]="AGS02MA TVOC Gas Sensor\r\nMCP9808 Temperature Sensor",
	[DEV_I2C_LIST_15]="MCP9808 Temperature Sensor",
	[DEV_I2C_LIST_16]="LIS3MDL Magetometer\r\nMCP9808 Temperature Sensor\r\nMMA845x 3-Axis Accelerometer\r\nFXOS8700 Accelerometer/Magnetometer\r\nMMA7455L",
	[DEV_I2C_LIST_17]="ADXL343 3-Axis Accelerometer\r\nADXL345 3-Axis Accelerometer\r\nFXOS8700 Accelerometer/Magnetometer\r\nLSM9DS0 9-Axis IMU\r\nMCP9808 Temperature Sensor\r\nMMA7455L\r\nMMA845x 3-Axis Accelerometer",
	[DEV_I2C_LIST_18]="FXOS8700 Accelerometer/Magnetometer\r\nHMC5883 Magnetometer\r\nLIS2MDL Magnetometer\r\nLIS3MDL Magnetometer\r\nLSM303 Accelerometer/Magnetometer\r\nLSM9DS0 9-Axis IMU\r\nMCP9808 Temperature Sensor",
	[DEV_I2C_LIST_19]="MCP9808 Temperature Sensor\r\nFXOS8700 Accelerometer & Magnetometer",
	[DEV_I2C_LIST_20]="FXAS21002 Gyroscope\r\nChirp! Water Sensor\r\nMCP23008 GPIO Expander\r\nMCP23017 GPIO Expander",
	[DEV_I2C_LIST_21]="FXAS21002 Gyroscope\r\nMCP23008 GPIO Expander\r\nMCP23017 GPIO Expander",
	[DEV_I2C_LIST_22]="MCP23008 GPIO Expander\r\nMCP23017 GPIO Expander",
	[DEV_I2C_LIST_23]="BH1750 Light Sensor\r\nMCP23008 GPIO Expander\r\nMCP23017 GPIO Expander",
	[DEV_I2C_LIST_24]="MCP23008 GPIO Expander\r\nMCP23017 GPIO Expander\r\nMSA301 3-Axis Accelerometer",
	[DEV_I2C_LIST_25]="BNO055 IMU\r\nCAP1188 8-Channel Capacitive Touch\r\nDS1841 Digital Logarithmic Potentiometer\r\nDS3502 Digital 10K Potentiometer\r\nPCT2075 Temperature Sensor\r\nTSL2591 Light Sensor",
	[DEV_I2C_LIST_26]="BNO055 IMU\r\nDS1841 Digital Logarithmic Potentiometer\r\nDS3502 Digital 10K Potentiometer\r\nPCT2075 Temperature Sensor\r\nTCS34725 Color Sensor\r\nTSL2561 Light Sensor\r\nTSL2591 Light Sensor\r\nVL53L0x ToF Sensor\r\nVL6180X ToF Sensor\r\nCAP1188 8-Channel Capacitive Touch",
	[DEV_I2C_LIST_27]="CAP1188 8-Channel Capacitive Touch\r\nDS1841 Digital Logarithmic Potentiometer\r\nDS3502 Digital 10K Potentiometer\r\nPCT2075 Temperature Sensor",
	[DEV_I2C_LIST_28]="CAP1188 8-Channel Capacitive Touch\r\nPCT2075 Temperature Sensor",
	[DEV_I2C_LIST_29]="PCT2075 Temperature Sensor",
	[DEV_I2C_LIST_30]="MLX90640 IR Thermal Camera",
	[DEV_I2C_LIST_31]="Adafruit Stemma QT Rotary Encoder with NeoPixel\r\nMAX17048 LiPoly/LiIon Fuel Gauge and Battery Monitor",
	[DEV_I2C_LIST_32]="Adafruit Stemma QT Rotary Encoder with NeoPixel",
	[DEV_I2C_LIST_33]="AHT20 Humidity/Temperature Sensor\r\nDHT20 Humidity/Temperature Sensor\r\nVEML6070 UV Index\r\nFT6x06 Capacitive Touch Driver\r\nNCP5623 RGB LED Driver\r\nAdafruit Stemma QT Rotary Encoder with NeoPixel",
	[DEV_I2C_LIST_34]="AS7341 Color Sensor\r\nTSL2561 Light Sensor\r\nVEML6070 UV Light Sensor\r\nAPDS-9960 IR/Color/Proximity Sensor\r\nAdafruit Stemma QT Rotary Encoder with NeoPixel",
	[DEV_I2C_LIST_35]="PCF8577C LCD direct/duplex driver\r\nAdafruit Stemma QT Rotary Encoder with NeoPixel",
	[DEV_I2C_LIST_36]="SSD1305 Monochrome OLED\r\nSSD1306 Monochrome OLED\r\nSSD1309 Monochrome OLED\r\nSSD1315 Monochrome OLED\r\nSH1106 Monochrome OLED\r\nCH1115 Monochrome OLED\r\nCH1116 Monochrome OLED\r\nSSD1327 16-Level Grayscale OLED\r\nST75256 4-Level Grayscale LCD\r\nAdafruit Stemma QT Rotary Encoder with NeoPixel",
	[DEV_I2C_LIST_37]="ST75256 4-Level Grayscale LCD",
	[DEV_I2C_LIST_38]="Si7021 Humidity/Temperature Sensor\r\nHTU21D-F Humidity/Temperature Sensor\r\nHTU31D Humidity/Temperature Sensor\r\nHDC1008 Humidity/Temperature Sensor\r\nMS8607 Humidity/Temperature/Pressure Sensor\r\nTMP007 IR Temperature Sensor\r\nTMP006 IR Temperature Sensor\r\nPCA9685 16-Channel PWM Driver (default address)\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor",
	[DEV_I2C_LIST_39]="HDC1008 Humidity/Temperature Sensor\r\nHTU31D Humidity/Temperature Sensor\r\nTMP007 IR Temperature Sensor\r\nTMP006 IR Temperature Sensor\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor\r\nSTMPE610/STMPE811 Resistive Touch Controller",
	[DEV_I2C_LIST_40]="HDC1008 Humidity/Temperature Sensor\r\nTMP007 IR Temperature Sensor\r\nTMP006 IR Temperature Sensor\r\nINA219 High-Side DC Current/Voltage Sensor",
	[DEV_I2C_LIST_41]="HDC1008 Humidity/Temperature Sensor\r\nTMP007 IR Temperature Sensor\r\nTMP006 IR Temperature Sensor\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor",
	[DEV_I2C_LIST_42]="SHT45 Humidity/Temperature Sensor\r\nSHT40 Humidity/Temperature Sensor\r\nSHT31 Humidity/Temperature Sensor\r\nTMP007 IR Temperature Sensor\r\nTMP006 IR Temperature Sensor\r\nISL29125 Color Sensor\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor\r\nSTMPE610/STMPE811 Resistive Touch controller",
	[DEV_I2C_LIST_43]="SHT31 Humidity/Temperature Sensor\r\nTMP007 IR Temperature Sensor\r\nTMP006 IR Temperature Sensor\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor",
	[DEV_I2C_LIST_44]="TMP007 IR Temperature Sensor\r\nTMP006 IR Temperature Sensor\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor",
	[DEV_I2C_LIST_45]="ADS1115 4-channel 16-Bit ADC\r\nADT7410 Temperature Sensor\r\nATSAMD09 Breakout with seesaw\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor\r\nPCF8591 Quad 8-Bit ADC + 8-Bit DAC\r\nPCT2075 Temperature Sensor\r\nPN532 NFC/RFID reader\r\nTMP102 Temperature Sensor\r\nTMP117 Temperature Sensor",
	[DEV_I2C_LIST_46]="ADS1115 4-Channel 16-Bit ADC\r\nADT7410 Temperature Sensor\r\nAS7262 Light/Color Sensor\r\nATSAMD09 Breakout with seesaw\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor\r\nPCF8591 Quad 8-Bit ADC + 8-Bit DAC\r\nPCT2075 Temperature Sensor\r\nTSL2561 Light Sensor\r\nTMP102 Temperature Sensor\r\nTMP117 Temperature Sensor",
	[DEV_I2C_LIST_47]="ADS1115 4-channel 16-Bit ADC\r\nADT7410 Temperature Sensor\r\nATSAMD09 Breakout with seesaw\r\nBNO085 9-DoF IMU\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor\r\nPCF8591 Quad 8-Bit ADC + 8-Bit DAC\r\nPCT2075 Temperature Sensor\r\nTMP102 Temperature Sensor\r\nTMP117 Temperature Sensor",
	[DEV_I2C_LIST_48]="EMC2101 Fan Controller\r\nINA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor\r\nPCF8591 Quad 8-Bit ADC + 8-Bit DAC\r\nPCT2075 Temperature Sensor",
	[DEV_I2C_LIST_49]="INA219 High-Side DC Current/Voltage Sensor\r\nINA260 Precision DC Current/Power Sensor\r\nPCF8591 Quad 8-Bit ADC + 8-Bit DAC\r\nPCT2075 Temperature Sensor",
	[DEV_I2C_LIST_50]="MB85RC FRAM",
	[DEV_I2C_LIST_51]="MB85RC FRAM\r\nNintendo Nunchuck Controller",
	[DEV_I2C_LIST_52]="ADXL343 3-Axis Accelerometer\r\nADXL345 3-Axis Accelerometer\r\nLTR390 UV Sensor\r\nMB85RC FRAM",
	[DEV_I2C_LIST_53]="MB85RC FRAM\r\nMAX3010x Pulse & Oximetry Sensor",
	[DEV_I2C_LIST_54]="AW9523 GPIO Expander and LED Driver\r\nTPA2016 Class-D Audio Amplifier\r\nSGP30 Gas Sensor",
	[DEV_I2C_LIST_55]="AW9523 GPIO Expander and LED Driver\r\nSGP40 Gas Sensor",
	[DEV_I2C_LIST_56]="AW9523 GPIO Expander and LED Driver\r\nMPR121 12-Point Capacitive Touch Sensor\r\nCCS811 VOC Sensor\r\nMLX9061x IR Temperature Sensor\r\nDRV2605 Haptic Motor Driver",
	[DEV_I2C_LIST_57]="AW9523 GPIO Expander and LED Driver\r\nMPR121 12-Point Capacitive Touch Sensor\r\nCCS811 VOC Sensor",
	[DEV_I2C_LIST_58]="AM2315 Humidity/Temp Sensor\r\nAM2320 Humidity/Temp Sensor\r\nBH1750 Light Sensor\r\nLPS22 Pressure Sensor\r\nLPS25 Pressure Sensor\r\nLPS33HW Ported Pressure Sensor\r\nLPS35HW Pressure Sensor\r\nMPR121 12-Point Capacitive Touch Sensor",
	[DEV_I2C_LIST_59]="LPS22 Pressure Sensor\r\nLPS25 Pressure Sensor\r\nLPS33HW Ported Pressure Sensor\r\nLPS35HW Pressure Sensor\r\nMPR121 12-Point Capacitive Touch Sensor",
	[DEV_I2C_LIST_60]="TLV493D 3-Axis Magnetometer",
	[DEV_I2C_LIST_61]="HTS221 Humidity/Temperature Sensor",
	[DEV_I2C_LIST_62]="ATECC608 Cryptographic Co-Processor\r\nMCP4728 Quad DAC\r\nMCP9600 Temperature Sensor\r\nMPL115A2 Barometric Pressure\r\nMPL3115A2 Barometric Pressure\r\nSi5351A Clock Generator\r\nSi1145 Light/IR Sensor\r\nMCP4725A0 12-Bit DAC\r\nTEA5767 Radio Receiver\r\nVCNL4040 Proximity and Ambient Light Sensor",
	[DEV_I2C_LIST_63]="MCP4725A0 12-Bit DAC\r\nMCP9600 Temperature Sensor\r\nSi5351A Clock Generator\r\nSCD30 Humidity/Temperature/CO2 Sensor",
	[DEV_I2C_LIST_64]="MCP4725A1 12-Bit DAC\r\nMCP9600 Temperature Sensor",
	[DEV_I2C_LIST_65]="MCP4725A1 12-Bit DAC\r\nMCP9600 Temperature Sensor\r\nSi4713 FM Transmitter with RDS",
	[DEV_I2C_LIST_66]="MCP4725A2 12-Bit DAC\r\nMCP9600 Temperature Sensor",
	[DEV_I2C_LIST_67]="MCP4725A3 12-Bit DAC\r\nMCP9600 Temperature Sensor",
	[DEV_I2C_LIST_68]="AMG8833 IR Thermal Camera Breakout\r\nDS1307 RTC\r\nDS3231 RTC\r\nICM-20649 Accelerometer + Gyroscope\r\nITG3200 Gyroscope\r\nMPU-9250 9-DoF IMU\r\nMPU-60X0 Accelerometer + Gyroscope\r\nPCF8523 RTC",
	[DEV_I2C_LIST_69]="AMG8833 IR Thermal Camera Breakout\r\nICM-20649 Accelerometer + Gyroscope\r\nMPU-9250 9-DoF IMU\r\nMPU-60X0 Accelerometer + Gyroscope\r\nITG3200 Gyroscope",
	[DEV_I2C_LIST_70]="ICM330DHC 6-Axis IMU\r\nL3GD20H Gyroscope\r\nLSM6DS33 6-Axis IMU\r\nLSM6DSOX 6-Axis IMU\r\nLSM9DS0 9-Axis IMU",
	[DEV_I2C_LIST_71]="HT16K33 LED Matrix Driver\r\nPCT2075 Temperature Sensor\r\nTCA9548 1-to-8 I2C Multiplexer\r\nSHTC3 Temp and Humidity Sensor",
	[DEV_I2C_LIST_72]="HT16K33 LED Matrix Driver\r\nPCT2075 Temperature Sensor\r\nTCA9548 1-to-8 I2C Multiplexer",
	[DEV_I2C_LIST_73]="HT16K33 LED Matrix Driver\r\nIS31FL3731 144-LED CharliePlex driver\r\nPCT2075 Temperature Sensor\r\nTCA9548 1-to-8 I2C Multiplexer",
	[DEV_I2C_LIST_74]="BME280 Temp/Barometric/Humidity \r\nBME680 Temp/Barometric/Humidity/Gas\r\nBMP280 Temp/Barometric \r\nBMP388 Temp/Barometric\r\nBMP390 Temp/Barometric\r\nDPS310 Barometric Sensor\r\nHT16K33 LED Matrix Driver\r\nIS31FL3731 144-LED CharliePlex driver\r\nMS5607/MS5611 Barometric Pressure\r\nMS8607 Temp/Barometric/Humidity\r\nPCT2075 Temperature Sensor\r\nTCA9548 1-to-8 I2C Multiplexer",
	[DEV_I2C_LIST_75]="BMA180 Accelerometer\r\nBME280 Temp/Barometric/Humidity \r\nBME680 Temp/Barometric/Humidity/Gas\r\nBMP180 Temp/Barometric\r\nBMP085 Temp/Barometric\r\nBMP280 Temp/Barometric \r\nBMP388 Temp/Barometric\r\nBMP390 Temp/Barometric\r\nDPS310 Barometric Sensor\r\nHT16K33 LED Matrix Driver\r\nIS31FL3731 144-LED CharliePlex driver\r\nMS5607/MS5611 Barometric Pressure\r\nPCT2075 Temperature Sensor\r\nTCA9548 1-to-8 I2C Multiplexer",
	[DEV_I2C_LIST_76]="Reserved for 10-bit I2C addressing",
	[DEV_I2C_LIST_77]="Reserved for future purposes",
	[DEV_I2C_LIST_78]="Reserved for Future Purposes",

};

static const char *dev_i2c_addresses[]={
	dev_i2c_addresses_text[DEV_I2C_LIST_0], //0x00
	dev_i2c_addresses_text[DEV_I2C_LIST_1], //0x01
	dev_i2c_addresses_text[DEV_I2C_LIST_2], //0x02
	dev_i2c_addresses_text[DEV_I2C_LIST_3], //0x03
	dev_i2c_addresses_text[DEV_I2C_LIST_4], //0x04
	dev_i2c_addresses_text[DEV_I2C_LIST_4], //0x05
	dev_i2c_addresses_text[DEV_I2C_LIST_4], //0x06
	dev_i2c_addresses_text[DEV_I2C_LIST_4], //0x07
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x08
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x09
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x0a
	dev_i2c_addresses_text[DEV_I2C_LIST_5], //0x0b
	dev_i2c_addresses_text[DEV_I2C_LIST_6], //0x0c
	dev_i2c_addresses_text[DEV_I2C_LIST_6], //0x0d
	dev_i2c_addresses_text[DEV_I2C_LIST_7], //0x0e
	dev_i2c_addresses_text[DEV_I2C_LIST_6], //0x0f
	dev_i2c_addresses_text[DEV_I2C_LIST_8], //0x10
	dev_i2c_addresses_text[DEV_I2C_LIST_9], //0x11
	dev_i2c_addresses_text[DEV_I2C_LIST_10], //0x12
	dev_i2c_addresses_text[DEV_I2C_LIST_11], //0x13
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x14
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x15
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x16
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x17
	dev_i2c_addresses_text[DEV_I2C_LIST_12], //0x18
	dev_i2c_addresses_text[DEV_I2C_LIST_13], //0x19
	dev_i2c_addresses_text[DEV_I2C_LIST_14], //0x1a
	dev_i2c_addresses_text[DEV_I2C_LIST_15], //0x1b
	dev_i2c_addresses_text[DEV_I2C_LIST_16], //0x1c
	dev_i2c_addresses_text[DEV_I2C_LIST_17], //0x1d
	dev_i2c_addresses_text[DEV_I2C_LIST_18], //0x1e
	dev_i2c_addresses_text[DEV_I2C_LIST_19], //0x1f
	dev_i2c_addresses_text[DEV_I2C_LIST_20], //0x20
	dev_i2c_addresses_text[DEV_I2C_LIST_21], //0x21
	dev_i2c_addresses_text[DEV_I2C_LIST_22], //0x22
	dev_i2c_addresses_text[DEV_I2C_LIST_23], //0x23
	dev_i2c_addresses_text[DEV_I2C_LIST_22], //0x24
	dev_i2c_addresses_text[DEV_I2C_LIST_22], //0x25
	dev_i2c_addresses_text[DEV_I2C_LIST_24], //0x26
	dev_i2c_addresses_text[DEV_I2C_LIST_22], //0x27
	dev_i2c_addresses_text[DEV_I2C_LIST_25], //0x28
	dev_i2c_addresses_text[DEV_I2C_LIST_26], //0x29
	dev_i2c_addresses_text[DEV_I2C_LIST_27], //0x2a
	dev_i2c_addresses_text[DEV_I2C_LIST_27], //0x2b
	dev_i2c_addresses_text[DEV_I2C_LIST_28], //0x2c
	dev_i2c_addresses_text[DEV_I2C_LIST_28], //0x2d
	dev_i2c_addresses_text[DEV_I2C_LIST_29], //0x2e
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x2f
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x30
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x31
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x32
	dev_i2c_addresses_text[DEV_I2C_LIST_30], //0x33
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x34
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x35
	dev_i2c_addresses_text[DEV_I2C_LIST_31], //0x36
	dev_i2c_addresses_text[DEV_I2C_LIST_32], //0x37
	dev_i2c_addresses_text[DEV_I2C_LIST_33], //0x38
	dev_i2c_addresses_text[DEV_I2C_LIST_34], //0x39
	dev_i2c_addresses_text[DEV_I2C_LIST_35], //0x3a
	dev_i2c_addresses_text[DEV_I2C_LIST_32], //0x3b
	dev_i2c_addresses_text[DEV_I2C_LIST_36], //0x3c
	dev_i2c_addresses_text[DEV_I2C_LIST_36], //0x3d
	dev_i2c_addresses_text[DEV_I2C_LIST_37], //0x3e
	dev_i2c_addresses_text[DEV_I2C_LIST_37], //0x3f
	dev_i2c_addresses_text[DEV_I2C_LIST_38], //0x40
	dev_i2c_addresses_text[DEV_I2C_LIST_39], //0x41
	dev_i2c_addresses_text[DEV_I2C_LIST_40], //0x42
	dev_i2c_addresses_text[DEV_I2C_LIST_41], //0x43
	dev_i2c_addresses_text[DEV_I2C_LIST_42], //0x44
	dev_i2c_addresses_text[DEV_I2C_LIST_43], //0x45
	dev_i2c_addresses_text[DEV_I2C_LIST_44], //0x46
	dev_i2c_addresses_text[DEV_I2C_LIST_44], //0x47
	dev_i2c_addresses_text[DEV_I2C_LIST_45], //0x48
	dev_i2c_addresses_text[DEV_I2C_LIST_46], //0x49
	dev_i2c_addresses_text[DEV_I2C_LIST_47], //0x4a
	dev_i2c_addresses_text[DEV_I2C_LIST_47], //0x4b
	dev_i2c_addresses_text[DEV_I2C_LIST_48], //0x4c
	dev_i2c_addresses_text[DEV_I2C_LIST_49], //0x4d
	dev_i2c_addresses_text[DEV_I2C_LIST_49], //0x4e
	dev_i2c_addresses_text[DEV_I2C_LIST_49], //0x4f
	dev_i2c_addresses_text[DEV_I2C_LIST_50], //0x50
	dev_i2c_addresses_text[DEV_I2C_LIST_50], //0x51
	dev_i2c_addresses_text[DEV_I2C_LIST_51], //0x52
	dev_i2c_addresses_text[DEV_I2C_LIST_52], //0x53
	dev_i2c_addresses_text[DEV_I2C_LIST_50], //0x54
	dev_i2c_addresses_text[DEV_I2C_LIST_50], //0x55
	dev_i2c_addresses_text[DEV_I2C_LIST_50], //0x56
	dev_i2c_addresses_text[DEV_I2C_LIST_53], //0x57
	dev_i2c_addresses_text[DEV_I2C_LIST_54], //0x58
	dev_i2c_addresses_text[DEV_I2C_LIST_55], //0x59
	dev_i2c_addresses_text[DEV_I2C_LIST_56], //0x5a
	dev_i2c_addresses_text[DEV_I2C_LIST_57], //0x5b
	dev_i2c_addresses_text[DEV_I2C_LIST_58], //0x5c
	dev_i2c_addresses_text[DEV_I2C_LIST_59], //0x5d
	dev_i2c_addresses_text[DEV_I2C_LIST_60], //0x5e
	dev_i2c_addresses_text[DEV_I2C_LIST_61], //0x5f
	dev_i2c_addresses_text[DEV_I2C_LIST_62], //0x60
	dev_i2c_addresses_text[DEV_I2C_LIST_63], //0x61
	dev_i2c_addresses_text[DEV_I2C_LIST_64], //0x62
	dev_i2c_addresses_text[DEV_I2C_LIST_65], //0x63
	dev_i2c_addresses_text[DEV_I2C_LIST_66], //0x64
	dev_i2c_addresses_text[DEV_I2C_LIST_66], //0x65
	dev_i2c_addresses_text[DEV_I2C_LIST_67], //0x66
	dev_i2c_addresses_text[DEV_I2C_LIST_67], //0x67
	dev_i2c_addresses_text[DEV_I2C_LIST_68], //0x68
	dev_i2c_addresses_text[DEV_I2C_LIST_69], //0x69
	dev_i2c_addresses_text[DEV_I2C_LIST_70], //0x6a
	dev_i2c_addresses_text[DEV_I2C_LIST_70], //0x6b
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x6c
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x6d
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x6e
	dev_i2c_addresses_text[DEV_I2C_LIST_NONE], //0x6f
	dev_i2c_addresses_text[DEV_I2C_LIST_71], //0x70
	dev_i2c_addresses_text[DEV_I2C_LIST_72], //0x71
	dev_i2c_addresses_text[DEV_I2C_LIST_72], //0x72
	dev_i2c_addresses_text[DEV_I2C_LIST_72], //0x73
	dev_i2c_addresses_text[DEV_I2C_LIST_73], //0x74
	dev_i2c_addresses_text[DEV_I2C_LIST_73], //0x75
	dev_i2c_addresses_text[DEV_I2C_LIST_74], //0x76
	dev_i2c_addresses_text[DEV_I2C_LIST_75], //0x77
	dev_i2c_addresses_text[DEV_I2C_LIST_76], //0x78
	dev_i2c_addresses_text[DEV_I2C_LIST_76], //0x79
	dev_i2c_addresses_text[DEV_I2C_LIST_76], //0x7a
	dev_i2c_addresses_text[DEV_I2C_LIST_76], //0x7b
	dev_i2c_addresses_text[DEV_I2C_LIST_77], //0x7c
	dev_i2c_addresses_text[DEV_I2C_LIST_77], //0x7d
	dev_i2c_addresses_text[DEV_I2C_LIST_77], //0x7e
	dev_i2c_addresses_text[DEV_I2C_LIST_78], //0x7f

};