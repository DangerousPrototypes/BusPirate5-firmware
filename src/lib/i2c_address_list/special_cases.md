# Special cases:

- PCA9685 can use any address between 0x40 to 0x7F.  Some of these addresses are fixed.  See the
  [datasheet](https://cdn-shop.adafruit.com/datasheets/PCA9685.pdf) for details.  This chip is used in:
    - PWM/Servo Breakout
    - PWM/Servo Shield
    - PWM/Servo HAT
    - PWM/Servo Bonnet
    - PWM/Servo Wing
    - DC & Stepper Motor Shield
    - DC & Stepper Motor HAT
    - DC & Stepper Motor Bonnet
    - DC & Stepper Motor Wing
- 0x00 - 0x07 and 0x78 - 0x7F are reserved I2C addresses
