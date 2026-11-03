# Sound and timing

## PC speaker

The sound driver programs PIT channel 2 and the PC speaker. Kernel callers can
use `sound_init()`, `play_sound(frequency)`, `nosound()`, and
`beep(frequency, duration_ms)` from `<driver/sound.h>`. `beep()` blocks for its
duration and then disables the tone. Boot and panic paths use short beeps.

This is tone output only; it is not a general audio device or file playback
system. Legacy references to OSF/audio playback are not backed by an implemented
OSF decoder.

## Delay helper

`void sleep_ms(uint32_t ms)` is declared in `<kernel/time.h>`. It provides a
busy-loop delay; it does not sleep a task, use a timer interrupt, or guarantee
wall-clock accuracy. The cooperative process scheduler does not preempt this
loop, and there is no timer-based sleep service. See
[sleep_ms reference](../time/sleep_ms.md).

## No userspace audio/timer API

The current syscall table has no sound, timer, or sleep syscall. `ok_yield()`
executes `pause` and switches to another runnable process when one is ready.
