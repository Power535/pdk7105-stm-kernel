/*
 * linux/arch/sh/kernel/cpu/irq/st40_ilc.h
 *
 * Copyright (C) 2003 STMicroelectronics Limited
 * Author: Henry Bell <henry.bell@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interrupt handling for ST40 Interrupt Level Controler (ILC).
 */

/*
 * ILC register locations
 */

#if defined(CONFIG_CPU_SUBTYPE_STM8000)
#define ILC_BASE_ADDR		0xb8300000
#elif defined(CONFIG_CPU_SUBTYPE_STI5528)
#define ILC_BASE_ADDR		0xba000000
#else
#error "ILC has to be interfaced by ST40 on: STm8000 and STi5528!"
#endif

/*
** The following set of macros is valid for both platforms:
** STm8000 and STi5528
*/
#define _BIT(_int)		     (1 << (_int % 32))
#define _REG_OFF(_int)		     (sizeof(int) * (_int / 32))

#define ILC_INTERRUPT_REG(_int)      (ILC_BASE_ADDR + 0x080 + _REG_OFF(_int))
#define ILC_STATUS_REG(_int)         (ILC_BASE_ADDR + 0x200 + _REG_OFF(_int))
#define ILC_CLR_STATUS_REG(_int)     (ILC_BASE_ADDR + 0x280 + _REG_OFF(_int))
#define ILC_ENABLE_REG(_int)         (ILC_BASE_ADDR + 0x400 + _REG_OFF(_int))
#define ILC_CLR_ENABLE_REG(_int)     (ILC_BASE_ADDR + 0x480 + _REG_OFF(_int))
#define ILC_SET_ENABLE_REG(_int)     (ILC_BASE_ADDR + 0x500 + _REG_OFF(_int))
#define ILC_EXT_WAKEUP_EN_REG        (ILC_BASE_ADDR + 0x600)
#define ILC_EXT_WAKPOL_EN_REG        (ILC_BASE_ADDR + 0x680)
#define ILC_PRIORITY_REG(_int)       (ILC_BASE_ADDR + 0x800 + (8 * _int))
#define ILC_TRIGMODE_REG(_int)       (ILC_BASE_ADDR + 0x804 + (8 * _int))

/*
 * Macros to get/set/clear ILC registers
 */
#define ILC_SET_ENABLE(_int)     ctrl_outl(_BIT(_int), ILC_SET_ENABLE_REG(_int))
#define ILC_CLR_ENABLE(_int)     ctrl_outl(_BIT(_int), ILC_CLR_ENABLE_REG(_int))
#define ILC_GET_ENABLE(_int)     (ctrl_inl(ILC_ENABLE_REG(_int)) & _BIT(_int))
#define ILC_CLR_STATUS(_int)     ctrl_outl(_BIT(_int), ILC_CLR_STATUS_REG(_int))
#define ILC_GET_STATUS(_int)     (ctrl_inl(ILC_STATUS_REG(_int)) & _BIT(_int))
#define ILC_SET_PRI(_int, _pri)  ctrl_outl((_pri), ILC_PRIORITY_REG(_int))

#define ILC_SET_TRIGMODE(_int, _mod) ctrl_outl((_mod), ILC_TRIGMODE_REG(_int))

#define ILC_TRIGGERMODE_HIGH	1
#define ILC_TRIGGERMODE_LOW	2