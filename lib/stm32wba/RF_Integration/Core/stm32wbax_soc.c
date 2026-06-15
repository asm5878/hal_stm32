/*
 * Copyright (c) 2023 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "RTDebug_dtb.h"

void soc_late_init_hook(void)
{
#ifdef CONFIG_STM32WBA_RT_DEBUG_IO_DTB
	/* DTB initialization and configuration */
	RT_DEBUG_DTBInit();
	RT_DEBUG_DTBConfig();
#endif
}
