#include <rom/rtc.h>               /* should be installed together with ESP32 Arduino install */
#include <list>                    /* should be installed together with ESP32 Arduino install */
//#include <SPI.h>                   /* should be installed together with ESP32 Arduino install */
#include <Wire.h>                  /* should be installed together with ESP32 Arduino install */
#include <FS.h>                    /* should be installed together with ESP32 Arduino install */
#include <FFat.h>                  /* should be installed together with ESP32 Arduino install */
#include <ESPmDNS.h>               /* should be installed together with ESP32 Arduino install */
#include <Preferences.h>           /* should be installed together with ESP32 Arduino install */
#include <WiFi.h>                  /* should be installed together with ESP32 Arduino install */
#include <AsyncTCP.h>              /* Reports as 1.0.3 https://github.com/me-no-dev/AsyncTCP */
#include <ESPAsyncWebServer.h>     /* Reports as 1.2.3 https://github.com/me-no-dev/ESPAsyncWebServer */
#include <OneWire.h>               /* Use this version instead of the standard Arduino library: https://github.com/stickbreaker/OneWire */
#include <moonPhase.h>             /* Install 1.0.0 https://github.com/CelliesProjects/moonPhase */
#include <FFatSensor.h>            /* Install 1.0.2 https://github.com/CelliesProjects/FFatSensor */
#include <Task.h>                  /* Install 1.0.0 https://github.com/CelliesProjects/Task */
#include "ledState.h"

const char * wifi_network = "sharrsp";                /* Change your WiFi username and password before compiling! */
const char * wifi_password = "19533591";                 /* Or use https://github.com/EspressifApp/EsptouchForAndroid/releases/latest for Android phones */
//const char * wifi_network = "97flatinet";                /* Change your WiFi username and password before compiling! */
//const char * wifi_password = "FireWind";                 /* Or use https://github.com/EspressifApp/EsptouchForAndroid/releases/latest for Android phones */
                                                           /* Or use https://github.com/EspressifApp/EsptouchForIOS/releases/tag/v1.0.0 for iPhones */

#define SET_STATIC_IP              false                 /* If SET_STATIC_IP is set to true then STATIC_IP, GATEWAY, SUBNET and PRIMARY_DNS have to be set to some sane values */

const IPAddress STATIC_IP(192, 168, 0, 80);              /* This should be outside your router dhcp range! */
const IPAddress GATEWAY(192, 168, 0, 1);                 /* Set to your gateway ip address */
const IPAddress SUBNET(255, 255, 255, 0);                /* Usually 255,255,255,0 but check in your router or pc connected to the same network */
const IPAddress PRIMARY_DNS(192, 168, 0, 30);            /* Check in your router */
const IPAddress SECONDARY_DNS( 192, 168, 0, 50 );        /* Check in your router */

#include "deviceSetup.h"
#include "devicePinSetup.h"

#if GIT_TAG
#include "gitTagVersion.h"
#else
const char * sketchVersion = "v1.6.8";
#endif

/**************************************************************************
       update frequency for LEDS in Hz
**************************************************************************/
#define UPDATE_FREQ_LEDS                   100

/**************************************************************************
       number of bit precission for LEDC timer
**************************************************************************/
#define LEDC_NUMBER_OF_BIT                 16


/**************************************************************************
       maximum allowable pwm frequency in Hz
       -remember the rise and fall times of a 330R gate resistor!
**************************************************************************/
#define LEDC_MAXIMUM_FREQ                  1300


/**************************************************************************
       the number of LED channels
**************************************************************************/
#define NUMBER_OF_CHANNELS                 5


/**************************************************************************
       the maximum number of timers allowed for each channel
**************************************************************************/
#define MAX_TIMERS                         50


/**************************************************************************
       default hostname if no hostname is set
**************************************************************************/
#define DEFAULT_HOSTNAME_PREFIX             "aquacontrol32"


/**************************************************************************
       defines for threeDigitPercentage()
**************************************************************************/
#define SHOW_PERCENTSIGN                    true
#define NO_PERCENTSIGN                      false

/**************************************************************************
      Setup included libraries
 *************************************************************************/
ledState                leds;

FFatSensor              logger;

moonPhase               moonPhase;

Preferences             preferences;

/**************************************************************************
       type definitions
**************************************************************************/
struct lightTimer_t
{
  time_t      time;                    /* time in seconds since midnight so range is 0-86400 */
  uint8_t     percentage;              /* in percentage so range is 0-100 */
};

struct channelData_t
{
  lightTimer_t    timer[MAX_TIMERS];
  char            name[15];            /* initially set to 'channel 1' 'channel 2' etc. */
  char            color[8];            /* interface color, not light color! in hex format*/
  /*                                      Example: '#ff0000' for bright red */
  float           currentPercentage;   /* what percentage is this channel set to */
  uint8_t         pin;                 /* which ESP32 pin is this channel on */
  uint8_t         numberOfTimers;      /* actual number of timers for this channel */
  float           fullMoonLevel;       /* a percentage between 0.0-1.0 set in the web interface */
};

/* const */
const char* defaultTimerFile   = "/default.aqu";

/* task priorities */
const uint8_t dimmerTaskPriority       = 8;
const uint8_t ntpTaskPriority          = 5;
#if 0
const uint8_t tftTaskPriority          = 6;
const uint8_t oledTaskPriority         = 4;
#endif
const uint8_t wifiTaskPriority         = 3;
const uint8_t webserverTaskPriority    = 1;

/* used esp32 HW timers */
const uint8_t HWTIMER0_SENSOR          = 0;
const uint8_t HWTIMER1_MOON            = 1;

/**************************************************************************
       start of global variables
**************************************************************************/
channelData_t           channel[NUMBER_OF_CHANNELS];

moonData_t              moonData;

TaskHandle_t            xDimmerTaskHandle            = NULL;

//Boot time is saved
timeval                 systemStart;

char                    hostName[30];

double                  ledcActualFrequency;
uint16_t                ledcMaxValue;
uint8_t                 ledcNumberOfBits;

/*****************************************************************************************
       end of global variables
*****************************************************************************************/

/* forward declarations  */
void wifiTask( void * pvParameters );

/* global functions */
inline float mapFloat( const float &x, const float &in_min, const float &in_max, const float &out_min, const float &out_max) {
  return ( x - in_min ) * ( out_max - out_min ) / ( in_max - in_min ) + out_min;
}

inline float invertFloat( const float &x, const float &out_min, const float &out_max) {
  return (1.0f - ( x - out_min ) / ( out_max - out_min )) * ( out_max - out_min ) + out_min;
}

// https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
// chapter 3.1.2
const char * resetStr[] =
{
  "NO REASON",
  "POWERON_RESET",           // 0x01
  "REASON 0xO2",             // 0x02
  "SW_RESET",                // 0x03
  "OWDT_RESET",              // 0x04
  "DEEPSLEEP_RESET",         // 0x05
  "SDIO_RESET",              // 0x06
  "TG0WDT_SYS_RESET",        // 0x07
  "TG1WDT_SYS_RESET",        // 0x08
  "RTCWDT_SYS_RESET",        // 0x09
  "REASON 0xOA",             // 0x0A
  "TGWDT_CPU_RESET",         // 0x0B
  "SW_CPU_RESET",            // 0x0C
  "RTCWDT_CPU_RESET",        // 0x0D
  "PRO_CPU_RESET",           // 0x0E
  "RTCWDT_BROWN_OUT_RESET",  // 0x0F
  "RTCWDT_RTC_RESET"         // 0x10
};
inline const char * resetString( const uint8_t core ) {
  return resetStr[rtc_get_reset_reason( core )];
}

const char * threeDigitPercentage( char * buffer, const uint8_t &bufferSize, const float &percentage, const bool &addPercentSign )
{
  if ( percentage < 0.005 )
    snprintf( buffer, bufferSize, addPercentSign ? "  0%%  " : "  0  " );
  else if ( percentage > 99.9 )
    snprintf( buffer, bufferSize, addPercentSign ? " 100%% " : " 100 " );
  else if ( percentage < 10 )
    snprintf( buffer,  bufferSize , addPercentSign ? " %1.2f%% " : " %1.2f ", percentage );
  else
    snprintf( buffer,  bufferSize , addPercentSign ? " %2.1f%% " : " %2.1f ", percentage );
  return buffer;
}

void setup()
{
  pinMode( LED0_PIN, OUTPUT );
  pinMode( LED1_PIN, OUTPUT );
  pinMode( LED2_PIN, OUTPUT );
  pinMode( LED3_PIN, OUTPUT );
  pinMode( LED4_PIN, OUTPUT );

  pinMode( I2C_SCL_PIN, INPUT_PULLUP );
  pinMode( I2C_SDA_PIN, INPUT_PULLUP );

  pinMode( ONEWIRE_PIN, INPUT );

  gpio_set_drive_capability( (gpio_num_t)LED0_PIN, GPIO_DRIVE_CAP_3 );
  gpio_set_drive_capability( (gpio_num_t)LED1_PIN, GPIO_DRIVE_CAP_3 );
  gpio_set_drive_capability( (gpio_num_t)LED2_PIN, GPIO_DRIVE_CAP_3 );
  gpio_set_drive_capability( (gpio_num_t)LED3_PIN, GPIO_DRIVE_CAP_3 );
  gpio_set_drive_capability( (gpio_num_t)LED4_PIN, GPIO_DRIVE_CAP_3 );

  gpio_set_drive_capability( (gpio_num_t)ONEWIRE_PIN, GPIO_DRIVE_CAP_3 );

  btStop();

  ESP_LOGI( TAG, "aquacontrol32 %s", sketchVersion );
  ESP_LOGI( TAG, "ESP32 SDK: %s", ESP.getSdkVersion() );

  preferences.begin( "aquacontrol32", false );

  /* check if a ffat partition is defined and halt the system if it is not defined*/
  if (!esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat")) {
    ESP_LOGE( TAG, "No FFat partition defined. Halting.\nCheck 'Tools>Partition Scheme' in the Arduino IDE and select a FFat partition." );
    const char * noffatStr = "No FFat found...";
    while (true) delay(1000); /* system is halted */
  }

  /* partition is defined - try to mount it */
  if ( FFat.begin() )
    ESP_LOGI( TAG, "FFat mounted." );

  /* partition is present, but does not mount so now we just format it */
  else {
    const char * formatStr = "Formatting...";
    ESP_LOGI( TAG, "%s", formatStr );
    if (!FFat.format( true, (char*)"ffat" ) || !FFat.begin()) {
      ESP_LOGE( TAG, "FFat error while formatting. Halting." );
      const char * errorffatStr = "FFat error.";
      while (true) delay(1000); /* system is halted */;
    }
  }

  xTaskCreatePinnedToCore(
    wifiTask,                       /* Function to implement the task */
    "wifiTask",                     /* Name of the task */
    3000,                           /* Stack size in words */
    NULL,                           /* Task input parameter */
    wifiTaskPriority,               /* Priority of the task */
    NULL,                           /* Task handle. */
    1);
}

void loop()
{
  vTaskDelete( NULL );
}
