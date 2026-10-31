/*
 * kilo_build.c — Build wrapper for kilo editor on Bus Pirate
 *
 * This file defines BUSPIRATE before including kilo.c so that the
 * #ifdef BUSPIRATE blocks in kilo.c activate our platform shims.
 *
 * CMakeLists.txt compiles this file (not kilo.c directly).
 * This way kilo.c's only modifications are the #ifdef blocks,
 * and updating from upstream is a clean merge.
 */
#define BUSPIRATE 1
#include "kilo.c"
