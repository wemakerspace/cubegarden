#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"
#include "radio.h"
#include <string.h>

#define NUM_LINES 4
#define LINE_NEWCUBE 3
#define LINE_TIMESYNC 2
#define LINE_DBMAX 1
#define LINE_DBBKGD 0

static uint8_t line = 0;
static uint8_t dBbkgd = 0;
static uint8_t dBmax = 0;
static uint32_t timesync = 0;
static uint32_t newcube = 0;

static void redraw_ui(void) {
  char tmp[] = "Tuning Cubes";
  char uiStr[32];
  
  coord_t width;
  coord_t height;
  font_t font;
  color_t draw_color = White;
  
  orchardGfxStart();
  // draw the title bar
  font = gdispOpenFont("fixed_5x8");
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);

  gdispClear(Black);
  gdispFillArea(0, 0, width, height, White);
  gdispDrawStringBox(0, 0, width, height,
                     tmp, font, Black, justifyCenter);

  // 4th line: dB bkgnd threshold setting
  chsnprintf(uiStr, sizeof(uiStr), "bkgd dB thresh: %d", dBbkgd);
  if( line == LINE_DBBKGD ) {
    gdispFillArea(0, height*(LINE_DBBKGD+1), width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height*(LINE_DBBKGD+1), width, height,
		     uiStr, font, draw_color, justifyLeft);

  // 5th line: dB max threshold setting
  chsnprintf(uiStr, sizeof(uiStr), "max dB thresh: %d", dBmax);
  if( line == LINE_DBMAX ) {
    gdispFillArea(0, height*(LINE_DBMAX+1), width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height*(LINE_DBMAX+1), width, height,
		     uiStr, font, draw_color, justifyLeft);

  // timesync interval
  chsnprintf(uiStr, sizeof(uiStr), "timesync interval: %d", timesync);
  if( line == LINE_TIMESYNC ) {
    gdispFillArea(0, height*(LINE_TIMESYNC+1), width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height*(LINE_TIMESYNC+1), width, height,
		     uiStr, font, draw_color, justifyLeft);

  // newcube discovery interval
  chsnprintf(uiStr, sizeof(uiStr), "newcube flash time: %d", newcube);
  if( line == LINE_NEWCUBE ) {
    gdispFillArea(0, height*(LINE_NEWCUBE+1), width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height*(LINE_NEWCUBE+1), width, height,
		     uiStr, font, draw_color, justifyLeft);

  gdispFlush();
  orchardGfxEnd();
}

static uint32_t mtune_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void mtune_start(OrchardAppContext *context) {
  (void)context;
  const struct userconfig *config;
  config = getConfig();
  dBmax = config->cfg_dBmax;
  dBbkgd = config->cfg_dBbkgd;
  timesync = config->cfg_timesync_interval;
  newcube = config->cfg_fx_newcube_time;

}

#define MTUNE_RETRIES 5
#define MTUNE_RETRY_DELAY 37
static void mtune_event(OrchardAppContext *context, const OrchardAppEvent *event) {

  (void)context;
  int i;
  char effect_cmd[128];
  const struct userconfig *config;
  config = getConfig();

  if (event->type == keyEvent) {
    if( (event->key.flags == keyDown) && ((event->key.code == keyBottom)) ) {
      line++;
      line %= NUM_LINES;
    } else if( (event->key.flags == keyDown) && ((event->key.code == keyTop)) ) {
      if( line > 0 )
	line--;
      else
	line = NUM_LINES - 1;
    } else if( (event->key.flags == keyDown) && ((event->key.code == keyTopR) || (event->key.code == keyBottomR)) ) {
      // this is the "A" key or "B" key
      // if "B" key transmit all the params
      if(line == LINE_DBBKGD || event->key.code == keyBottomR) {
	for( i = 0; i < MTUNE_RETRIES; i++ ) {
	  chsnprintf(effect_cmd, sizeof(effect_cmd), "tune dBbkgd %d", dBbkgd);
	  radioAcquire(radioDriver);
	  radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_forward, strlen(effect_cmd) + 1, effect_cmd);
	  radioRelease(radioDriver);
	  chThdSleepMilliseconds(MTUNE_RETRY_DELAY);	  
	}
      } else if(line == LINE_DBMAX || event->key.code == keyBottomR) {
	for( i = 0; i < MTUNE_RETRIES; i++ ) {
	  chsnprintf(effect_cmd, sizeof(effect_cmd), "tune dBmax %d", dBmax);
	  radioAcquire(radioDriver);
	  radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_forward, strlen(effect_cmd) + 1, effect_cmd);
	  radioRelease(radioDriver);
	  chThdSleepMilliseconds(MTUNE_RETRY_DELAY);	  
	}
      } else if(line == LINE_TIMESYNC || event->key.code == keyBottomR) {
	// this is just a local commit
	if(timesync != config->cfg_timesync_interval) {
	  configSetTimeSyncInterval(timesync);
	}
      } else if(line == LINE_NEWCUBE || event->key.code == keyBottomR) {
	for( i = 0; i < MTUNE_RETRIES; i++ ) {
	  chsnprintf(effect_cmd, sizeof(effect_cmd), "tune newcube %d", newcube);
	  radioAcquire(radioDriver);
	  radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_forward, strlen(effect_cmd) + 1, effect_cmd);
	  radioRelease(radioDriver);
	  chThdSleepMilliseconds(MTUNE_RETRY_DELAY);	  
	}
      }
    }
    
    if(event->key.code == keyRight  && (event->key.flags != keyUp)){
      if(line == LINE_DBBKGD) {
	dBbkgd = dBbkgd == 120 ? 120 : dBbkgd + 1; //max usable value of 120
      }
      if(line == LINE_DBMAX){
	dBmax = dBmax == 120 ? 120 : dBmax + 1; //max usable value of 120
      }
      if(line == LINE_TIMESYNC ) {
	timesync = timesync == 120 ? 120 : timesync + 1; // don't go more than 2 minutes without a timesync
      }
      if(line == LINE_NEWCUBE ) {
	newcube = newcube == 20 ? 20 : newcube + 1; // don't go more than 20 seconds for newcube discovery
      }
    }
    if(event->key.code == keyLeft  && (event->key.flags != keyUp)){
      if(line == LINE_DBBKGD) {
	dBbkgd = dBbkgd == 0 ? 0 : dBbkgd - 1; //min of 0
      }
      if(line == LINE_DBMAX){
	dBmax = dBmax == 0 ? 0 : dBmax - 1; //min of 0
      }
      if(line == LINE_TIMESYNC ) {
	timesync = timesync == 1 ? 1 : timesync - 1; // don't go below once per second
      }
      if(line == LINE_NEWCUBE ) {
	newcube = newcube == 1 ? 1 : newcube - 1; // don't go below 1 second
      }
    }
  }
  
  redraw_ui();
}

static void mtune_exit(OrchardAppContext *context) {
  (void)context;
  const struct userconfig *config;
  config = getConfig();

  if(dBbkgd != config->cfg_dBbkgd){
    configSetdBbkgd(dBbkgd);
  }
  if(dBmax != config->cfg_dBmax){
    configSetdBmax(dBmax);
  }
  if(timesync != config->cfg_timesync_interval) {
    configSetTimeSyncInterval(timesync);
  }
  if(newcube != config->cfg_fx_newcube_time ) {
    configSetFxNewcubeTime(newcube);
  }
     
}

orchard_app("Tune cubes", mtune_init, mtune_start, mtune_event, mtune_exit);
