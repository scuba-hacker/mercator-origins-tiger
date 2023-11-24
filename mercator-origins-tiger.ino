// M5StickC Nixie tube Clock: 2019.06.06 
#include <M5StickCPlus.h>

#include <memory.h>

#include "MapScreen.h"

std::unique_ptr<MapScreen> mapScreen;

// rename the git file "mercator_secrets_template.c" to the filename below, filling in your wifi credentials etc.
#include "mercator_secrets.c"

bool writeLogToSerial=false;

bool goProButtonsPrimaryControl = true;

#include <esp_now.h>
#include <WiFi.h>
#include <freertos/queue.h>

// ************** ESPNow variables **************

uint16_t ESPNowMessagesDelivered = 0;
uint16_t ESPNowMessagesFailedToDeliver = 0;

const uint8_t ESPNOW_CHANNEL=1;
const uint8_t ESPNOW_NO_PEER_CHANNEL_FLAG = 0xFF;
const uint8_t ESPNOW_PRINTSCANRESULTS = 0;
const uint8_t ESPNOW_DELETEBEFOREPAIR = 0;

esp_now_peer_info_t ESPNow_mako_peer;
bool isPairedWithMako = false;

const int RESET_ESPNOW_SEND_RESULT = 0xFF;
esp_err_t ESPNowSendResult=(esp_err_t)RESET_ESPNOW_SEND_RESULT;

QueueHandle_t msgsReceivedQueue;


bool ESPNowActive = false;

// Nixie Clock graphics files
// #include "vfd_18x34.c"
// #include "vfd_35x67.c"

const int SCREEN_LENGTH = 240;
const int SCREEN_WIDTH = 135;

bool landscape_clock = false;

bool text_clock = true;

const uint8_t BUTTON_REED_TOP_PIN=25;
const uint8_t BUTTON_REED_SIDE_PIN=0;
const uint8_t UNUSED_GPIO_36_PIN=36;
const uint8_t M5_POWER_SWITCH_PIN=255;
const uint32_t MERCATOR_DEBOUNCE_MS=10;
const uint8_t PENETRATOR_LEAK_DETECTOR_PIN=26;

Button ReedSwitchGoProTop = Button(BUTTON_REED_TOP_PIN, true, MERCATOR_DEBOUNCE_MS);    // from utility/Button.h for M5 Stick C Plus
Button ReedSwitchGoProSide = Button(BUTTON_REED_SIDE_PIN, true, MERCATOR_DEBOUNCE_MS); // from utility/Button.h for M5 Stick C Plus
Button LeakDetectorSwitch = Button(PENETRATOR_LEAK_DETECTOR_PIN, true, MERCATOR_DEBOUNCE_MS); // from utility/Button.h for M5 Stick C Plus
uint16_t sideCount = 0, topCount = 0;

const uint16_t mode_label_y_offset = 170;

#include <WiFi.h>
#include "time.h" 

#include <AsyncTCP.h>           // OTA updates
#include <ESPAsyncWebServer.h>  // OTA updates
#include <AsyncElegantOTA.h>    // OTA updates

AsyncWebServer asyncWebServer(80);      // OTA updates
const bool enableOTAServer=false; // OTA updates
bool otaActiveListening=true;   // OTA updates toggle

const bool enableESPNow = !enableOTAServer; // cannot have OTA server on regular wifi and espnow concurrently running

Button* p_primaryButton = NULL;
Button* p_secondButton = NULL;

bool primaryButtonIsPressed = false;
uint32_t primaryButtonPressedTime = 0;
uint32_t lastPrimaryButtonPressLasted = 0;

bool secondButtonIsPressed = false;
uint32_t secondButtonPressedTime = 0;
uint32_t lastSecondButtonPressLasted = 0;

bool primaryButtonIndicatorNeedsClearing = false;
bool secondButtonIndicatorNeedsClearing = false;

double latitude=51.460015;
double longitude=-0.548316;
double heading=0.0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;        // timezone offset
int   daylightOffset_sec = 0;   // DST offset
const uint8_t max_NTP_connect_attempts=5;

RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

const char* leakAlarmMsg = "\nWATER\n\nLEAK\n\nALARM";

int mode_ = 3; // clock
/*
const uint8_t*n[] = { // vfd font 18x34
  vfd_18x34_0,vfd_18x34_1,vfd_18x34_2,vfd_18x34_3,vfd_18x34_4,
  vfd_18x34_5,vfd_18x34_6,vfd_18x34_7,vfd_18x34_8,vfd_18x34_9
  };
const uint8_t*m[] = { // vfd font 35x67
  vfd_35x67_0,vfd_35x67_1,vfd_35x67_2,vfd_35x67_3,vfd_35x67_4,
  vfd_35x67_5,vfd_35x67_6,vfd_35x67_7,vfd_35x67_8,vfd_35x67_9
  };
const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
*/
#define USB_SERIAL Serial

const int defaultBrightness = 100;

int countdownFrom=59;
bool haltCountdown=false;
bool showDate=true;  // 

bool showPowerStats=false;

void resetMap();
void resetClock();
void resetMicDisplay();

void resetCountDownTimer();
void resetCountUpTimer();

const float minimumUSBVoltage=2.0;
long USBVoltageDropTime=0;
long milliSecondsToWaitForShutDown=1000;

bool setupOTAWebServer(const char* _ssid, const char* _password, const char* label, uint32_t timeout);

void updateButtonsAndBuzzer();

void setRotationForClockStyle()
{
  if (landscape_clock)
    M5.Lcd.setRotation(1);
  else
    M5.Lcd.setRotation(0);
}

void toggleOTAActiveAndWifiIfUSBPowerOff()
{
  if (M5.Axp.GetVBusVoltage() < minimumUSBVoltage)
  {
    if (USBVoltageDropTime == 0)
      USBVoltageDropTime=millis();
    else 
    {
      if (millis() > USBVoltageDropTime + milliSecondsToWaitForShutDown)
      {
       delay(1000);
       M5.Lcd.fillScreen(TFT_ORANGE);
       M5.Lcd.setCursor(0,0);
       // flip ota on/off
       if (otaActiveListening)
       {
         asyncWebServer.end();
         M5.Lcd.printf("OTA Disabled");
         otaActiveListening=false;
         WiFi.disconnect();
         M5.Lcd.printf("Wifi Disabled");
         delay (1000);
       }
       else
       {
         asyncWebServer.begin();
//         AsyncElegantOTA.begin(&asyncWebServer);    // Start AsyncElegantOTA
         M5.Lcd.printf("OTA Enabled");
         otaActiveListening=true;
         M5.Lcd.printf("Enabling Wifi...");
         
         delay (1000);
        if (!setupOTAWebServer(ssid_1, password_1, label_1, timeout_1))
          if (!setupOTAWebServer(ssid_2, password_2, label_2, timeout_2))
            setupOTAWebServer(ssid_3, password_3, label_3, timeout_3);       
       }

       M5.Lcd.fillScreen(TFT_BLACK);
      }
    }
  }
  else
  {
    if (USBVoltageDropTime != 0)
      USBVoltageDropTime = 0;
  }  
}

void shutdownIfUSBPowerOff()
{
  if (M5.Axp.GetVBusVoltage() < minimumUSBVoltage)
  {
    if (USBVoltageDropTime == 0)
      USBVoltageDropTime=millis();
    else 
    {
      if (millis() > USBVoltageDropTime + milliSecondsToWaitForShutDown)
      {
       // initiate shutdown after 3 seconds.
       delay(1000);
       fadeToBlackAndShutdown();
      }
    }
  }
  else
  {
    if (USBVoltageDropTime != 0)
      USBVoltageDropTime = 0;
  }
}

void  initialiseRTCfromNTP()
{
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0,0);

  if (!enableOTAServer)
  {
    //connect to WiFi
    M5.Lcd.printf("Connect to\n%s\n", label_1);
    int maxAttempts=15;
    WiFi.begin(ssid_1, password_1);
    while (WiFi.status() != WL_CONNECTED && --maxAttempts) 
    {
      delay(300);
      M5.Lcd.print(".");
    }

    if (maxAttempts == 0 && WiFi.status() != WL_CONNECTED)
    {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0,0);
      
      M5.Lcd.printf("Connect to\n%s\n", label_2);
      int maxAttempts=15;
      WiFi.begin(ssid_2, password_2);
      while (WiFi.status() != WL_CONNECTED && --maxAttempts) 
      {
          delay(300);
          M5.Lcd.print(".");
      }
    }

    if (maxAttempts == 0 && WiFi.status() != WL_CONNECTED)
    {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.printf("Connect to\n%s\n", label_3);
      M5.Lcd.setCursor(0,0);
      int maxAttempts=15;
      WiFi.begin(ssid_3, password_3);
      while (WiFi.status() != WL_CONNECTED && --maxAttempts) 
      {
          delay(300);
          M5.Lcd.print(".");
      }
    }
  }

  delay(1000);

  if (WiFi.status() == WL_CONNECTED)
  {
    M5.Lcd.println("Wifi OK");
  
    //init and get the time
    _initialiseTimeFromNTP:
    
    struct tm timeinfo;
    for (uint8_t i=0; i<max_NTP_connect_attempts; i++)
    {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      if(!getLocalTime(&timeinfo))
      {
        USB_SERIAL.println("No time available (yet)");
        // Let RTC continue with existing settings
        M5.Lcd.println("Wait for NTP Time\n");
        delay(1000);
      }
      else
      {
        break;        
      }
    }

    if(!getLocalTime(&timeinfo))
    {
      // Let RTC continue with existing settings
      M5.Lcd.println("No NTP Server\n");
    }
    else
    {
      // USB_SERIAL.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
      // Use NTP to update RTC
    
      RTC_TimeTypeDef TimeStruct;
      TimeStruct.Hours   = timeinfo.tm_hour;
      TimeStruct.Minutes = timeinfo.tm_min;
      TimeStruct.Seconds = timeinfo.tm_sec;
      M5.Rtc.SetTime(&TimeStruct);

      RTC_DateTypeDef DateStruct;
      DateStruct.Month = timeinfo.tm_mon+1;
      DateStruct.Date = timeinfo.tm_mday;
      DateStruct.Year = timeinfo.tm_year+1900;
      DateStruct.WeekDay = timeinfo.tm_wday;
      M5.Rtc.SetDate(&DateStruct);    
      if (daylightOffset_sec == 0)
        M5.Lcd.println("RTC to GMT");
      else
        M5.Lcd.println("RTC set to BST");
  
      delay(300);
    }

    if (daylightOffset_sec == 0)
    {
      // check if British Summer Time

      int day = timeinfo.tm_wday;
      int date = timeinfo.tm_mday;
      int month = timeinfo.tm_mon;

      if (month == 2 && date > 24)    // is date after or equal to last Sunday in March?
      {
        if (date - day >= 25)
        {
          daylightOffset_sec=3600;
          // reinitialise time from NTP with correct offset.
          // this doesn't deal with the exact changeover time for BST, but doesn't matter
          goto _initialiseTimeFromNTP;
        }
          /*
        day == 0 && date >= 25   TRUE
        day == 1 && date >= 26   TRUE
        day == 2 && date >= 27   TRUE
        day == 3 && date >= 28   TRUE
        day == 4 && date >= 29   TRUE
        day == 5 && date >= 30   TRUE
        day == 6 && date >= 31   TRUE
        */
      }
    }

    if (!enableOTAServer)
    {
        //disconnect WiFi as it's no longer needed
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        M5.Lcd.println("WiFi Off");
    }
  }
  else
  {
    M5.Lcd.println(" FAILED");
    delay(5000);
  }

  M5.Lcd.fillScreen(BLACK);
  
  setRotationForClockStyle();

  resetClock();
}

char rxQueueItemBuffer[256];
const uint8_t queueLength=4;

char currentTarget[256];
bool refreshTargetShown = false;  

void setup()
{ 
  M5.begin();

  USB_SERIAL.begin(115200);

  pinMode(UNUSED_GPIO_36_PIN,INPUT);

  mapScreen.reset(new MapScreen(&M5.Lcd,&M5));

  msgsReceivedQueue = xQueueCreate(queueLength,sizeof(rxQueueItemBuffer));

  if (writeLogToSerial && msgsReceivedQueue == NULL)
  {
    USB_SERIAL.println("Failed to create queue");
  }

  if (goProButtonsPrimaryControl)
  {
    p_primaryButton = &ReedSwitchGoProTop;
    p_secondButton = &ReedSwitchGoProSide;
  }
  else
  {
    p_primaryButton = &M5.BtnA;
    p_secondButton = &M5.BtnB;
  }
    
  M5.Beep.setBeep(1200, 100);

  setRotationForClockStyle();

  M5.Lcd.setTextSize(2);
  M5.Axp.ScreenBreath(defaultBrightness);
  
  USB_SERIAL.begin(115200);

  if (enableOTAServer)
  {
    if (!setupOTAWebServer(ssid_1, password_1, label_1, timeout_1))
      if (!setupOTAWebServer(ssid_2, password_2, label_2, timeout_2))
        setupOTAWebServer(ssid_3, password_3, label_3, timeout_3);

    setRotationForClockStyle();
  }

  initialiseRTCfromNTP();

  // override clock screen to be test for target received from espnow
  mode_ = 3;
  
  if (enableESPNow && msgsReceivedQueue)
  {
    configAndStartUpESPNow();

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0,0);
    isPairedWithMako = pairWithPeer(ESPNow_mako_peer,"Mako",5); // 5 connection attempts
    
    if (isPairedWithMako)
    {
      // send message to tiger to give first target
      publishToMakoTestMessage("Conn Ok");
    }
  }
}

void checkForLeak(const char* msg, const uint8_t pin)
{
  bool leakStatus = false;
  
  if (pin == M5_POWER_SWITCH_PIN && M5.Axp.GetBtnPress())
  {
    leakStatus = true;
  }
  else
  {
    leakStatus = !(digitalRead(pin));
  }

  if (leakStatus)
  {
    M5.Lcd.fillScreen(TFT_RED);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(5, 10);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_RED);
    M5.Lcd.print(msg);
    M5.Beep.setBeep(1200, 100);
    M5.Beep.beep();
    delay(100);
    updateButtonsAndBuzzer();

    M5.Lcd.fillScreen(TFT_ORANGE);
    M5.Lcd.setCursor(5, 10);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_ORANGE);
    M5.Lcd.print(msg);
    M5.Beep.setBeep(1500, 100);
    M5.Beep.beep();
    delay(100);

    updateButtonsAndBuzzer();
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Beep.mute();
  }
}

void readAndTestGoProReedSwitches()
{
  updateButtonsAndBuzzer();

  bool btnTopPressed = p_primaryButton->pressedFor(15);
  bool btnSidePressed = p_secondButton->pressedFor(15);

  if (btnTopPressed && btnSidePressed)
  {
    sideCount++;
    topCount++;
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.printf("TOP+SIDE %d %d", topCount, sideCount);
  }
  else if (btnTopPressed)
  {
    topCount++;
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.printf("TOP %d", topCount);
  }
  else if (btnSidePressed)
  {
    sideCount++;
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.printf("SIDE %d", sideCount);
  }
}

void resetCountUpTimer()
{
  M5.Lcd.fillScreen(BLACK);
  RTC_TimeTypeDef TimeStruct;         // Hours, Minutes, Seconds 
  TimeStruct.Hours   = 0;
  TimeStruct.Minutes = 0;
  TimeStruct.Seconds = 0;

  M5.Rtc.SetTime(&TimeStruct);
  mode_ = 1;
}

void resetCountDownTimer()
{
  haltCountdown=false;
  resetCountUpTimer();
  mode_ = 4;
}

void resetCurrentTarget()
{
  refreshTargetShown = true;
  mode_ = 5;
}

void resetMap()
{
  mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(latitude, longitude, heading);
  mode_ = 6;
}

void resetClock()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    USB_SERIAL.println("Failed to obtain time");
    return;
  }
  
  M5.Lcd.fillScreen(BLACK);
  RTC_TimeTypeDef TimeStruct;
  TimeStruct.Hours   = timeinfo.tm_hour;
  TimeStruct.Minutes = timeinfo.tm_min;
  TimeStruct.Seconds = timeinfo.tm_sec;
  M5.Rtc.SetTime(&TimeStruct);

  RTC_DateTypeDef DateStruct;
  DateStruct.Month = timeinfo.tm_mon+1;
  DateStruct.Date = timeinfo.tm_mday;
  DateStruct.Year = timeinfo.tm_year+1900;
  DateStruct.WeekDay = timeinfo.tm_wday;
  M5.Rtc.SetDate(&DateStruct);    

  mode_ = 3; // change back to 3
}

void fadeToBlackAndShutdown()
{
  for (int i = 90; i > 0; i=i-15)
  {
    M5.Axp.ScreenBreath(i);             // fade to black
    delay(100);
  }

  M5.Axp.PowerOff();
}

bool cycleDisplays()
{
    bool changeMade = false;
    
    /* Original Screen Cycle:
    // Screen cycle command
    if (mode_ == 4) // countdown mode, next is clock
    {
      resetClock(); changeMade = true;
    }
    else if (mode_ == 3) // clock mode, next is show current target
    {
      resetCurrentTarget(); changeMade = true;
    }
    else if (mode_ == 5)     // show current target, next is count up timer
    {
      resetCountUpTimer(); changeMade = true;
    }
    else if (mode_ == 1) // countup timer mode, next is countdown
    {
      resetCountDownTimer(); changeMade = true;
    }
    */
    
    if (mode_ == 3) // clock mode, next is show current target
    {
      resetCurrentTarget(); changeMade = true;
    }
    else if (mode_ == 5)     // show current target, next is map
    {
      resetMap(); changeMade = true;
    }
    else if (mode_ == 6)     // show map, next is clock
    {
      mapScreen->clearMapPreserveZoom();
      resetClock(); changeMade = true;
    }

    return changeMade;
}

bool checkReedSwitches()
{
  bool changeMade = false;

  bool reedSwitchTop;
  uint32_t activationTime=0;
    
  updateButtonsAndBuzzer();

  int pressedPrimaryButtonX, pressedPrimaryButtonY, pressedSecondButtonX, pressedSecondButtonY;

  if (landscape_clock)
  {   
    pressedPrimaryButtonX = 210;
    pressedPrimaryButtonY = 5;
  
    pressedSecondButtonX = 210;
    pressedSecondButtonY = 110;
  }
  else
  {   
    pressedPrimaryButtonX = 110;
    pressedPrimaryButtonY = 5; 
  
    pressedSecondButtonX = 5;
    pressedSecondButtonY = 210;
  }
    
  if (primaryButtonIsPressed && millis()-primaryButtonPressedTime > 250)
  {
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_RED);
    M5.Lcd.setCursor(pressedPrimaryButtonX,pressedPrimaryButtonY);
    M5.Lcd.printf("%i",(millis()-primaryButtonPressedTime)/1000);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    primaryButtonIndicatorNeedsClearing=true;
  }
  else
  {
    if (primaryButtonIndicatorNeedsClearing)
    {
      primaryButtonIndicatorNeedsClearing = false;
      M5.Lcd.setTextSize(3);
      M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Lcd.setCursor(pressedPrimaryButtonX,pressedPrimaryButtonY);
      M5.Lcd.print(" ");
    }
  }

  if (secondButtonIsPressed && millis()-secondButtonPressedTime > 250)
  {
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLUE);
    M5.Lcd.setCursor(pressedSecondButtonX,pressedSecondButtonY);
    M5.Lcd.printf("%i",(millis()-secondButtonPressedTime)/1000);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    secondButtonIndicatorNeedsClearing=true;
  }
  else
  {
    if (secondButtonIndicatorNeedsClearing)
    {
      secondButtonIndicatorNeedsClearing = false;
      
      M5.Lcd.setTextSize(3);
      M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Lcd.setCursor(pressedSecondButtonX,pressedSecondButtonY);
      M5.Lcd.print(" ");
    }
  }

  if (p_primaryButton->wasReleasefor(100))
  {
    activationTime = lastPrimaryButtonPressLasted;
    reedSwitchTop = true;

    changeMade = cycleDisplays();
  }

  // press second button for 5 seconds to attempt WiFi connect and enable OTA
  if (p_secondButton->wasReleasefor(10000))
  { 
     esp_restart();
  }
  else if (p_secondButton->wasReleasefor(5000))
  { 
    activationTime = lastSecondButtonPressLasted;
    reedSwitchTop = false;

    TeardownESPNow();
    
    // enable OTA
    if (!setupOTAWebServer(ssid_1, password_1, label_1, timeout_1))
      if (!setupOTAWebServer(ssid_2, password_2, label_2, timeout_2))
        setupOTAWebServer(ssid_3, password_3, label_3, timeout_3);

    setRotationForClockStyle();
 
    changeMade = true;
  }
  // press second button for 1 second...
  else if (p_secondButton->wasReleasefor(1000))
  {
    activationTime = lastSecondButtonPressLasted;
    reedSwitchTop = false;

    // Screen modification command
    if (mode_ == 4)       // Countdown mode, reduce timer by 15 mins
    {
      countdownFrom=countdownFrom-15;
      if (countdownFrom <= 0)
        countdownFrom = 59;
      changeMade = true;
    }
    else if (mode_ == 6)    // toggle showing all features on the map
    {
      mapScreen->toggleDrawAllFeatures();
      mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(latitude, longitude, heading);
      changeMade = true;
    }
  }
  // press second button for 0.1 second...
  else if (p_secondButton->wasReleasefor(100))
  {
    activationTime = lastSecondButtonPressLasted;
    reedSwitchTop = false;

    // Screen reset command
    if (mode_ == 4)
    {
      // revert the countdown timer to the start
      resetCountDownTimer(); changeMade = true;
    }
    else if (mode_ == 1)
    {
      // revert the countup timer to the start 00:00
      resetCountUpTimer(); changeMade = true;
    }
    else if (mode_ == 3) // clock mode
    {
      showDate=!showDate; changeMade = true;
      
      M5.Lcd.fillScreen(BLACK);
    }
    else if (mode_ == 6) // map mode
    {
      mapScreen->cycleZoom();
      mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(latitude, longitude, heading);
    }
  }
  
  if (activationTime > 0)
  {
    USB_SERIAL.println("Reed Activated...");
    publishToMakoReedActivation(reedSwitchTop, activationTime);
  }
  
  return changeMade;
}

char mako_espnow_buffer[256];

void publishToMakoTestMessage(const char* testMessage)
{     
  if (isPairedWithMako && ESPNow_mako_peer.channel == ESPNOW_CHANNEL)
  {
    /*
    memset(mako_espnow_buffer,0,sizeof(mako_espnow_buffer));
    mako_espnow_buffer[0] = 'T';  // command T = Tiger
    mako_espnow_buffer[1] = '\0';
    strncpy(mako_espnow_buffer+1,testMessage,sizeof(mako_espnow_buffer)-2);
*/
    snprintf(mako_espnow_buffer,sizeof(mako_espnow_buffer),"T%s",testMessage);
    if (writeLogToSerial)
    {
      USB_SERIAL.println("Sending ESP T msg to Mako...");
      USB_SERIAL.println(mako_espnow_buffer);
    }
    
    ESPNowSendResult = esp_now_send(ESPNow_mako_peer.peer_addr, (uint8_t*)mako_espnow_buffer, strlen(mako_espnow_buffer)+1);
  }
}

void publishToMakoReedActivation(const bool topReed, const uint32_t ms)
{     
  if (isPairedWithMako && ESPNow_mako_peer.channel == ESPNOW_CHANNEL)
  {
    snprintf(mako_espnow_buffer,sizeof(mako_espnow_buffer),"R%c%lu       ",(topReed ? 'T' : 'B'),ms);
    if (writeLogToSerial)
    {
      USB_SERIAL.println("Sending ESP R msg to Mako...");
      USB_SERIAL.println(mako_espnow_buffer);
    }
    ESPNowSendResult = esp_now_send(ESPNow_mako_peer.peer_addr, (uint8_t*)mako_espnow_buffer, strlen(mako_espnow_buffer)+1);
  }
}


void loop()
{ 
  shutdownIfUSBPowerOff();
//  toggleOTAActiveAndWifiIfUSBPowerOff();

  if (msgsReceivedQueue)
  {
    if (xQueueReceive(msgsReceivedQueue,&(rxQueueItemBuffer),(TickType_t)0))
    {
      switch(rxQueueItemBuffer[0])
      {
        case 'c':   // current target
        {
          strncpy(currentTarget,rxQueueItemBuffer+1,sizeof(currentTarget));
          refreshTargetShown = true;
          break;
        }
        
        case 'l':   // Bright light detected by Mako - switch to next screen
        {
          cycleDisplays();
          break;
        }

        case 'X':   // location, heading and current Target info.
        {
          // format: targetCode[7],lat,long,heading,targetText
          
          char targetCode[7];
          
          strncpy(targetCode,rxQueueItemBuffer+1,sizeof(targetCode));
          
          Serial.printf("targetCode: %s\n",targetCode);
                    
          memcpy(&latitude,  rxQueueItemBuffer+8,  8);
          memcpy(&longitude, rxQueueItemBuffer+16, 8);
          memcpy(&heading,   rxQueueItemBuffer+24, 8);

          Serial.printf("latitude: %f\n",latitude);
          Serial.printf("longitude: %f\n",longitude);
          Serial.printf("heading: %f\n",heading);
          
          strncpy(currentTarget,rxQueueItemBuffer+32,sizeof(currentTarget));

          Serial.printf("currentTarget: %s\n",currentTarget);

          mapScreen->setTargetWaypointByLabel(targetCode);
          if (mode_ == 6) // map on screen
            mapScreen->drawDiverOnBestFeaturesMapAtCurrentZoom(latitude, longitude, heading);
        }
        default:
        {
          break;
        }
      }
    }
  }
  
  if ( mode_ == 6) { vfd_6_map();}
  if ( mode_ == 5) { vfd_5_current_target();}
  if ( mode_ == 4) { vfd_4_line_countdown(countdownFrom);}   // mm,ss, optional dd mm
  if ( mode_ == 3 ){ vfd_3_line_clock();}   // hh,mm,ss, optional dd mm
  if ( mode_ == 2 ){ vfd_2_line();}   // yyyy,mm,dd,hh,mm,ss - not used.
  if ( mode_ == 1 ){ vfd_1_line_countup();}   // mm,ss, optional dd mm

//  readAndTestGoProReedSwitches();
  
  for (int m=0;m<10;m++)
  {
    delay(50);
    checkForLeak(leakAlarmMsg,PENETRATOR_LEAK_DETECTOR_PIN);
    
    if (checkReedSwitches()) // If a change occurred break out of wait loop to make change asap.
    {
      break;
    }
  }
}

void vfd_6_map()
{
  // do nothing
}

void vfd_5_current_target()
{
  if (refreshTargetShown == true)
  {
    M5.Lcd.fillScreen(TFT_BLACK);
    refreshTargetShown = false;
  }

  if (ESPNowActive)
  {    
    M5.Lcd.setCursor(0,0);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.println(currentTarget);
  }
  else
  {
    M5.Lcd.setCursor(0,0);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.println("  No\nTarget\n\nESPNow\nIs Off");    
  }

  M5.Lcd.setCursor(0, mode_label_y_offset+10);
  M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Lcd.print("Towards");
}

void vfd_4_line_countdown(const int countdownFrom){ // Countdown mode, minutes, seconds
  int minutesRemaining = 0, secondsRemaining = 0;
  
  if (!haltCountdown)
  {
    M5.Rtc.GetTime(&RTC_TimeStruct);
    M5.Rtc.GetDate(&RTC_DateStruct);
    int minutesRemaining = countdownFrom - RTC_TimeStruct.Minutes;
    int secondsRemaining = 59 - RTC_TimeStruct.Seconds;
        
    int i1 = int(minutesRemaining / 10 );
    int i2 = int(minutesRemaining - i1*10 );
    int s1 = int(secondsRemaining / 10 );
    int s2 = int(secondsRemaining - s1*10 );

    draw_digits(i1, i2, s1, s2, -1, -1);
/*
    M5.Lcd.pushImage(  2,6,35,67, (uint16_t *)m[i1]);
    M5.Lcd.pushImage( 41,6,35,67, (uint16_t *)m[i2]);
    M5.Lcd.drawPixel( 79,28, ORANGE); M5.Lcd.drawPixel( 79,54,ORANGE); 
    M5.Lcd.drawPixel( 79,27, YELLOW); M5.Lcd.drawPixel( 79,53,YELLOW); 
    M5.Lcd.pushImage( 83,6,35,67, (uint16_t *)m[s1]);
    M5.Lcd.pushImage(121,6,35,67, (uint16_t *)m[s2]);
*/
    drawDate();

    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    
    M5.Lcd.setCursor(25, mode_label_y_offset);
    M5.Lcd.print("Count");

    M5.Lcd.setCursor(35, mode_label_y_offset+28);
    M5.Lcd.print("Down");

    if ( s1 == 0 && s2 == 0 ){ fade();}

    if (minutesRemaining == 0 && secondsRemaining == 0)
    {
      haltCountdown=true;
    }
  }
  else
  {
    fade();
    fade();
    fade();
    fade();
    fade();
  }  
}

void draw_digits(int h1, int h2, int i1, int i2, int s1, int s2)
{
  if (text_clock)
    draw_digit_text(h1,h2,i1,i2,s1,s2);
  else
    draw_digit_images(h1,h2,i1,i2,s1,s2);
}


void draw_digit_text(int h1, int h2, int i1, int i2, int s1, int s2)
{
  M5.Lcd.setTextSize(9);
  M5.Lcd.setTextFont(0);
  M5.Lcd.setTextColor(TFT_ORANGE,TFT_BLACK);

  if (landscape_clock)
  {
    M5.Lcd.setCursor(0,5);
    M5.Lcd.printf(" %i%i:%i%i",h1,h2,i1,i2);
    if (s1 != -1 && s2 != -1)
    {
      M5.Lcd.setTextSize(4);
      M5.Lcd.setCursor(50,50);
      M5.Lcd.printf("%i%i",s1,s2);
    }
  }
  else
  {
    M5.Lcd.setCursor(5,5);
    M5.Lcd.printf("%i%i",h1,h2);

    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.setCursor(M5.Lcd.getCursorX()-10,M5.Lcd.getCursorY());
    M5.Lcd.printf(":\n",h1,h2);
    M5.Lcd.setTextColor(TFT_ORANGE,TFT_BLACK);
    M5.Lcd.setCursor(M5.Lcd.getCursorX()+5,M5.Lcd.getCursorY());
    M5.Lcd.printf("%i%i",i1,i2);
    
    if (s1 != -1 && s2 != -1)
    {
      M5.Lcd.setTextSize(4);
      M5.Lcd.setCursor(50,120);
      M5.Lcd.printf("%i%i",s1,s2);
    }
  }
}

void draw_digit_images(int h1, int h2, int i1, int i2, int s1, int s2)
{
  /*
  if (landscape_clock)
  {
    M5.Lcd.pushImage(  2,0,35,67, (uint16_t *)m[h1]);
    M5.Lcd.pushImage( 41,0,35,67, (uint16_t *)m[h2]);
    M5.Lcd.drawPixel( 79,22, ORANGE); M5.Lcd.drawPixel( 79,48,ORANGE); 
    M5.Lcd.drawPixel( 79,21, YELLOW); M5.Lcd.drawPixel( 79,47,YELLOW); 
    M5.Lcd.pushImage( 83,0,35,67, (uint16_t *)m[i1]);
    M5.Lcd.pushImage(121,0,35,67, (uint16_t *)m[i2]);
    if (s1 != -1 && s2 != -1)
    {
      M5.Lcd.pushImage(120,45,18,34, (uint16_t *)n[s1]);
      M5.Lcd.pushImage(140,45,18,34, (uint16_t *)n[s2]);
    }
  }
  else
  {
    M5.Lcd.pushImage(  2,0,35,67, (uint16_t *)m[h1]);
    M5.Lcd.pushImage( 41,0,35,67, (uint16_t *)m[h2]);
    M5.Lcd.drawPixel( 79,22, ORANGE); M5.Lcd.drawPixel( 79,48,ORANGE);
    M5.Lcd.drawPixel( 79,21, YELLOW); M5.Lcd.drawPixel( 79,47,YELLOW); 
    M5.Lcd.pushImage( 2,70,35,67, (uint16_t *)m[i1]);
    M5.Lcd.pushImage(41,70,35,67, (uint16_t *)m[i2]);
    if (s1 != -1 && s2 != -1)
    {
      M5.Lcd.pushImage(39,115,18,34, (uint16_t *)n[s1]);
      M5.Lcd.pushImage(60,115,18,34, (uint16_t *)n[s2]);
    }
  }
  */
}

void vfd_3_line_clock(){    // Clock mode - Hours, mins, secs with optional date
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetDate(&RTC_DateStruct);
  int h1 = int(RTC_TimeStruct.Hours / 10 );
  int h2 = int(RTC_TimeStruct.Hours - h1*10 );
  int i1 = int(RTC_TimeStruct.Minutes / 10 );
  int i2 = int(RTC_TimeStruct.Minutes - i1*10 );
  int s1 = int(RTC_TimeStruct.Seconds / 10 );
  int s2 = int(RTC_TimeStruct.Seconds - s1*10 );

  // print current and voltage of USB
  if (showPowerStats)
  {
    M5.Lcd.setCursor(5,5);
    M5.Lcd.printf("USB %.1fV, %.0fma\n",  M5.Axp.GetVBusVoltage(),M5.Axp.GetVBusCurrent());
    M5.Lcd.printf("Batt Charge %.0fma\n",  M5.Axp.GetBatChargeCurrent());
    M5.Lcd.printf("Batt %.1fV %.0fma\n",  M5.Axp.GetBatVoltage(), M5.Axp.GetBatCurrent());
  }
  else
  {
    draw_digits(h1, h2, i1, i2, s1, s2);

    drawDate();

    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    
    M5.Lcd.setCursor(35, mode_label_y_offset+28);
    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.print("Time");
  }
   
  if ( s1 == 0 && s2 == 0 ){ fade();}
}
 
void vfd_1_line_countup(){  // Timer Mode - Minutes and Seconds, with optional date
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetDate(&RTC_DateStruct);
  int i1 = int(RTC_TimeStruct.Minutes / 10 );
  int i2 = int(RTC_TimeStruct.Minutes - i1*10 );
  int s1 = int(RTC_TimeStruct.Seconds / 10 );
  int s2 = int(RTC_TimeStruct.Seconds - s1*10 );
  
  draw_digits(i1, i2, s1, s2, -1, -1);

  drawDate();

  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Lcd.setCursor(25, mode_label_y_offset);
  M5.Lcd.print("Count");
  M5.Lcd.setCursor(55, mode_label_y_offset+28);
  M5.Lcd.print("Up");

  if ( s1 == 0 && s2 == 0 ){ fade();}
}

void drawDate()
{
  /*
  if (showDate)
  {
    int j1 = int(RTC_DateStruct.Month   / 10);
    int j2 = int(RTC_DateStruct.Month   - j1*10 );
    int d1 = int(RTC_DateStruct.Date    / 10 );
    int d2 = int(RTC_DateStruct.Date    - d1*10 );

    if (landscape_clock)
    {
      M5.Lcd.pushImage(35, 75,18,34, (uint16_t *)n[d1]);
      M5.Lcd.pushImage(54, 75,18,34, (uint16_t *)n[d2]);
      M5.Lcd.pushImage(85, 75 ,18,34, (uint16_t *)n[j1]);
      M5.Lcd.pushImage(105, 75,18,34, (uint16_t *)n[j2]);
    }
    else
    {
//      M5.Lcd.pushImage(35, 75,18,34, (uint16_t *)n[d1]);
//     M5.Lcd.pushImage(54, 75,18,34, (uint16_t *)n[d2]);
//      M5.Lcd.pushImage(85, 75 ,18,34, (uint16_t *)n[j1]);
//      M5.Lcd.pushImage(105, 75,18,34, (uint16_t *)n[j2]);
    }
  }
  */
}

void fade(){
  for (int i=0;i<100;i=i+15){M5.Axp.ScreenBreath(i);delay(25);}
  for (int i=100;i>0;i=i-15){M5.Axp.ScreenBreath(i);delay(25);}
  M5.Axp.ScreenBreath(defaultBrightness);
}

void vfd_2_line(){      // Unused mode - full date and time with year.
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetDate(&RTC_DateStruct);
  //USB_SERIAL.printf("Data: %04d-%02d-%02d\n",RTC_DateStruct.Year,RTC_DateStruct.Month,RTC_DateStruct.Date);
  //USB_SERIAL.printf("Week: %d\n",RTC_DateStruct.WeekDay);
  //USB_SERIAL.printf("Time: %02d : %02d : %02d\n",RTC_TimeStruct.Hours,RTC_TimeStruct.Minutes,RTC_TimeStruct.Seconds);
  // Data: 2019-06-06
  // Week: 0
  // Time: 09 : 55 : 26
  int y1 = int(RTC_DateStruct.Year    / 1000 );
  int y2 = int((RTC_DateStruct.Year   - y1*1000 ) / 100 );
  int y3 = int((RTC_DateStruct.Year   - y1*1000 - y2*100 ) / 10 );
  int y4 = int(RTC_DateStruct.Year    - y1*1000 - y2*100 - y3*10 );
  int j1 = int(RTC_DateStruct.Month   / 10);
  int j2 = int(RTC_DateStruct.Month   - j1*10 );
  int d1 = int(RTC_DateStruct.Date    / 10 );
  int d2 = int(RTC_DateStruct.Date    - d1*10 );
  int h1 = int(RTC_TimeStruct.Hours   / 10) ;
  int h2 = int(RTC_TimeStruct.Hours   - h1*10 );
  int i1 = int(RTC_TimeStruct.Minutes / 10 );
  int i2 = int(RTC_TimeStruct.Minutes - i1*10 );
  int s1 = int(RTC_TimeStruct.Seconds / 10 );
  int s2 = int(RTC_TimeStruct.Seconds - s1*10 );
   /*
  M5.Lcd.pushImage(  0, 0,18,34, (uint16_t *)n[y1]); 
  M5.Lcd.pushImage( 19, 0,18,34, (uint16_t *)n[y2]);
  M5.Lcd.pushImage( 38, 0,18,34, (uint16_t *)n[y3]);
  M5.Lcd.pushImage( 57, 0,18,34, (uint16_t *)n[y4]);
  M5.Lcd.drawPixel( 77,13, ORANGE); M5.Lcd.drawPixel( 77,23,ORANGE);
  M5.Lcd.pushImage( 80, 0,18,34, (uint16_t *)n[j1]);
  M5.Lcd.pushImage( 99, 0,18,34, (uint16_t *)n[j2]);
  M5.Lcd.drawPixel(118,13, ORANGE); M5.Lcd.drawPixel(119,23,ORANGE);
  M5.Lcd.pushImage(120, 0,18,34, (uint16_t *)n[d1]);
  M5.Lcd.pushImage(140, 0,18,34, (uint16_t *)n[d2]);
                                                    
  M5.Lcd.pushImage( 00,40,18,34, (uint16_t *)n[h1]);
  M5.Lcd.pushImage( 20,40,18,34, (uint16_t *)n[h2]);
  M5.Lcd.drawPixel( 48,54, ORANGE); M5.Lcd.drawPixel( 48,64,ORANGE); 
  M5.Lcd.pushImage( 60,40,18,34, (uint16_t *)n[i1]);
  M5.Lcd.pushImage( 80,40,18,34, (uint16_t *)n[i2]);
  M5.Lcd.drawPixel(108,54, ORANGE); M5.Lcd.drawPixel(108,64,ORANGE);
  M5.Lcd.pushImage(120,40,18,34, (uint16_t *)n[s1]);
  M5.Lcd.pushImage(140,40,18,34, (uint16_t *)n[s2]);
 */
  if ( i1 == 0 && i2 == 0 ){ fade();}
}

bool setupOTAWebServer(const char* _ssid, const char* _password, const char* label, uint32_t timeout)
{
  bool forcedCancellation = false;

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(2);
  bool connected = false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _password);

  // Wait for connection for max of timeout/1000 seconds
  M5.Lcd.printf("%s Wifi", label);
  int count = timeout / 500;
  while (WiFi.status() != WL_CONNECTED && --count > 0)
  {
    // check for cancellation button - top button.
    updateButtonsAndBuzzer();

    if (p_primaryButton->isPressed()) // cancel connection attempts
    {
      forcedCancellation = true;
      break;
    }

    M5.Lcd.print(".");
    delay(500);
  }
  M5.Lcd.print("\n\n");

  if (WiFi.status() == WL_CONNECTED)
  {
    asyncWebServer.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
      request->send(200, "text/plain", "To upload firmware use /update");
    });

    AsyncElegantOTA.begin(&asyncWebServer);    // Start AsyncElegantOTA
    asyncWebServer.begin();    
    M5.Lcd.setRotation(0);
    
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0,155);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("%s\n\n",WiFi.localIP().toString());
    M5.Lcd.println(WiFi.macAddress());
    connected = true;

    M5.Lcd.qrcode("http://"+WiFi.localIP().toString()+"/update",0,0,135);

    connected = true;

    updateButtonsAndBuzzer();

    if (p_secondButton->isPressed())
    {
      M5.Lcd.print("\n\n20\nsecond pause");
      delay(20000);
    }
  }
  else
  {
    if (forcedCancellation)
      M5.Lcd.print("\n     Cancelled\n Connection Attempts");
    else
      M5.Lcd.print("No Connection");
  }

  delay(5000);

  M5.Lcd.fillScreen(TFT_BLACK);

  return connected;
}

void updateButtonsAndBuzzer()
{
  p_primaryButton->read();
  p_secondButton->read();
  LeakDetectorSwitch.read();
  M5.Beep.update();

  if (p_primaryButton->isPressed())
  {
    if (!primaryButtonIsPressed)
    {
      primaryButtonIsPressed=true;
      primaryButtonPressedTime=millis();
    }
  }
  else
  {
    if (primaryButtonIsPressed)
    {
      lastPrimaryButtonPressLasted = millis() - primaryButtonPressedTime;
      primaryButtonIsPressed=false;
      primaryButtonPressedTime=0;
    }
  }

  if (p_secondButton->isPressed())
  {
    if (!secondButtonIsPressed)
    {
      secondButtonIsPressed=true;
      secondButtonPressedTime=millis();
    }
  }
  else
  {
    if (secondButtonIsPressed)
    {
      lastSecondButtonPressLasted = millis() - secondButtonPressedTime;
      secondButtonIsPressed=false;
      secondButtonPressedTime=0;
    }
  }
}

char ESPNowbuffer[256];

void configAndStartUpESPNow()
{
  if (writeLogToSerial)
    USB_SERIAL.println("ESPNow/Basic Example");
  
  //Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  
  // configure device AP mode
  configESPNowDeviceAP();
  
  // This is the mac address of this peer in AP Mode
  if (writeLogToSerial)
    USB_SERIAL.print("AP MAC: "); USB_SERIAL.println(WiFi.softAPmacAddress());
  
  // Init ESPNow with a fallback logic
  InitESPNow();
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_send_cb(OnESPNowDataSent);
  esp_now_register_recv_cb(OnESPNowDataRecv);
}

void configESPNowDeviceAP() 
{
  String Prefix = "Tiger:";
  String Mac = WiFi.macAddress();
  String SSID = Prefix + Mac;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), ESPNOW_CHANNEL, 0);

  if (writeLogToSerial)
  {
    if (!result) 
    {
      USB_SERIAL.println("AP Config failed.");
    } 
    else 
    {
      USB_SERIAL.printf("AP Config Success. Broadcasting with AP: %s\n",String(SSID).c_str());
      USB_SERIAL.printf("WiFi Channel: %d\n",WiFi.channel());
    }
  }  
}

void InitESPNow() 
{
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) 
  {
    if (writeLogToSerial)
      USB_SERIAL.println("ESPNow Init Success");
    ESPNowActive = true;
  }
  else 
  {
    if (writeLogToSerial)
      USB_SERIAL.println("ESPNow Init Failed");
    ESPNowActive = false;
  }
}

// callback when data is sent from Master to Peer
void OnESPNowDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    ESPNowMessagesDelivered++;
  }
  else
  {
    ESPNowMessagesFailedToDeliver++;
  }
}

// callback when data is recv from Master
void OnESPNowDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) 
{
  if (writeLogToSerial)
  {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    USB_SERIAL.printf("Last Packet Recv from: %s\n",macStr);
    USB_SERIAL.printf("Last Packet Recv 1st Byte: '%c'\n",*data);
    USB_SERIAL.printf("Last Packet Recv Length: %d\n",data_len);
    USB_SERIAL.println((char*)data);
  }

  xQueueSend(msgsReceivedQueue, (void*)data, (TickType_t)0);  // don't block on enqueue
}

bool TeardownESPNow()
{
  bool result = false;

  if (enableESPNow && ESPNowActive)
  {
    WiFi.disconnect();
    ESPNowActive = false;
    result = true;
  }
  
  return result;
}

// Scan for peers in AP mode
bool ESPNowScanForPeer(esp_now_peer_info_t& peer, const char* peerSSIDPrefix)
{
  bool peerFound = false;
  
  M5.Lcd.println("Scanning   Networks...");
  int8_t scanResults = WiFi.scanNetworks();
  M5.Lcd.println("Complete");
  
  // reset on each scan 
  memset(&peer, 0, sizeof(peer));

  if (writeLogToSerial)
    USB_SERIAL.println("");

  if (scanResults == 0) 
  {   
    if (writeLogToSerial)
      USB_SERIAL.println("No WiFi devices in AP Mode found");

    peer.channel = ESPNOW_NO_PEER_CHANNEL_FLAG;
  } 
  else 
  {
    if (writeLogToSerial)
    {
      USB_SERIAL.print("Found "); USB_SERIAL.print(scanResults); USB_SERIAL.println(" devices ");
    }
    
    for (int i = 0; i < scanResults; ++i) 
    {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (writeLogToSerial && ESPNOW_PRINTSCANRESULTS) 
      {
        USB_SERIAL.print(i + 1);
        USB_SERIAL.print(": ");
        USB_SERIAL.print(SSID);
        USB_SERIAL.print(" (");
        USB_SERIAL.print(RSSI);
        USB_SERIAL.print(")");
        USB_SERIAL.println("");
      }
      
      delay(10);
      
      // Check if the current device starts with the peerSSIDPrefix
      if (SSID.indexOf(peerSSIDPrefix) == 0) 
      {
        if (writeLogToSerial)
        {
          // SSID of interest
          USB_SERIAL.println("Found a peer.");
          USB_SERIAL.print(i + 1); USB_SERIAL.print(": "); USB_SERIAL.print(SSID); USB_SERIAL.print(" ["); USB_SERIAL.print(BSSIDstr); USB_SERIAL.print("]"); USB_SERIAL.print(" ("); USB_SERIAL.print(RSSI); USB_SERIAL.print(")"); USB_SERIAL.println("");
        }
                
        // Get BSSID => Mac Address of the Slave
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) 
        {
          for (int ii = 0; ii < 6; ++ii ) 
          {
            peer.peer_addr[ii] = (uint8_t) mac[ii];
          }
        }

        peer.channel = ESPNOW_CHANNEL; // pick a channel
        peer.encrypt = 0; // no encryption

        peer.priv = (void*)peerSSIDPrefix;   // distinguish between different peers

        peerFound = true;
        // we are planning to have only one slave in this example;
        // Hence, break after we find one, to be a bit efficient
        break;
      }
    }
  }

  if (peerFound) 
  {
    M5.Lcd.println("Peer Found");
    if (writeLogToSerial)
      USB_SERIAL.println("Peer Found, processing..");
  } 
  else 
  {
    M5.Lcd.println("Peer Not Found");
    if (writeLogToSerial)
      USB_SERIAL.println("Peer Not Found, trying again.");
  }
  
  // clean up ram
  WiFi.scanDelete();

  return peerFound;
}

bool pairWithPeer(esp_now_peer_info_t& peer, const char* peerSSIDPrefix, int maxAttempts)
{
  bool isPaired = false;
  while(maxAttempts-- && !isPaired)
  {
    bool result = ESPNowScanForPeer(peer,peerSSIDPrefix);

    // check if peer channel is defined
    if (result && peer.channel == ESPNOW_CHANNEL)
    { 
      isPaired = ESPNowManagePeer(peer);
      M5.Lcd.printf("%s Pair ok\n",peerSSIDPrefix);
    }
    else
    {
      peer.channel = ESPNOW_NO_PEER_CHANNEL_FLAG;
      M5.Lcd.printf("%s Pair fail\n",peerSSIDPrefix);
    }
  }

  delay(1000);
  
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0,0);
  
  return isPaired;
}

// Check if the peer is already paired with the master.
// If not, pair the peer with master
bool ESPNowManagePeer(esp_now_peer_info_t& peer)
{
  bool result = false;
  
  if (peer.channel == ESPNOW_CHANNEL) 
  {
    if (ESPNOW_DELETEBEFOREPAIR) 
    {
      ESPNowDeletePeer(peer);
    }

    if (writeLogToSerial)
      USB_SERIAL.print("Peer Status: ");
      
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(peer.peer_addr);
    
    if (exists) 
    {
      // Peer already paired.
      if (writeLogToSerial)
        USB_SERIAL.println("Already Paired");

      M5.Lcd.println("Already paired");
      result = true;
    } 
    else 
    {
      // Peer not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(&peer);
      
      if (addStatus == ESP_OK) 
      {
        // Pair success
        if (writeLogToSerial)
          USB_SERIAL.println("Pair success");
        M5.Lcd.println("Pair success");
        result = true;
      } 
      else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) 
      {
        // How did we get so far!!
        if (writeLogToSerial)
          USB_SERIAL.println("ESPNOW Not Init");
        result = false;
      } 
      else if (addStatus == ESP_ERR_ESPNOW_ARG) 
      {
        if (writeLogToSerial)
            USB_SERIAL.println("Invalid Argument");
        result = false;
      } 
      else if (addStatus == ESP_ERR_ESPNOW_FULL) 
      {
        if (writeLogToSerial)
            USB_SERIAL.println("Peer list full");
        result = false;
      } 
      else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) 
      {
        if (writeLogToSerial)
          USB_SERIAL.println("Out of memory");
        result = false;
      } 
      else if (addStatus == ESP_ERR_ESPNOW_EXIST) 
      {
        if (writeLogToSerial)
          USB_SERIAL.println("Peer Exists");
        result = true;
      } 
      else 
      {
        if (writeLogToSerial)
          USB_SERIAL.println("Not sure what happened");
        result = false;
      }
    }
  }
  else 
  {
    // No peer found to process
    if (writeLogToSerial)
      USB_SERIAL.println("No Peer found to process");
    
    M5.Lcd.println("No Peer found to process");
    result = false;
  }

  return result;
}

void ESPNowDeletePeer(esp_now_peer_info_t& peer) 
{
  if (peer.channel != ESPNOW_NO_PEER_CHANNEL_FLAG)
  {
    esp_err_t delStatus = esp_now_del_peer(peer.peer_addr);
    
    if (writeLogToSerial)
    {
      USB_SERIAL.print("Peer Delete Status: ");
      if (delStatus == ESP_OK) 
      {
        // Delete success
        USB_SERIAL.println("ESPNowDeletePeer::Success");
      } 
      else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) 
      {
        // How did we get so far!!
        USB_SERIAL.println("ESPNowDeletePeer::ESPNOW Not Init");
      } 
      else if (delStatus == ESP_ERR_ESPNOW_ARG) 
      {
        USB_SERIAL.println("ESPNowDeletePeer::Invalid Argument");
      } 
      else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) 
      {
        USB_SERIAL.println("ESPNowDeletePeer::Peer not found.");
      } 
      else 
      {
        USB_SERIAL.println("Not sure what happened");
      }
    }
  }
}
 
