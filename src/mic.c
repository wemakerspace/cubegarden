#include "ch.h"
#include "hal.h"

#include "orchard.h"

#include "mic.h"

#include <string.h>
#include <limits.h>

#include "shell.h"
#include "chprintf.h"
#include "orchard-test.h"
#include "test-audit.h"

void i2s_handler(I2SDriver *i2sp, size_t offset, size_t n);
//int32_t rx_samples[NUM_RX_SAMPLES];
extern int32_t __ram1_start__[];
//int32_t *rx_samples = __ram1__;
#define rx_samples __ram1_start__

int16_t rx_savebuf[NUM_RX_SAMPLES];
uint32_t rx_cb_count = 0;

uint8_t gen_mic_event = 0;

static I2SConfig i2s_config = {
  NULL,
  rx_samples,
  NUM_RX_SAMPLES,
  i2s_handler,
  { // sai_tx_state
    //    {44100u, 11289600 /*mclk freq*/, 32, kSaiStereo},  // mclk must be at least 2x bitclock
    {32000u, 8192000 /*mclk freq*/, 32, kSaiStereo},
    NULL,
    0,
    0,
    NULL,
    NULL,
    kSaiModeAsync,
    0,
    4,
    kSaiMaster,
    kSaiBusI2SType,
    //    NULL,  // semaphore_t
    FALSE,
    0,
  },
  { // sai_rx_state
    //    {44100u, 11289600 /*mclk freq*/, 32, kSaiStereo},  // mclk must be at least 2x bitclock
    {32000u, 8192000 /*mclk freq*/, 32, kSaiStereo},
    (uint8_t *) rx_samples,  // regardless fo sample size, driver thinks of this as char stream...for now.
    NUM_RX_SAMPLES,
    0,
    NULL,
    NULL,
    kSaiModeAsync,
    0,
    4,
    kSaiMaster,
    kSaiBusI2SType,
    //    NULL,  // semaphore_t
    false,  // use DMA
    0,
  },
  { // tx_userconfig
    kSaiMclkSourceSysclk,
    0,
    kSaiModeAsync,
    kSaiBusI2SType,
    kSaiMaster,
    kSaiBclkSourceMclkDiv,
    4,
    0,
  },
  { // rx_userconfig
    kSaiMclkSourceSysclk,
    0,
    kSaiModeAsync,
    kSaiBusI2SType,
    kSaiMaster,
    kSaiBclkSourceMclkDiv,
    4,
    0,
  }
};

extern event_source_t i2s_full_event;

int rx_dma_count = 0;

static void force_ordering(void) {
  DMA->TCD[KINETIS_I2S_DMA_CHANNEL].DADDR = I2SD1.config->rx_buffer;
}
static void force_ordering2(void) {
  DMA->CINT = KINETIS_I2S_DMA_CHANNEL;
}

// need to make vector derived from KINETIS_I2S_DMA_CHANNEL
OSAL_IRQ_HANDLER(KINETIS_DMA2_IRQ_VECTOR) {
  int i;
  
  OSAL_IRQ_PROLOGUE();

  //  memcpy( rx_savebuf, rx_samples, NUM_RX_SAMPLES * sizeof(uint32_t) );
  for( i = 0; i < NUM_RX_SAMPLES; i++ ) {
    // !! we actually throw away a couple of bits, >> 14 would give us everything the mic gives us,
    // but we're limited on space and CPU so toss the LSBs...
    rx_savebuf[i] = (int16_t) (rx_samples[i] >> 16);
  }

  // osalSysLockFromISR();
  // reset the destination address pointer
  // we do this before the copy under the theory that we'll start
  // overwriting the buffe only after some of the data has copied
  force_ordering();
  force_ordering2();
  
  DMA->SERQ = KINETIS_I2S_DMA_CHANNEL; // re-enable requests after the rxbuffer pointer is reset
  // osalSysUnlockFromISR();  

  rx_dma_count++;

  // kick out an event to write data to disk
  chSysLockFromISR();
  chEvtBroadcastI(&i2s_full_event);
  chSysUnlockFromISR();

  OSAL_IRQ_EPILOGUE();
}

void i2s_handler(I2SDriver *i2sp, size_t offset, size_t n) {
  (void) i2sp;
  (void) offset;
  (void) n;
  int i;
  
  rx_cb_count++; // this is a count of I2S overflows

  I2S0->RCSR |= 0x40000; // clear the fifo error flag, that's typically why we get here
    
#if 1
  //  while( DMA->TCD[KINETIS_I2S_DMA_CHANNEL].CSR |= 0x40 ) // wait until the channel is not active
  //    ;
  // reset the rx buffer so we're not overflowing into surrounding memory
  force_ordering();
  //DMA->TCD[KINETIS_I2S_DMA_CHANNEL].DADDR = I2SD1.config->rx_buffer; // this gets pulled into force_ordering
  
  // for now just copy it into the save buffer over and over again.
  // in the future, this would then kick off a SPI MMC data write event to save out the blocks
  
  DMA->SERQ = KINETIS_I2S_DMA_CHANNEL; // re-enable requests after the rxbuffer pointer is reset
#endif
 
#if 0   // this gets called now only if we have an I2S error
  //  memcpy( rx_savebuf, rx_samples, NUM_RX_SAMPLES * sizeof(uint32_t) );
  for( i = 0; i < NUM_RX_SAMPLES; i++ ) { 
    //    rx_savebuf[i] = (uint16_t) ((rx_samples[i] + INT_MAX + 1) >> 16) & 0xFFFF;
    rx_savebuf[i] = (int16_t) ((rx_samples[i] >> 16) & 0xFFFF);
  }
  
  // kick out an event to write data to disk
  //  chSysLockFromISR();
  chEvtBroadcastI(&i2s_full_event);
  // chSysUnlockFromISR();
#endif
}

void micStart(void) {
  i2sStart(&I2SD1, (const I2SConfig *) &i2s_config);
}

void analogUpdateMic(void) {
  gen_mic_event = 1;
}

int16_t *analogReadMic(void) {
  return rx_savebuf;
}

OrchardTestResult test_mic(const char *my_name, OrchardTestType test_type) {
  (void) my_name;
  uint16_t min, max;
  int i, j;
  char prompt[16];
  
  switch(test_type) {
  case orchardTestPoweron:
  case orchardTestTrivial:
    return orchardResultNoTest;
  case orchardTestInteractive:
  case orchardTestComprehensive:
    orchardTestPrompt("speak into", "microphone", 0);
    min = 65535; max = 0;

    for( j = 0; j < 20; j++ ) {
      gen_mic_event = 1;
      
      chThdYield();
      chThdSleepMilliseconds(200);  // wait for mic to sample

      for( i = 0; i < NUM_RX_SAMPLES; i++ ) { // input sample buffer is deeper, search all the way through
	if( rx_savebuf[i] > max )
	  max = rx_savebuf[i];
	if( rx_savebuf[i] < min )
	  min = rx_savebuf[i];
      }
    }
    
    uint16_t span = max - min;
    chsnprintf(prompt, sizeof(prompt), "span %d", span);

    if( span > 100 ) {
      orchardTestPrompt("mic test PASS", prompt, 0);
      chprintf(stream, "mic test pass, span: %d\n\r", span);
      return orchardResultPass;
    } else {
      orchardTestPrompt("mic test FAIL", prompt, 0);
      chprintf(stream, "mic test fail, span: %d\n\r", span);
      return orchardResultFail;
    }
    
  default:
    return orchardResultNoTest;
  }
  
  return orchardResultNoTest;
}
orchard_test("mic", test_mic);

