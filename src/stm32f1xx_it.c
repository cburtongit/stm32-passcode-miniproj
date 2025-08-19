#include "stm32f1xx_hal.h"
//#include "stm32f1xx_it.h"

void SysTick_Handler(void)
{
  HAL_IncTick();   // keeps HAL_GetTick / HAL_Delay working
}
