#include "ch.h"
#include "hal.h"
#include "led.h"
#include "orchard-effects.h"
#include "chprintf.h"
#include "stdlib.h"

#include <string.h>
#include <math.h>

static void lg2FB(struct effects_config *config) {
  do_lightgene(config);
}
orchard_effects("Lg2", lg2FB);

