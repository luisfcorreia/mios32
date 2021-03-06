// $Id: seq_ui_trkdiv.c 2145 2015-03-15 19:40:30Z tk $
/*
 * Track clock divider page
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
#include "seq_cc.h"
#include <glcd_font.h>

/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#define NUM_OF_ITEMS       6
#define ITEM_GXTY          0
#define ITEM_DIVIDER       1
#define ITEM_TRIPLET       2
#define ITEM_SYNCH_TO_MEASURE 3
#define ITEM_MANUAL        4
#define ITEM_Q_SELECT      5

/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

static const char quicksel_str[8][6] =  { "  1  ", "  2  ", "  4  ", "  8  ", "  8T ", "  16 ", "  16T", "  32 " };
static const u16 quicksel_timebase[8] = {  255,      127,     63,      31,   31|0x100,    15,   15|0x100,    7   };


/////////////////////////////////////////////////////////////////////////////
// Search for item in quick selection list
/////////////////////////////////////////////////////////////////////////////
static s32 QUICKSEL_SearchItem(u8 track)
{
  u16 search_pattern = SEQ_CC_Get(track, SEQ_CC_CLK_DIVIDER) | 
                    ((SEQ_CC_Get(track, SEQ_CC_CLKDIV_FLAGS)&2)<<7);
  int i;
  for(i=0; i<8; ++i)
    if( quicksel_timebase[i] == search_pattern )
      return i;

  return -1; // item not found
}


/////////////////////////////////////////////////////////////////////////////
// Local LED handler function
/////////////////////////////////////////////////////////////////////////////
static s32 LED_Handler(u16 *gp_leds)
{
  if( ui_cursor_flash ) // if flashing flag active: no LED flag set
    return 0;
/*
  switch( ui_selected_item ) {
    case ITEM_GXTY: *gp_leds = 0x0001; break;
    case ITEM_DIVIDER: *gp_leds = 0x0002; break;
    case ITEM_TRIPLET: *gp_leds = 0x000c; break;
    case ITEM_SYNCH_TO_MEASURE: *gp_leds = 0x0070; break;
    case ITEM_MANUAL:  *gp_leds = 0x0080; break;
  }

  {
    u8 visible_track = SEQ_UI_VisibleTrackGet();
    u8 manual_clock = SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<2);

    if( !manual_clock ) {
      s32 quicksel_item = QUICKSEL_SearchItem(SEQ_UI_VisibleTrackGet());
   
	if( quicksel_item >= 0 )
	*gp_leds |= 1 << (quicksel_item+8);
	
    }
  }
*/
  *gp_leds = 0x0000;
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
  u8 manual_clock = SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<2);

  switch( encoder ) {
    case SEQ_UI_ENCODER_GP1:
	if( SEQ_UI_Var8_Inc(&ui_selected_item, 0, NUM_OF_ITEMS-1, incrementer) >= 0 )
		return 1;
	else
		return 0; 	
      //ui_selected_item = ITEM_GXTY;
      break;

    case SEQ_UI_ENCODER_GP2:
      ui_selected_item = ITEM_DIVIDER;
      break;

    case SEQ_UI_ENCODER_GP3:
    case SEQ_UI_ENCODER_GP4:
      ui_selected_item = ITEM_TRIPLET;
      break;

    case SEQ_UI_ENCODER_GP5:
    case SEQ_UI_ENCODER_GP6:
    case SEQ_UI_ENCODER_GP7:
      ui_selected_item = ITEM_SYNCH_TO_MEASURE;
      break;

    case SEQ_UI_ENCODER_GP8:
      ui_selected_item = ITEM_MANUAL;
      break;


    case SEQ_UI_ENCODER_GP9:
    case SEQ_UI_ENCODER_GP10:
    case SEQ_UI_ENCODER_GP11:
    case SEQ_UI_ENCODER_GP12:
    case SEQ_UI_ENCODER_GP13:
    case SEQ_UI_ENCODER_GP14:
    case SEQ_UI_ENCODER_GP15:
    case SEQ_UI_ENCODER_GP16: {
      if( !manual_clock ) {

	int quicksel = encoder - 8;

	SEQ_UI_CC_Set(SEQ_CC_CLK_DIVIDER, quicksel_timebase[quicksel] & 0xff);
	SEQ_UI_CC_SetFlags(SEQ_CC_CLKDIV_FLAGS, (1<<1), (quicksel_timebase[quicksel] & 0x100) ? (1<<1) : 0);

      }
      return 1; // value has been changed
    }
  }

  // for GP encoders and Datawheel
 if (encoder == SEQ_UI_ENCODER_Datawheel) {  
  switch( ui_selected_item ) {
  case ITEM_GXTY:          return SEQ_UI_GxTyInc(incrementer);
  case ITEM_DIVIDER: 
    if( manual_clock )
      return 0;
    return SEQ_UI_CC_Inc(SEQ_CC_CLK_DIVIDER, 0, 255, incrementer);

  case ITEM_TRIPLET:
    if( manual_clock )
      return 0;
    if( !incrementer ) // toggle flag
      incrementer = (SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<1)) ? -1 : 1;
    return SEQ_UI_CC_SetFlags(SEQ_CC_CLKDIV_FLAGS, (1<<1), (incrementer >= 0) ? (1<<1) : 0);

  case ITEM_SYNCH_TO_MEASURE:
    if( manual_clock )
      return 0;
    if( !incrementer ) // toggle flag
      incrementer = (SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<0)) ? -1 : 1;
    return SEQ_UI_CC_SetFlags(SEQ_CC_CLKDIV_FLAGS, (1<<0), (incrementer >= 0) ? (1<<0) : 0);

  case ITEM_MANUAL:
    if( !incrementer ) // toggle flag
      incrementer = (SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<2)) ? -1 : 1;
    return SEQ_UI_CC_SetFlags(SEQ_CC_CLKDIV_FLAGS, (1<<2), (incrementer >= 0) ? (1<<2) : 0);
  }
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

  if( button <= SEQ_UI_BUTTON_GP8 ) {
    // -> forward to encoder handler
    return Encoder_Handler((int)button+8, 0);
  }


  switch( button ) {
    case SEQ_UI_BUTTON_Select:
    case SEQ_UI_BUTTON_Right:
      if( ++ui_selected_item >= NUM_OF_ITEMS )
	ui_selected_item = 0;
      return 1; // value always changed

    case SEQ_UI_BUTTON_Left:
      if( ui_selected_item == 0 )
	ui_selected_item = NUM_OF_ITEMS-1;
      else
	--ui_selected_item;
      return 1; // value always changed

    case SEQ_UI_BUTTON_Up:
      return Encoder_Handler(SEQ_UI_ENCODER_Datawheel, 1);

    case SEQ_UI_BUTTON_Down:
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

  // layout:
  // 00000000001111111111222222222233333333330000000000111111111122222222223333333333
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
  // <--------------------------------------><-------------------------------------->
  // Trk. Clock Divider  SyncToMeasure Manual         Quick Selection: Timebase      
  // G1T1   4 (normal)        yes        no    1    2    4    8    8T   16  16T   32 

  u8 visible_track = SEQ_UI_VisibleTrackGet();

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 0);
  if( ui_selected_item == ITEM_GXTY ) {
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL_INV);
    SEQ_LCD_PrintGxTy(ui_selected_group, ui_selected_tracks);
    //SEQ_LCD_PrintSpaces(2);
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  } else {
    SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
	SEQ_LCD_PrintGxTy(ui_selected_group, ui_selected_tracks);
    //SEQ_LCD_PrintSpaces(2);
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  }  
  
  SEQ_LCD_PrintString(" TRCK Clck Dividr"); 
//  SEQ_LCD_CursorSet(0, 2);
//  SEQ_LCD_PrintString("Sync to Measure:      ");
//  SEQ_LCD_CursorSet(0, 3);
//  SEQ_LCD_PrintString("Manual:               ");  
//  SEQ_LCD_CursorSet(0, 4);
//  SEQ_LCD_PrintString("Quick Sel: Timebase  ");

  u8 manual_clock = SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<2);
  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 1);
	SEQ_LCD_PrintSpaces(21);
/*	
  if( ui_selected_item == ITEM_GXTY && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(6);
  } else {
    SEQ_LCD_PrintGxTy(ui_selected_group, ui_selected_tracks);
    SEQ_LCD_PrintSpaces(2);
  }
*/
  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 2);
  if( ui_selected_item == ITEM_DIVIDER ) {
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL_INV);
    SEQ_LCD_PrintString("Divider  : ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  } else {
    SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
	SEQ_LCD_PrintString("Divider  : ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  }  
//  if( ui_selected_item == ITEM_DIVIDER && ui_cursor_flash ) {
//    SEQ_LCD_PrintSpaces(4);
//  } else {
    if( manual_clock ) {
      SEQ_LCD_PrintString(" ---");
    } else {
      SEQ_LCD_PrintFormattedString("%3d ", SEQ_CC_Get(visible_track, SEQ_CC_CLK_DIVIDER)+1);
    }
//  }

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 3);
  if( ui_selected_item == ITEM_TRIPLET ) {
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL_INV);
    SEQ_LCD_PrintString("Mode     : ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  } else {
    SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
	SEQ_LCD_PrintString("Mode     : ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  }  
//  if( ui_selected_item == ITEM_TRIPLET && ui_cursor_flash ) {
    SEQ_LCD_PrintSpaces(1);
//  } else {
    if( manual_clock ) {
      SEQ_LCD_PrintString("--------   ");
    } else {
      SEQ_LCD_PrintString((SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<1)) ? "(triplet) " : "(normal)  ");
    }
//  }

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 4);
  if( ui_selected_item == ITEM_SYNCH_TO_MEASURE ) {
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL_INV);
    SEQ_LCD_PrintString("Sync to Measure: ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  } else {
    SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
	SEQ_LCD_PrintString("Sync to Measure: ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  }  
//  if( ui_selected_item == ITEM_SYNCH_TO_MEASURE && ui_cursor_flash ) {
//    SEQ_LCD_PrintSpaces(5);
//  } else {
//    SEQ_LCD_PrintSpaces(6);
    if( manual_clock ) {
      SEQ_LCD_PrintString("---");
    } else {
      SEQ_LCD_PrintString((SEQ_CC_Get(visible_track, SEQ_CC_CLKDIV_FLAGS) & (1<<0)) ? "yes" : "no ");
    }
    SEQ_LCD_PrintSpaces(1);
//  }

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 5);
  if( ui_selected_item == ITEM_MANUAL ) {
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL_INV);
    SEQ_LCD_PrintString("Manual   : ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  } else {
    SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
	SEQ_LCD_PrintString("Manual   : ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  }  
//  if( ui_selected_item == ITEM_MANUAL && ui_cursor_flash ) {
//    SEQ_LCD_PrintSpaces(7);
//  } else {
//    SEQ_LCD_PrintSpaces(2);
    SEQ_LCD_PrintString(manual_clock ? "yes" : "no ");
    SEQ_LCD_PrintSpaces(6);
//  }

  ///////////////////////////////////////////////////////////////////////////
  SEQ_LCD_CursorSet(0, 6);
  if( ui_selected_item == ITEM_Q_SELECT ) {
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL_INV);
    SEQ_LCD_PrintString("Quick Sel:  ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  } else {
    SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
	SEQ_LCD_PrintString("Quick Sel:  ");
	SEQ_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);
  }  
  s32 quicksel_item = QUICKSEL_SearchItem(SEQ_UI_VisibleTrackGet());

      if( manual_clock ) {
	SEQ_LCD_PrintString(" --- ");
      } else {
	SEQ_LCD_PrintString((char *)quicksel_str[quicksel_item]);
      }

	  
  SEQ_LCD_CursorSet(0, 7);
  SEQ_LCD_PrintString(" (use GP Button 1-8) ");  
/* 
 int i;
  for(i=0; i<8; ++i) {
    if( quicksel_item == i && ui_cursor_flash ) {
      SEQ_LCD_PrintSpaces(5);
    } else {
      if( manual_clock ) {
	SEQ_LCD_PrintString(" --- ");
      } else {
	SEQ_LCD_PrintString((char *)quicksel_str[i]);
      }
    }
  }
*/
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
// Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 SEQ_UI_TRKDIV_Init(u32 mode)
{
  // install callback routines
  SEQ_UI_InstallButtonCallback(Button_Handler);
  SEQ_UI_InstallEncoderCallback(Encoder_Handler);
  SEQ_UI_InstallLEDCallback(LED_Handler);
  SEQ_UI_InstallLCDCallback(LCD_Handler);

  return 0; // no error
}
