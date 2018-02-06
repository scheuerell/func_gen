#include <IOShieldOled.h>
#include <EEPROM.h>
//Serial.println();
//**************************************************************************************
// type definitions START
typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned long   uint32;
typedef signed char     sint8;
typedef short           sint16;
typedef long            sint32;
typedef float           float32;
typedef double          float64;
// type definitions END
//**************************************************************************************

//**************************************************************************************
// macro definitions START
#define TRUE          1
#define FALSE         0
#define UP            1
#define DOWN          0
#define pin_led       26
#define pin_J7_2      5
#define pin_SW1       2
#define pin_SW2       7
#define pin_SW3       8
#define pin_SW4       35
#define pin_BTN1      4
#define pin_BTN2      34
#define pin_BTN3      36
#define pin_BTN4      37
#define ON_TIME   0
#define OFF_TIME  1
#define ON_MIN    2
#define OFF_MIN   3
#define ON_MAX    4
#define OFF_MAX   5
#define ON_RATE   6
#define OFF_RATE  7
#define DUTY      8
#define NVM_CRC_OFFSET      0
#define NVM_CONFIG_OFFSET   4
#define NVM_CONFIG_SIZE     sizeof(config_nvm_autosave)
#define NVM_CONFIG_ADDRESS  &config_nvm_autosave
#define toggle_output(pin_id)                   \
  if(digitalRead(pin_id) == HIGH)               \
  {                                             \
    digitalWrite(pin_id, LOW);                  \
  }                                             \
  else                                          \
  {                                             \
    digitalWrite(pin_id, HIGH);                 \
  }                                             \
  dwell_on.time_base_dwell = time_in_ms;        \
  dwell_off.time_base_dwell = time_in_ms;

// macro definitions END
//**************************************************************************************

//**************************************************************************************
// struct definitions START
struct dwell_config
{
  sint16  dwell_time;
  sint16  dwell_min;
  sint16  dwell_max;
  sint16  dwell_sweep_rate_x1000;
};
struct dwell_control
{
  dwell_config*  config_ptr;
  sint16  dwell_sweep_rate_x1000;
  uint16  time_base_dwell;
  uint16  time_base_dwell_sweep;
  uint16  time_base_incdec_control;
};
// all nvm-backed storage must be allocated in this structure
struct config_nvm_autosave
{
  dwell_config  on_config;
  dwell_config  off_config; 
} config_nvm_autosave;

// struct definitions END
//**************************************************************************************

//**************************************************************************************
// module-level symbols START
  static struct dwell_control dwell_on;
  static struct dwell_control dwell_off;   
// module-level symbols END
//**************************************************************************************

//**************************************************************************************
// setup function START
void setup()
{
  // initialize the display: power up, clear buffer, and turn off auto-update
  IOShieldOled.begin();
  IOShieldOled.clearBuffer();
  IOShieldOled.setCharUpdate(0);
  // initialize gpio
  pinMode(pin_led, OUTPUT);
  pinMode(pin_J7_2, OUTPUT);
  pinMode(pin_SW1, INPUT);
  pinMode(pin_SW2, INPUT);
  pinMode(pin_SW3, INPUT);
  pinMode(pin_SW4, INPUT);
  pinMode(pin_BTN1, INPUT);
  pinMode(pin_BTN2, INPUT);
  pinMode(pin_BTN3, INPUT);
  pinMode(pin_BTN4, INPUT);
  // initialize uart
  Serial.begin(9600);
  // initialize variables and load defaults
  //example code: memset(&dwell_on, 0, sizeof(dwell_on));
  //example code: memset(&dwell_off, 0, sizeof(dwell_off));
  dwell_on.config_ptr = &config_nvm_autosave.on_config;
  dwell_off.config_ptr = &config_nvm_autosave.off_config;
  dwell_on.config_ptr = &config_nvm_autosave.on_config;
  dwell_off.config_ptr = &config_nvm_autosave.off_config;
  dwell_on.config_ptr->dwell_max = 1000;
  dwell_off.config_ptr->dwell_max = 1000;
  dwell_on.config_ptr->dwell_sweep_rate_x1000 = 10;
  dwell_off.config_ptr->dwell_sweep_rate_x1000 = -10;
  // initialize non-volatile data
  nvm_config_init();
}
// setup function END
//**************************************************************************************
//**************************************************************************************
// loop function START

void loop()
{
  // constants

  // automatics
  uint16 time_in_ms;
  // statics
  static sint16 config_index = 0;
  static uint16 time_base_inc_dec = 0;
  static uint16 time_base_inc_dec_rate = 0;

  // get time
  time_in_ms = millis();

  // process dwell time reset button
  if (digitalRead(pin_BTN4) == HIGH)
  {
    dwell_on.config_ptr->dwell_time = dwell_on.config_ptr->dwell_min;
    dwell_off.config_ptr->dwell_time = dwell_off.config_ptr->dwell_min;
  }

  // process config save: save if menu button is selected and sweep is paused
  if (digitalRead(pin_BTN3) == HIGH && digitalRead(pin_SW4) == HIGH && digitalRead(pin_SW3) == HIGH )
  {
    nvm_config_save();    
  }

  // process menu buttons
  if ( pulse_every_200ms_while_active( &time_base_inc_dec, time_in_ms, digitalRead(pin_BTN3)))
  {
    // increment config_index 
    factored_adjust_and_roll( &config_index, 1, ON_TIME-1, OFF_RATE );
  }
  // inc/dec factor increases in magnitude while button is active
  sint32 factor = (uint16)(time_in_ms - time_base_inc_dec_rate) / 200 + 1;
  if ( pulse_every_200ms_while_active( &time_base_inc_dec, time_in_ms, digitalRead(pin_BTN2)))
  {
    // increment config parameter
    adjust_config(config_index, &dwell_on, &dwell_off, factor);
  }
  if ( pulse_every_200ms_while_active( &time_base_inc_dec, time_in_ms, digitalRead(pin_BTN1)))
  {
    // deccrement config parameter
    adjust_config(config_index, &dwell_on, &dwell_off, -factor);
  }

  if ( !digitalRead(pin_BTN2) && !digitalRead(pin_BTN1) )
  {
    // reset inc/dec rate timer
    time_base_inc_dec_rate = time_in_ms;
  }


  // process sweep of dwell time
  process_dwell_sweep(&dwell_on, time_in_ms, pin_SW4 );
  process_dwell_sweep(&dwell_off, time_in_ms, pin_SW3 );

  // update output state
  if (digitalRead(pin_led) == HIGH && (uint16)(time_in_ms - dwell_on.time_base_dwell) >= dwell_on.config_ptr->dwell_time || 
      digitalRead(pin_led) == LOW && (uint16)(time_in_ms - dwell_off.time_base_dwell) >= dwell_off.config_ptr->dwell_time )
  {
    toggle_output(pin_led)
    toggle_output(pin_J7_2)
  }

  // update display
  set_display(&dwell_on, &dwell_off, config_index );
}
// loop function END
//**************************************************************************************

//**************************************************************************************
// function definitions START

////////////////////////////////////////////////////////////////////////////////////////
uint32 nvm_crc(uint32 start_index, uint32 data_size) 
{
   uint32 crc = ~0L;
   for (uint32 index = start_index; index < start_index + data_size; ++index) 
   {
      crc = crc_calc( crc, EEPROM.read(index));
   }
   return crc;
}
////////////////////////////////////////////////////////////////////////////////////////
uint32 ram_crc(void *start_address, uint32 data_size) 
{
  uint32 crc = ~0L;
  uint8 *byte_ptr;
  byte_ptr = (uint8*)(start_address);
  for (uint32 i = 0; i < data_size; i++)
  {
    crc = crc_calc( crc, *(byte_ptr + i));
  }
  return crc;
}
////////////////////////////////////////////////////////////////////////////////////////
uint32 crc_calc(uint32 crc, uint8 data_in) 
{
   const uint32 crc_table[16] = 
   {
     0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
     0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
     0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
     0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
   };
     crc = crc_table[(crc ^ data_in) & 0x0f] ^ (crc >> 4);
     crc = crc_table[(crc ^ (data_in >> 4)) & 0x0f] ^ (crc >> 4);
     crc = ~crc;
   return crc;
}
////////////////////////////////////////////////////////////////////////////////////////
void nvm_config_init(void)
{
    uint32 temp_config_crc = 0;
    nvm_load(&temp_config_crc, sizeof(temp_config_crc), NVM_CRC_OFFSET);
    if ( nvm_crc(NVM_CONFIG_OFFSET, NVM_CONFIG_SIZE) != temp_config_crc) 
    {
      // save defaults to nvm if the calculated ram crc doesnt match the nvm stored crc
      nvm_save(NVM_CONFIG_ADDRESS, NVM_CONFIG_SIZE, NVM_CONFIG_OFFSET);
      temp_config_crc = ram_crc(NVM_CONFIG_ADDRESS, NVM_CONFIG_SIZE);
      nvm_save(&temp_config_crc, sizeof(temp_config_crc), NVM_CRC_OFFSET);
    }
    else
    {
      // load config from nvm
      nvm_load(NVM_CONFIG_ADDRESS, NVM_CONFIG_SIZE, NVM_CONFIG_OFFSET);
    }
}
////////////////////////////////////////////////////////////////////////////////////////
void nvm_config_save(void)
{
    uint32 temp_config_crc = 0;
    nvm_load(&temp_config_crc, sizeof(temp_config_crc), NVM_CRC_OFFSET);
    if ( nvm_crc(NVM_CONFIG_OFFSET, NVM_CONFIG_SIZE) != temp_config_crc || ram_crc(NVM_CONFIG_ADDRESS, NVM_CONFIG_SIZE) != temp_config_crc ) 
    {
      // save defaults to nvm if the calculated ram crc doesnt match the nvm stored crc
      nvm_save(NVM_CONFIG_ADDRESS, NVM_CONFIG_SIZE, NVM_CONFIG_OFFSET);
      temp_config_crc = ram_crc(NVM_CONFIG_ADDRESS, NVM_CONFIG_SIZE);
      nvm_save(&temp_config_crc, sizeof(temp_config_crc), NVM_CRC_OFFSET);
      Serial.println("NVM CONFIG SAVED");
    }
}
////////////////////////////////////////////////////////////////////////////////////////
void nvm_save(void* data_ptr, uint32 data_size, sint16 nvm_address_offset)
{
  for (uint8 i = 0; i < data_size; i++)
  { 
    EEPROM.write(nvm_address_offset + i, *((uint8*)(data_ptr) + i)); 
  }
}
////////////////////////////////////////////////////////////////////////////////////////
void nvm_load(void * data_ptr, uint32 data_size, sint16 nvm_address_offset)
{
  for (uint8 i = 0; i < data_size; i++)
  { 
    *((uint8*)(data_ptr) + i) = EEPROM.read(nvm_address_offset + i); 
  }
}
////////////////////////////////////////////////////////////////////////////////////////
boolean pulse_every_200ms_while_active( uint16 *time_base, uint16 time_in_ms, boolean boolean_state )
{
  if ( boolean_state == HIGH && (uint16)(time_in_ms - *time_base) > 200 )
  {
    // reset timer
    *time_base = time_in_ms;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}
////////////////////////////////////////////////////////////////////////////////////////
void process_dwell_sweep(dwell_control * dwell_control, uint16 time_in_ms, uint8 pause_pin )
{
  sint16 factor = (sint16)(((uint16)(time_in_ms - dwell_control->time_base_dwell_sweep)) * dwell_control->config_ptr->dwell_sweep_rate_x1000) / 1000;
  if ( factor != 0)
  {
    // reset timer
    dwell_control->time_base_dwell_sweep = time_in_ms;
    // update DWELL_ON if not paused
    if (digitalRead(pause_pin) == LOW)
    {
      factored_adjust_and_roll( &dwell_control->config_ptr->dwell_time, factor, dwell_control->config_ptr->dwell_min, dwell_control->config_ptr->dwell_max );
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////
void factored_adjust_and_roll( sint16 * parameter, sint16 factor, sint16 parameter_min, sint16 parameter_max )
{
  // roll under at lower limit
  if (factor > 0)
  {
    if (*parameter > parameter_max - factor)
    {
      // roll over the remainder
      *parameter = parameter_min + (factor - ( parameter_max - *parameter));
    }
    else
    {
      *parameter = *parameter + factor;
    }
  }
  if (factor < 0)
  {
    if ( *parameter < parameter_min - factor )
    {
      // roll under the remainder
      *parameter = parameter_max + (factor - ( parameter_min - *parameter));
    }
    else
    {
      *parameter = *parameter + factor;
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////
void set_display( dwell_control * dwell_control_on, dwell_control * dwell_control_off, sint16 config_index )
{
#define NUM_ROWS    3
#define NUM_PARAMS  8
  //  const uint8 config_array[NUM_PARAMS] = {0, 4, 1, 5, 2, 6, 3, 7};
  uint8 i = 0;
  sint16 config_param_index = config_index;

  IOShieldOled.clearBuffer();
  for (i = 0; i < NUM_ROWS; i++)
  {
    display_buffer_row_write(i, config_param_index, dwell_control_on, dwell_control_off);
    factored_adjust_and_roll( &config_param_index, 1, ON_TIME-1, OFF_RATE );
  }
  // bottom row is always duty cycle
  display_buffer_row_write(3, DUTY, dwell_control_on, dwell_control_off);
  IOShieldOled.updateDisplay();
}
////////////////////////////////////////////////////////////////////////////////////////
void display_buffer_row_write(uint8 row_index, sint16 config_param_index, dwell_control * dwell_control_on, dwell_control * dwell_control_off)
{
  char *string_1;
  char string_2[10];
  char *string_3;
  uint8 duty_pct;

  switch (config_param_index)
  {
    case ON_TIME:
      string_1 = "ON TIME";
      //sprintf(string_1, "%s", "ON TIME");
      sprintf(string_2, "%i", dwell_control_on->config_ptr->dwell_time);
      string_3 = "ms";
      break;
    case OFF_TIME:
      string_1 = "OFF TIME";
      sprintf(string_2, "%i", dwell_control_off->config_ptr->dwell_time);
      string_3 = "ms";
      break;
    case ON_MIN:
      string_1 = "ON MIN";
      sprintf(string_2, "%i", dwell_control_on->config_ptr->dwell_min);
      string_3 = "ms";
      break;
    case OFF_MIN:
      string_1 = "OFF MIN";
      sprintf(string_2, "%i", dwell_control_off->config_ptr->dwell_min);
      string_3 = "ms";
      break;
    case ON_MAX:
      string_1 = "ON MAX";
      sprintf(string_2, "%i", dwell_control_on->config_ptr->dwell_max);
      string_3 = "ms";
      break;
    case OFF_MAX:
      string_1 = "OFF MAX";
      sprintf(string_2, "%i", dwell_control_off->config_ptr->dwell_max);
      string_3 = "ms";
      break;
    case ON_RATE:
      string_1 = "ON RATE";
      sprintf(string_2, "%4.3f", (float32)(dwell_control_on->config_ptr->dwell_sweep_rate_x1000) / 1000 );
      string_3 = "";
      break;
    case OFF_RATE:
      string_1 = "OFF RATE";
      sprintf(string_2, "%4.3f", (float32)(dwell_control_off->config_ptr->dwell_sweep_rate_x1000) / 1000 );
      string_3 = "";
      break;
    case DUTY:
      string_1 = "  DUTY";
      // update duty cycle reading
      if (dwell_control_on->config_ptr->dwell_time + dwell_control_off->config_ptr->dwell_time > 0)
      {
        duty_pct = (dwell_control_on->config_ptr->dwell_time * 100) / (dwell_control_on->config_ptr->dwell_time + dwell_control_off->config_ptr->dwell_time);
      }
      else
      {
        duty_pct = 0;
      }
      sprintf(string_2, "%u", duty_pct);
      string_3 = "%";
      break;
  }
  IOShieldOled.setCursor(0, row_index);
  IOShieldOled.putString(string_1);
  IOShieldOled.setCursor(9, row_index);
  IOShieldOled.putString(string_2);
  IOShieldOled.setCursor(13, row_index);
  IOShieldOled.putString(string_3);
}
////////////////////////////////////////////////////////////////////////////////////////
void adjust_config(sint16 config_param_index, dwell_control * dwell_control_on, dwell_control * dwell_control_off, sint16 factor)
{
  switch (config_param_index)
  {
    case ON_TIME:
      factored_adjust_and_roll( &dwell_control_on->config_ptr->dwell_time, factor, dwell_control_on->config_ptr->dwell_min, dwell_control_on->config_ptr->dwell_max );
      break;
    case OFF_TIME:
      factored_adjust_and_roll( &dwell_control_off->config_ptr->dwell_time, factor, dwell_control_off->config_ptr->dwell_min, dwell_control_off->config_ptr->dwell_max );
      break;
    case ON_MIN:
      factored_adjust_and_roll( &dwell_control_on->config_ptr->dwell_min, factor, 0, 5000 );
      break;
    case OFF_MIN:
      factored_adjust_and_roll( &dwell_control_off->config_ptr->dwell_min, factor, 0, 5000 );
      break;
    case ON_MAX:
      factored_adjust_and_roll( &dwell_control_on->config_ptr->dwell_max, factor, 0, 5000 );
      break;
    case OFF_MAX:
      factored_adjust_and_roll( &dwell_control_off->config_ptr->dwell_max, factor, 0, 5000 );
      break;
    case ON_RATE:
      factored_adjust_and_roll( &dwell_control_on->config_ptr->dwell_sweep_rate_x1000, factor,  -10000, 10000 );
      break;
    case OFF_RATE:
      factored_adjust_and_roll( &dwell_control_off->config_ptr->dwell_sweep_rate_x1000, factor, -10000, 10000 );
      break;
    default:
      break;
  }
}
// function definitions END
//**************************************************************************************
