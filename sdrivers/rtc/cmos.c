#include <stdint.h>
#include <kernel/ports.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

#define CMOS_SECONDS 0x00
#define CMOS_MINUTES 0x02
#define CMOS_HOURS   0x04
#define CMOS_DAY     0x07
#define CMOS_MONTH   0x08
#define CMOS_YEAR    0x09
#define CMOS_STATUS_A 0x0A
#define CMOS_STATUS_B 0x0B

#define CMOS_UIP 0x80
#define CMOS_BINARY_MODE 0x04
#define CMOS_24_HOUR_MODE 0x02

struct rtc_time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

static uint8_t cmos_read(uint8_t reg)
{
    port_byte_out(CMOS_ADDRESS, reg);
    return port_byte_in(CMOS_DATA);
}

static uint8_t bcd_to_binary(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static int rtc_wait_update(void)
{
    uint32_t timeout = 1000000;

    while ((cmos_read(CMOS_STATUS_A) & CMOS_UIP) && timeout != 0) {
        timeout--;
    }

    return timeout != 0;
}

static int rtc_read_raw(struct rtc_time *time)
{
    uint8_t status_b;
    uint8_t hour;

    if (!rtc_wait_update()) {
        return -1;
    }

    time->second = cmos_read(CMOS_SECONDS);
    time->minute = cmos_read(CMOS_MINUTES);
    hour = cmos_read(CMOS_HOURS);
    time->day = cmos_read(CMOS_DAY);
    time->month = cmos_read(CMOS_MONTH);
    time->year = cmos_read(CMOS_YEAR);

    status_b = cmos_read(CMOS_STATUS_B);

    /* Convert BCD values when the RTC is not in binary mode. */
    if (!(status_b & CMOS_BINARY_MODE)) {
        time->second = bcd_to_binary(time->second);
        time->minute = bcd_to_binary(time->minute);
        time->day = bcd_to_binary(time->day);
        time->month = bcd_to_binary(time->month);
        time->year = bcd_to_binary(time->year);

        hour = (uint8_t)((hour & 0x80) |
                         bcd_to_binary(hour & 0x7F));
    }

    /* Handle 12-hour and 24-hour formats. */
    if (!(status_b & CMOS_24_HOUR_MODE)) {
        uint8_t pm = hour & 0x80;
        hour &= 0x7F;

        if (hour == 12) {
            hour = 0;
        }

        if (pm) {
            hour = (uint8_t)(hour + 12);
        }
    }

    time->hour = hour;

    return 0;
}

int rtc_get_time(struct rtc_time *time)
{
    struct rtc_time first;
    struct rtc_time second;

    if (time == 0) {
        return -1;
    }

    /*
     * Read twice to avoid returning a partially updated time
     * if the RTC changes between register reads.
     */
    do {
        if (rtc_read_raw(&first) != 0 ||
            rtc_read_raw(&second) != 0) {
            return -1;
        }
    } while (
        first.second != second.second ||
        first.minute != second.minute ||
        first.hour != second.hour ||
        first.day != second.day ||
        first.month != second.month ||
        first.year != second.year
    );

    *time = second;

    return 0;
}