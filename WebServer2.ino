// For Web Server
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Wire.h>

//For Time
#include <NTPClient.h>
#include <WiFiUdp.h>

//For Camera
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <StringArray.h>
#include <FS.h>

//For Sensor Board
#include "SparkFun_ENS160.h"  // Click here to get the library: http://librarymanager/All#SparkFun_ENS160
#include "SparkFunBME280.h"   // Click here to get the library: http://librarymanager/All#SparkFun_BME280
SparkFun_ENS160 myENS;
BME280 myBME280;

//For ChatGPT
#include <ArduinoJson.h>
#include <HTTPClient.h>
extern "C" {
  #include "crypto/base64.h"
}

//Camera & GPT function variables
int gptIntervalCounter = 0;
boolean takeNewPhoto = false;
String latestPhotoDate = "N/A";

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/latestPhoto.jpg"
int resetCounter = 0;

//Camera Pins
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM       5
#define Y2_GPIO_NUM       4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// Replace with your network credentials
const char* ssid = "HawkAEye";
const char* apiKey = "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";


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

void resetSensorData(){
  bool removeFile = SPIFFS.remove("/HawkAEyeData.csv");
  File dataFile = SPIFFS.open("/HawkAEyeData.csv", "w");
  dataFile.println("Time,Motion,Light,Temp,Humidity,Pressure,Co2,AQI,TVOC,Latest Photo Date,GPT Response");
  dataFile.close();
}



//Write Sensor Values to the csv file
void writeSensorData(){
  File dataFile = SPIFFS.open("/HawkAEyeData.csv", "a");
  if(dataFile)
  {
      timeClient.update();
      dataFile.print(timeClient.getFormattedTime()); //Unix Date
      
      dataFile.print(",");
      
      randomSeed(millis());
      dataFile.print(random(2)); //Motion
      
      dataFile.print(",");
      
      dataFile.print(int((4095 - analogRead(33))/40.95)); //Light
      
      dataFile.print(",");
      
      dataFile.print(myBME280.readTempF()); //Temperature
      
      dataFile.print(",");
      
      dataFile.print(myBME280.readFloatHumidity()); //Humidity
      
      dataFile.print(",");
      
      dataFile.print(myBME280.readFloatPressure()); //Pressure
      
      dataFile.print(",");
      
      dataFile.print(myENS.getECO2()); //Co2
      
      dataFile.print(",");
      
      dataFile.print(myENS.getAQI()); //AQI
      
      dataFile.print(",");
      
      dataFile.print(myENS.getTVOC()); //TVOC
      
      dataFile.print(",");
      
      dataFile.print(latestPhotoDate); //Last captured photo date

      dataFile.print(",");
      
      dataFile.println("Nothing"); //Last captured photo date
      
      dataFile.close();
   }
   else{
    Serial.println("File write unsuccessful");
   }
}


void getGPTAnalysis(){
  Serial.println("GPT analysis");
  int input[3];
  String response;
  int i;
  
  File imageFile = SPIFFS.open("/latestPhoto.jpg", "r");

  while (imageFile.available()) {
    for(i=0; i<3; i++){
      if(imageFile.available()){
        input[i] = imageFile.read();
      }
    }
    response.concat(decToBase64(input));
  }
  imageFile.close();
  Serial.println("\nFinished Base64 conversion");
  Serial.println(response);



 // Send request to OpenAI API
  String inputText = "Hello, ChatGPT!";
  String apiUrl = "https://api.openai.com/v1/chat/completions";
  String payload = "{  \"model\": \"gpt-4-turbo\", \"messages\": [{\"role\": \"system\", \"content\": \"You are an AI trained to detect hazards to human life or property using an image as an input. You will respond with one sentence explaining all of the hazards you see in the image. If there are none, reply with \"All Clear\"\"},{\"role\": \"user\",\"content\":[{ \"type\": \"image_url\", \"image_url\": { \"url\": f\"data:image/jpeg;base64,{"+ response +"}\"}}]}]}";

  HTTPClient http;
  http.begin(apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(apiKey));

  int httpResponseCode = http.POST(payload);
  if (httpResponseCode == 200) {
    String response = http.getString();
   
    // Parse JSON response
    DynamicJsonDocument jsonDoc(1024);
    deserializeJson(jsonDoc, response);
    String outputText = jsonDoc["choices"][0]["message"]["content"];
    Serial.println(outputText);
  }
  else{
    String response = http.getString();
  
    Serial.println(response);
  }
  
}


String decToBase64(int input[3]){
  int binary[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int binaryIndices[4][8];
  int integerIndices[4] = {0, 0, 0, 0};
  char lookup[64]={'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'};
  
  int i = 0;
  int j = 0;
  int n = 0;
  
  //Convert three integers to binary
  for(i=0; i<3; i++){
    j=7;
    while(input[i]>0){
      binary[(i*8)+j] = input[i] % 2;
      input[i] = (input[i]/2);
      j--;
    }
  }
  //Create 4 Binary Base64 Indices from 3 binary numbers
  for(i=0; i<4; i++){
    binaryIndices[i][0]=0;
    binaryIndices[i][1]=0;
    for(j=2; j<8; j++){
      binaryIndices[i][j]=binary[n];
      n++;
    }
  }
  //Convert 4 Binary Base64 Indices to 4 Integer Base64 Indices
  for(i=0; i<4; i++){
    for(j=0; j<8; j++){
      integerIndices[i] += (binaryIndices[i][j]*round(pow(2,7-j)));
    }
  }
  //Convert 4 Integer Base64 Indices to Base64 characters
  String output;
  for(i=0; i<4; i++){
    output.concat(lookup[integerIndices[i]]);
  }
  Serial.print(output);
  return output;
  
 }


void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.softAP(ssid);
  
   
  // Print ESP32 Local IP Address
  Serial.println(WiFi.softAPIP());
  server.begin();
  
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 11;
  config.fb_count = 2;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });
  server.on("/HawkAEyeData.csv", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/HawkAEyeData.csv");
  });
  server.on("/logo.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/logo.png");
  });
   server.on("/latestPhoto.jpg", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/latestPhoto.jpg");
  });
  // Start server
  server.begin();

  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // EST = GMT -4 -> -4*3600=-14400
  timeClient.setTimeOffset(-14400);

  pinMode(33, INPUT);
  Wire.begin(0, 2);
  if (myBME280.beginI2C() == false) //Begin communication over I2C
  {
    Serial.println("The sensor did not respond. Please check wiring.");
  }
  
  if( !myENS.begin() )
  {
    Serial.println("Could not communicate with the ENS160, check wiring.");
  }
  if( myENS.setOperatingMode(SFE_ENS160_RESET) )
    Serial.println("Ready.");
  myENS.setOperatingMode(SFE_ENS160_STANDARD);

  capturePhotoSaveSpiffs();
   takeNewPhoto = false;
}

 
void loop(){
  if(gptIntervalCounter > 120){
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
    gptIntervalCounter = 0;
   // getGPTAnalysis();
  }
  gptIntervalCounter++;
  if(resetCounter > 60){
    resetSensorData();
    resetCounter = 0;
  }
  resetCounter++;
  writeSensorData();
  delay(1000);
}


// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}


// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  timeClient.update();
  latestPhotoDate = timeClient.getFormattedTime(); //Unix Date

  
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    } 
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );

  
}
