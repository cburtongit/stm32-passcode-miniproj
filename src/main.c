#include "stm32f1xx_hal.h"
#include <stdlib.h>
#include <string.h>

/* -- PIN DEFINITIONS -- */
// Friendly names (alias the HAL pin masks)
#define led1 GPIO_PIN_6
#define led2 GPIO_PIN_5
#define led3 GPIO_PIN_4
#define btn1 GPIO_PIN_2
#define btn2 GPIO_PIN_1
#define btn3 GPIO_PIN_0
// Bit masks! - pins on the same bank/port can be masked together
#define led_mask (led1 | led2 | led3)
#define btn_mask (btn1 | btn2 |btn3)

/* -- BUFFER -- */
#define BUFFER_SIZE 9
typedef struct Buffer {
    int buffer[BUFFER_SIZE];
    int head; // start of code input
    int tail;
    int size;
} Buffer;
enum { EVT_BTN1 = 1, EVT_BTN2 = 2, EVT_BTN3 = 3 }; // Event codes for the buttons

/* -- FUNCTION DECLARATIONS --*/
// STM32 specific
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
// buffer
Buffer *buffer_init();
void buffer_free(Buffer *b);
void buffer_add(Buffer *b, int input);
int buffer_ends_with(const Buffer* b, const int* code, int code_len);
void buffer_clear(Buffer *b);
// code checking
void led_success_anim(int flashes);
int check_success(const int *snap, const int snap_len, const int *code, const int code_len);


/* -- PASSCODE -- */
static const int correct[9] = {1, 1, 1, 2, 2, 2, 3, 3, 3};
#define CODE_LEN (int)(sizeof(correct)/sizeof(correct[0]))


int main(void) {
    HAL_Init();               // needs SysTick_Handler in stm32f1xx_it.c
    SystemClock_Config();     // safe, HSI-based clocking
    MX_GPIO_Init();           // set up GPIO Pins

    Buffer *b = buffer_init();  // buffer for storing code input

    GPIO_PinState p1 = GPIO_PIN_SET;           // previous states (idle HIGH with pull-up)
    GPIO_PinState p2 = GPIO_PIN_SET;
    GPIO_PinState p3 = GPIO_PIN_SET;

    while (1) {
      GPIO_PinState s1 = HAL_GPIO_ReadPin(GPIOA, btn1); // store the state of buttons so that inputs can be detected
      GPIO_PinState s2 = HAL_GPIO_ReadPin(GPIOA, btn2);
      GPIO_PinState s3 = HAL_GPIO_ReadPin(GPIOA, btn3);

      // turn on LEDs while buttons are held
      HAL_GPIO_WritePin(GPIOA, led1, (s1 == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET); 
      HAL_GPIO_WritePin(GPIOA, led2, (s2 == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOA, led3, (s3 == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET);

      // edge detect: on transition HIGH->LOW, push an event
      if (s1 == GPIO_PIN_RESET && p1 == GPIO_PIN_SET) { // check if button is pressed whne it wasn't before
        buffer_add(b, EVT_BTN1); // store most recent button input
        if (buffer_ends_with(b, correct, CODE_LEN)) { 
          buffer_clear(b); led_success_anim(5); 
        }
      }
      if (s2 == GPIO_PIN_RESET && p2 == GPIO_PIN_SET) {
        buffer_add(b, EVT_BTN2);
        if (buffer_ends_with(b, correct, CODE_LEN)) { 
          buffer_clear(b); led_success_anim(5); 
        }
      }
      if (s3 == GPIO_PIN_RESET && p3 == GPIO_PIN_SET) {
        buffer_add(b, EVT_BTN3);
        if (buffer_ends_with(b, correct, CODE_LEN)) { 
          buffer_clear(b); led_success_anim(5); 
        }
      }

      p1 = s1; p2 = s2; p3 = s3;               // update previous states
      HAL_Delay(10);                           // tiny debounce / CPU breather
    }

}

/* Buffer functions */
Buffer *buffer_init() {
  Buffer *b = malloc(sizeof(*b));
  memset(b, 0, sizeof(*b));
  b->size = 0, b->head = 0, b->tail = 0;
  return b;
}
void buffer_free(Buffer *b) {
  free(b);
}
void buffer_add(Buffer *b, int input) {
  if (b->size == BUFFER_SIZE) { // if full, drop the last input
      b->tail = (b->tail + 1) % BUFFER_SIZE;
  } else {
      b->size++;
  }
  b->buffer[b->head] = input;
  b->head = (b->head + 1) % BUFFER_SIZE;
}
int buffer_ends_with(const Buffer* b, const int* code, int code_len){
  if (b->size < code_len) return 0;
  for (int i = 0; i < code_len; i++){
    int idx = (b->head - code_len + i + BUFFER_SIZE) % BUFFER_SIZE; // last N entries
    if (b->buffer[idx] != code[i]) return 0;
  }
  return 1;
}
inline void buffer_clear(Buffer *b) {  // inline because it's tiny
  b->head = 0;
  b->tail = 0;
  b->size = 0;
}

/* Passcode functions */
void led_success_anim(int flashes) {
  for (int i = 0; i < flashes; i++) {
    HAL_GPIO_WritePin(GPIOA, led_mask, GPIO_PIN_SET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOA, led_mask, GPIO_PIN_RESET);
    HAL_Delay(100);
  }
}
int check_success(const int *snap, const int snap_len, const int *code, const int code_len) {
  if (snap_len < code_len) return 0; // not enough inputs yet
  int start = snap_len - code_len; // compare last code_len entries
  for (int i = 0; i < code_len; i++) {
    if (snap[start + i] != code[i]) return 0;
  }
  return 1;
}

/* STM32 Functions */
void SystemClock_Config(void) { //Tiny, clone-safe HSI clock
  RCC_OscInitTypeDef o = {0};
  RCC_ClkInitTypeDef c = {0};

  o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  o.HSIState = RCC_HSI_ON;
  o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  o.PLL.PLLState = RCC_PLL_NONE;
  HAL_RCC_OscConfig(&o);

  c.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  c.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  c.AHBCLKDivider = RCC_SYSCLK_DIV1;
  c.APB1CLKDivider = RCC_HCLK_DIV1;
  c.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&c, FLASH_LATENCY_0);
}
static void MX_GPIO_Init(void) {
  
  /* ENABLE the clock on the GPIO banks (allows the registers to work)*/
  __HAL_RCC_GPIOA_CLK_ENABLE();    // enable GPIO section A (pins: A0 to A15)
  // __HAL_RCC_GPIOB_CLK_ENABLE();     // enable GPIO section B (pins: B0 to A15)
  // __HAL_RCC_GPIOC_CLK_ENABLE();     // enable GPIO section C (pins: C13 to C15)

  /* SET UP PINS - set the pins functions and behavious here e.g. PULLUP, INPUT etc.*/
  GPIO_InitTypeDef g = {0}; // useful, struct that ZEROs all GPIO vars preventing them
                            // from having rubbish in them. Can be used on multiple pins
                            // by re-assigning g.Pin to another GPIO_PIN_XX

  /* set pin variables using the pin struct*/
  g.Pin   = led_mask; // LED1|LED2|LED3 since they're on the same port we can edit them at the same time
  g.Mode  = GPIO_MODE_OUTPUT_PP; // sets to output (PP: Push-pull)
  g.Pull  = GPIO_NOPULL; // since we're output, we don't need to worry about pullup/down
  g.Speed = GPIO_SPEED_FREQ_LOW; // use low for LEDS (more speed = more EMI/power)
  HAL_GPIO_Init(GPIOA, &g); // applies the g.X configs to the specified port (group of pins)
  HAL_GPIO_WritePin(GPIOA, led_mask, GPIO_PIN_RESET); // all OFF (assuming active-high LEDs)

  // Buttons: PA2,1,0 as inputs with pull-up (pressed = LOW)
  g = (GPIO_InitTypeDef){0}; // reused the same struct and just reconfigure the values
  g.Pin  = btn_mask; // same as before but for buttons
  g.Mode = GPIO_MODE_INPUT; // input
  g.Pull = GPIO_PULLUP; // pull up since 
  HAL_GPIO_Init(GPIOA, &g);

}
