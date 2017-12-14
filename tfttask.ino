const uint16_t TFT_BACK_COLOR         = ILI9341_BLACK;
const bool     TFT_SHOW_RAW           = false;            /* show raw PWM values */
const uint16_t TFT_TEXT_COLOR         = ILI9341_YELLOW;
const uint16_t TFT_DATE_COLOR         = ILI9341_WHITE;
const uint16_t TFT_TEMP_COLOR         = ILI9341_WHITE;
const uint8_t  TFT_BACKLIGHT_BITDEPTH = 16;               /*min 11 bits, max 16 bits */
const uint8_t  TFT_BACKLIGHT_CHANNEL  = NUMBER_OF_CHANNELS;
const uint16_t TFT_BUTTON_WIDTH       = 100;
const uint16_t TFT_BUTTON_HEIGHT      =  40;

enum displayState {
  normal, menu
};

struct tftPoint_t
{
  uint16_t x;
  uint16_t y;
};

struct button_t
{
  uint16_t x;
  uint16_t y;
  int16_t  w;
  int16_t  h;
  char     text[15];
};

/* buttons used in menu*/
const button_t LIGHTSON_BUTTON
{
  10, 10, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "ON"
};

const button_t LIGHTSOFF_BUTTON
{
  10, 90, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "OFF"
};

const button_t LIGHTSAUTO_BUTTON
{
  10, 170, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "AUTO"
};

const button_t EXIT_BUTTON
{
  210, 170, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "EXIT"
};

/* slider for backlight control - note: XPOS is center of slider*/
const uint16_t SLIDER_XPOS = 160;
const uint16_t SLIDER_YPOS = 40;
const uint16_t SLIDER_WIDTH = 60;
const uint16_t SLIDER_HEIGHT = 180;

/* area to check for touch to control backlight */
const button_t sliderArea
{
  SLIDER_XPOS - SLIDER_WIDTH / 2, SLIDER_YPOS, SLIDER_WIDTH, SLIDER_HEIGHT
};

/* button used on main screen */
const button_t MENU_BUTTON
{
  210, 10, TFT_BUTTON_WIDTH, TFT_BUTTON_HEIGHT, "MENU"
};

uint16_t backlightMaxvalue;

displayState tftState = normal;

bool tftClearScreen = true;

void tftTask( void * pvParameters )
{
  const time_t tftTaskdelayTime = ( 1000 / UPDATE_FREQ_TFT) / portTICK_PERIOD_MS;

  tft.fillScreen( TFT_BACK_COLOR );

  /* setup backlight pwm */
  ledcAttachPin( TFT_BACKLIGHT_PIN, TFT_BACKLIGHT_CHANNEL );
  double backlightFrequency = ledcSetup( TFT_BACKLIGHT_CHANNEL , LEDC_MAXIMUM_FREQ, TFT_BACKLIGHT_BITDEPTH );

  backlightMaxvalue = ( 0x00000001 << TFT_BACKLIGHT_BITDEPTH ) - 1;

  tftBrightness = readInt8NVS( "tftbrightness", tftBrightness );
  ledcWrite( TFT_BACKLIGHT_CHANNEL, map( tftBrightness, 0, 100, 0, backlightMaxvalue ) );

  ( readStringNVS( "tftorientation", "normal" ) == "normal" ) ? tftOrientation = TFT_ORIENTATION_NORMAL : tftOrientation = TFT_ORIENTATION_UPSIDEDOWN;
  tft.setRotation( tftOrientation );
  while ( !xDimmerTaskHandle )
  {
    vTaskDelay( 10 / portTICK_PERIOD_MS );
  }
  ledcWrite( TFT_BACKLIGHT_CHANNEL, 0 );

  while (1)
  {
    switch ( tftState )
    {
      case normal: showStatus();
        break;
      case menu:   showMenu();
        break;
      default:     break;
    }
    vTaskDelay( tftTaskdelayTime / portTICK_PERIOD_MS );
  }
}

void showMenu()
{
  if ( tftClearScreen )
  {
    ledcWrite( TFT_BACKLIGHT_CHANNEL, 0 );
    tft.fillScreen( ILI9341_BLACK );
    drawMenuButtons();
    drawBacklightSlider();
    ledcWrite( TFT_BACKLIGHT_CHANNEL, map( tftBrightness, 0, 100, 0, backlightMaxvalue ) );
    tftClearScreen = false;
  }

  if ( touch.touched() )
  {
    TS_Point p = touch.getPoint();

    if ( buttonPressed( LIGHTSON_BUTTON , p ) )
    {
      lightsOn();
      drawMenuButtons();
    }
    else if ( buttonPressed( LIGHTSOFF_BUTTON , p ) )
    {
      lightsOff();
      drawMenuButtons();
    }
    else if ( buttonPressed( LIGHTSAUTO_BUTTON , p ) )
    {
      lightsAuto();
      drawMenuButtons();
    }
    else if ( buttonPressed( sliderArea , p ) )
    {
      tftPoint_t touchedLocation;

      touchedLocation = mapToTft( p.x, p.y );
      tftBrightness = map( touchedLocation.y , SLIDER_YPOS, SLIDER_HEIGHT + SLIDER_YPOS, 100, 0 );
      drawBacklightSlider();
    }
    else if ( buttonPressed( EXIT_BUTTON , p ) )
    {
      ledcWrite( TFT_BACKLIGHT_CHANNEL, 0 );
      tftClearScreen = true;
      tftState = normal;
    }
  }
}

void showStatus()
{
  const uint16_t BARS_BOTTOM      = 190;
  const uint16_t BARS_HEIGHT      = BARS_BOTTOM;
  const uint16_t BARS_BORDER      = 10;
  const uint16_t BARS_WIDTH       = 200 / 5; /* note: total width is 200 px */
  const float    HEIGHT_FACTOR    = BARS_HEIGHT / 100.0;

  button_t TFT_CLOCK_AREA
  {
    10, 205, 110, 30
  };

  button_t TFT_NETWORK_AREA
  {
    140, 205, 170, 30
  };

  uint16_t channelColor565[NUMBER_OF_CHANNELS];

  if ( tftClearScreen )
  {
    tft.fillScreen( ILI9341_BLACK );
    drawButton( MENU_BUTTON, ILI9341_BLUE, 0 );
    //tft.drawRect( TFT_CLOCK_AREA.x, TFT_CLOCK_AREA.y, TFT_CLOCK_AREA.w, TFT_CLOCK_AREA.h, ILI9341_WHITE );
    //tft.drawRect( TFT_NETWORK_AREA.x, TFT_NETWORK_AREA.y, TFT_NETWORK_AREA.w, TFT_NETWORK_AREA.h, ILI9341_WHITE );
    tftClearScreen = false;
  }

  tft.startWrite();
  for ( uint8_t channelNumber = 0; channelNumber < NUMBER_OF_CHANNELS; channelNumber++ )
  {
    // redraw the top part of the bar
    tft.writeFillRect( channelNumber * BARS_WIDTH + BARS_BORDER,
                       BARS_BOTTOM - BARS_HEIGHT,
                       BARS_WIDTH - BARS_BORDER * 2,
                       BARS_HEIGHT - channel[channelNumber].currentPercentage * HEIGHT_FACTOR,
                       TFT_BACK_COLOR );
    /*
          //100% water mark
          tft.drawFastHLine( channelNumber * BARS_WIDTH + BARS_BORDER,
                             BARS_BOTTOM - BARS_HEIGHT - 1,
                             BARS_WIDTH - BARS_BORDER * 2,
                             tft.color565( r, g, b ) );
    */

    uint32_t color = strtol( &channel[channelNumber].color[1], NULL, 16 );
    channelColor565[channelNumber] = tft.color565( ( color & 0xFF0000 ) >> 16, ( color & 0x00FF00 ) >> 8, color & 0x0000FF  );

    // redraw the bottom part of the bar
    tft.writeFillRect( channelNumber * BARS_WIDTH + BARS_BORDER,
                       BARS_BOTTOM - channel[channelNumber].currentPercentage * HEIGHT_FACTOR,
                       BARS_WIDTH - BARS_BORDER * 2,
                       channel[channelNumber].currentPercentage * HEIGHT_FACTOR,
                       channelColor565[channelNumber]);
  }
  tft.endWrite();

  uint32_t averageLedBrightness = 0;

  for ( uint8_t channelNumber = 0; channelNumber < NUMBER_OF_CHANNELS; channelNumber++ )
  {
    tft.setCursor( channelNumber * BARS_WIDTH + 2, BARS_BOTTOM + 4 );
    tft.setTextSize( 1 );
    tft.setTextColor( channelColor565[channelNumber] , TFT_BACK_COLOR );

    char content[8];
    if ( TFT_SHOW_RAW )
    {
      snprintf( content, sizeof( content ), "%04X", ledcRead( channelNumber ) );
    }
    else
    {
      snprintf( content, sizeof( content ), "%*" ".1f%%", 5, channel[channelNumber].currentPercentage );
    }
    tft.print( content );
    averageLedBrightness += ledcRead( channelNumber );
  }
  averageLedBrightness = averageLedBrightness / NUMBER_OF_CHANNELS;

  uint16_t rawBrightness = map( tftBrightness, 0, 100, 0, backlightMaxvalue );

  ledcWrite( TFT_BACKLIGHT_CHANNEL, ( averageLedBrightness > rawBrightness ) ? rawBrightness : averageLedBrightness );

  //draw temps under the menu button
  tft.setTextSize( 2 );
  if ( numberOfFoundSensors )
  {
    tft.setTextColor( TFT_TEMP_COLOR , TFT_BACK_COLOR );
    for ( uint8_t thisSensor = 0; thisSensor < numberOfFoundSensors; thisSensor++ )
    {
      button_t tempArea;

      tempArea.x = 220;
      tempArea.y = 80 + thisSensor * 40;
      tempArea.w = TFT_BUTTON_WIDTH - 20;
      tempArea.h = 30;
      snprintf( tempArea.text, sizeof( tempArea.text ), "%.1f%c", sensor[thisSensor].tempCelcius, char(247) );
      drawButton( tempArea, 0, ILI9341_GREEN );
    }
  }

  struct tm timeinfo;

  getLocalTime( &timeinfo );

  strftime( TFT_CLOCK_AREA.text, sizeof( TFT_CLOCK_AREA.text ), "%T", &timeinfo );
  drawButton( TFT_CLOCK_AREA, 0, 0 );

  tcpip_adapter_ip_info_t ip_info;

  ESP_ERROR_CHECK( tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &ip_info ) );
  snprintf( TFT_NETWORK_AREA.text, sizeof( TFT_NETWORK_AREA.text ), "%s", ip4addr_ntoa( &ip_info.ip ) );
  drawButton( TFT_NETWORK_AREA, 0, 0 );


  if ( touch.touched() )
  {
    TS_Point p = touch.getPoint();
    if ( buttonPressed( MENU_BUTTON , p ) )
    {
      tftState = menu;
      tftClearScreen = true;
    }
  }
}

static inline __attribute__((always_inline)) void drawButton( struct button_t button, uint16_t color, uint16_t bordercolor )
{
  tft.setTextSize( 2 );
  tft.setTextColor( ILI9341_YELLOW, color );
  if ( color )
  {
    tft.fillRect( button.x, button.y, button.w, button.h, color );
  }
  if ( bordercolor )
  {
    tft.drawRect( button.x, button.y, button.w, button.h, bordercolor );
  }
  int16_t x, y;
  uint16_t w, h;
  tft.getTextBounds( button.text, 0, 0, &x, &y, &w, &h);
  tft.setCursor( ( button.x + button.w / 2 ) - w / 2,
                 ( button.y + button.h / 2 ) - h / 2 );
  tft.print( button.text );
}

static inline __attribute__((always_inline)) void drawMenuButtons()
{
  drawButton( LIGHTSON_BUTTON,     lightStatus == LIGHTS_ON  ? ILI9341_RED : ILI9341_BLUE, 0 );
  drawButton( LIGHTSOFF_BUTTON,    lightStatus == LIGHTS_OFF ? ILI9341_RED : ILI9341_BLUE, 0 );
  drawButton( LIGHTSAUTO_BUTTON,   lightStatus == LIGHTS_AUTO ? ILI9341_RED : ILI9341_BLUE, 0 );
  drawButton( EXIT_BUTTON, ILI9341_BLUE, 0 );
}

static inline __attribute__((always_inline)) bool buttonPressed( struct button_t button, const TS_Point p )
{
  tftPoint_t clickedLocation;
  clickedLocation = mapToTft( p.x, p.y);
  return ( clickedLocation.x > button.x && clickedLocation.x < button.x + button.w ) && ( clickedLocation.y > button.y && clickedLocation.y < button.y + button.h );
}

static inline __attribute__((always_inline)) struct tftPoint_t mapToTft( uint16_t touchX, uint16_t touchY )
{
  uint16_t x, y;

  if ( tftOrientation == TFT_ORIENTATION_UPSIDEDOWN )
  {
    x = mapFloat( touchX, 340, 3900, 0, 320 );
    y = mapFloat( touchY, 200, 3850, 0, 240 );
  }
  else if ( tftOrientation == TFT_ORIENTATION_NORMAL )
  {
    x = mapFloat( touchX, 340, 3900, 320, 0 );
    y = mapFloat( touchY, 200, 3850, 240, 0 );
  }
  return { x, y };
}

static inline __attribute__((always_inline)) void drawBacklightSlider()
{
  //TODO: only erase the button, not the slider
  button_t sliderButton;

  tft.startWrite();
  tft.writeFillRect( sliderArea.x, sliderArea.y, sliderArea.w, sliderArea.h, ILI9341_BLACK);
  tft.endWrite();
  tft.drawRect( SLIDER_XPOS - 2, SLIDER_YPOS, 4, SLIDER_HEIGHT, ILI9341_YELLOW );

  //drawButton( sliderArea, 0, ILI9341_GREEN ); //debug

  uint16_t ypos = map( tftBrightness, 0, 100, SLIDER_HEIGHT, SLIDER_YPOS );
  sliderButton = { SLIDER_XPOS - ( SLIDER_WIDTH / 2 ), ypos, SLIDER_WIDTH, 30 };

  snprintf( sliderButton.text, sizeof( sliderButton.text ), "%.0f%%", tftBrightness );
  drawButton( sliderButton, ILI9341_BLUE, 0 );
  //set backlight
  ledcWrite( TFT_BACKLIGHT_CHANNEL, map( tftBrightness, 0, 100, 0, backlightMaxvalue ) );
}
