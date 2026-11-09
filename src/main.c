#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "queue.h"
#include "task.h"
#include <can2040.h>
#include <hardware/regs/intctrl.h>
#include <pico/stdlib.h>
#include <stdio.h>

static QueueHandle_t msg_queue;

static struct can2040 cbus;

#define MSG_PROC_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define MSG_PROC_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

static void can2040_cb(struct can2040 *cd, uint32_t notify,
                       struct can2040_msg *msg) {
  xQueueSend(msg_queue, (void *)&msg, 10);
}

static void PIOx_IRQHandler(void) { can2040_pio_irq_handler(&cbus); }

void canbus_setup(void) {
  uint32_t pio_num = 0;
  uint32_t sys_clock = 125000000, bitrate = 500000;
  uint32_t gpio_rx = 4, gpio_tx = 5;

  // Setup canbus
  can2040_setup(&cbus, pio_num);
  can2040_callback_config(&cbus, can2040_cb);

  // Enable irqs
  irq_set_exclusive_handler(PIO0_IRQ_0, PIOx_IRQHandler);
  irq_set_priority(PIO0_IRQ_0, PICO_DEFAULT_IRQ_PRIORITY - 1);
  irq_set_enabled(PIO0_IRQ_0, 1);

  // Start canbus
  can2040_start(&cbus, sys_clock, bitrate, gpio_rx, gpio_tx);
}

void msg_process_thread() {
  struct can2040_msg msg;
  for (;;)
    if (xQueueReceive(msg_queue, &msg, portMAX_DELAY) == pdTRUE)
      printf("sad");
}

int main(void) {
  stdio_init_all();
  canbus_setup();

  msg_queue = xQueueCreate(50, sizeof(struct can2040_msg));

  TaskHandle_t task;
  xTaskCreate(msg_process_thread, "MessageProcessThread",
              MSG_PROC_TASK_STACK_SIZE, NULL, MSG_PROC_TASK_PRIORITY, &task);
  vTaskStartScheduler();

  while (true) {
    tight_loop_contents();
  }
}
