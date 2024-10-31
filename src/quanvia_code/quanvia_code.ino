#include <SoftwareSerial.h>
#include <SdFat.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MMC56x3.h>
#include <Adafruit_BMP3XX.h>
#include <DFRobot_WT61PC.h>
#include <TinyGPSPlus.h>


// 0.Debugging LEDS
const int GPSworkingLed = 6;
const int setupFailLed = 7;
const int csvWritingLed = 8;
static bool csvWrittingFlag = true;

///////////////
#define SENSOR_POWER_PIN 9  //Output to the base of the transistor that feeds the barometer (whenever it fails, it gets desconnected through this mechanism)
///////////////

// 1.SD card module variables
#define SD_FAT_TYPE 1
const uint8_t chipSelect = 10;
#define SPI_CLOCK SD_SCK_MHZ(50)
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
SdFat sd;
File dataFile;
String fileName;
// CSV logging variables
static int16_t N_batch = 10;
static unsigned long timeStamp = 1;
static String dataString = "";

// 2. Sensors initializations
// Pressure
#define SEALEVELPRESSURE_HPA (1022.00) //hPa
Adafruit_BMP3XX bmp;

// Accelerometer, gyro and tilt sensor
static const int RXPin = 2, TXPin = 3;
SoftwareSerial ss(RXPin, TXPin);
DFRobot_WT61PC accSensor(&ss);

// Magnetometer
Adafruit_MMC5603 mmc = Adafruit_MMC5603(12345);
const float MAG_OFFSET_HARD_IRON[3] = { -5.42, 1.54, -40.62 };  //magnetometer hard iron offset
const float MAG_OFFSET_SOFT_IRON[3][3] = {               //soft iron offset scaling
    { .938,  .013,  .044 },
    { .013, 1.038, -.003 },
    { .044, -.003, 1.030 }
};

const float MAG_DECLINATION = -1.3833; //magnetic declination; specific for a certain geographical location (https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml#declination)
const float Pi = 3.14159;

// GPS
TinyGPSPlus gps;


// Different frequency reading categories and time intervals for the readings
const uint8_t HIGH_FREQ = 12;  // 12 Hz for magnetometer, accelerometer and gyroscope
const uint8_t MED_FREQ = 5;   // 3 Hz for pressure/temperature
const uint8_t LOW_FREQ = 1;    // 1 Hz for data logging (and future GPS)
const unsigned long HIGH_FREQ_INTERVAL = 1000 / HIGH_FREQ;
const unsigned long MED_FREQ_INTERVAL = 1000 / MED_FREQ;
const unsigned long LOW_FREQ_INTERVAL = 1000 / LOW_FREQ;
unsigned long lastHighFreqTime = 0;
unsigned long lastMedFreqTime = 0;
unsigned long lastLowFreqTime = 0;

// Timestamp storage
unsigned long currentTime;

// 3D vector structure
typedef struct {
    float x, y, z;
} Vector3D;

// Struct for high-frequency data
static struct SensorDataStruct {
  Vector3D acceleration;
  Vector3D gyroscope;
  Vector3D tilt;

  Vector3D magnetometer;
  float heading;

  float temperature;
  float pressure;
  float altitude;

  float latitude;
  float longitude;
  float speed;
  float altitudeGPS;
  float headingGPS;
  int satellites;
  float hdop;
  unsigned long age;
  int hour;
  int minute;
  int second;
  int centisecond;

  int count_acc;
  int count_mag;
  int count_bar;
} sensorData;


// Function prototypes
void setupSD();
void setupSensors();
void printSensorDataStruct(const SensorDataStruct* data);
void makeZerosStruct(void* structPtr, size_t size);
void readIMU();
void readMagnetometer();
void powerCycleSensor();
void readBarometer();
void printGPSData();
void readGPS();
void logData();


void setup() {
  // Init serial
  Serial.begin(9600);

  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, HIGH);

  //leds init
  pinMode(GPSworkingLed, OUTPUT);
  digitalWrite(GPSworkingLed, LOW);
  pinMode(setupFailLed, OUTPUT);
  digitalWrite(setupFailLed, HIGH);
  pinMode(csvWritingLed, OUTPUT);
  digitalWrite(csvWritingLed, HIGH); 

  // Setup SD module
  pinMode(chipSelect, OUTPUT); //digital pin for SD module
  setupSD();

  // Setup all the sensors
  setupSensors();

  // Zero initialize the data struct
  makeZerosStruct(&sensorData, sizeof(SensorDataStruct));

  // LED off if the setup was correct
  digitalWrite(setupFailLed, LOW);

}


void loop() {
  currentTime = millis();

  // High-frequency sensor readings
  if (currentTime - lastHighFreqTime >= HIGH_FREQ_INTERVAL) {
    //Serial.println(currentTime - lastHighFreqTime);

    lastHighFreqTime = millis();
    ////Serial.print("High freq interval: ");
    ////Serial.print(currentTime - lastHighFreqTime);
    ////Serial.print("   Goal: ");
    ////Serial.println(HIGH_FREQ_INTERVAL);

    readIMU();
    //Serial.print("Elapsed time  HIGH: ");
    //Serial.println(millis() - lastHighFreqTime);
  }

  // Medium-frequency sensor readings
  if (currentTime - lastMedFreqTime >= MED_FREQ_INTERVAL) {
    lastMedFreqTime = millis();

    ////Serial.print("MED freq interval: ");
    ////Serial.print(currentTime - lastMedFreqTime);
    ////Serial.print("   Goal: ");
    ////Serial.println(MED_FREQ_INTERVAL);
    
    readBarometer();
    readMagnetometer();

    
    ////Serial.print("Elapsed time   MED: ");
    ////Serial.println(lastMedFreqTime - millis());
  }

  // Low-frequency sensor readings and data logging
  if (currentTime - lastLowFreqTime >= LOW_FREQ_INTERVAL) {
    
    
    ////Serial.print("LOW freq interval: ");
    ////Serial.println(currentTime - lastLowFreqTime);
    ////Serial.print("ACC COUNTER: ");
    ////Serial.println(sensorData.count_acc);
    ////Serial.print("MAG COUNTER: ");
    ////Serial.println(sensorData.count_mag);
    ////Serial.print("PRESS COUNTER: ");
    ////Serial.println(sensorData.count_bar);
    ////Serial.println();

    readGPS();
    //printGPSData();

    logData();  // Log all the sensors data to the csv file every second

    lastLowFreqTime = millis();
    makeZerosStruct(&sensorData, sizeof(SensorDataStruct));
    timeStamp++;
  }
}



void setupSD() {
  // Initialize SD card and create CSV file
  while (!sd.begin(chipSelect)) {}

  // Generate filename with four-digit number
  int fileNumber = 1;
  String baseFileName = "data";

  do {
    fileName = baseFileName + String(fileNumber < 100 ? "0" : "") + String(fileNumber < 10 ? "0" : "") + String(fileNumber) + ".csv";
    fileNumber++;
  } while (sd.exists(fileName) && fileNumber <= 9999);

  dataFile = sd.open(fileName, FILE_WRITE);
  //Serial.println("Created a file:");
  //Serial.println(fileName);

  if (dataFile) {
    dataFile.println("Time;msDelta;Acc_X;Acc_Y;Acc_Z;Gyro_X;Gyro_Y;Gyro_Z;Tilt_X;Tilt_Y;Tilt_Z;Mag_X;Mag_Y;Mag_Z;Head;Temp;Press;Alt;Lat;Lng;Speed;AltGPS;HeadGPS");
  }
}

void setupSensors() {

  // Initialize MMC5603 Magnetometer
  Wire1.begin();
  mmc.reset();
  while (!mmc.begin(MMC56X3_DEFAULT_ADDRESS, &Wire1))
  { 
    mmc.reset();
    delay(100);
  }
  mmc.magnetSetReset();  //purge possible parasite currents in the sensor;
  mmc.setDataRate(50);   // Set data rate of the mag (in Hz)

  // Initialize BMP3XX Pressure/Temperature Sensor
  Wire.begin();
  Wire.setClock(50000);
  while (!bmp.begin_I2C()) {
    Serial.println("Could not find a valid BMP3 sensor, check wiring!");
    powerCycleSensor();
    delay(100);
  }
  // Next settings are based on table from: https://community.bosch-sensortec.com/t5/MEMS-sensors-forum/BMP390-Wrong-values/td-p/78368
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmp.setOutputDataRate(BMP3_ODR_6_25_HZ);
  // Purge the first altitude measurement (https://forums.adafruit.com/viewtopic.php?t=209906).
  // All Bosch BMPXXX seem to suffer this same issue of saving the last reading
  bmp.readAltitude(SEALEVELPRESSURE_HPA);

  // Initialize WT61PC sensor
  ss.begin(9600);
  while (!accSensor.available()) {}
  accSensor.modifyFrequency(FREQUENCY_50HZ);

  // Initialize GPS module
  Serial1.begin(9600);
}


// Function to set all the measurements in a SensorDataStruct object to zero
void makeZerosStruct(void* structPtr, size_t size) {
    memset(structPtr, 0, size);
}

// Function to print all the info in a SensorDataStruct object
void printSensorDataStruct(const SensorDataStruct* data) {
    printf("sensorData:\n");
    printf("  Acceleration: (%f, %f, %f)\n", data->acceleration.x, data->acceleration.y, data->acceleration.z);
    printf("  Gyro: (%f, %f, %f)\n", data->gyroscope.x, data->gyroscope.y, data->gyroscope.z);
    printf("  Tilt: (%f, %f, %f)\n", data->tilt.x, data->tilt.y, data->tilt.z);
    printf("  Magnetometer: (%f, %f, %f)\n", data->magnetometer.x, data->magnetometer.y, data->magnetometer.z);
    
    printf("  Heading: %f\n", data->heading);
    printf("  Temperature: %f\n", data->temperature);
    printf("  Pressure: %f\n", data->pressure);
    printf("  Altitude: %f\n", data->altitude);

    printf("  Count acc: %u\n", data->count_acc);
    printf("  Count mag %u\n", data->count_mag);
    printf("  Count bar %u\n", data->count_bar);
}

// Function to read accelerometer, gyroscope and angle data
void readIMU() {

  if (accSensor.available()) {
    sensorData.count_acc++;
    float inv_count = 1.0f / sensorData.count_acc;

    // Direct assignment for the first reading
    if (sensorData.count_acc == 1) {
      sensorData.acceleration = {accSensor.Acc.X, accSensor.Acc.Y, accSensor.Acc.Z};
      sensorData.gyroscope = {accSensor.Gyro.X, accSensor.Gyro.Y, accSensor.Gyro.Z};
      sensorData.tilt = {accSensor.Angle.X, accSensor.Angle.Y, accSensor.Angle.Z};
    } else {
      // Streaming average calculation
      sensorData.acceleration.x += (accSensor.Acc.X - sensorData.acceleration.x) * inv_count;
      sensorData.acceleration.y += (accSensor.Acc.Y - sensorData.acceleration.y) * inv_count;
      sensorData.acceleration.z += (accSensor.Acc.Z - sensorData.acceleration.z) * inv_count;
      
      sensorData.gyroscope.x += (accSensor.Gyro.X - sensorData.gyroscope.x) * inv_count;
      sensorData.gyroscope.y += (accSensor.Gyro.Y - sensorData.gyroscope.y) * inv_count;
      sensorData.gyroscope.z += (accSensor.Gyro.Z - sensorData.gyroscope.z) * inv_count;
      
      sensorData.tilt.x += (accSensor.Angle.X - sensorData.tilt.x) * inv_count;
      sensorData.tilt.y += (accSensor.Angle.Y - sensorData.tilt.y) * inv_count;
      sensorData.tilt.z += (accSensor.Angle.Z - sensorData.tilt.z) * inv_count;
    }
  } /*else {
    Serial.println("ERROR LECTURA acelerometro");
  }*/
}


// Function to read magnetometer data
void readMagnetometer() {

  sensors_event_t mag_event;
  if (mmc.getEvent(&mag_event)) {
    sensorData.count_mag++;
    float inv_count_mag = 1.0f / sensorData.count_mag;
    //printf("Timestamp: %d \t  Magnetometer: (%f, %f, %f)\n", timeStamp, mag_event.magnetic.x, mag_event.magnetic.y, mag_event.magnetic.z);
    // Process magnetometer data with calibration parameters
    float mag_x = MAG_OFFSET_SOFT_IRON[0][0] * (mag_event.magnetic.x - MAG_OFFSET_HARD_IRON[0]) +
                  MAG_OFFSET_SOFT_IRON[0][1] * (mag_event.magnetic.y - MAG_OFFSET_HARD_IRON[1]) +
                  MAG_OFFSET_SOFT_IRON[0][2] * (mag_event.magnetic.z - MAG_OFFSET_HARD_IRON[2]);
    float mag_y = MAG_OFFSET_SOFT_IRON[1][0] * (mag_event.magnetic.x - MAG_OFFSET_HARD_IRON[0]) +
                  MAG_OFFSET_SOFT_IRON[1][1] * (mag_event.magnetic.y - MAG_OFFSET_HARD_IRON[1]) +
                  MAG_OFFSET_SOFT_IRON[1][2] * (mag_event.magnetic.z - MAG_OFFSET_HARD_IRON[2]);
    float mag_z = MAG_OFFSET_SOFT_IRON[2][0] * (mag_event.magnetic.x - MAG_OFFSET_HARD_IRON[0]) +
                  MAG_OFFSET_SOFT_IRON[2][1] * (mag_event.magnetic.y - MAG_OFFSET_HARD_IRON[1]) +
                  MAG_OFFSET_SOFT_IRON[2][2] * (mag_event.magnetic.z - MAG_OFFSET_HARD_IRON[2]);

    // Calculate heading
    float heading = (atan2(mag_y, mag_x) * 180) / Pi;
    
    heading += (MAG_DECLINATION - 90);

    if (heading < 0) {heading += 360;}

    // Direct assignment for the first reading
    if (sensorData.count_mag == 1) {
      sensorData.magnetometer = {mag_x, mag_y, mag_z};
      sensorData.heading = heading;
    } else {
      // Streaming average calculation
      sensorData.magnetometer.x += (mag_x - sensorData.magnetometer.x) * inv_count_mag;
      sensorData.magnetometer.y += (mag_y - sensorData.magnetometer.y) * inv_count_mag;
      sensorData.magnetometer.z += (mag_z - sensorData.magnetometer.z) * inv_count_mag;
      sensorData.heading += (heading - sensorData.heading) * inv_count_mag;
    }
  } /*else {
    Serial.println("ERROR LECTURA magnetometro");
  }*/
}


void powerCycleSensor() {
  digitalWrite(SENSOR_POWER_PIN, LOW);  // Turn off sensor
  delay(1000);  // Wait for 1 second
  digitalWrite(SENSOR_POWER_PIN, HIGH);  // Turn on sensor
  delay(1000);  // Wait for the sensor to power up
  //Serial.println("Sensor power cycled");
}


// Function to read barometer data with retries and soft reset on failure
void readBarometer() {
  int max_retries = 3;  // Number of retries
  bool success = false;

  for (int i = 0; i < max_retries; i++) {
    if (bmp.performReading()) {
      digitalWrite(setupFailLed, LOW);
      success = true;

      sensorData.count_bar++;
      float inv_count = 1.0f / sensorData.count_bar;
      //printf("Timestamp: %d \t  Barometer: (%f, %f, %f)\n", timeStamp, bmp.temperature, bmp.pressure / 100.0f, bmp.readAltitude(SEALEVELPRESSURE_HPA));

      // Direct assignment for the first reading
      if (sensorData.count_bar == 1) {
        sensorData.temperature = bmp.temperature;
        sensorData.pressure = bmp.pressure / 100.0f;
        sensorData.altitude = bmp.readAltitude(SEALEVELPRESSURE_HPA);    
      } else {
        // Streaming average calculation
        sensorData.temperature += (bmp.temperature - sensorData.temperature) * inv_count;
        sensorData.pressure += ((bmp.pressure / 100.0f) - sensorData.pressure) * inv_count;
        sensorData.altitude += ((bmp.readAltitude(SEALEVELPRESSURE_HPA)) - sensorData.altitude) * inv_count;
      }

      break;

    } else {
      digitalWrite(setupFailLed, HIGH);
      //Serial.println("Reading failed, retrying...");
      delay(100);  // Small delay before retrying
    }
  }

  if (!success) {
    //Serial.println("All retries failed, resetting sensor...");
    
    powerCycleSensor();

    /*if (!bmp.begin_I2C()) {
      Serial.println("Failed to softReset and begin_I2C BMP3XX sensor");
    } else {

      if (!bmp.performReading()) {
        Serial.println("Failed to read after reset");
      } else {
        Serial.println("Successfully read after reset");
      }

    }*/
  }
}


void printGPSData() {
  Serial.println(F("GPS Data:"));
  Serial.print(F("Time: "));
  if (gps.time.isValid()) {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d.%02d", sensorData.hour, sensorData.minute, sensorData.second, sensorData.centisecond);
    Serial.print(sz);
  } else {
    Serial.print(F("INVALID"));
  }
  Serial.print(F(" ("));
  Serial.print(millis() - lastLowFreqTime);
  Serial.println(F("ms since last GPS meas)"));
  
  Serial.print(F("Lat: ")); Serial.print(sensorData.latitude, 6);
  Serial.print(F(" Lon: ")); Serial.println(sensorData.longitude, 6);
  Serial.print(F("Alt: ")); Serial.print(sensorData.altitudeGPS);
  Serial.print(F(" Speed: ")); Serial.println(sensorData.speed);
  Serial.print(F("Course: ")); Serial.print(sensorData.headingGPS);
  Serial.print(F(" Satellites: ")); Serial.println(sensorData.satellites);
  Serial.print(F("HDOP: ")); Serial.print(sensorData.hdop);
  Serial.print(F(" Age: ")); Serial.println(sensorData.age);
  Serial.println();
}

// Function to read GPS data
void readGPS() {
  // Read available data from GPS
  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }

  // Update GPS data structure
  if (gps.location.isUpdated()) {
    digitalWrite(GPSworkingLed, HIGH);
    sensorData.latitude = gps.location.lat();
    sensorData.longitude = gps.location.lng();
    sensorData.speed = gps.speed.kmph();
    sensorData.altitudeGPS = gps.altitude.meters();
    sensorData.headingGPS = gps.course.deg();
    sensorData.satellites = gps.satellites.value();
    sensorData.hdop = gps.hdop.hdop();
    sensorData.age = gps.location.age();
    sensorData.hour = gps.time.hour();
    sensorData.minute = gps.time.minute();
    sensorData.second = gps.time.second();
    sensorData.centisecond = gps.time.centisecond();
  } else {
    digitalWrite(GPSworkingLed, LOW);
  }
}

// Function to generate and log the data batches to the CSV file in the SD card
void logData() {
  // Add timestamp (s) and sensor measurements. Time could be checked with RTC, but since logging is at 1Hz,
  // using the timestamp counter method is faster without actually loosing accuracy
  dataString += String(timeStamp) + ";" + String(millis() - lastLowFreqTime) + ";" +
                String(sensorData.acceleration.x, 4) + ";" + String(sensorData.acceleration.y, 4) + ";" + String(sensorData.acceleration.z, 4) + ";" +
                String(sensorData.gyroscope.x, 4) + ";" + String(sensorData.gyroscope.y, 4) + ";" + String(sensorData.gyroscope.z, 4) + ";" +
                String(sensorData.tilt.x, 4) + ";" + String(sensorData.tilt.y, 4) + ";" + String(sensorData.tilt.z, 4) + ";" +
                String(sensorData.magnetometer.x, 4) + ";" + String(sensorData.magnetometer.y, 4) + ";" + String(sensorData.magnetometer.z, 4) + ";" + String(sensorData.heading, 4) + ";" +
                String(sensorData.temperature, 4) + ";" + String(sensorData.pressure, 4) + ";" + String(sensorData.altitude, 4) + ";" +
                String(sensorData.latitude, 6) + ";" + String(sensorData.longitude, 6) + ";" + String(sensorData.speed, 4) + ";" + String(sensorData.altitudeGPS, 4) + ";" + String(sensorData.headingGPS, 4) + "\n";

  // Every N_batches seconds, the data is logged to the SD
  if (timeStamp % N_batch == 0) {
    // Write data to SD card
    dataFile.print(dataString);
    dataFile.flush();
    dataString = "";
    if (csvWrittingFlag){
      digitalWrite(csvWritingLed, LOW); 
      csvWrittingFlag = false;
    } else {
      digitalWrite(csvWritingLed, HIGH); 
      csvWrittingFlag = true;
    }
  }
}