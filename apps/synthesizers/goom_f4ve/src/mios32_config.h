// $Id$
/*
 * Local MIOS32 configuration file
 *
 * this file allows to disable (or re-configure) default functions of MIOS32
 * available switches are listed in $MIOS32_PATH/modules/mios32/MIOS32_CONFIG.txt
 *
 */

#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// The boot message which is print during startup and returned on a SysEx query
#define MIOS32_LCD_BOOT_MSG_LINE1 "GOOM Synth"
#define MIOS32_LCD_BOOT_MSG_LINE2 "(C) 2014 Mark Owen"

#define MIOS32_USB_PRODUCT_STR  "GOOM Synth"

#if defined(MIOS32_BOARD_MBHP_CORE_STM32F4VE)

    // disable SRIO for now, may be needed later!
    #define MIOS32_DONT_USE_SRIO

    // I2S support has to be enabled explicitely
    #define MIOS32_USE_I2S

    // enable MCLK pin (not for STM32 primer)
    #define MIOS32_I2S_MCLK_ENABLE  1

    // Sample rate: use the same like in the original Goom project
    // (72 MHz / 2048)
    #define MIOS32_I2S_AUDIO_FREQ  35156

#endif

#endif /* _MIOS32_CONFIG_H */
