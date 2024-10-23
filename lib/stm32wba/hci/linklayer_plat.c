/*
 * Copyright (c) 2023 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "scm.h"

/* 2.4GHz RADIO ISR callbacks */
typedef void (*radio_isr_cb_t) (void);

extern const struct device *rng_dev;

/* Radio critical sections */
volatile int32_t irq_counter;

/* Radio bus clock control variables */
uint8_t AHB5_SwitchedOff;
uint32_t radio_sleep_timer_val;

void LINKLAYER_PLAT_ClockInit(void)
{
	AHB5_SwitchedOff = 0;
	radio_sleep_timer_val = 0;

	LL_PWR_EnableBkUpAccess();

	/* Select LSE as Sleep CLK */
	__HAL_RCC_RADIOSLPTIM_CONFIG(RCC_RADIOSTCLKSOURCE_LSE);

	LL_PWR_DisableBkUpAccess();

	/* Enable AHB5ENR peripheral clock (bus CLK) */
	__HAL_RCC_RADIO_CLK_ENABLE();
}

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_DelayUs(uint32_t delay)
{
__IO register uint32_t Delay = delay * (SystemCoreClock / 1000000U);
	do
	{
		__NOP();
	}
	while (Delay --);
}
#endif

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_Assert(uint8_t condition)
{
	assert_param(condition);
}
#endif

void LINKLAYER_PLAT_WaitHclkRdy(void)
{
	while (HAL_RCCEx_GetRadioBusClockReadiness() != RCC_RADIO_BUS_CLOCK_READY) {
	}
}

void LINKLAYER_PLAT_AclkCtrl(uint8_t enable)
{
	if (enable) {
		/* Enable RADIO baseband clock (active CLK) */
		HAL_RCCEx_EnableRadioBBClock();

		/* Polling on HSE32 activation */
		while (LL_RCC_HSE_IsReady() == 0) {
		}
	} else {
		/* Disable RADIO baseband clock (active CLK) */
		HAL_RCCEx_DisableRadioBBClock();
	}
}

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_GetRNG(uint8_t *ptr_rnd, uint32_t len)
{
	uint32_t nb_remaining_rng = len;
	uint32_t generated_rng;

	/* Get the requested RNGs (4 bytes by 4bytes) */
	while (nb_remaining_rng >= 4)
	{
		generated_rng = 0;
		HW_RNG_Get(1, &generated_rng);
		memcpy((ptr_rnd+(len-nb_remaining_rng)), &generated_rng, 4);
		nb_remaining_rng -=4;
	}

	/* Get the remaining number of RNGs */
	if (nb_remaining_rng>0) {
	generated_rng = 0;
	HW_RNG_Get(1, &generated_rng);
	memcpy((ptr_rnd+(len-nb_remaining_rng)), &generated_rng, nb_remaining_rng);
	}
}
#endif

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_SetupRadioIT(void (*intr_cb)())
{
	radio_callback = intr_cb;
	HAL_NVIC_SetPriority((IRQn_Type) RADIO_INTR_NUM, RADIO_INTR_PRIO_HIGH, 0);
	HAL_NVIC_EnableIRQ((IRQn_Type) RADIO_INTR_NUM);
}
#endif

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_SetupSwLowIT(void (*intr_cb)())
{
	low_isr_callback = intr_cb;

	HAL_NVIC_SetPriority((IRQn_Type) RADIO_SW_LOW_INTR_NUM, RADIO_SW_LOW_INTR_PRIO, 0);
	HAL_NVIC_EnableIRQ((IRQn_Type) RADIO_SW_LOW_INTR_NUM);
}
#endif

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_TriggerSwLowIT(uint8_t priority)
{
  uint8_t low_isr_priority = RADIO_INTR_PRIO_LOW;

	if (NVIC_GetActive(RADIO_SW_LOW_INTR_NUM) == 0) {
		/* No nested SW low ISR, default behavior */

		if (priority == 0) {
			low_isr_priority = RADIO_SW_LOW_INTR_PRIO;
		}

		HAL_NVIC_SetPriority((IRQn_Type) RADIO_SW_LOW_INTR_NUM, low_isr_priority, 0);
  	} else {
    		/* Nested call detected */
    		/* No change for SW radio low interrupt priority for the moment */
    		if (priority != 0) {
			/* At the end of current SW radio low ISR, this pending SW low interrupt
       			* will run with RADIO_INTR_PRIO_LOW priority
       			**/
      			radio_sw_low_isr_is_running_high_prio = 1;
    		}
  	}

  HAL_NVIC_SetPendingIRQ((IRQn_Type) RADIO_SW_LOW_INTR_NUM);
}
#endif

void LINKLAYER_PLAT_EnableIRQ(void)
{
	irq_counter = MAX(0, irq_counter - 1);

	if (irq_counter == 0) {
		__enable_irq();
	}
}

void LINKLAYER_PLAT_DisableIRQ(void)
{
	__disable_irq();

	irq_counter++;
}

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_EnableSpecificIRQ(uint8_t isr_type)
{
	if ((isr_type & LL_HIGH_ISR_ONLY) != 0) {
		prio_high_isr_counter--;
		if (prio_high_isr_counter == 0) {
			HAL_NVIC_EnableIRQ((IRQn_Type) RADIO_INTR_NUM);
		}
	}

	if ((isr_type & LL_LOW_ISR_ONLY) != 0) {
		prio_low_isr_counter--;
		if (prio_low_isr_counter == 0) {
			HAL_NVIC_EnableIRQ((IRQn_Type) RADIO_SW_LOW_INTR_NUM);
		}
	}

	if ((isr_type & SYS_LOW_ISR) != 0) {
		prio_sys_isr_counter--;
		if (prio_sys_isr_counter == 0) {
			__set_BASEPRI(local_basepri_value);
		}
	}
}
#endif

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_DisableSpecificIRQ(uint8_t isr_type)
{

	if ((isr_type & LL_HIGH_ISR_ONLY) != 0) {
		prio_high_isr_counter++;
		if (prio_high_isr_counter == 1) {
			HAL_NVIC_DisableIRQ(RADIO_INTR_NUM);
		}
	}

	if ((isr_type & LL_LOW_ISR_ONLY) != 0) {
		prio_low_isr_counter++;
		if (prio_low_isr_counter == 1) {
			HAL_NVIC_DisableIRQ(RADIO_SW_LOW_INTR_NUM);
		}
	}

	if ((isr_type & SYS_LOW_ISR) != 0) {
		prio_sys_isr_counter++;
		if (prio_sys_isr_counter == 1) {
			local_basepri_value = __get_BASEPRI();
			__set_BASEPRI_MAX(RADIO_INTR_PRIO_LOW_Z << 4);
		}
	}
}
#endif

void LINKLAYER_PLAT_EnableRadioIT(void)
{
	NVIC_EnableIRQ(RADIO_INTR_NUM);
}

void LINKLAYER_PLAT_DisableRadioIT(void)
{
	NVIC_DisableIRQ(RADIO_INTR_NUM);
}

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_StartRadioEvt(void)
{
	__HAL_RCC_RADIO_CLK_SLEEP_ENABLE();

	NVIC_SetPriority(RADIO_INTR_NUM, RADIO_INTR_PRIO_HIGH_Z);

	scm_notifyradiostate(SCM_RADIO_ACTIVE);
}
#endif

#ifndef __ZEPHYR__
void LINKLAYER_PLAT_StopRadioEvt(void)
{
	__HAL_RCC_RADIO_CLK_SLEEP_DISABLE();

	NVIC_SetPriority(RADIO_INTR_NUM, RADIO_INTR_PRIO_LOW_Z);

	scm_notifyradiostate(SCM_RADIO_NOT_ACTIVE);
}
#endif

void LINKLAYER_PLAT_NotifyWFIEnter(void)
{
	/* Check if Radio state will allow the AHB5 clock to be cut */

	/* AHB5 clock will be cut in the following cases:
	 * - 2.4GHz radio is not in ACTIVE mode (in SLEEP or DEEPSLEEP mode).
	 * - RADIOSMEN and STRADIOCLKON bits are at 0.
	 */
	if ((LL_PWR_GetRadioMode() != LL_PWR_RADIO_ACTIVE_MODE) ||
	   ((__HAL_RCC_RADIO_IS_CLK_SLEEP_ENABLED() == 0) &&
	   (LL_RCC_RADIO_IsEnabledSleepTimerClock() == 0))) {
		AHB5_SwitchedOff = 1;
	}
}

void LINKLAYER_PLAT_NotifyWFIExit(void)
{
	/* Check if AHB5 clock has been turned of and needs resynchronisation */
	if (AHB5_SwitchedOff) {
		/* Read sleep register as earlier as possible */
		radio_sleep_timer_val = ll_intf_cmn_get_slptmr_value();
	}
}
