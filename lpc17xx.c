#include "lpc17xx.h"

volatile lpc_periph_emac_t   * const LPC_EMAC   = (lpc_periph_emac_t *)0x50000000;
volatile lpc_periph_sc_t     * const LPC_SC     = (lpc_periph_sc_t *)0x400FC000;
volatile lpc_periph_pincon_t * const LPC_PINCON = (lpc_periph_pincon_t *)0x4002C000;
volatile lpc_periph_timer_t  * const LPC_TIM0   = (lpc_periph_timer_t *)0x40004000;
volatile lpc_periph_timer_t  * const LPC_TIM1   = (lpc_periph_timer_t *)0x40008000;
volatile lpc_periph_timer_t  * const LPC_TIM2   = (lpc_periph_timer_t *)0x40090000;
volatile lpc_periph_timer_t  * const LPC_TIM3   = (lpc_periph_timer_t *)0x40094000;
volatile lpc_core_nvic_t     * const LPC_NVIC   = (lpc_core_nvic_t *)0xE000E100;
volatile lpc_core_scb_t      * const LPC_SCB    = (lpc_core_scb_t  *)0xE000ED00;
