#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <can2040.h>
#include <hardware/regs/intctrl.h>
#include <pico/stdlib.h>
#include <stdio.h>

static QueueHandle_t msg_queue;

static struct can2040 cbus;

static uint32_t ping_time; // when A sent the last ping (us)

#define HI 0x100   // ping, sent by node A
#define PONG 0x101 // reply, sent by node B

#define MSG_PROC_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define MSG_PROC_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

/* -- MESSAGE PROCESSING LOGIC:
  this is called to process massages:
 */
static void can2040_cb(struct can2040 *cd, uint32_t notify,
                       struct can2040_msg *msg) {
  if (notify == CAN2040_NOTIFY_RX) {
    BaseType_t higher_priority_task_woken = pdFALSE;

    xQueueSendFromISR(msg_queue, msg, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
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
      .id = HI, .dlc = 2, .data[0] = 'H', .data[1] = 'i', .data[2] = '.'};
  can2040_transmit(&cbus, &msg);

  vTaskDelay((TickType_t)(1000));
}

// -- INTERRUPT FOR HANDLING MESSAGES RX & TX:
static void PIOx_IRQHandler(void) { can2040_pio_irq_handler(&cbus); }

void canbus_setup(void) {
  uint32_t pio_num = 1; // pio0 is used by the wifi chip (led)
  uint32_t sys_clock = 125000000, bitrate = 500000;
  uint32_t gpio_rx = 4, gpio_tx = 5;

  // Setup canbus
  can2040_setup(&cbus, pio_num);
  can2040_callback_config(&cbus, can2040_cb);

  // Enable irqs
  irq_set_exclusive_handler(PIO1_IRQ_0, PIOx_IRQHandler);
  irq_set_priority(PIO1_IRQ_0, PICO_DEFAULT_IRQ_PRIORITY - 1);
  irq_set_enabled(PIO1_IRQ_0, 1);

  // Start canbus
  can2040_start(&cbus, sys_clock, bitrate, gpio_rx, gpio_tx);
}

static void send_thread(void *pvParameters) {
  (void)pvParameters;

  uint8_t seq = 0;
  while (true) {
    struct can2040_msg msg = {
        .id = HI, .dlc = 5, .data = {'P', 'I', 'N', 'G', seq}};
    printf("TX PING seq=%u\n", seq);
    ping_time = time_us_32();
    can2040_transmit(&cbus, &msg);
    seq++;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// This runs independently of CAN.  If it blinks and prints, USB stdio and
// FreeRTOS are working even if there is no other CAN node on the bus.
static void status_thread(void *pvParameters) {
  (void)pvParameters;

  // Give the host time to enumerate the USB CDC device before the first line.
  vTaskDelay(pdMS_TO_TICKS(2000));
  printf("CAN activity started; USB stdio and FreeRTOS are running.\n");

  for (;;) {
    // if tx_attempt keeps going up but tx doesnt, nobody is acking (check bus)
    struct can2040_stats stats;
    can2040_get_statistics(&cbus, &stats);
    printf("heartbeat | rx %lu tx %lu tx_attempt %lu parse_err %lu\n",
           (unsigned long)stats.rx_total,
           (unsigned long)stats.tx_total, (unsigned long)stats.tx_attempt,
           (unsigned long)stats.parse_error);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void msg_process_thread(void *pvParameters) {
  (void)pvParameters;

  struct can2040_msg msg;
  while (true) {
    // Block here until the ISR supplies a CAN message.
    // This uses no CPU time while there is no message.
    if (xQueueReceive(msg_queue, &msg, portMAX_DELAY) == pdTRUE) {
      uint32_t now = time_us_32();

      // node B: answer pings right away (before printing so its fast)
      // only one task sends on each node so can2040_transmit is safe
      if (msg.id == HI) {
        struct can2040_msg reply = {
            .id = PONG, .dlc = 5, .data = {'P', 'O', 'N', 'G', msg.data[4]}};
        can2040_transmit(&cbus, &reply);
      }

      printf("RECIEVED: CAN ID: 0x%03lx | DLC: %u | Data:",
             (unsigned long)msg.id, (unsigned int)msg.dlc);

      for (uint8_t i = 0; i < msg.dlc; i++) {
        printf(" %02X", msg.data[i]);
      }

      printf("\n");

      // node A: how long the ping took to come back
      if (msg.id == PONG) {
        printf("PONG seq=%u rtt=%lu us\n", msg.data[4],
               (unsigned long)(now - ping_time));
      }
    }
  }
}

int main(void) {
  stdio_init_all();

  msg_queue = xQueueCreate(50, sizeof(struct can2040_msg));
  configASSERT(msg_queue != NULL);

  canbus_setup();

  // dont wrap these in configASSERT, its assert() and gets compiled out in
  // release so the tasks never get made
  xTaskCreate(msg_process_thread, "MessageProcessThread", MSG_PROC_TASK_STACK_SIZE,
              NULL, MSG_PROC_TASK_PRIORITY, NULL);
#ifdef NODE_A // only A pings, B just answers
  xTaskCreate(send_thread, "MessageSendThread", MSG_PROC_TASK_STACK_SIZE,
              NULL, MSG_PROC_TASK_PRIORITY, NULL);
#endif
  xTaskCreate(status_thread, "StatusThread", MSG_PROC_TASK_STACK_SIZE,
              NULL, MSG_PROC_TASK_PRIORITY, NULL);
  vTaskStartScheduler();

  while (true) {
    tight_loop_contents();
  }
}
