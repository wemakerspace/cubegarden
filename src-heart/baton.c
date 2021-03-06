#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "chprintf.h"

#include "led.h"
#include "orchard.h"
#include "radio.h"
#include "baton.h"
#include "orchard-math.h"

static BatonState bstate;
uint8_t maxActualCubes = MAX_ACTUAL_CUBES;


// telemetry to the UI
extern uint8_t baton_holder_g;
extern baton_packet_type last_baton_packet_g;
extern uint8_t baton_target_g;
extern uint8_t baton_passing_g;
extern uint32_t last_ping_g;
extern uint8_t baton_fx_g;

BatonState *getBatonState(void) {
  return &bstate;
}

void initBaton(void) {
  bstate.state = baton_not_holding;
  bstate.passing_to_addr = 0;
  bstate.retry_interval = 0;
  bstate.strategy = baton_random;
  bstate.retry_time = chVTGetSystemTime();
  bstate.announce_time = chVTGetSystemTime();
  bstate.fx = 0;
}

uint8_t getBatonFx(void) {
  return bstate.fx;
}

void setBatonFx(uint8_t fx) {
  bstate.fx = fx;
}

void sendBatonAck(void) {
  BatonPacket pkt;

  pkt.type = baton_ack;
  pkt.address = 254;
  pkt.fx = 0;

  int i;
  // this handler runs in the event thread so it means we won't be able
  // to respond to events for the duration of the retry (e.g. chgcheck event, radio page, radio ping, gyro)
  for( i = 0; i < BATON_RADIO_ACK_DUP; i++ ) {
    radioAcquire(radioDriver);
    radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_baton, sizeof(pkt), &pkt);
    radioRelease(radioDriver);
    chThdSleepMilliseconds(BATON_RADIO_ACK_DUP_DELAY);
  }
}

baton_return_type hasBaton(void) {
  return bstate.state;
}

void handleRadioBaton(uint8_t prot, uint8_t src, uint8_t dst, uint8_t length, const void *data) {
  (void) prot;
  (void) src;
  (void) dst;
  (void) length;

  BatonPacket *pkt = (BatonPacket *)data;

  last_baton_packet_g = pkt->type;
  
  switch(pkt->type) {
  case baton_holder: // the "true" baton holder is confirming its baton holding
    if( 254 != pkt->address ) {
      bstate.state = baton_not_holding;
      bstate.fx = pkt->fx; // fx updates based on the holder beacon
      baton_fx_g = bstate.fx;
    }
    

    last_ping_g = chVTGetSystemTime();
    if( baton_holder_g != pkt->address ) // case of we missed the ack, but the baton was actually passed
      baton_passing_g = 0;
    
    baton_holder_g = pkt->address;
    break;
  case baton_pass:
    if( 254 == pkt->address ) {
      bstate.state = baton_holding;
      bstate.announce_time = chVTGetSystemTime();
      bstate.fx = pkt->fx;
      baton_fx_g = bstate.fx;
      sendBatonAck();
    }
    baton_target_g = pkt->address;
    baton_passing_g = 1;
    break;
  case baton_ack:
    baton_holder_g = pkt->address;
    if( bstate.state == baton_passing )
      bstate.state = baton_not_holding;
    // right now, *any* ack will clear this -- we might get dups etc.
    // we could also check address of the ack sent to see if it's from the cube we were targeting
    // to catch the case where we have two batons...
    baton_passing_g = 0;
    break;
  case baton_maxcube:
    // master badge should know this
    chprintf(stream, "baton: master badge should never receive baton_maxcube packet type\n\r");
    // maxActualCubes = pkt->address;
    break;
  default:
    chprintf(stream, "baton: bad packet type\n\r");
  }
}

void abortBatonPass(void) {
  bstate.state = baton_holding;
  bstate.retry_interval = 0;
  
  bstate.passing_to_addr = 0;
}

// this will send the baton pass exactly once -- rely on the native retry mechanism for now?
void sendBatonPassPacket(void) {
  BatonPacket pkt;

  pkt.type = baton_pass;
  pkt.address = bstate.passing_to_addr;
  pkt.fx = bstate.fx;

  radioAcquire(radioDriver);
  radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_baton, sizeof(pkt), &pkt);
  radioRelease(radioDriver);
}

baton_return_type passBaton(baton_strategy_type strategy, uint8_t address, uint32_t retry) {
  // we can't pass the baton if we aren't holding it...
  if( bstate.state == baton_not_holding )
    return baton_not_holding;

  bstate.state = baton_passing;
  bstate.strategy = strategy;
  bstate.retry_interval = retry;
  bstate.retry_time = chVTGetSystemTime();
  if( bstate.strategy == baton_specified ) {
    bstate.passing_to_addr = address;
  } else if( bstate.strategy == baton_increment ) {
    bstate.passing_to_addr = bstate.passing_to_addr + 1; // scan through the space of cubes
    if( bstate.passing_to_addr > maxActualCubes )
      bstate.passing_to_addr = 1;
  } else if( bstate.strategy == baton_random ) {
    bstate.passing_to_addr = (((uint32_t) rand()) % maxActualCubes) + 1;
  }

  sendBatonPassPacket();
  // wait to see if we get an ack
  chThdSleepMilliseconds(BATON_PASS_WAIT);
  
  // if an ack arrived, the event thread would have updated our internal variables
  return bstate.state;
}

baton_return_type retryBatonPass(void) {
  sendBatonPassPacket();
  // wait to see if we get an ack
  chThdSleepMilliseconds(BATON_PASS_WAIT);

  // reset the retry timer, so that manual calls to this continuously push the timer out
  bstate.retry_time = chVTGetSystemTime();
  
  // if an ack arrived, the event thread would have updated our internal variables
  return bstate.state;
}

void sendBatonHoldingPacket(void) {
  if( bstate.state != baton_holding )
    return;  // don't allow someone to accidentally broadcast this

  BatonPacket pkt;

  pkt.type = baton_holder;
  pkt.address = 254; // this is the master badge reserved address
  pkt.fx = bstate.fx;

  radioAcquire(radioDriver);
  radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_baton, sizeof(pkt), &pkt);
  radioRelease(radioDriver);
}

// save memory on the master badge -- no background thread manager for baton pass auto-retries
// or baton holding rebroadcast
#if 0
static thread_t *batonThr = NULL;
static THD_WORKING_AREA(waBatonThread, 0x200);
static THD_FUNCTION(baton_thread, arg) {
  (void)arg;
  
  chRegSetThreadName("BatonManager");
  initBaton();
  
  while (!chThdShouldTerminateX()) {
    if( (bstate.state == baton_passing) && (bstate.retry_interval > 0) ) {
      // we've specified a retry interval, so let's see if it's time to retry
      if( chVTTimeElapsedSinceX(bstate.retry_time) > bstate.retry_interval ) {
	sendBatonPassPacket();
	bstate.retry_time = chVTGetSystemTime();
      }
    }

    // if I'm the baton holder, periodically announce my holding
    if( bstate.state == baton_holding ) {
      if( chVTTimeElapsedSinceX(bstate.announce_time) > BATON_HOLDER_INTERVAL ) {
	bstate.announce_time = chVTGetSystemTime();
	sendBatonHoldingPacket();
      }
    }
    
    chThdSleepMilliseconds(10); // give some time for other threads
    // this also rate-limits retry packets to once every 10ms
  }

  chSysLock();
  chThdExitS(MSG_OK);
}
#endif

void startBaton(void) {
  initBaton();
#if 0
  // start the baton management
  batonThr = chThdCreateStatic(waBatonThread,
			       sizeof(waBatonThread),
			       (NORMALPRIO + 1), // slightly higher than the main thread
			       baton_thread,
			       NULL);
#endif
}

void setMaxCubes(uint8_t maxcubes) {
  BatonPacket pkt;

  pkt.type = baton_maxcube;
  pkt.address = maxcubes;
  pkt.fx = bstate.fx;

  int i;
  for( i = 0; i < 5; i++ ) {
    radioAcquire(radioDriver);
    radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_baton, sizeof(pkt), &pkt);
    radioRelease(radioDriver);
    chThdSleepMilliseconds(33);
  }
}
