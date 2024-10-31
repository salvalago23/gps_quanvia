#include <TinyGPSPlus.h>

// The TinyGPSPlus object
TinyGPSPlus gps;

// GPS data structure
struct GPSData {
  float latitude;
  float longitude;
  float altitude;
  float speed;
  float course;
  int satellites;
  float hdop;
  unsigned long age;
  int hour;
  int minute;
  int second;
  int centisecond;
};

GPSData gpsData;
unsigned long gpsExecutionTime = 0; // Variable to store execution time
unsigned long lastPrintTime = 0; // To track time between prints

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);

  Serial.println(F("GPS Sensor Integration with Time and Execution Time Measurement"));
  Serial.println(F("Reading GPS data every second"));
  Serial.println();
}

void loop() {
  static unsigned long lastGPSRead = 0;
  
  // Read GPS every 1 second
  if (millis() - lastGPSRead >= 1000) {
    unsigned long startTime = micros(); // Start time measurement
    readGPS();
    gpsExecutionTime = micros() - startTime; // Calculate execution time
    
    printGPSData();
    lastGPSRead = millis();
  }

  // Here you can add code to read other sensors
  // readOtherSensors();

  // Process data or perform other tasks
  // processData();
}

void readGPS() {
  // Read available data from GPS
  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }

  // Update GPS data structure
  if (gps.location.isUpdated()) {
    gpsData.latitude = gps.location.lat();
    gpsData.longitude = gps.location.lng();
    gpsData.altitude = gps.altitude.meters();
    gpsData.speed = gps.speed.kmph();
    gpsData.course = gps.course.deg();
    gpsData.satellites = gps.satellites.value();
    gpsData.hdop = gps.hdop.hdop();
    gpsData.age = gps.location.age();
    gpsData.hour = gps.time.hour();
    gpsData.minute = gps.time.minute();
    gpsData.second = gps.time.second();
    gpsData.centisecond = gps.time.centisecond();
  }
}

void printGPSData() {
  unsigned long currentTime = millis();
  unsigned long timeSinceLastPrint = currentTime - lastPrintTime;
  lastPrintTime = currentTime;

  Serial.println(F("GPS Data:"));
  Serial.print(F("Time: "));
  if (gps.time.isValid()) {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d.%02d", gpsData.hour, gpsData.minute, gpsData.second, gpsData.centisecond);
    Serial.print(sz);
  } else {
    Serial.print(F("INVALID"));
  }
  Serial.print(F(" ("));
  Serial.print(timeSinceLastPrint);
  Serial.println(F("ms since last print)"));
  
  Serial.print(F("Lat: ")); Serial.print(gpsData.latitude, 6);
  Serial.print(F(" Lon: ")); Serial.println(gpsData.longitude, 6);
  Serial.print(F("Alt: ")); Serial.print(gpsData.altitude);
  Serial.print(F(" Speed: ")); Serial.println(gpsData.speed);
  Serial.print(F("Course: ")); Serial.print(gpsData.course);
  Serial.print(F(" Satellites: ")); Serial.println(gpsData.satellites);
  Serial.print(F("HDOP: ")); Serial.print(gpsData.hdop);
  Serial.print(F(" Age: ")); Serial.println(gpsData.age);
  Serial.print(F("Execution Time (us): ")); Serial.println(gpsExecutionTime);
  Serial.println();
}