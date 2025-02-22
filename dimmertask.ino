volatile bool moonUpdate = true;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR _moonISR()
{
  portENTER_CRITICAL_ISR(&timerMux);
  moonUpdate = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR dimmerTask ( void * pvParameters )
{
  const TickType_t dimmerTaskdelayTime = 1000 / UPDATE_FREQ_LEDS / portTICK_PERIOD_MS;

  if ( defaultTimersLoaded() ) ESP_LOGI( TAG, "Default timers loaded." );
  else {
    ESP_LOGI( TAG, "No timers loaded." );
    setEmptyTimers();
  }

  /* setup pwm on leds */
  channel[ 0 ].pin = LED0_PIN;
  channel[ 1 ].pin = LED1_PIN;
  channel[ 2 ].pin = LED2_PIN;
  channel[ 3 ].pin = LED3_PIN;
  channel[ 4 ].pin = LED4_PIN;
  for ( uint8_t num = 0; num < NUMBER_OF_CHANNELS; num++ ) {
    char NVSKeyName[32];

    snprintf( NVSKeyName, sizeof( NVSKeyName ), "channelname%i", num );
    snprintf( channel[ num ].name, sizeof( channel[ num ].name ), preferences.getString( NVSKeyName, "channelname" ).c_str() );

    snprintf( NVSKeyName, sizeof( NVSKeyName ), "channelcolor%i", num );
    snprintf( channel[ num ].color, sizeof( channel[ num ].color ), preferences.getString( NVSKeyName, "#fffe7a" ).c_str() );

    snprintf( NVSKeyName, sizeof( NVSKeyName ), "channelminimum%i", num );
    channel[ num ].fullMoonLevel  = preferences.getFloat( NVSKeyName, 0  );

    ledcAttachPin( channel[num].pin, num);
  }

  setupDimmerPWMfrequency( preferences.getDouble( "pwmfrequency", LEDC_MAXIMUM_FREQ ),
                           preferences.getUInt( "pwmdepth", LEDC_NUMBER_OF_BIT ) );

  hw_timer_t * moonTimer = timerBegin( HWTIMER1_MOON, 80, true );
  timerAttachInterrupt( moonTimer, &_moonISR, true );
  timerAlarmWrite( moonTimer, 1000000 * 10, true );
  timerAlarmEnable( moonTimer );

  leds.setState( LIGHTS_AUTO );

  TickType_t xLastWakeTime = xTaskGetTickCount();

  ESP_LOGI( TAG, "Lights running after %i ms.", millis() );

  while (1) {
    if ( moonUpdate ) {
      moonData = moonPhase.getPhase();
      ESP_LOGI( TAG, "Moon phase updated: %i degrees %.6f%% lit", moonData.angle, moonData.percentLit * 100.0f );
      portENTER_CRITICAL(&timerMux);
      moonUpdate = false;
      portEXIT_CRITICAL(&timerMux);
    }

    lightState_t currentState = leds.state();
    if ( currentState != LIGHTS_AUTO ) {
      uint16_t pwmValue = ( currentState == LIGHTS_OFF ) ? ledcMaxValue : 0;
      float percentage = ( pwmValue == 0 ) ? 100.0f : 0.0f;
      for ( uint8_t num = 0; num < NUMBER_OF_CHANNELS; num++ ) {
        channel[num].currentPercentage = percentage;
        ledcWrite( num, pwmValue );
      }
      while ( leds.state() == currentState ) delay( dimmerTaskdelayTime );
    }
    else {
      struct timeval microSecondTime;
      gettimeofday( &microSecondTime, NULL );

      struct tm localTime;
      localtime_r( &microSecondTime.tv_sec, &localTime );

      suseconds_t milliSecondsToday = ( localTime.tm_hour       * 3600000U ) +
                                      ( localTime.tm_min        * 60000U ) +
                                      ( localTime.tm_sec        * 1000U ) +
                                      ( microSecondTime.tv_usec / 1000U );

      if ( milliSecondsToday ) { /* to solve flashing at 00:00:000 due to the fact that the first timer has no predecessor */
        for ( uint8_t num = 0; num < NUMBER_OF_CHANNELS; num++ ) {
          uint8_t thisTimer = 0;

          while ( channel[num].timer[thisTimer].time * 1000U < milliSecondsToday )
            thisTimer++;

          float newPercentage;
          /* only do a lot of float math if really neccesary */
          if ( channel[num].timer[thisTimer].percentage != channel[num].timer[thisTimer - 1].percentage ) {
            newPercentage = mapFloat( milliSecondsToday,
                                      channel[num].timer[thisTimer - 1].time * 1000U,
                                      channel[num].timer[thisTimer].time * 1000U,
                                      channel[num].timer[thisTimer - 1].percentage,
                                      channel[num].timer[thisTimer].percentage );
          }
          else {
            /* timers are equal so no math neccesary */
            newPercentage = channel[num].timer[thisTimer].percentage;
          }

          /* calculate moon light */
          if ( newPercentage < ( channel[num].fullMoonLevel * moonData.percentLit ) )
            newPercentage = channel[num].fullMoonLevel * moonData.percentLit;

          /* done, set the channel */
          channel[num].currentPercentage = newPercentage;
          ledcWrite( num, invertFloat(mapFloat( channel[num].currentPercentage,
                                    0.0f,
                                    100.0f,
                                    0.0f,
                                    ledcMaxValue ), 0.0f, ledcMaxValue) );
        }
      }
    }
    vTaskDelayUntil( &xLastWakeTime, dimmerTaskdelayTime );
  }
}

bool defaultTimersLoaded() {
  //find 'default.aqu' on selected storage and if present load the timerdata from this file
  //return true on success
  //return false on error
  if ( !FFat.exists( defaultTimerFile ) ) {
    return false;
  }

  File f = FFat.open( defaultTimerFile, "r" );
  if ( !f.available() ) {
    ESP_LOGI( TAG, "Error opening default timer file. [%s]", defaultTimerFile );
    return false;
  }
  byte currentTimer = 0;
  uint8_t chan;
  //String data;
  while ( f.position() < f.size() ) {
    String data = f.readStringUntil( '\n' );
    if ( 0 == data.indexOf( "[" ) ) {
      chan = data.substring( 1, 3 ).toInt();
      currentTimer = 0;
    }
    else if ( currentTimer < MAX_TIMERS - 1 ) {
      channel[chan].timer[currentTimer].time = data.substring( 0, data.indexOf(",") ).toInt();
      channel[chan].timer[currentTimer].percentage = data.substring( data.indexOf(",") + 1 ).toInt();
      currentTimer++;
      channel[chan].numberOfTimers = currentTimer;
    }

  }
  f.close();
  //add the 24:00 timers ( copy of timer percentage no: 0 )
  for (chan = 0; chan < NUMBER_OF_CHANNELS; chan++ ) {
    channel[chan].timer[channel[chan].numberOfTimers].time = 86400;
    channel[chan].timer[channel[chan].numberOfTimers].percentage = channel[chan].timer[0].percentage;
    currentTimer++;
    channel[chan].numberOfTimers = currentTimer;
  }
  return true;
}

void setEmptyTimers() {
  for ( uint8_t num = 0; num < NUMBER_OF_CHANNELS; num++) {
    channel[num].timer[0] = {0, 0};
    channel[num].timer[1] = {86400, 0};
    channel[num].numberOfTimers = 1;
  }
}

void setupDimmerPWMfrequency( const double frequency, const uint8_t numberOfBits ) {
  /* Setup timers and pwm bit depth */
  for ( uint8_t num = 0; num < NUMBER_OF_CHANNELS; num++ ) {
    ledcActualFrequency = ledcSetup( num, frequency, numberOfBits );
  }
  ledcMaxValue = ( 0x00000001 << numberOfBits ) - 1;
  ledcNumberOfBits = numberOfBits;
  ESP_LOGI( TAG, "PWM frequency set to %.2f kHz.", ledcActualFrequency / 1000);
  ESP_LOGI( TAG, "PWM bit depth set to %i bits.", ledcNumberOfBits);
  ESP_LOGI( TAG, "Maximum raw value set to 0x%x or %i decimal.", ledcMaxValue, ledcMaxValue);
}
