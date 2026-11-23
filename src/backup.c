
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "pico/stdlib.h"
#include "can2040.h"

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define HI 0x100

static struct can2040 cbus;

// Que for handling messages:
static QueueHandle_t can_rx_queue;


/* -- MESSAGE PROCESSING LOGIC:
  this is called to process massages:
 */
static void can2040_cb(struct can2040 *cd, uint32_t notify,struct can2040_msg *msg) {

  // received a message
  if (notify == CAN2040_NOTIFY_RX) {
    BaseType_t wake_task = pdFALSE;
    portYIELD_FROM_ISR(wake_task);
    xQueueSendFromISR(
          can_rx_queue,
          msg,
          &wake_task
        );
    // swtich to which ever task is waiting on wake_task imediatly after handler
    portYIELD_FROM_ISR(wake_task);
  }
  // message was transmitted
  else if (notify == CAN2040_NOTIFY_TX) {
        // At least one other active CAN node ACKed this frame.
  }
  else if (notify == CAN2040_NOTIFY_ERROR) {
        // can2040 receive buffer overflowed
  }

}

/* -- SEND FUNCTION:
    msg.id = 0x123;       // message type / identifier
    msg.dlc = 6;          // how many bytes you are sending: Max 8
    msg.data[0] = 'H';
    msg.data[1] = 'e';
    msg.data[2] = 'l';
    msg.data[3] = 'l';
    msg.data[4] = 'o';
    msg.data[5] = '!';
    // remaining two are ignored
*/
void send_message(void) {
    struct can2040_msg msg = {
        .id = HI,
        .dlc = 2,
        .data[0] = 'H',
        .data[1] = 'i',
        .data[2] = '.'
    };
    can2040_transmit(&cbus, &msg);
}

// -- INTERRUPT FOR HANDLING MESSAGES RX & TX:
static void PIOx_IRQHandler(void) { can2040_pio_irq_handler(&cbus); }

// ----- CAN SET UP ----
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

static void CAN_task(void *params) {
    struct can2040_msg msg;
    while (true) {
      // Block here until the ISR supplies a CAN message.
      // This uses no CPU time while there is no message.
      if (xQueueReceive(can_rx_queue, &msg, portMAX_DELAY) == pdTRUE) {
        printf("CAN ID: 0x%03lx | DLC: %u | Data:",
              (unsigned long)msg.id,
              (unsigned int)msg.dlc);

        for (uint8_t i = 0; i < msg.dlc; i++) {
            printf(" %02X", msg.data[i]);
        }

        printf("\n");
      }
    }
}

int main2(void) {
  stdio_init_all();
  canbus_setup();
  // Queue holds 8 complete CAN frames
  can_rx_queue = xQueueCreate(8, sizeof(struct can2040_msg));
  configASSERT(can_rx_queue != NULL);
  xTaskCreate(CAN_task, "MainThread", TASK_STACK_SIZE, NULL,
              TASK_PRIORITY, NULL);

  vTaskStartScheduler(); // block
}
