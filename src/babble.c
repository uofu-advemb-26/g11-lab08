#include "FreeRTOS.h"
#include "task.h"
#include <can2040.h>
#include <hardware/regs/intctrl.h>
#include <pico/stdlib.h>
#include <stdio.h>

/* activity 2: babbling node
   A (can_babbler)  spams LOUD as fast as it can
   B (can_periodic) sends QUIET every 100ms
   both count what they get from the other one (no printing per msg, way too
   many msgs for that) */

static struct can2040 cbus;

#define LOUD 0x100  // low id = wins arbitration
#define QUIET 0x101 // loses to LOUD every time

#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define TASK_STACK_SIZE configMINIMAL_STACK_SIZE

// only written in the can irq, the status task just reads them
static volatile uint32_t loud_rx, quiet_rx, loud_tx, quiet_tx;

// B: true while a quiet msg is sitting in can2040 waiting to get on the bus
static volatile bool quiet_waiting;
static volatile uint32_t quiet_skipped; // times we couldnt send cause still waiting

// A: busy wait after every babble msg. status task sweeps it 0..400us
// (or press + / - in the terminal to pick one, a = back to auto sweep)
static volatile uint32_t delay_us = 0;
static bool auto_sweep = true;
#define SWEEP_STEP_US 10
#define SWEEP_MAX_US 400
#define SWEEP_SECONDS 2 // how long to sit on each delay

static void can2040_cb(struct can2040 *cd, uint32_t notify,
                       struct can2040_msg *msg) {
  (void)cd;
  if (notify == CAN2040_NOTIFY_RX) {
    if (msg->id == LOUD)
      loud_rx++;
    else if (msg->id == QUIET)
      quiet_rx++;
  } else if (notify == CAN2040_NOTIFY_TX) {
    if (msg->id == LOUD) {
      loud_tx++;
    } else if (msg->id == QUIET) {
      quiet_tx++;
      quiet_waiting = false;
    }
  }
}

static void PIOx_IRQHandler(void) { can2040_pio_irq_handler(&cbus); }

void canbus_setup(void) {
  uint32_t pio_num = 1;
  uint32_t sys_clock = 125000000, bitrate = 500000;
  uint32_t gpio_rx = 4, gpio_tx = 5;

  can2040_setup(&cbus, pio_num);
  can2040_callback_config(&cbus, can2040_cb);

  irq_set_exclusive_handler(PIO1_IRQ_0, PIOx_IRQHandler);
  irq_set_priority(PIO1_IRQ_0, PICO_DEFAULT_IRQ_PRIORITY - 1);
  irq_set_enabled(PIO1_IRQ_0, 1);

  can2040_start(&cbus, sys_clock, bitrate, gpio_rx, gpio_tx);
}

#ifdef BABBLER
// A: never stops. keeps can2040's tx buffer full so the bus is never idle
// (only this task transmits on A)
static void babble_thread(void *pvParameters) {
  (void)pvParameters;
  uint32_t count = 0;

  while (true) {
    if (can2040_check_transmit(&cbus)) {
      struct can2040_msg msg = {.id = LOUD, .dlc = 8};
      msg.data32[0] = count++;
      msg.data32[1] = delay_us;
      can2040_transmit(&cbus, &msg);
      busy_wait_us_32(delay_us); // the lab's "fix": slow the babbler down
    }
  }
}
#else
// B: one quiet msg every 100ms (10/s), but only if the last one made it out
static void periodic_thread(void *pvParameters) {
  (void)pvParameters;
  uint8_t seq = 0;

  while (true) {
    if (!quiet_waiting) {
      struct can2040_msg msg = {.id = QUIET, .dlc = 1, .data = {seq++}};
      quiet_waiting = true;
      can2040_transmit(&cbus, &msg);
    } else {
      quiet_skipped++;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
#endif

// prints how many of each msg went out / came in during the last second
static void status_thread(void *pvParameters) {
  (void)pvParameters;
  uint32_t last_loud_rx = 0, last_quiet_rx = 0, last_loud_tx = 0,
           last_quiet_tx = 0, last_skipped = 0;

  vTaskDelay(pdMS_TO_TICKS(2000));
  uint32_t seconds = 0;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    seconds++;

    uint32_t lrx = loud_rx, qrx = quiet_rx, ltx = loud_tx, qtx = quiet_tx,
             sk = quiet_skipped;
#ifdef BABBLER
    printf("A | delay %3lu us | babble sent %lu/s | quiet rx %lu/s (B sends "
           "10/s)\n",
           (unsigned long)delay_us, (unsigned long)(ltx - last_loud_tx),
           (unsigned long)(qrx - last_quiet_rx));
#else
    printf("B | quiet sent %lu/s skipped %lu/s | babble rx %lu/s\n",
           (unsigned long)(qtx - last_quiet_tx),
           (unsigned long)(sk - last_skipped),
           (unsigned long)(lrx - last_loud_rx));
#endif
    last_loud_rx = lrx;
    last_quiet_rx = qrx;
    last_loud_tx = ltx;
    last_quiet_tx = qtx;
    last_skipped = sk;

#ifdef BABBLER
    int c = getchar_timeout_us(0);
    if (c == '+') {
      auto_sweep = false;
      delay_us += SWEEP_STEP_US;
    } else if (c == '-' && delay_us >= SWEEP_STEP_US) {
      auto_sweep = false;
      delay_us -= SWEEP_STEP_US;
    } else if (c == 'a') {
      auto_sweep = true;
    }

    if (auto_sweep && seconds % SWEEP_SECONDS == 0) {
      delay_us += SWEEP_STEP_US;
      if (delay_us > SWEEP_MAX_US)
        delay_us = 0;
    }
#endif
  }
}

int main(void) {
  stdio_init_all();

  canbus_setup();

#ifdef BABBLER
  xTaskCreate(babble_thread, "Babble", TASK_STACK_SIZE, NULL, TASK_PRIORITY,
              NULL);
#else
  xTaskCreate(periodic_thread, "Periodic", TASK_STACK_SIZE, NULL,
              TASK_PRIORITY, NULL);
#endif
  xTaskCreate(status_thread, "Status", TASK_STACK_SIZE, NULL, TASK_PRIORITY,
              NULL);
  vTaskStartScheduler();

  while (true) {
    tight_loop_contents();
  }
}
