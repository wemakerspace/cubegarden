#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"
#include "radio.h"

#define NUM_LINES 3
static uint8_t line = 0;
uint8_t anonymous = 0;

static void redraw_ui(void) {
  char tmp[] = "Settings";
  char uiStr[32];
  
  coord_t width;
  coord_t height;
  font_t font;
  color_t draw_color = White;
  const struct userconfig *config;

  config = getConfig();
  
  orchardGfxStart();
  // draw the title bar
  font = gdispOpenFont("fixed_5x8");
  width = gdispGetWidth();
  height = gdispGetFontMetric(font, fontHeight);

  gdispClear(Black);
  gdispFillArea(0, 0, width, height, White);
  gdispDrawStringBox(0, 0, width, height,
                     tmp, font, Black, justifyCenter);

  // 1st line: Channel
  chsnprintf(uiStr, sizeof(uiStr), "Channel: %d", config->cfg_channel);
  if( line == 0 ) {
    gdispFillArea(0, height*1, width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height, width, height,
		     uiStr, font, draw_color, justifyLeft);

  // 2nd line: Tx boost
  chsnprintf(uiStr, sizeof(uiStr), "Tx boost: %s", config->cfg_txboost ? "on" : "off");
  if( line == 1 ) {
    gdispFillArea(0, height*2, width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height*2, width, height,
		     uiStr, font, draw_color, justifyLeft);

  // 3rd line: anonymous mode
  chsnprintf(uiStr, sizeof(uiStr), "Silent mode: %s", anonymous ? "on" : "off");
  if( line == 2 ) {
    gdispFillArea(0, height*3, width, height, White);
    draw_color = Black;
  } else {
    draw_color = White;
  }
  gdispDrawStringBox(0, height*3, width, height,
		     uiStr, font, draw_color, justifyLeft);
  
  gdispFlush();
  orchardGfxEnd();
}

static uint32_t setting_init(OrchardAppContext *context) {

  (void)context;
  return 0;
}

static void setting_start(OrchardAppContext *context) {

  (void)context;

}

static void setting_event(OrchardAppContext *context, const OrchardAppEvent *event) {

  (void)context;

  if (event->type == keyEvent) {
    if( (event->key.flags == keyDown) && ((event->key.code == keyBottom)) ) {
      line++;
      line %= NUM_LINES;
    } else if( (event->key.flags == keyDown) && ((event->key.code == keyTop)) ) {
      if( line > 0 )
	line--;
      else
	line = NUM_LINES - 1;
    } else if( (event->key.flags == keyDown) && (event->key.code == keySelect) ) {
      orchardAppExit();
    }
  }

  redraw_ui();
}

static void setting_exit(OrchardAppContext *context) {

  (void)context;
}

orchard_app("Settings", setting_init, setting_start, setting_event, setting_exit);
