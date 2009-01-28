// $Id$
/*
 * Trigger selection page (entered with Trigger Layer C button)
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
#include "seq_lcd.h"
#include "seq_ui.h"

#include "seq_trg.h"
#include "seq_cc.h"


/////////////////////////////////////////////////////////////////////////////
// Local LED handler function
/////////////////////////////////////////////////////////////////////////////
static s32 LED_Handler(u16 *gp_leds)
{
  if( ui_cursor_flash ) // if flashing flag active: no LED flag set
    return 0;

  u8 visible_track = SEQ_UI_VisibleTrackGet();
  u8 event_mode = SEQ_CC_Get(visible_track, SEQ_CC_MIDI_EVENT_MODE);
  int num_layers = SEQ_TRG_NumLayersGet(visible_track);
  u8 selected_trg_layer = ui_selected_trg_layer;
  u8 accent_available = SEQ_TRG_AssignmentGet(visible_track, 1);

  if( event_mode == SEQ_EVENT_MODE_Drum && accent_available ) {
    selected_trg_layer %= (num_layers / 2);
  }

  *gp_leds = 1 << selected_trg_layer;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Local encoder callback function
// Should return:
//   1 if value has been changed
//   0 if value hasn't been changed
//  -1 if invalid or unsupported encoder
/////////////////////////////////////////////////////////////////////////////
static s32 Encoder_Handler(seq_ui_encoder_t encoder, s32 incrementer)
{
  u8 visible_track = SEQ_UI_VisibleTrackGet();
  u8 event_mode = SEQ_CC_Get(visible_track, SEQ_CC_MIDI_EVENT_MODE);
  int num_layers = SEQ_TRG_NumLayersGet(visible_track);
  u8 accent_available = SEQ_TRG_AssignmentGet(visible_track, 1);

  if( event_mode == SEQ_EVENT_MODE_Drum && accent_available ) {
    num_layers /= 2;
  }

  if( encoder < num_layers ) {
    // select new layer
    ui_selected_trg_layer = encoder;

#if DEFAULT_BEHAVIOUR_BUTTON_STEPVIEW
    // if toggle function active: jump back to previous menu
    // this is especially useful for the emulated MBSEQ, where we can only click on a single button
    // (trigger gets deactivated when clicking on GP button or moving encoder)
    if( seq_ui_button_state.TRG_LAYER_SEL ) {
      seq_ui_button_state.TRG_LAYER_SEL = 0;
      SEQ_UI_PageSet(ui_trglayer_prev_page);
    }
#endif

    return 1; // value changed
  } else if( encoder == SEQ_UI_ENCODER_Datawheel ) {
    return SEQ_UI_Var8_Inc(&ui_selected_step_view, 0, num_layers-1, incrementer);
  }

  return -1; // invalid or unsupported encoder
}


/////////////////////////////////////////////////////////////////////////////
// Local button callback function
// Should return:
//   1 if value has been changed
//   0 if value hasn't been changed
//  -1 if invalid or unsupported button
/////////////////////////////////////////////////////////////////////////////
static s32 Button_Handler(seq_ui_button_t button, s32 depressed)
{
  if( depressed ) return 0; // ignore when button depressed

#if 0
  // leads to: comparison is always true due to limited range of data type
  if( button >= SEQ_UI_BUTTON_GP1 && button <= SEQ_UI_BUTTON_GP16 ) {
#else
  if( button <= SEQ_UI_BUTTON_GP16 ) {
#endif
    // -> same handling like for encoders
    return Encoder_Handler(button, 0);
  }

  switch( button ) {
    case SEQ_UI_BUTTON_Select:
      return -1; // unsupported (yet)

    case SEQ_UI_BUTTON_Right:
    case SEQ_UI_BUTTON_Up:
      if( depressed ) return 0; // ignore when button depressed
      return Encoder_Handler(SEQ_UI_ENCODER_Datawheel, 1);

    case SEQ_UI_BUTTON_Left:
    case SEQ_UI_BUTTON_Down:
      if( depressed ) return 0; // ignore when button depressed
      return Encoder_Handler(SEQ_UI_ENCODER_Datawheel, -1);
  }

  return -1; // invalid or unsupported button
}


/////////////////////////////////////////////////////////////////////////////
// Local Display Handler function
// IN: <high_prio>: if set, a high-priority LCD update is requested
/////////////////////////////////////////////////////////////////////////////
static s32 LCD_Handler(u8 high_prio)
{
  if( high_prio )
    return 0; // there are no high-priority updates

  // layout normal mode:
  // 00000000001111111111222222222233333333330000000000111111111122222222223333333333
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
  // <--------------------------------------><-------------------------------------->
  // Gate Acc. Roll Glide Skip R.G  R.V No Fx                                        
  //   A    B    C    D    E    F    G    H                                          

  // layout drum mode (lower line shows drum labels):
  // 00000000001111111111222222222233333333330000000000111111111122222222223333333333
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
  // <--------------------------------------><-------------------------------------->
  // Select Drum Instrument:                                                         
  //  BD   SD   LT   MT   HT   CP   MA   RS   CB   CY   OH   CH  Smp1 Smp2 Smp3 Smp4 


  u8 visible_track = SEQ_UI_VisibleTrackGet();
  u8 event_mode = SEQ_CC_Get(visible_track, SEQ_CC_MIDI_EVENT_MODE);
  int num_layers = SEQ_TRG_NumLayersGet(visible_track);

  if( event_mode == SEQ_EVENT_MODE_Drum ) {
    u8 accent_available = SEQ_TRG_AssignmentGet(visible_track, 1);
    if( accent_available )
      num_layers /= 2;

    ///////////////////////////////////////////////////////////////////////////
    SEQ_LCD_CursorSet(0, 0);
    SEQ_LCD_PrintString("Select Drum Instrument:");
    SEQ_LCD_PrintSpaces(17 + 40);

    ///////////////////////////////////////////////////////////////////////////
    SEQ_LCD_CursorSet(0, 1);

    int i;
    for(i=0; i<num_layers; ++i)
      SEQ_LCD_PrintTrackDrum(visible_track, i, (char *)seq_core_trk[visible_track].name);

    SEQ_LCD_PrintSpaces(80 - (5*num_layers));
  } else {

    int i;
    for(i=0; i<num_layers; ++i)
      SEQ_LCD_PrintString(SEQ_TRG_TypeStr(i));

    //	  u8 value = SEQ_CC_Get(visible_track, SEQ_CC_ASG_GATE+i);
 
    SEQ_LCD_PrintSpaces(80 - (5*num_layers));

    ///////////////////////////////////////////////////////////////////////////
    SEQ_LCD_CursorSet(0, 1);

    for(i=0; i<num_layers; ++i) {
      SEQ_LCD_PrintChar(' ');
      SEQ_LCD_PrintChar((i == ui_selected_trg_layer) ? '>' : ' ');

      if( i == ui_selected_trg_layer && ui_cursor_flash )
	SEQ_LCD_PrintChar(' ');
      else {
	SEQ_LCD_PrintChar('A' + i);
      }

      SEQ_LCD_PrintChar((i == ui_selected_trg_layer) ? '<' : ' ');
      SEQ_LCD_PrintChar(' ');
    }

    SEQ_LCD_PrintSpaces(80 - (5*num_layers));
  }


  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_UI_TRGSEL_Init(u32 mode)
{
  // install callback routines
  SEQ_UI_InstallButtonCallback(Button_Handler);
  SEQ_UI_InstallEncoderCallback(Encoder_Handler);
  SEQ_UI_InstallLEDCallback(LED_Handler);
  SEQ_UI_InstallLCDCallback(LCD_Handler);

  return 0; // no error
}