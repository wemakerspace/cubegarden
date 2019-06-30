#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"
#include "radio.h"
#include <string.h>

#define NUM_LINES 3
#define LINE_PRESS 2
#define LINE_DBMAX 1
#define LINE_DBBKGD 0

static uint8_t line = 0;
static uint8_t dBbkgd = 0;
static uint8_t dBmax = 0;
static uint8_t pressure = 0;
static struct OrchardUiContext listUiContext;

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

  // 6th line: pressure change sensitivity
  chsnprintf(uiStr, sizeof(uiStr), "press thresh(mPa): %d", pressure);
  if( line == LINE_PRESS ) {
    gdispFillArea(0, height*(LINE_PRESS+1), width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height*(LINE_PRESS+1), width, height,
		     uiStr, font, draw_color, justifyLeft);

  gdispFlush();
  orchardGfxEnd();
}

static uint32_t tune_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void tune_start(OrchardAppContext *context) {
  (void)context;
  const struct userconfig *config;
  config = getConfig();
  dBmax = config->cfg_dBmax;
  dBbkgd = config->cfg_dBbkgd;
  pressure = config->cfg_pressuretrig;

}

static void tune_event(OrchardAppContext *context, const OrchardAppEvent *event) {

  (void)context;
  uint8_t selected;

  if (event->type == keyEvent) {
    if( (event->key.flags == keyDown) && ((event->key.code == keyBottom)) ) {
      line++;
      line %= NUM_LINES;
    } else if( (event->key.flags == keyDown) && ((event->key.code == keyTop)) ) {
      if( line > 0 )
	line--;
      else
	line = NUM_LINES - 1;
    }
  } 
  
  if(event->key.code == keyRight){
       if(line == LINE_DBBKGD) {
          dBbkgd = dBbkgd == 120 ? 120 : dBbkgd + 1; //max usable value of 120
        }
       if(line == LINE_DBMAX){
          dBmax = dBmax == 120 ? 120 : dBmax + 1; //max usable value of 120
        }
       if(line == LINE_PRESS){
           pressure = pressure == 255 ? 255 : pressure + 1; //don't let value wrap around
       }
  }
  if(event->key.code == keyLeft){
       if(line == LINE_DBBKGD) {
          dBbkgd = dBbkgd == 0 ? 0 : dBbkgd - 1; //min of 0
       }
       if(line == LINE_DBMAX){
          dBmax = dBmax == 0 ? 0 : dBmax - 1; //min of 0
       }
       if(line == LINE_PRESS){
           pressure = pressure == 0 ? 0 : pressure - 1; //don't let value wrap around
       }
  }
  
  
  else if( event->type == uiEvent ) {
    chHeapFree(listUiContext.itemlist); // free the itemlist passed to the UI
    selected = (uint8_t) context->instance->ui_result;
    context->instance->ui = NULL;
    context->instance->uicontext = NULL;
    
    // handle channel config
    configSetChannel((uint32_t) selected);
    radioUpdateChannelFromConfig(radioDriver);
    friendClear();
  }

  if( context->instance->ui == NULL ) {
    redraw_ui();
  }
}

static void tune_exit(OrchardAppContext *context) {
  (void)context;
  const struct userconfig *config;
  config = getConfig();
  char effect_cmd[128];

  if(dBbkgd != config->cfg_dBbkgd){
    configSetdBbkgd(dBbkgd);
    chsnprintf(effect_cmd, sizeof(effect_cmd), "tune dBbkgd %d", dBbkgd);
    radioAcquire(radioDriver);
    radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_forward, strlen(effect_cmd) + 1, effect_cmd);
    radioRelease(radioDriver);
  }
  if(dBmax != config->cfg_dBmax){
    configSetdBmax(dBmax);
    chsnprintf(effect_cmd, sizeof(effect_cmd), "tune dBmax %d", dBmax);
    radioAcquire(radioDriver);
    radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_forward, strlen(effect_cmd) + 1, effect_cmd);
    radioRelease(radioDriver);
  }
  if(pressure != config->cfg_pressuretrig){
    configSetpressuretrig(pressure);
    chsnprintf(effect_cmd, sizeof(effect_cmd), "tune pressure %d", pressure);
    radioAcquire(radioDriver);
    radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_forward, strlen(effect_cmd) + 1, effect_cmd);
    radioRelease(radioDriver);
  }
}

orchard_app("tune", tune_init, tune_start, tune_event, tune_exit);