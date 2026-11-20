#include "frames.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>

#include <atomic>

#include "face_screen.h"

namespace frames {
namespace {

constexpr size_t kHalfBytes = face::Screen::W * face::Screen::HALF * sizeof(uint16_t);
uint16_t* gFrame[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};  // [frame][top, bottom]
int gBack = 0;

TaskHandle_t gPusher = nullptr;
std::atomic<bool> gPushBusy{false};
volatile int gPushFrame = 0;

// A half is 32 KB, about 6.5 ms on the wire. This task sleeps through most of each one instead of spinning on the
// DMA: with Wi-Fi off the face draws flat out, and core 0 then never went idle, so the watchdog restarted the robot
// after 5 s (2026-11-10). Sleeping also saves battery.
constexpr TickType_t kHalfSleep = pdMS_TO_TICKS(5);

void sendFrame(int f) {
  // DMA: the SPI hardware keeps sending while this core does rows for the face (the row worker outranks this task).
  M5.Display.startWrite();
  M5.Display.pushImageDMA(0, 0, face::Screen::W, face::Screen::HALF,
                          reinterpret_cast<const lgfx::swap565_t*>(gFrame[f][0]));
  vTaskDelay(kHalfSleep);
  M5.Display.pushImageDMA(0, face::Screen::HALF, face::Screen::W, face::Screen::HALF,
                          reinterpret_cast<const lgfx::swap565_t*>(gFrame[f][1]));
  vTaskDelay(kHalfSleep);
  M5.Display.endWrite();  // waits for whatever is left of the transfer
}

void pushTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    sendFrame(gPushFrame);
    gPushBusy.store(false, std::memory_order_release);
  }
}

uint16_t* allocHalf(bool psram) {
  return static_cast<uint16_t*>(heap_caps_malloc(
      kHalfBytes, psram ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));
}

// The row worker: the Arduino loop (and so the face) runs on core 1; this takes the odd rows on core 0.
TaskHandle_t gWorker = nullptr;
volatile face::RowJob gJob = nullptr;
void* volatile gJobCtx = nullptr;
std::atomic<bool> gJobDone{true};

void workerTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    gJob(gJobCtx, 1);
    gJobDone.store(true, std::memory_order_release);
  }
}
void splitOverCores(face::RowJob job, void* ctx) {
  gJob = job;
  gJobCtx = ctx;
  gJobDone.store(false, std::memory_order_release);
  xTaskNotifyGive(gWorker);
  job(ctx, 0);
  while (!gJobDone.load(std::memory_order_acquire)) {
  }
}

}  // namespace

bool begin() {
  gFrame[0][0] = allocHalf(false);
  gFrame[0][1] = allocHalf(false);
  if (!gFrame[0][0]) gFrame[0][0] = allocHalf(true);
  if (!gFrame[0][1]) gFrame[0][1] = allocHalf(true);
  if (!gFrame[0][0] || !gFrame[0][1]) return false;
  memset(gFrame[0][0], 0, kHalfBytes);
  memset(gFrame[0][1], 0, kHalfBytes);
  xTaskCreatePinnedToCore(pushTask, "facePush", 4096, nullptr, 3, &gPusher, 0);
  if (xTaskCreatePinnedToCore(workerTask, "faceRows", 6144, nullptr, 5, &gWorker, 0) == pdPASS)
    face::splitRows = splitOverCores;
  return true;
}

bool addSecond(size_t keepFree) {
  if (gFrame[1][0]) return true;
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 2 * kHalfBytes + keepFree) return false;
  gFrame[1][0] = allocHalf(false);
  gFrame[1][1] = allocHalf(false);
  if (!gFrame[1][0] || !gFrame[1][1] || heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < keepFree) {
    heap_caps_free(gFrame[1][0]);
    heap_caps_free(gFrame[1][1]);
    gFrame[1][0] = gFrame[1][1] = nullptr;
    return false;
  }
  return true;
}

uint16_t* top() { return gFrame[gBack][0]; }
uint16_t* bottom() { return gFrame[gBack][1]; }

void wait() {
  while (gPushBusy.load(std::memory_order_acquire)) delay(0);
}

void ready() {
  if (!gFrame[1][0]) wait();
}

void present() {
  wait();
  gPushFrame = gBack;
  gPushBusy.store(true, std::memory_order_release);
  xTaskNotifyGive(gPusher);
  if (gFrame[1][0]) gBack ^= 1;
}

int count() { return gFrame[1][0] ? 2 : 1; }

void releaseSecond() {
  wait();
  if (!gFrame[1][0]) return;
  heap_caps_free(gFrame[1][0]);
  heap_caps_free(gFrame[1][1]);
  gFrame[1][0] = gFrame[1][1] = nullptr;
  gBack = 0;
}

}  // namespace frames
