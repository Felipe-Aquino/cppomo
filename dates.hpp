#ifndef __DATES_H__
#define __DATES_H__

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

namespace dates {

template <typename Value>
struct option_t {
    bool is_none;
    Value value;
};

template <typename Value>
option_t<Value> opt_some(Value value) {
    return option_t<Value> { false, value };
}

template <typename Value>
option_t<Value> opt_none() {
    option_t<Value> opt;
    opt.is_none = true;

    return opt;
}

struct datetime_t {
    int milliseconds;
    int seconds;
    int minutes;
    int hours;
    int day;
    int month;
    int year;
    int tz;
};

void write_iso_datetime(const datetime_t &d, char *dest, int size) {
    if (size < 30) {
        return;
    }

    sprintf(
        dest,
        "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
        d.year,
        d.month,
        d.day,
        d.hours,
        d.minutes,
        d.seconds,
        d.milliseconds
    );

    if (d.tz > 0) {
        int min = (d.tz % 3600) / 60;
        int hr = (d.tz - d.tz % 3600) / 3600;

        sprintf(dest + 23, "+%02d:%02d", hr, min);
    } else if (d.tz < 0) {
        int tz = -d.tz;
        int min = (tz % 3600) / 60;
        int hr = (tz - tz % 3600) / 3600;

        sprintf(dest + 23, "-%02d:%02d", hr, min);
    } else {
        dest[23] = 'Z';
    }
}

datetime_t get_current_date(bool local_tz = false) {
    struct tm *ptm = nullptr;
    struct timespec ts;

    timespec_get(&ts, TIME_UTC);

    int tz = 0;

    if (!local_tz) {
        ptm = gmtime(&ts.tv_sec);
    } else {
        ptm = localtime(&ts.tv_sec);

        char buff[6] = {0};
        strftime(buff, 6, "%z", ptm);
        tz = atoi(buff);
        tz = 3600 * tz / 100 + 60 * (tz % 100);
    }

    return datetime_t {
        .milliseconds = int(ts.tv_nsec / 1000000L),
        .seconds = ptm->tm_sec,
        .minutes = ptm->tm_min,
        .hours = ptm->tm_hour,
        .day = ptm->tm_mday,
        .month = ptm->tm_mon + 1,
        .year = 1900 + ptm->tm_year,
        .tz = tz,
    };
}

bool is_digit(char c) {
    return '0' <= c && c <= '9';
}

bool atoi2(const char *str, int size, int *n) {
    int aux = 0;

    for (int i = 0; i < size; i++) {
        if (!is_digit(str[i])) {
            return false;
        }

        aux = 10 * aux + int(str[i] - '0');
    }

    *n = aux;

    return true;
}

int mask_match(const char *str, const char *mask, int *values) {
    int str_size = strlen(str);
    int mask_size = strlen(mask);

    int value_idx = 0;

    int sign = 1;
    bool reading_number = false;
    int number = 0;
    bool ok = true;

    int k = 0;

    for (int j = 0; j < mask_size; j += 1) {
        if (k >= str_size) {
            ok = false;
            break;
        }

        switch (mask[j]) {
            case '#': // digit
                reading_number = true;

                if (!is_digit(str[k])) {
                    ok = false;
                } else {
                    number = 10 * number + int(str[k] - '0');
                }
                break;
            case 'd': // Optinal digit
                if (reading_number) {
                    if (!is_digit(str[k])) {
                        // Hack: An easier way to deal with milliseconds
                        if (mask[j + 1] == 'd') {
                            number *= 100;
                        } else {
                            number *= 10;
                        }

                        values[value_idx] = sign * number;
                        value_idx += 1;

                        reading_number = false;
                        number = 0;
                        sign = 1;

                        k -= 1;
                    } else {
                        number = 10 * number + int(str[k] - '0');
                    }
                } else {
                    k -= 1;
                }

                break;
            case 'T': // T or space
                if (str[k] != 'T' && str[k] != ' ') {
                    ok = false;
                }
                break;
            case '+': // +/- sign
                if (str[k] != '+' && str[k] != '-') {
                    ok = false;
                } else if (str[k] == '-') {
                    sign = -1;
                }
                break;
            case 'Z':
            case '.': // dot
            case '-': // dash
            case ':': // colon
                if (str[k] != mask[j]) {
                    ok = false;
                }
                break;
            case 'c': // optional colon
                break;
            default:
                ok = false;
        }

        if (mask[j] == 'c') { // colon
            if (str[k] == ':') {
                k += 1;
            }

            continue;
        }

        if (!ok) {
            break;
        }

        if (reading_number && mask[j] != '#' && mask[j] != 'd') {
            values[value_idx] = sign * number;
            value_idx += 1;

            reading_number = false;
            number = 0;
            sign = 1;
        }

        k += 1;
    }

    if (!ok) {
        for (int i = 0; i < value_idx; i += 1) {
            values[value_idx] = 0;
        }

        return -1;
    }

    if (reading_number) {
        values[value_idx] = sign * number;
    }

    return k;
}

// TODO: Better error treatment
// TODO: Validate min/max values of day, month, hour, etc 
option_t<datetime_t> from_iso_string(const char *str) {
    int str_size = strlen(str);

    if (str_size < 10) {
        return opt_none<datetime_t>();
    }

    int values[8] = {0};

    const char *main_mask = "####-##-##T##:##";
    const char *sec_masks[] = {
        ":##.#dd",
        ":##",
    };

    const char *tz_masks[] = {
        "+##c##",
        "Z",
    };

    int count = mask_match(str, main_mask, values);

    if (count < 0) {
        return opt_none<datetime_t>();
    }

    for (int i = 0; i < 2; i += 1) {
        int k = mask_match(str + count, sec_masks[i], values + 5);

        if (k >= 0) {
            count += k;
            break;
        }
    }

    for (int i = 0; i < 2; i += 1) {
        int k = mask_match(str + count, tz_masks[i], values + 7);

        if (k >= 0) {
            count += k;
            break;
        }
    }

    if (count != str_size) {
        return opt_none<datetime_t>();
    }

    datetime_t dt = {
        .milliseconds = values[6],
        .seconds = values[5],
        .minutes = values[4],
        .hours = values[3],
        .day = values[2],
        .month = values[1],
        .year = values[0],
        .tz = 3600 * (values[7] / 100) + 60 * (values[7] % 100),
    };

    return opt_some<datetime_t>(dt);
}

}

#endif // __DATES_H__
