# FB Scope Mode

## Intro

This is a quick and dirty explanation on how to use the initial BP scope
mode - this code is expoerimental,'feel free to play with it, don't expect
it to work well, any feedback is happily received - Paul taniwha@gmail.com

First grab the 'scope' branch from this repo, load the "scope.uf2" binary into your
Bus Piratei (BP).


## Docs

You can enter 'scope' mode by typing "M" and then "7".

You should see an oscilloscope show up on the BP screen, it's rotated 90 degrees 
from the normal BP display - mostly so that we can show longer waveforms.

A brief description of the display:

* the red 'T' is the trigger position
* the text in the top right is the scale (voltage X time), the size of 1 yellow square on the screen - by
default "1V x 100uS" - 1 volt by 100 microseconds
* the text in the top left is the offset (in time) of the left hand vertical yellow line
* the text in the bottom left is the offset (in voltage) of the bottom yellow line
* the text in the bottom right tells you which input pin is being sampled

By default at startup the screen displays 0V-5V in the vertical direction and 0uS to 640uS horizontally.
The displayed screen is window into a larger display buffer, after taking a trace you can move around 
inside that buffer and explore that data.

Controls are done from the console, just like other BP functions - there are 5 commands that
can be entered:

* sr <pin> <mode> - run optional pin (0-7) and/or mode "o n a" Once, Normal, Auto
* ss - stop
* x - edit timebase (x axis)
* y - edit voltage (y access)
* t - edit trigger

all of these commands enter a more interactive UI where you can type single characters and have things
happen, you can skip back out to the normal BP UI by typing <ENTER> - there are 3 main modes (ss and sr start/stop the display and dump you into the "edit timebase" mode.

In trigger mode you will see a red crosshair that shows the time and voltage that the trigger occurs at,
there's also red test that describes it. If the crosshair is off the screen you'll see a red arrow
pointing at where it's located.

Interactive commands - some of these are different in different modes:

* <>^v - arrows move you around in the sample buffer. Or in edit trigger mode move the trigger voltage and time around

* +- - changes the time scale in x mode - where possible we over sample (up to 10 samples per pixel) so you can often zoom in 10x and continue to get more (after that it's interpolating) - 100uS/div is exactly 1 sample/pixel
* +- - changes the voltage scale in y modes - the hardware can sample 0-5V in 12 bits, the display is sampling/scaling within thati
* +-b* - in trigger mode says what edge we're sampling +/- are positive and negative going edges, b means both edges and * means no edges (the trigger is turned off, you need to manually stop the scope or run it in 
auto mode

* r - starts the scope in the mode it was run in last time (pressing the button if the scope is stopped does the same thing)
* s - stops the scope (pressing the button if the scope is running also stops it)
* o - starts the scope in 'one time' mode (waits for one trigger and the stops)
* n - starts the scope in 'normal' mode - the scope waits for a trigger, displays the trace and then starts again
* a - starts the scope in 'auto mode' - the scope waits for a triggers, but every second it just displays what it's seeing (use this for measuring voltages)

* BME - in trigger mode Beginning/Middle/End moves the trigger to the beginning/middle/end of the buffer (if you change the y time scale you might want to set this again)
* T - moves the y offset to the trigger

## Quick Tutorial

Here's a quick start tutorial using the internal FB commands - we're going to work on pin # 2, initially wagging it up and down and then setting up a clock.

So:

* use the 'd' command to enter 'Scope' mode
* go into I2C mode using the 'm' command and choosing the defaults
* set the I/O power supply to 3.3v using the 'W' command (W <enter> <enter> <enter>)
* set the trigger to 'both' enter t<enter> to get into trigger mode and then press 'b' press <enter> to exit
* start the display with (sr 2n <enter>) that starts the display on pin 2 in normal mode
* next turn on the pullups (P <enter>) you should see trace with a rising edge
* and turn them off with a (p <enter>) you should see an exponential decay curve
* turn them on again with P then enter trigger mode with t and then enter + to only
trigger on rising edges then <enter> to leave trigger mode
* start a clock on pin 2 with (G <enter> 2 <enter> 1ms <enter> 33% <enter>)
* enter x mode (x <enter>) you should see a bunch of square waves, try + and - to change the scale, and the left and right arrows to move around. Freeze a trace by typing 'o'.
* enter y mode and change the voltage scale with + and - (and use the up down arrows to move the display)

## Limitations and bugs

We're very much limited by the hardware:

* we can't make more than 500k samples/sec
* 12 bits 
* tiny display
* no way to make a dual scope (we might be able to have a digital trigger in the future)

There are bugs

* at the moment integration with the rest of the BP software is poor - in particular the analog subsystem
gets locked out when the scope is grabbing samples
* the scope does trigger processing from an ISR (might have to always do that)

