/**
 * @file freq.c
 * @brief Frequency measurement command implementation.
 * @details Implements the f/F commands for measuring signal frequency and duty cycle:
 *          - f: Single measurement
 *          - F: Continuous measurement with real-time updates
 *          
 *          Command syntax:
 *          - f [pin]: Single measurement on pin (or menu if no pin)
 *          - F [pin]: Continuous measurement on pin (or menu if no pin)
 *          
 *          Measurement method:
 *          - Uses RP2040 PWM hardware in counter mode
 *          - Can measure period and duty cycle simultaneously
 *          - Supports frequencies from ~1Hz to several MHz
 *          - Only available on pins with PWM channel B
 *          
 *          Limitations:
 *          - Cannot share PWM slice with active PWM output
 *          - Cannot measure on pins 0-1 if PSU uses PWM (hardware conflict)
 */

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "command_struct.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_info.h"
#include "lib/bp_args/bp_cmd.h"

#include "commands/global/freq.h"

static const char* const freq_single_usage[] = {
    "f [pin]",
    "Measure frequency once:%s f",
    "Measure frequency on pin 2:%s f 2",
};

static const char* const freq_cont_usage[] = {
    "F [pin]",
    "Continuous frequency measurement:%s F",
    "Continuous frequency on pin 2:%s F 2",
};

static const bp_command_positional_t freq_positionals[] = {
    { "pin", "pin", 0, false },
    { 0 }
};

const bp_command_def_t freq_single_def = {
    .name         = "f",
    .description  = T_CMDLN_FREQ_ONE,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .positionals      = freq_positionals,
    .positional_count = 1,
    .usage        = freq_single_usage,
    .usage_count  = count_of(freq_single_usage),
};

const bp_command_def_t freq_cont_def = {
    .name         = "F",
    .description  = T_CMDLN_FREQ_CONT,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .positionals      = freq_positionals,
    .positional_count = 1,
    .usage        = freq_cont_usage,
    .usage_count  = count_of(freq_cont_usage),
};

void freq_single(struct command_result* res) {
    if (bp_cmd_help_check(&freq_single_def, res->help_flag)) {
        return;
    }
    uint32_t temp;
    bool has_value = bp_cmd_get_positional_uint32(&freq_single_def, 1, &temp);
    if (!has_value) // show config menu
    {
        if (!freq_configure_disable()) {
            res->error = 1;
        }
    } else // single measurement
    {
        if (!freq_measure(temp, false)) {
            res->error = 1;
        }
    }
}

void freq_cont(struct command_result* res) {
    if (bp_cmd_help_check(&freq_cont_def, res->help_flag)) {
        return;
    }
    uint32_t temp;
    bool has_value = bp_cmd_get_positional_uint32(&freq_cont_def, 1, &temp);
    if (!has_value) // show config menu
    {
        if (!freq_configure_enable()) {
            res->error = 1;
        }
    } else // continuous measurement
    {
        if (!freq_measure(temp, true)) {
            res->error = 1;
        }
    }
}

bool freq_check_pin_is_available(const struct ui_prompt* menu, uint32_t* i) {
    // 1=channel b can measure frequency
    // label should be 0, not in use
    // PWM on the A channel should not be in use!

    // bounds check
    if ((*i) >= count_of(bio2bufiopin)) {
        return 0;
    }

    // temp fix for power supply PWM sharing
    #ifdef BP_HW_PSU_PWM_IO_BUG
        if ((*i) == 0 || (*i) == 1) {
            return 0;
        }
    #endif

    return (pwm_gpio_to_channel(bio2bufiopin[(*i)]) == PWM_CHAN_B && system_config.pin_labels[(*i) + 1] == 0 &&
            !(system_config.pwm_active & (0b11 << ((uint8_t)((*i) % 2 ? (*i) - 1 : (*i))))));
}

bool freq_check_pin_is_active(const struct ui_prompt* menu, uint32_t* i) {
    return (system_config.freq_active & (0x01 << ((uint8_t)(*i))));
}

// TODO: use PIO to do measurements/PWM on all A channels, making it fully flexible
uint32_t freq_print(uint8_t pin, bool refresh) {
    // now do single or continuous measurement on the pin
    float measured_period = freq_measure_period(bio2bufiopin[pin]);
    float measured_duty_cycle = freq_measure_duty_cycle(bio2bufiopin[pin]);

    float freq_friendly_value;
    uint8_t freq_friendly_units;
    freq_display_hz(&measured_period, &freq_friendly_value, &freq_friendly_units);

    float ns_friendly_value, freq_ns_value;
    uint8_t ns_friendly_units;
    if (measured_period == 0.f) {
        freq_ns_value = 0;
    } else {
        freq_ns_value = ((float)1 / (float)(measured_period)) * (float)1000000000;
    }

    freq_display_ns(&freq_ns_value, &ns_friendly_value, &ns_friendly_units);

    printf("\e[0K%s%s%s IO%s%d%s: %s%.2f%s%s %s%.2f%s%s (%s%.0f%sHz), %s%s:%s %s%.1f%s%%\r%s",
           ui_term_color_info(),
           GET_T(T_MODE_FREQ_FREQUENCY),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           pin,
           ui_term_color_reset(),
           ui_term_color_num_float(),
           freq_friendly_value,
           ui_term_color_reset(),
           ui_const_freq_labels[freq_friendly_units],
           ui_term_color_num_float(),
           ns_friendly_value,
           ui_term_color_reset(),
           ui_const_freq_labels[ns_friendly_units],
           ui_term_color_num_float(),
           measured_period,
           ui_term_color_reset(),
           ui_term_color_info(),
           GET_T(T_MODE_FREQ_DUTY_CYCLE),
           ui_term_color_reset(),
           ui_term_color_num_float(),
           measured_duty_cycle * 100.f,
           ui_term_color_reset(),
           (refresh ? "" : "\n"));

    return 1;
}

// F.null - setup continuous frequency measurement on IO pin
uint32_t freq_configure_enable(void) {
    uint32_t pin;

    printf("%s%s%s", ui_term_color_info(), GET_T(T_MODE_FREQ_MEASURE_FREQUENCY), ui_term_color_reset());

    static const struct ui_prompt_config freq_menu_enable_config = {
        false, false, false, true, &ui_prompt_menu_bio_pin, &ui_prompt_prompt_ordered_list, &freq_check_pin_is_available
    };

    static const struct ui_prompt freq_menu_enable = { T_MODE_CHOOSE_AVAILABLE_PIN, 0, 0, 0, 0, 0, 0, 0,
                                                       &freq_menu_enable_config };

    prompt_result result;
    ui_prompt_uint32(&result, &freq_menu_enable, &pin);

    if (result.exit) {
        return 0;
    }

    if (result.error) {
        // no pins available, show user current pinout
        // TODO: instead show channel/slice representation and explain assigned function
        ui_info_print_pin_names();
        ui_info_print_pin_labels();
        return 0;
    }

    // register the freq active, apply the pin label
    system_bio_update_purpose_and_label(true, (uint8_t)pin, BP_PIN_FREQ, ui_const_pin_states[4]);
    system_set_active(true, (uint8_t)pin, &system_config.freq_active);

    printf("\r\n%s%s:%s %s on IO%s%d%s\r\n",
           ui_term_color_notice(),
           GET_T(T_MODE_FREQ_MEASURE_FREQUENCY),
           ui_term_color_reset(),
           GET_T(T_MODE_ENABLED),
           ui_term_color_num_float(),
           (uint8_t)pin,
           ui_term_color_reset());

    // initial measurement
    freq_print((uint8_t)pin, false);
    return 1;
}

// f.null - disable an active frequency measurement, if there is only one active disable it, else prompt for the IO to
// disable
uint32_t freq_configure_disable(void) {
    uint32_t pin;
    if (system_config.freq_active == 0) // no freq active, just exit
    {
        printf("Error: Frequency measurement not active on any IO pins");
        return 0;
    } // if only one freq is active, just disable that one
    else if (system_config.freq_active && !(system_config.freq_active & (system_config.freq_active - 1))) {
        // set freq to disable
        pin = log2(system_config.freq_active);
    } else // multiple freqs active, show menu for user to choose which to disable
    {
        printf("%s%s%s", ui_term_color_info(), "Disable frequency measurement", ui_term_color_reset());

        static const struct ui_prompt_config freq_menu_disable_config = { false,
                                                                          false,
                                                                          false,
                                                                          true,
                                                                          &ui_prompt_menu_bio_pin,
                                                                          &ui_prompt_prompt_ordered_list,
                                                                          &freq_check_pin_is_active };

        static const struct ui_prompt freq_menu_disable = { T_MODE_CHOOSE_AVAILABLE_PIN, 0, 0, 0, 0, 0, 0, 0,
                                                            &freq_menu_disable_config };

        prompt_result result;
        ui_prompt_uint32(&result, &freq_menu_disable, &pin);

        if (result.exit) {
            return 0;
        }
    }

    // disable
    pwm_set_enabled(pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)pin]), false);
    gpio_set_function(bio2bufiopin[(uint8_t)pin], GPIO_FUNC_SIO);
    bio_input((uint8_t)pin);

    // unregister, remove pin label
    system_bio_update_purpose_and_label(false, (uint8_t)pin, 0, 0);
    system_set_active(false, (uint8_t)pin, &system_config.freq_active);

    printf("\r\n%s%s:%s %s on IO%s%d%s",
           ui_term_color_notice(),
           GET_T(T_MODE_FREQ_MEASURE_FREQUENCY),
           ui_term_color_reset(),
           GET_T(T_MODE_DISABLED),
           ui_term_color_num_float(),
           (uint8_t)pin,
           ui_term_color_reset());

    return 1;
}

// f.x or F.x
uint32_t freq_measure(int32_t pin, int refresh) {

    // first make sure the freq is present and available
    if (pin >= count_of(bio2bufiopin)) {
        printf("Pin IO%d is invalid!", pin);
        return 0;
    }
    // even pin with no freq
    if (pwm_gpio_to_channel(bio2bufiopin[pin]) != PWM_CHAN_B) {
        printf("IO%d has no frequency measurement hardware!\r\nFreq. measure is currently only possible on odd pins "
               "(1,3,5,7).\r\nIn the future we will fix this using the RP2040 PIO.\r\n",
               pin);
        return 0;
    }
    // this pin or adjacent pin is PWM
    if (system_config.pwm_active & (0b11 << ((uint8_t)(pin % 2 ? pin - 1 : pin)))) {
        printf("IO%d is unavailable beacuse IO%d or IO%d is in use as a frequency generator (PWM)!\r\nIn the future we "
               "will fix this using the RP2040 PIO\r\n",
               pin,
               pin - 1,
               pin);
        return 0;
    }
    // pin is in use for purposes other than freq
    if (system_config.pin_labels[pin + 1] != 0 && !(system_config.freq_active & (0x01 << ((uint8_t)pin)))) {
        printf("IO%d is in use by %s!\r\n", pin, system_config.pin_labels[pin + 1]);
        return 0;
    }

    // now do single or continuous measurement on the pin
    if (refresh) {
        // press any key to continue
        prompt_result result;
        ui_prompt_any_key_continue(&result, 250, &freq_print, pin, true);
    }

    // print once (also handles final \n for continuous mode)
    freq_print(pin, false);

    return 1;
}

// we pre-calculate the disable bitmask for the enable function
// to avoid blowing out active PWM when we're done with a FREQ measure
static volatile uint32_t freq_irq_disable_bitmask;
// frequency measurement
int64_t freq_timer_callback(alarm_id_t id, void* user_data) {
    // disable all at once (but leave any other PWM slices running)
    // turn off only the ones we turned on and don't turn on any other that weren't running already
    pwm_set_mask_enabled(freq_irq_disable_bitmask & pwm_hw->en);
    freq_irq_disable_bitmask = 0;
    for (uint8_t i = 0; i < count_of(bio2bufiopin); i++) {
        if (system_config.freq_active & (0x01 << i)) {
            uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[i]);
            gpio_set_function(bio2bufiopin[i], GPIO_FUNC_SIO);
            uint16_t counter = (uint16_t)pwm_get_counter(slice_num);
            float freq = counter * 100.f;
            system_config.freq_config[i].period = freq;
        }
    }
    return 0;
}

// trigger all frequency measurements at once using only one delay
void freq_measure_period_irq(void) {
    uint32_t mask = 0;

    // dont do anything if user has no active freq measurements
    if (system_config.freq_active == 0) {
        return;
    }
    //dont do anything if the counters are running
    if (freq_irq_disable_bitmask)
        return;

    for (uint8_t i = 0; i < count_of(bio2bufiopin); i++) {
        if (system_config.freq_active & (0x01 << i)) {
            uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[i]);

            pwm_config cfg = pwm_get_default_config();
            pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
            pwm_config_set_clkdiv(&cfg, 1);
            pwm_init(slice_num, &cfg, false);
            gpio_set_function(bio2bufiopin[i], GPIO_FUNC_PWM);
            pwm_set_counter(slice_num, 0);

            // build mask, slices 0-7...
            mask |= (0x01 << slice_num);
        }
    }

    freq_irq_disable_bitmask = ~mask;
    //set the mask and the ones already running
    pwm_set_mask_enabled(mask | pwm_hw->en);
    add_alarm_in_ms(10, freq_timer_callback, NULL, false);
}

const uint max_gate_time_ms = 100;

float freq_measure_period(uint gpio) {
    // Only the PWM B pins can be used as inputs.
    assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_enabled(slice_num, false);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
    pwm_config_set_clkdiv(&cfg, 1);
    pwm_init(slice_num, &cfg, false);
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    pwm_set_counter(slice_num, 0);

    pwm_set_enabled(slice_num, true);
    busy_wait_ms(1);
    pwm_set_enabled(slice_num, false);

    uint16_t counter = pwm_get_counter(slice_num);
    float freq = counter * 1000;
    if (counter < 0x8000) {
        uint gate_time_ms = MIN((65000 / (counter ? counter : 1)), max_gate_time_ms);
        pwm_set_counter(slice_num, 0);
        pwm_set_enabled(slice_num, true);
        busy_wait_ms(gate_time_ms);
        pwm_set_enabled(slice_num, false);
        counter = pwm_get_counter(slice_num);
        freq = 1000.0f * counter / gate_time_ms;
    }

    gpio_set_function(gpio, GPIO_FUNC_SIO);
    return freq;
}

float freq_measure_duty_cycle(uint gpio) {
    // Only the PWM B pins can be used as inputs.
    assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_enabled(slice_num, false);
    // Count once for every div cycles the PWM B input is high
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_HIGH);
    // div set for max counter value at 100% duty cycle
    uint div = (clock_get_hz(clk_sys) / 10 + 0xFFFE) / 0xFFFF;
    pwm_config_set_clkdiv(&cfg, div);
    pwm_init(slice_num, &cfg, false);
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    pwm_set_counter(slice_num, 0);
    pwm_set_enabled(slice_num, true);
    busy_wait_ms(100);
    pwm_set_enabled(slice_num, false);
    uint16_t counter = pwm_get_counter(slice_num);
    float counting_rate = clock_get_hz(clk_sys) / div;
    float max_possible_count = counting_rate * 0.1;
    gpio_set_function(gpio, GPIO_FUNC_SIO);

    return counter / max_possible_count;
}

void freq_display_hz(float* freq_hz_value, float* freq_friendly_value, uint8_t* freq_friendly_units) {
    // friendly frequency value
    uint32_t freq_friendly_divider;
    if ((uint32_t)(*freq_hz_value / 1000000) > 0) {
        freq_friendly_divider = 1000000;
        *freq_friendly_units = freq_mhz;
    } else if ((uint32_t)(*freq_hz_value / 1000) > 0) {
        freq_friendly_divider = 1000;
        *freq_friendly_units = freq_khz;
    } else {
        freq_friendly_divider = 1;
        *freq_friendly_units = freq_hz;
    }

    *freq_friendly_value = (float)((float)*freq_hz_value / (float)freq_friendly_divider);
}

void freq_display_ns(float* freq_ns_value, float* period_friendly_value, uint8_t* period_friendly_units) {
    // friendly period value
    uint32_t period_friendly_divider;
    if ((uint32_t)(*freq_ns_value / 1000000) > 0) {
        period_friendly_divider = 1000000;
        *period_friendly_units = freq_ms;
    } else if ((uint32_t)(*freq_ns_value / 1000) > 0) {
        period_friendly_divider = 1000;
        *period_friendly_units = freq_us;
    } else {
        period_friendly_divider = 1;
        *period_friendly_units = freq_ns;
    }
    *period_friendly_value = (*freq_ns_value / (float)period_friendly_divider);
}
/*
from micropython import const
import rp2
from rp2 import PIO, asm_pio

@asm_pio(sideset_init=PIO.OUT_HIGH)
def gate():
    """PIO to generate gate signal."""
    mov(x, osr)                                            # load gate time (in clock pulses) from osr
    wait(0, pin, 0)                                        # wait for input to go low
    wait(1, pin, 0)                                        # wait for input to go high - effectively giving us rising
edge detection label("loopstart") jmp(x_dec, "loopstart") .side(0)                       # keep gate low for time
programmed by setting x reg wait(0, pin, 0)                                        # wait for input to go low wait(1,
pin, 0) .side(1)                               # set gate to high on rising edge irq(block, 0) # set interrupt 0 flag
and wait for system handler to service interrupt wait(1, irq, 4)                                        # wait for irq
from clock counting state machine wait(1, irq, 5)                                        # wait for irq from pulse
counting state machine

@asm_pio()
def clock_count():
    """PIO for counting clock pulses during gate low."""
    mov(x, osr)                                            # load x scratch with max value (2^32-1)
    wait(1, pin, 0)                                        # detect falling edge
    wait(0, pin, 0)                                        # of gate signal
    label("counter")
    jmp(pin, "output")                                     # as long as gate is low //
    jmp(x_dec, "counter")                                  # decrement x reg (counting every other clock cycle - have to
multiply output value by 2) label("output") mov(isr, x)                                            # move clock count
value to isr push()                                                 # send data to FIFO irq(block, 4) # set irq and wait
for gate PIO to acknowledge

@asm_pio(sideset_init=PIO.OUT_HIGH)
def pulse_count():
    """PIO for counting incoming pulses during gate low."""
    mov(x, osr)                                            # load x scratch with max value (2^32-1)
    wait(1, pin, 0)
    wait(0, pin, 0) .side(0)                               # detect falling edge of gate
    label("counter")
    wait(0, pin, 1)                                        # wait for rising
    wait(1, pin, 1)                                        # edge of input signal
    jmp(pin, "output")                                     # as long as gate is low //
    jmp(x_dec, "counter")                                  # decrement x req counting incoming pulses (probably will
count one pulse less than it should - to be checked later) label("output") mov(isr, x) .side(1) # move pulse count value
to isr and set pin to high to tell clock counting sm to stop counting push() # send data to FIFO irq(block, 5) # set irq
and wait for gate PIO to acknowledge


def init_sm(freq, input_pin, gate_pin, pulse_fin_pin):
    """Starts state machines."""
    gate_pin.value(1)
    pulse_fin_pin.value(1)
    max_count = const((1 << 32) - 1)

    sm0 = rp2.StateMachine(0, gate, freq=freq, in_base=input_pin, sideset_base=gate_pin)
    sm0.put(freq)
    sm0.exec("pull()")

    sm1 = rp2.StateMachine(1, clock_count, freq=freq, in_base=gate_pin, jmp_pin=pulse_fin_pin)
    sm1.put(max_count)
    sm1.exec("pull()")

    sm2 = rp2.StateMachine(2, pulse_count, freq=freq, in_base=gate_pin, sideset_base = pulse_fin_pin, jmp_pin=gate_pin)
    sm2.put(max_count-1)
    sm2.exec("pull()")

    sm1.active(1)
    sm2.active(1)
    sm0.active(1)

    return sm0, sm1, sm2

if __name__ == "__main__":
    from machine import Pin
    import uarray as array

    update_flag = False
    data = array.array("I", [0, 0])
    def counter_handler(sm):
        print("IRQ")
        global update_flag
        if not update_flag:
            sm0.put(125_000)
            sm0.exec("pull()")
            data[0] = sm1.get() # clock count
            data[1] = sm2.get() # pulse count
            update_flag = True

    sm0, sm1, sm2 = init_sm(125_000_000, Pin(15, Pin.IN, Pin.PULL_UP), Pin(14, Pin.OUT), Pin(13, Pin.OUT))
    sm0.irq(counter_handler)

    print("Starting test")
    i = 0
    while True:
        if update_flag:
            clock_count = 2*(max_count - data[0]+1)
            pulse_count = max_count - data[1]
            freq = pulse_count * (125000208.6 / clock_count)
            print(i)
            print("Clock count: {}".format(clock_count))
            print("Input count: {}".format(pulse_count))
            print("Frequency:   {}".format(freq))
            i += 1
            update_flag = False
*/