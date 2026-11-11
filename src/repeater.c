/**
 * @file
 * @brief This file will repeat any CAN messages
 */

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
  (void)cd;
  if (notify == CAN2040_NOTIFY_RX) {
    BaseType_t higher_priority_task_woken = pdFALSE;

    // Do not print from this interrupt.  The task below reports each frame.
    if (xQueueSendFromISR(msg_queue, msg, &higher_priority_task_woken) !=
        pdPASS) {
      // The queue is full; dropping this frame is safer than blocking in an ISR.
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
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

static void msg_process_thread(void *pvParameters) {
  (void)pvParameters;
  struct can2040_msg msg;
  for (;;) {
    if (xQueueReceive(msg_queue, &msg, portMAX_DELAY) == pdTRUE) {
      printf("RX CAN ID: 0x%03lx | DLC: %u | Data:",
             (unsigned long)msg.id, (unsigned int)msg.dlc);
      for (uint8_t i = 0; i < msg.dlc; ++i) {
        printf(" %02X", msg.data[i]);
      }
      printf("\nRepeating frame...\n");
      can2040_transmit(&cbus, &msg);
    }
  }
}

// Runs without any CAN traffic so it proves the firmware, scheduler, USB CDC,
// and Pico W on-board LED are all alive.
static void status_thread(void *pvParameters) {
  (void)pvParameters;
  bool led_on = false;

  // Allow the USB serial device to enumerate before the first diagnostic line.
  vTaskDelay(pdMS_TO_TICKS(2000));
  printf("CAN repeater started; waiting for received CAN frames.\n");

  for (;;) {
    led_on = !led_on;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
    printf("heartbeat: LED %s | queue depth: %lu\n",
           led_on ? "on" : "off",
           (unsigned long)uxQueueMessagesWaiting(msg_queue));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

int main(void) {
  stdio_init_all();

  // The Pico W LED is controlled by the CYW43 chip rather than a normal GPIO.
  if (cyw43_arch_init() != 0) {
    printf("ERROR: could not initialize the Pico W LED controller.\n");
  }

  msg_queue = xQueueCreate(50, sizeof(struct can2040_msg));
  configASSERT(msg_queue != NULL);

  canbus_setup();

  configASSERT(xTaskCreate(msg_process_thread, "MessageProcessThread",
                           MSG_PROC_TASK_STACK_SIZE, NULL,
                           MSG_PROC_TASK_PRIORITY, NULL) == pdPASS);
  configASSERT(xTaskCreate(status_thread, "StatusThread",
                           MSG_PROC_TASK_STACK_SIZE, NULL,
                           MSG_PROC_TASK_PRIORITY, NULL) == pdPASS);
  vTaskStartScheduler();

  while (true) {
    tight_loop_contents();
  }
}
