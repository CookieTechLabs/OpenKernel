#include <driver/keyboard.h>
#include <kernel/interrupts.h>
#include <kernel/ports.h>
#include <stdint.h>

#define PS2_DATA                 0x60
#define PS2_STATUS               0x64
#define PS2_COMMAND              0x64

#define PS2_OUTPUT_FULL          0x01
#define PS2_INPUT_FULL           0x02
#define PS2_AUX_DATA             0x20

#define KEYBOARD_ACK             0xFA
#define KEYBOARD_RESEND          0xFE

#define KEYBOARD_SET_LEDS        0xED
#define KEYBOARD_SET_TYPEMATIC   0xF3
#define KEYBOARD_ENABLE_SCAN     0xF4

#define KEYBOARD_TYPEMATIC_SLOW  0x7F

#define KEYBOARD_BUFFER_SIZE     128
#define KEYBOARD_BUFFER_MASK     (KEYBOARD_BUFFER_SIZE - 1)

#define KEY_LEFT_SHIFT           0x2A
#define KEY_RIGHT_SHIFT          0x36
#define KEY_CTRL                  0x1D
#define KEY_ALT                   0x38
#define KEY_CAPS_LOCK             0x3A
#define KEY_NUM_LOCK              0x45
#define KEY_SCROLL_LOCK           0x46

#define KEY_EXT_RIGHT_CTRL        0x1D
#define KEY_EXT_RIGHT_ALT         0x38

static const char keymap[128] = {
    [0x01] = 27,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
    [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x37] = '*',
    [0x39] = ' '
};

static const char shifted_keymap[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x1A] = '{', [0x1B] = '}', [0x27] = ':', [0x28] = '"',
    [0x29] = '~', [0x2B] = '|', [0x33] = '<', [0x34] = '>',
    [0x35] = '?'
};

static volatile char character_buffer[KEYBOARD_BUFFER_SIZE];

static volatile uint8_t buffer_head;
static volatile uint8_t buffer_tail;

static volatile uint8_t left_shift;
static volatile uint8_t right_shift;
static volatile uint8_t left_ctrl;
static volatile uint8_t right_ctrl;
static volatile uint8_t left_alt;
static volatile uint8_t right_alt;

static volatile uint8_t caps_lock;
static volatile uint8_t num_lock;
static volatile uint8_t scroll_lock;

static volatile uint8_t extended_scancode;
static volatile uint8_t pause_bytes_remaining;
static volatile uint8_t keyboard_ready;

static volatile uint8_t key_down[128];

static int wait_for_input_empty(void) {
    uint32_t timeout = 100000;

    while (timeout-- != 0) {
        if ((port_byte_in(PS2_STATUS) & PS2_INPUT_FULL) == 0) {
            return 1;
        }
    }

    return 0;
}

static int wait_for_output_full(void) {
    uint32_t timeout = 100000;

    while (timeout-- != 0) {
        if ((port_byte_in(PS2_STATUS) & PS2_OUTPUT_FULL) != 0) {
            return 1;
        }
    }

    return 0;
}

static int send_keyboard_command(uint8_t command) {
    for (uint32_t attempt = 0; attempt < 3; attempt++) {
        if (!wait_for_input_empty()) {
            return 0;
        }

        port_byte_out(PS2_DATA, command);

        if (!wait_for_output_full()) {
            return 0;
        }

        uint8_t response = port_byte_in(PS2_DATA);

        if (response == KEYBOARD_ACK) {
            return 1;
        }

        if (response != KEYBOARD_RESEND) {
            return 0;
        }
    }

    return 0;
}

static int send_keyboard_data(uint8_t data) {
    for (uint32_t attempt = 0; attempt < 3; attempt++) {
        if (!wait_for_input_empty()) {
            return 0;
        }

        port_byte_out(PS2_DATA, data);

        if (!wait_for_output_full()) {
            return 0;
        }

        uint8_t response = port_byte_in(PS2_DATA);

        if (response == KEYBOARD_ACK) {
            return 1;
        }

        if (response != KEYBOARD_RESEND) {
            return 0;
        }
    }

    return 0;
}

static void queue_character(char character) {
    uint8_t next = (uint8_t)(
        (buffer_head + 1) & KEYBOARD_BUFFER_MASK
    );

    if (next == buffer_tail) {
        return;
    }

    character_buffer[buffer_head] = character;
    buffer_head = next;
}

static void keyboard_update_leds(void) {
    uint8_t leds = 0;

    if (scroll_lock) {
        leds |= 0x01;
    }

    if (num_lock) {
        leds |= 0x02;
    }

    if (caps_lock) {
        leds |= 0x04;
    }

    if (!send_keyboard_command(KEYBOARD_SET_LEDS)) {
        return;
    }

    (void)send_keyboard_data(leds);
}

static void keyboard_update_modifier(uint8_t code,
                                     uint8_t released,
                                     uint8_t extended) {
    if (!extended) {
        if (code == KEY_LEFT_SHIFT) {
            left_shift = !released;
        } else if (code == KEY_RIGHT_SHIFT) {
            right_shift = !released;
        } else if (code == KEY_CTRL) {
            left_ctrl = !released;
        } else if (code == KEY_ALT) {
            left_alt = !released;
        }
    } else {
        if (code == KEY_EXT_RIGHT_CTRL) {
            right_ctrl = !released;
        } else if (code == KEY_EXT_RIGHT_ALT) {
            right_alt = !released;
        }
    }
}

static void keyboard_process_scancode(uint8_t scancode) {
    uint8_t released;
    uint8_t code;
    uint8_t extended;
    uint8_t shift;
    char character;

    if (pause_bytes_remaining != 0) {
        pause_bytes_remaining--;
        return;
    }

    if (scancode == 0xE1) {
        pause_bytes_remaining = 5;
        extended_scancode = 0;
        return;
    }

    if (scancode == 0xE0) {
        extended_scancode = 1;
        return;
    }

    extended = extended_scancode;
    extended_scancode = 0;

    released = (scancode & 0x80) != 0;
    code = scancode & 0x7F;

    if (extended) {
        keyboard_update_modifier(code, released, 1);

        /* Extended navigation keys are not ASCII characters. */
        return;
    }

    keyboard_update_modifier(code, released, 0);

    if (code == KEY_CAPS_LOCK) {
        if (!released && !key_down[code]) {
            caps_lock ^= 1;
            keyboard_update_leds();
        }

        key_down[code] = !released;
        return;
    }

    if (code == KEY_NUM_LOCK) {
        if (!released && !key_down[code]) {
            num_lock ^= 1;
            keyboard_update_leds();
        }

        key_down[code] = !released;
        return;
    }

    if (code == KEY_SCROLL_LOCK) {
        if (!released && !key_down[code]) {
            scroll_lock ^= 1;
            keyboard_update_leds();
        }

        key_down[code] = !released;
        return;
    }

    if (released) {
        key_down[code] = 0;
        return;
    }

    /*
     * Do not suppress repeated make codes.
     * The keyboard controller generates these while a key is held.
     */
    key_down[code] = 1;

    if (code >= sizeof(keymap) || keymap[code] == 0) {
        return;
    }

    shift = left_shift || right_shift;

    character = keymap[code];

    if (character >= 'a' && character <= 'z') {
        if (shift ^ caps_lock) {
            character = (char)(character - 'a' + 'A');
        }
    } else if (shift && shifted_keymap[code] != 0) {
        character = shifted_keymap[code];
    }

    if (left_ctrl || right_ctrl) {
        if (character >= 'a' && character <= 'z') {
            character = (char)(character - 'a' + 1);
        } else if (character >= 'A' && character <= 'Z') {
            character = (char)(character - 'A' + 1);
        }
    }

    queue_character(character);
}

int init_keyboard(void) {
    uint8_t config;

    keyboard_ready = 0;

    __asm__ volatile ("cli" ::: "memory");

    if (!wait_for_input_empty()) {
        return -1;
    }

    port_byte_out(PS2_COMMAND, 0xAD);

    if (!wait_for_input_empty()) {
        return -1;
    }

    port_byte_out(PS2_COMMAND, 0xAA);

    if (!wait_for_output_full()) {
        return -1;
    }

    if (port_byte_in(PS2_DATA) != 0x55) {
        return -1;
    }

    if (!wait_for_input_empty()) {
        return -1;
    }

    port_byte_out(PS2_COMMAND, 0x20);

    if (!wait_for_output_full()) {
        return -1;
    }

    config = port_byte_in(PS2_DATA);

    config |= 0x01;
    config &= (uint8_t)~0x10;

    if (!wait_for_input_empty()) {
        return -1;
    }

    port_byte_out(PS2_COMMAND, 0x60);

    if (!wait_for_input_empty()) {
        return -1;
    }

    port_byte_out(PS2_DATA, config);

    if (!wait_for_input_empty()) {
        return -1;
    }

    port_byte_out(PS2_COMMAND, 0xAE);

    if (!wait_for_input_empty()) {
        return -1;
    }

    /* Select scan code set 1. */
    if (send_keyboard_command(0xF0)) {
        (void)send_keyboard_data(0x01);
    }

    /* Configure a slower typematic rate. */
    if (send_keyboard_command(KEYBOARD_SET_TYPEMATIC)) {
        (void)send_keyboard_data(KEYBOARD_TYPEMATIC_SLOW);
    }

    /* Clear modifier and lock state. */
    left_shift = 0;
    right_shift = 0;
    left_ctrl = 0;
    right_ctrl = 0;
    left_alt = 0;
    right_alt = 0;

    caps_lock = 0;
    num_lock = 0;
    scroll_lock = 0;

    extended_scancode = 0;
    pause_bytes_remaining = 0;

    buffer_head = 0;
    buffer_tail = 0;

    for (uint32_t i = 0; i < 128; i++) {
        key_down[i] = 0;
    }

    keyboard_ready = 1;

    if (!send_keyboard_command(KEYBOARD_ENABLE_SCAN)) {
        keyboard_ready = 0;
        return -1;
    }

    interrupts_enable_irq(1);

    return 0;
}

void keyboard_irq_handler(void) {
    uint8_t status = port_byte_in(PS2_STATUS);

    if (!keyboard_ready) {
        return;
    }

    if ((status & PS2_OUTPUT_FULL) == 0) {
        return;
    }

    if (status & PS2_AUX_DATA) {
        return;
    }

    keyboard_process_scancode(port_byte_in(PS2_DATA));
}

int keyboard_try_get_char(char *character) {
    uint32_t flags;

    if (character == 0 || !keyboard_ready) {
        return 0;
    }

    __asm__ volatile (
        "pushfl\n"
        "popl %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    if (buffer_tail == buffer_head) {
        __asm__ volatile (
            "pushl %0\n"
            "popfl"
            :
            : "r"(flags)
            : "memory", "cc"
        );

        return 0;
    }

    *character = character_buffer[buffer_tail];

    buffer_tail = (uint8_t)(
        (buffer_tail + 1) & KEYBOARD_BUFFER_MASK
    );

    __asm__ volatile (
        "pushl %0\n"
        "popfl"
        :
        : "r"(flags)
        : "memory", "cc"
    );

    return 1;
}

char keyboard_wait_and_get_char(void) {
    char character;

    while (!keyboard_try_get_char(&character)) {
        __asm__ volatile ("pause" ::: "memory");
    }

    return character;
}