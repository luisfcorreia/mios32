// $Id$
/*
 * Checks the handling of a button/LED matrix
 *
 * ==========================================================================
 *
 *  Copyright (C) 2008 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>
#include "app.h"

#include <blm.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define PRIORITY_TASK_BLM_CHECK		( tskIDLE_PRIORITY + 2 )

#define BLM_MIDI_STARTNOTE 24


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////
static void TASK_BLM_Check(void *pvParameters);


/////////////////////////////////////////////////////////////////////////////
// Global Variables
/////////////////////////////////////////////////////////////////////////////

u8 last_din_pin = 0;
u8 last_din_value = 1;
u8 last_dout_pin = 0;
u8 last_dout_value = 1;


/////////////////////////////////////////////////////////////////////////////
// This hook is called after startup to initialize the application
/////////////////////////////////////////////////////////////////////////////
void APP_Init(void)
{
  s32 i;

  // initialize all LEDs
  MIOS32_BOARD_LED_Init(0xffffffff);

  // initialize BLM driver
  BLM_Init(0);
  BLM_DebounceDelaySet(10);

  // start BLM check task
  xTaskCreate(TASK_BLM_Check, (signed portCHAR *)"BLM_Check", configMINIMAL_STACK_SIZE, NULL, PRIORITY_TASK_BLM_CHECK, NULL);
}


/////////////////////////////////////////////////////////////////////////////
// This task is running endless in background
/////////////////////////////////////////////////////////////////////////////
void APP_Background(void)
{
  // init LCD
  MIOS32_LCD_Clear();

  // endless loop: print status information on LCD
  while( 1 ) {
    // toggle the state of all LEDs (allows to measure the execution speed with a scope)
    MIOS32_BOARD_LED_Set(0xffffffff, ~MIOS32_BOARD_LED_Get());
    
    // print text on LCD screen
    MIOS32_LCD_CursorSet(0, 0);
    printf("DIN  Pin #%3d %c", last_din_pin, last_din_value ? 'o' : '*');

    // print text on LCD screen
    MIOS32_LCD_CursorSet(0, 1);
    printf("DOUT Pin #%3d %c", last_dout_pin, last_dout_value ? 'o' : '*');
  }
}


/////////////////////////////////////////////////////////////////////////////
//  This hook is called when a complete MIDI event has been received
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedEvent(mios32_midi_port_t port, mios32_midi_package_t midi_package)
{
  unsigned char pin, value;

  MIOS32_MIDI_SendPackage(port, midi_package);

  pin = (midi_package.note - BLM_MIDI_STARTNOTE) & 0x7f;
  if( midi_package.chn == Chn1 && ((midi_package.event == NoteOff) || (midi_package.event == NoteOn)) && (pin < 0x40 ) )
  {
    // 90 xx 00 is the same like a note off event!
    // (-> http://www.borg.com/~jglatt/tech/midispec.htm)
    value = (midi_package.event == NoteOff) || (midi_package.velocity == 0x00);

    // store last pin number and value
    last_dout_pin = pin;
    last_dout_value = value;

    // set LEDs
#if BLM_NUM_COLOURS >= 1
    BLM_DOUT_PinSet(0, pin, value ? 0 : 1); // red, pin, value
#endif
#if BLM_NUM_COLOURS >= 2
    BLM_DOUT_PinSet(1, pin, value ? 0 : 1); // green, pin, value
#endif
#if BLM_NUM_COLOURS >= 3
    BLM_DOUT_PinSet(2, pin, value ? 0 : 1); // blue, pin, value
#endif	
  }
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a SysEx byte has been received
/////////////////////////////////////////////////////////////////////////////
void APP_NotifyReceivedSysEx(mios32_midi_port_t port, u8 sysex_byte)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called before the shift register chain is scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServicePrepare(void)
{
  // prepare DOUT registers to drive the column
  BLM_PrepareCol();
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called after the shift register chain has been scanned
/////////////////////////////////////////////////////////////////////////////
void APP_SRIO_ServiceFinish(void)
{
  // call the BLM_GetRow function after scan is finished to capture the read DIN values
  BLM_GetRow();
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when a button has been toggled
// pin_value is 1 when button released, and 0 when button pressed
/////////////////////////////////////////////////////////////////////////////
void APP_DIN_NotifyToggle(u32 pin, u32 pin_value)
{
}


/////////////////////////////////////////////////////////////////////////////
// This hook is called when an encoder has been moved
// incrementer is positive when encoder has been turned clockwise, else
// it is negative
/////////////////////////////////////////////////////////////////////////////
void APP_ENC_NotifyChange(u32 encoder, s32 incrementer)
{
}


/////////////////////////////////////////////////////////////////////////////
// This task is called each mS to check the BLM button states
/////////////////////////////////////////////////////////////////////////////

// will be called on BLM pin changes (see TASK_BLM_Check)
void DIN_BLM_NotifyToggle(u32 pin, u32 pin_value)
{
  // remember pin and value
  last_din_pin = pin;
  last_din_value = pin_value;

  // map pin and value, invert DIN value (so that LED lit when button pressed)
#if BLM_NUM_COLOURS >= 1
  BLM_DOUT_PinSet(0, pin, pin_value ? 0 : 1); // red, pin, value
#endif
#if BLM_NUM_COLOURS >= 2
  BLM_DOUT_PinSet(1, pin, pin_value ? 0 : 1); // green, pin, value
#endif
#if BLM_NUM_COLOURS >= 3
  BLM_DOUT_PinSet(2, pin, pin_value ? 0 : 1); // blue, pin, value
#endif

  // send MIDI event
  MIOS32_MIDI_SendNoteOn(USB0, Chn1, (pin + BLM_MIDI_STARTNOTE) & 0x7f, pin_value ? 0x00 : 0x7f);
}

static void TASK_BLM_Check(void *pvParameters)
{
  portTickType xLastExecutionTime;

  // Initialise the xLastExecutionTime variable on task entry
  xLastExecutionTime = xTaskGetTickCount();

  while( 1 ) {
    vTaskDelayUntil(&xLastExecutionTime, 1 / portTICK_RATE_MS);

    // check for BLM pin changes, call DIN_BLM_NotifyToggle on each toggled pin
    BLM_ButtonHandler(DIN_BLM_NotifyToggle);
  }
}