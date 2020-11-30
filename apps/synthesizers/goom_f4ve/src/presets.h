// $Id$
/*
 * Preset handling
 *
 * ==========================================================================
 *
 *  Copyright (C) 2009 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#ifndef _PRESETS_H
#define _PRESETS_H

#include <eeprom.h>

/////////////////////////////////////////////////////////////////////////////
// Global definitions
/////////////////////////////////////////////////////////////////////////////

// EEPROM locations
#define PRESETS_EEPROM_SIZE    EEPROM_EMULATED_SIZE // prepared for up to 128 entries

#define PRESETS_ADDR_RESERVED  0x00 // should not be written
#define PRESETS_ADDR_MAGIC01   0x01
#define PRESETS_ADDR_MAGIC23   0x02

#define PRESETS_016 0x08
#define PRESETS_017 0x09
#define PRESETS_018 0x10
#define PRESETS_019 0x11
#define PRESETS_020 0x12
#define PRESETS_021 0x13
#define PRESETS_022 0x14
#define PRESETS_023 0x15
#define PRESETS_024 0x16
#define PRESETS_025 0x17
#define PRESETS_026 0x18
#define PRESETS_027 0x19
#define PRESETS_028 0x1a
#define PRESETS_029 0x1b
#define PRESETS_030 0x1c
#define PRESETS_031 0x1d
#define PRESETS_102 0x1e
#define PRESETS_103 0x1f
#define PRESETS_104 0x20
#define PRESETS_105 0x21
#define PRESETS_106 0x22
#define PRESETS_107 0x23
#define PRESETS_108 0x24
#define PRESETS_109 0x25


/*
    Controller Function
    CC from MIDI
    16	Oscillator 1 waveform duty cycle
    17	Oscillator 1 waveform slope
    18	Oscillator 1 coarse detune
    19	Oscillator 1 fine detune
    20	Oscillator 1 attack time
    21	Oscillator 1 decay time
    22	Oscillator 1 output level
    23	Oscillator 1 frequency mode: 0=key, 64=high, 127=low
    24	Filter EG attack time
    25	Filter EG decay time
    26	Filter EG sustain level
    27	Filter EG release time
    28	Amplitude EG attack time
    29	Amplitude EG decay time
    30	Amplitude EG sustain level
    31	Amplitude EG release time
    102	Oscillator 0 waveform duty cycle
    103	Oscillator 0 waveform slope
    104	Oscillator combine mode: 0=mix, 64=FM, 127=FM plus feedback
    105	Filter EG sensitivity: 64=none
    106	Filter cutoff
    107	Filter resonance
    108	Volume
    109	Pan
*/

/////////////////////////////////////////////////////////////////////////////
// Global Types
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////

extern s32 PRESETS_Init(u32 mode);

extern u16 PRESETS_Read16(u8 addr);
extern u32 PRESETS_Read32(u8 addr);

extern s32 PRESETS_Write16(u8 addr, u16 value);
extern s32 PRESETS_Write32(u8 addr, u32 value);

extern s32 PRESETS_StoreAll(void);


/////////////////////////////////////////////////////////////////////////////
// Export global variables
/////////////////////////////////////////////////////////////////////////////

#endif /* _PRESETS_H */
