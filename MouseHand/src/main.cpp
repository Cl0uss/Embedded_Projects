#include <Arduino.h>
#include <Wire.h>
#include <math.h>

constexpr uint8_t SDA_PIN = 5;
constexpr uint8_t SCL_PIN = 6;
constexpr uint8_t MPU_ADDRESS = 0x68;

constexpr uint32_t SERIAL_BAUD_RATE = 115200;
constexpr uint32_t SENSOR_INTERVAL_US = 10000;
constexpr uint32_t OUTPUT_INTERVAL_MS = 25;

constexpr float ACCEL_SCALE = 16384.0f;
constexpr float GYRO_SCALE = 131.0f;

constexpr uint16_t CALIBRATION_SAMPLES = 500;
constexpr uint32_t CALIBRATION_TIMEOUT_MS = 10000;

constexpr float COMPLEMENTARY_FILTER_ALPHA = 0.98f;

struct SensorRawData {
  int16_t accelX;
  int16_t accelY;
  int16_t accelZ;
  int16_t temperature;
  int16_t gyroX;
  int16_t gyroY;
  int16_t gyroZ;
};

SensorRawData rawData;

float accelX = 0.0f;
float accelY = 0.0f;
float accelZ = 0.0f;

float gyroX = 0.0f;
float gyroY = 0.0f;
float gyroZ = 0.0f;

float gyroOffsetX = 0.0f;
float gyroOffsetY = 0.0f;
float gyroOffsetZ = 0.0f;

float pitch = 0.0f;
float roll = 0.0f;

bool orientationInitialized = false;
bool sensorReady = false;

uint32_t previousSensorTimeUs = 0;
uint32_t previousOutputTimeMs = 0;
uint32_t previousErrorTimeMs = 0;

bool deviceExists() {
  Wire.beginTransmission(MPU_ADDRESS);
  return Wire.endTransmission(true) == 0;
}

bool writeRegister(
  uint8_t registerAddress,
  uint8_t value
) {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(registerAddress);
  Wire.write(value);

  return Wire.endTransmission(true) == 0;
}

bool readSensorRawData() {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(0x3B);

  uint8_t transmissionError = Wire.endTransmission(false);

  if (transmissionError != 0) {
    return false;
  }

  constexpr uint8_t bytesRequired = 14;

  size_t bytesReceived = Wire.requestFrom(
    static_cast<uint8_t>(MPU_ADDRESS),
    bytesRequired,
    true
  );

  if (bytesReceived != bytesRequired) {
    while (Wire.available()) {
      Wire.read();
    }

    return false;
  }

  rawData.accelX = static_cast<int16_t>(
    (static_cast<uint16_t>(Wire.read()) << 8) |
    Wire.read()
  );

  rawData.accelY = static_cast<int16_t>(
    (static_cast<uint16_t>(Wire.read()) << 8) |
    Wire.read()
  );

  rawData.accelZ = static_cast<int16_t>(
    (static_cast<uint16_t>(Wire.read()) << 8) |
    Wire.read()
  );

  rawData.temperature = static_cast<int16_t>(
    (static_cast<uint16_t>(Wire.read()) << 8) |
    Wire.read()
  );

  rawData.gyroX = static_cast<int16_t>(
    (static_cast<uint16_t>(Wire.read()) << 8) |
    Wire.read()
  );

  rawData.gyroY = static_cast<int16_t>(
    (static_cast<uint16_t>(Wire.read()) << 8) |
    Wire.read()
  );

  rawData.gyroZ = static_cast<int16_t>(
    (static_cast<uint16_t>(Wire.read()) << 8) |
    Wire.read()
  );

  return true;
}

bool initializeMPU6050() {
  Serial.println("STATUS,Initializing MPU-6050");

  if (!deviceExists()) {
    Serial.println("ERROR,MPU-6050 not found at 0x68");
    return false;
  }

  Serial.println("STATUS,MPU-6050 found at 0x68");

  // Wake up the sensor and use the X-axis gyroscope as clock source.
  if (!writeRegister(0x6B, 0x01)) {
    Serial.println("ERROR,Failed to configure power management");
    return false;
  }

  delay(100);

  // Sample rate: 1 kHz / (1 + 9) = 100 Hz.
  if (!writeRegister(0x19, 0x09)) {
    Serial.println("ERROR,Failed to configure sample rate");
    return false;
  }

  // Digital low-pass filter.
  if (!writeRegister(0x1A, 0x03)) {
    Serial.println("ERROR,Failed to configure low-pass filter");
    return false;
  }

  // Gyroscope range: ±250 degrees per second.
  if (!writeRegister(0x1B, 0x00)) {
    Serial.println("ERROR,Failed to configure gyroscope");
    return false;
  }

  // Accelerometer range: ±2 g.
  if (!writeRegister(0x1C, 0x00)) {
    Serial.println("ERROR,Failed to configure accelerometer");
    return false;
  }

  Serial.println("STATUS,MPU-6050 initialized");

  return true;
}

void convertRawValues() {
  accelX = static_cast<float>(
    rawData.accelX
  ) / ACCEL_SCALE;

  accelY = static_cast<float>(
    rawData.accelY
  ) / ACCEL_SCALE;

  accelZ = static_cast<float>(
    rawData.accelZ
  ) / ACCEL_SCALE;

  gyroX = (
    static_cast<float>(rawData.gyroX) / GYRO_SCALE
  ) - gyroOffsetX;

  gyroY = (
    static_cast<float>(rawData.gyroY) / GYRO_SCALE
  ) - gyroOffsetY;

  gyroZ = (
    static_cast<float>(rawData.gyroZ) / GYRO_SCALE
  ) - gyroOffsetZ;
}

bool calibrateGyroscope() {
  Serial.println("CALIBRATION,Keep the sensor still");

  float gyroSumX = 0.0f;
  float gyroSumY = 0.0f;
  float gyroSumZ = 0.0f;

  uint16_t successfulSamples = 0;
  uint32_t calibrationStartTime = millis();

  while (successfulSamples < CALIBRATION_SAMPLES) {
    if (
      millis() - calibrationStartTime >
      CALIBRATION_TIMEOUT_MS
    ) {
      Serial.println("ERROR,Gyroscope calibration timeout");
      return false;
    }

    if (!readSensorRawData()) {
      delay(5);
      continue;
    }

    gyroSumX += static_cast<float>(rawData.gyroX);
    gyroSumY += static_cast<float>(rawData.gyroY);
    gyroSumZ += static_cast<float>(rawData.gyroZ);

    successfulSamples++;

    if (successfulSamples % 100 == 0) {
      Serial.print("CALIBRATION,");
      Serial.print(successfulSamples);
      Serial.print("/");
      Serial.println(CALIBRATION_SAMPLES);
    }

    delay(3);
  }

  gyroOffsetX = (
    gyroSumX / static_cast<float>(successfulSamples)
  ) / GYRO_SCALE;

  gyroOffsetY = (
    gyroSumY / static_cast<float>(successfulSamples)
  ) / GYRO_SCALE;

  gyroOffsetZ = (
    gyroSumZ / static_cast<float>(successfulSamples)
  ) / GYRO_SCALE;

  Serial.println("CALIBRATION,Completed");

  return true;
}

float calculateAccelerometerPitch() {
  return atan2f(
    -accelX,
    sqrtf(
      accelY * accelY +
      accelZ * accelZ
    )
  ) * 180.0f / PI;
}

float calculateAccelerometerRoll() {
  return atan2f(
    accelY,
    accelZ
  ) * 180.0f / PI;
}

void updateOrientation(float deltaTimeSeconds) {
  float accelerometerPitch = calculateAccelerometerPitch();
  float accelerometerRoll = calculateAccelerometerRoll();

  if (!orientationInitialized) {
    pitch = accelerometerPitch;
    roll = accelerometerRoll;
    orientationInitialized = true;
    return;
  }

  float pitchFromGyroscope =
    pitch + gyroY * deltaTimeSeconds;

  float rollFromGyroscope =
    roll + gyroX * deltaTimeSeconds;

  pitch =
    COMPLEMENTARY_FILTER_ALPHA * pitchFromGyroscope +
    (1.0f - COMPLEMENTARY_FILTER_ALPHA) *
    accelerometerPitch;

  roll =
    COMPLEMENTARY_FILTER_ALPHA * rollFromGyroscope +
    (1.0f - COMPLEMENTARY_FILTER_ALPHA) *
    accelerometerRoll;
}

void printOrientationData() {
  // Format:
  // DATA,pitch,roll

  Serial.print("DATA,");
  Serial.print(pitch, 2);
  Serial.print(",");
  Serial.println(roll, 2);
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);

  uint32_t serialStartTime = millis();

  while (
    !Serial &&
    millis() - serialStartTime < 3000
  ) {
    delay(10);
  }

  delay(500);

  Serial.println();
  Serial.println("STATUS,ESP32 started");

  if (!Wire.begin(SDA_PIN, SCL_PIN)) {
    Serial.println("ERROR,Failed to start I2C");
    return;
  }

  Wire.setClock(100000);
  Wire.setTimeOut(200);

  Serial.println("STATUS,I2C started on SDA 4 SCL 5");

  if (!initializeMPU6050()) {
    return;
  }

  if (!calibrateGyroscope()) {
    return;
  }

  if (!readSensorRawData()) {
    Serial.println("ERROR,Initial sensor read failed");
    return;
  }

  convertRawValues();

  pitch = calculateAccelerometerPitch();
  roll = calculateAccelerometerRoll();

  orientationInitialized = true;
  sensorReady = true;

  previousSensorTimeUs = micros();
  previousOutputTimeMs = millis();

  Serial.println("STATUS,Ready");
}

void loop() {
  if (!sensorReady) {
    delay(100);
    return;
  }

  uint32_t currentTimeUs = micros();

  if (
    currentTimeUs - previousSensorTimeUs <
    SENSOR_INTERVAL_US
  ) {
    delay(1);
    return;
  }

  float deltaTimeSeconds =
    static_cast<float>(
      currentTimeUs - previousSensorTimeUs
    ) / 1000000.0f;

  previousSensorTimeUs = currentTimeUs;

  if (!readSensorRawData()) {
    if (
      millis() - previousErrorTimeMs >= 1000
    ) {
      previousErrorTimeMs = millis();
      Serial.println("ERROR,Sensor read failed");
    }

    return;
  }

  convertRawValues();
  updateOrientation(deltaTimeSeconds);

  uint32_t currentTimeMs = millis();

  if (
    currentTimeMs - previousOutputTimeMs >=
    OUTPUT_INTERVAL_MS
  ) {
    previousOutputTimeMs = currentTimeMs;
    printOrientationData();
  }
}