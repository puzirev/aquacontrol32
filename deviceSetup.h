/**************************************************************************
       country code for ntp server selection
       https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2
**************************************************************************/
#define COUNTRY_CODE_ISO_3166              "ru"


/**************************************************************************
       number of DS18B20 sensors
**************************************************************************/
#define NUMBER_OF_SENSORS                  3


#if 0
/**************************************************************************
       OLED I2C address
**************************************************************************/
#define OLED_ADDRESS                       0x3C


/**************************************************************************
       false = show light and temps on oled
       true = show system data on oled
**************************************************************************/
#define OLED_SHOW_SYSTEMDATA               false


/**************************************************************************
       Some tft/sdcard breakout boards have their TFT MISO pin unconnected.
       These displays will not be auto-detected by aquacontrol.
       Set TFT_HAS_NO_MISO to false to use device detection.
       Set TFT_HAS_NO_MISO to true to override detection and use these tft boards.
**************************************************************************/
#define TFT_HAS_NO_MISO                    false


/**************************************************************************
       TFT SPI CLOCK SPEED (DEFAULT = 10MHz)
**************************************************************************/
#define TFT_SPI_CLOCK                      10 * 1000 * 1000
#endif

/**************************************************************************
       Some tft/sdcard breakout boards have their touch coordinates reversed.
       Set to true if your touchscreen has reversed coordinates.
**************************************************************************/
#define TOUCH_IS_INVERTED                  false


/**************************************************************************
       Set to true to use the flash and compile scripts.
**************************************************************************/
#define GIT_TAG                             true
