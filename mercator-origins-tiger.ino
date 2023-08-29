// M5StickC Nixie tube Clock: 2019.06.06 
#include <M5StickCPlus.h>

// rename the git file "mercator_secrets_template.c" to the filename below, filling in your wifi credentials etc.
#include "mercator_secrets.c"

// Nixie Clock graphics files
#include "vfd_18x34.c"
#include "vfd_35x67.c"

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

#include <WiFi.h>
#include "time.h" 

#include <AsyncTCP.h>           // OTA updates
#include <ESPAsyncWebServer.h>  // OTA updates
#include <AsyncElegantOTA.h>    // OTA updates

AsyncWebServer asyncWebServer(80);      // OTA updates
const bool enableOTAServer=false; // OTA updates
bool otaActiveListening=true;   // OTA updates toggle

Button* p_primaryButton = NULL;
Button* p_secondButton = NULL;

bool primaryButtonIsPressed = false;
uint32_t primaryButtonPressedTime = 0;
bool secondButtonIsPressed = false;
uint32_t secondButtonPressedTime = 0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;        // timezone offset
int   daylightOffset_sec = 0;   // DST offset

RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

const char* leakAlarmMsg = "    Cable\n\n  Penetrator\n\n    Leak!";

// initial mode is Clock
int mode_ = 3; // 3:2Lines 2: 2Lines(YYMM), 1:1Line

// temp change: mic screen is Clock
// int mode_ = 5; 


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

const int defaultBrightness = 15;

int countdownFrom=59;
bool haltCountdown=false;
bool showDate=true;  // 

bool showPowerStats=false;

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
//       soundFile.close();
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

  if (WiFi.status() == WL_CONNECTED)
  {
    M5.Lcd.println("Wifi OK");
  
    //init and get the time
    _initialiseTimeFromNTP:
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    if(!getLocalTime(&timeinfo))
    {
      Serial.println("No time available (yet)");
      // Let RTC continue with existing settings
      M5.Lcd.println("Wait for NTP Time\n");
      delay(1000);
    }
    else
    {
      // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
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
      M5.Rtc.SetData(&DateStruct);    
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
  }

  M5.Lcd.fillScreen(BLACK);
  
  setRotationForClockStyle();

  resetClock();
}

bool goProButtonsPrimaryControl = true;

void setup()
{ 
  // force BST for now, so shows right time even when not online
  // force Gozo time for now.
  daylightOffset_sec = 3600; // BST
   
  M5.begin();

  pinMode(UNUSED_GPIO_36_PIN,INPUT);

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
  M5.Axp.ScreenBreath(defaultBrightness);             // 7-15
  
  Serial.begin(115200);

  if (enableOTAServer)
  {
    if (!setupOTAWebServer(ssid_1, password_1, label_1, timeout_1))
      if (!setupOTAWebServer(ssid_2, password_2, label_2, timeout_2))
        setupOTAWebServer(ssid_3, password_3, label_3, timeout_3);

    setRotationForClockStyle();
  }

  initialiseRTCfromNTP();
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
    M5.Lcd.setTextSize(3);
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

void resetClock()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
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
  M5.Rtc.SetData(&DateStruct);    

  mode_ = 3; // change back to 3
}

void fadeToBlackAndShutdown()
{
  for (int i=14; i>6; i--)
  {
    M5.Axp.ScreenBreath(i);             // 7-14 fade to black
    delay(100);
  }

  M5.Axp.PowerOff(); 
}

bool checkReedSwitches()
{
  bool changeMade = false;
    
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
  
    
  if (primaryButtonIsPressed)
  {
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_RED);
    M5.Lcd.setCursor(pressedPrimaryButtonX,pressedPrimaryButtonY);
    M5.Lcd.printf("%i",(millis()-primaryButtonPressedTime)/1000);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  else
  {
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(pressedPrimaryButtonX,pressedPrimaryButtonY);
    M5.Lcd.print(" ");
  }
  
  if (secondButtonIsPressed)
  {
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLUE);
    M5.Lcd.setCursor(pressedSecondButtonX,pressedSecondButtonY);
    M5.Lcd.printf("%i",(millis()-secondButtonPressedTime)/1000);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  else
  {
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(pressedSecondButtonX,pressedSecondButtonY);
    M5.Lcd.print(" ");
  }

  if (p_primaryButton->wasReleasefor(100))
  {
    // Screen cycle command
    if (mode_ == 4) // countdown mode, next is clock
    {
      resetClock(); changeMade = true;
    }
    else if (mode_ == 3) // clock mode, next is timer
    {
      resetCountUpTimer(); changeMade = true;
    }
    else if (mode_ == 1) // countup timer mode, next is countdown
    {
      resetCountDownTimer(); changeMade = true;
    }
  }

  // press second button for 5 seconds to attempt WiFi connect and enable OTA
  if (p_secondButton->wasReleasefor(5000))
  { 
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
    // Screen modification command
    if (mode_ == 4)       // Countdown mode, reduce timer by 15 mins
    {
      countdownFrom=countdownFrom-15;
      if (countdownFrom <= 0)
        countdownFrom = 59;
      changeMade = true;
    }
  }
  // press second button for 0.1 second...
  else if (p_secondButton->wasReleasefor(100))
  {
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
  }
  return changeMade;
}

void loop(void)
{ 
  shutdownIfUSBPowerOff();
//  toggleOTAActiveAndWifiIfUSBPowerOff();

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

void vfd_4_line_countdown(const int countdownFrom){ // Countdown mode, minutes, seconds
  int minutesRemaining = 0, secondsRemaining = 0;
  
  if (!haltCountdown)
  {
    M5.Rtc.GetTime(&RTC_TimeStruct);
    M5.Rtc.GetData(&RTC_DateStruct);
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
}

void vfd_3_line_clock(){    // Clock mode - Hours, mins, secs with optional date
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);
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
  }
   
  if ( s1 == 0 && s2 == 0 ){ fade();}
}
 
void vfd_1_line_countup(){  // Timer Mode - Minutes and Seconds, with optional date
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);
  int i1 = int(RTC_TimeStruct.Minutes / 10 );
  int i2 = int(RTC_TimeStruct.Minutes - i1*10 );
  int s1 = int(RTC_TimeStruct.Seconds / 10 );
  int s2 = int(RTC_TimeStruct.Seconds - s1*10 );
  
  draw_digits(i1, i2, s1, s2, -1, -1);

  drawDate();

  if ( s1 == 0 && s2 == 0 ){ fade();}
}

void drawDate()
{
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
}

void fade(){
  for (int i=7;i<16;i++){M5.Axp.ScreenBreath(i);delay(25);}
  for (int i=15;i>7;i--){M5.Axp.ScreenBreath(i);delay(25);}
  M5.Axp.ScreenBreath(defaultBrightness);             // 7-15
}

void vfd_2_line(){      // Unused mode - full date and time with year.
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);
  //Serial.printf("Data: %04d-%02d-%02d\n",RTC_DateStruct.Year,RTC_DateStruct.Month,RTC_DateStruct.Date);
  //Serial.printf("Week: %d\n",RTC_DateStruct.WeekDay);
  //Serial.printf("Time: %02d : %02d : %02d\n",RTC_TimeStruct.Hours,RTC_TimeStruct.Minutes,RTC_TimeStruct.Seconds);
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
      secondButtonIsPressed=false;
      secondButtonPressedTime=0;
    }
  }
}
