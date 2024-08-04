#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "binmode/binmodes.h"
#include "binmode/dirtyproto.h"
#include "lib/arduino-ch32v003-swio/arduino_ch32v003.h"

struct _binmode binmodes[]={
    {&dirtyproto_mode},
    {&arduino_ch32v003},
};
