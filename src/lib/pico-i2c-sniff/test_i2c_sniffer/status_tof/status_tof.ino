#include <Wire.h>

// The Arduino two-wire interface uses a 7-bit number for the address,
// and sets the last bit correctly based on reads and writes
#define ADDRESS_DEFAULT             0b0101001
#define XSHUT_PIN                   17          // Pin de control de modo bajo consumo.
#define RESULT_INTERRUPT_STATUS     0x13

uint8_t last_status; // status of last I2C transmission

void setup() {
  // Precaucion
  // El pin Shutdown del VL53L0X es activo bajo y no tolera 5V. Se puede quemar.
  pinMode( XSHUT_PIN, OUTPUT );

  delay( 5 );

  pinMode( XSHUT_PIN, INPUT );

  //For power-up procedure t-boot max 1.2ms "Datasheet: 2.9 Power sequence"
  delay( 10 );

  Wire.begin();
  Wire.setClock(400000);

  Serial.begin(115200);           // start serial for output
  Serial.println("Start status TOF");
}

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t val = readReg(RESULT_INTERRUPT_STATUS);

  Serial.print(millis());
  Serial.print(" slave: ");
  Serial.print((ADDRESS_DEFAULT << 1), HEX);
  Serial.print(", register: ");
  Serial.print(RESULT_INTERRUPT_STATUS, HEX);
  Serial.print(", read status: ");
  Serial.println(val, HEX);

  delay(10);
}

// Read an 8-bit register
uint8_t readReg(uint8_t reg)
{
  uint8_t value;

  Wire.beginTransmission(ADDRESS_DEFAULT);
  Wire.write(reg);
  last_status = Wire.endTransmission();

  Wire.requestFrom((uint8_t)ADDRESS_DEFAULT, (uint8_t)1);
  value = Wire.read();

  return value;
}
