#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef NOTIFY
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "dates.hpp"

#define CLEAR_LINE "\033[K\r"
#define SAVE_CURSOR "\033[s"
#define RESTORE_CURSOR "\033[u"

#define NLINES_UP(n) "\033[" #n "F"

#define print_above(up_count, fmt, ...) \
    printf(SAVE_CURSOR NLINES_UP(up_count) CLEAR_LINE fmt RESTORE_CURSOR, __VA_ARGS__); \
    fflush(stdout);

#ifdef DEBUG
#define FOCUS_TIME 30.0f
#define BREAK_TIME 5.0f
#define LONG_BREAK_TIME 15.0f
#else
#define FOCUS_TIME 25.0f * 60.0f
#define BREAK_TIME 5.0f * 60.0f
#define LONG_BREAK_TIME 15.0f * 60.0f
#endif

enum class PomoState {
    Focus,
    Break,
    LongBreak,
};

enum class TimerState {
    Running,
    Stopped,
    Paused,
};

struct App {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    bool quit = false;

    PomoState pomo = PomoState::Focus;
    TimerState timer = TimerState::Stopped;

    float time_remaining = FOCUS_TIME;

    int focus_count = 0;
    int break_count = 0;

    float focus_total = 0;
    float break_total = 0;
};

const char* pomo_state_str(PomoState p);
const char* timer_state_str(TimerState t);

void* sleep_and_update(void *data);

void save_timings(App *app);

inline bool str_equal(const char *str, const char *str2) {
    return strcmp(str, str2) == 0;
}

int main(void) {
    char s[16] = {0};

    App app;

    pthread_t thread1;

    pthread_create(&thread1, NULL, sleep_and_update, (void *)&app);

    int minutes = int(app.time_remaining / 60.0f);
    float seconds = app.time_remaining - float(minutes * 60);

    printf("   %s #%d (%s)\n", pomo_state_str(app.pomo), 1, timer_state_str(app.timer));
    printf("   %02d:%05.02f\n> ", minutes, seconds);

    while (!app.quit) {
        fgets(s, 16, stdin);

        int len = strlen(s);
        if (len > 1) {
            s[len - 1] = '\0';
        } else {
            s[0] = '\0';
        }

        printf(NLINES_UP(1) CLEAR_LINE "> ");

        pthread_mutex_lock(&app.mutex);

        TimerState old_timer = app.timer;

        switch (app.timer) {
        case TimerState::Running:
            if (str_equal(s, "stop")) {
                app.timer = TimerState::Stopped;
            } else if (str_equal(s, "pause")) {
                app.timer = TimerState::Paused;
            }
            break;
        case TimerState::Stopped:
            if (str_equal(s, "start")) {
                app.timer = TimerState::Running;

                switch (app.pomo) {
                case PomoState::Focus:
                    app.focus_count += 1;
                    break;
                case PomoState::Break:
                case PomoState::LongBreak:
                    app.break_count += 1;
                    break;
                }
            }
            break;
        case TimerState::Paused:
            if (str_equal(s, "continue")) {
                app.timer = TimerState::Running;
            } else if (str_equal(s, "stop")) {
                app.timer = TimerState::Stopped;
            }
            break;
        }

        PomoState old_pomo = app.pomo;

        switch (app.pomo) {
        case PomoState::Focus:
            if (str_equal(s, "break")) {
                app.pomo = PomoState::Break;
                app.timer = TimerState::Stopped;

                app.focus_total += FOCUS_TIME - app.time_remaining;
                app.time_remaining = BREAK_TIME;
            } else if (str_equal(s, "lbreak")) {
                app.pomo = PomoState::LongBreak;
                app.timer = TimerState::Stopped;

                app.focus_total += FOCUS_TIME - app.time_remaining;
                app.time_remaining = LONG_BREAK_TIME;
            }
            break;
        case PomoState::Break:
        case PomoState::LongBreak:
            if (str_equal(s, "focus")) {
                app.pomo = PomoState::Focus;
                app.timer = TimerState::Stopped;

                app.break_total += app.pomo == PomoState::Break
                    ? BREAK_TIME - app.time_remaining
                    : LONG_BREAK_TIME - app.time_remaining;

                app.time_remaining = FOCUS_TIME;
            }
        }

        app.quit = str_equal(s, "q");

        if (old_timer != app.timer || old_pomo != app.pomo) {
            minutes = int(app.time_remaining / 60.0f);
            seconds = app.time_remaining - float(minutes * 60);

            int count = app.pomo == PomoState::Focus
                ? app.focus_count
                : app.break_count;

            // Only when timer start the counter is incremented, this ensure the right
            // count before the timer starts
            if (old_pomo != app.pomo) {
                count += 1;
            }

            print_above(1, "   %02d:%05.02f", minutes, seconds);
            print_above(
                2,
                "   %s #%d (%s)\n",
                pomo_state_str(app.pomo),
                count,
                timer_state_str(app.timer)
            );
        }

        pthread_mutex_unlock(&app.mutex);
    }

    pthread_join(thread1, NULL);

    printf("\n");

    switch (app.pomo) {
    case PomoState::Focus:
        app.focus_total += FOCUS_TIME - app.time_remaining;
        break;
    case PomoState::Break:
    case PomoState::LongBreak:
        app.break_total += app.pomo == PomoState::Break
            ? BREAK_TIME - app.time_remaining
            : LONG_BREAK_TIME - app.time_remaining;
        break;
    }

    save_timings(&app);

    return 0;
}

void *sleep_and_update(void *data) {
    App *app = (App *) data;

    struct timespec spec;
    spec.tv_sec = 0;
    spec.tv_nsec = 1e9 / 4;

    while (!app->quit) {
        if (nanosleep(&spec , NULL) < 0) {
            printf("\n\n\nNano sleep system call failed \n");

            pthread_mutex_lock(&app->mutex);
            app->quit = true;
            pthread_mutex_unlock(&app->mutex);
            break;
        }

#ifdef NOTIFY
        bool notify = false;
#endif

        if (app->timer == TimerState::Running) {
            pthread_mutex_lock(&app->mutex);

            app->time_remaining -= 0.25f;

            if (app->time_remaining <= 0.0f) {
                switch (app->pomo) {
                case PomoState::Focus:
                    app->focus_total += FOCUS_TIME;

                    if (app->focus_count % 3 == 0) {
                        app->pomo = PomoState::LongBreak;
                        app->time_remaining = LONG_BREAK_TIME;
                    } else {
                        app->pomo = PomoState::Break;
                        app->time_remaining = BREAK_TIME;
                    }
                    break;
                case PomoState::Break:
                    app->pomo = PomoState::Focus;
                    app->time_remaining = FOCUS_TIME;
                    app->break_total += BREAK_TIME;
                    break;
                case PomoState::LongBreak:
                    app->pomo = PomoState::Focus;
                    app->time_remaining = FOCUS_TIME;
                    app->break_total += LONG_BREAK_TIME;
                    break;
                }

                app->timer = TimerState::Stopped;

                int count = app->pomo == PomoState::Focus
                    ? app->focus_count
                    : app->break_count;

                // Only when timer start the counter is incremented, this ensure the right
                // count before the timer starts
                if (
                    app->time_remaining == FOCUS_TIME ||
                    app->time_remaining == BREAK_TIME ||
                    app->time_remaining == LONG_BREAK_TIME
                ) {
                    count += 1;
                }

                print_above(
                    2,
                    "   %s #%d (%s)\n",
                    pomo_state_str(app->pomo),
                    count,
                    timer_state_str(app->timer)
                );

#ifdef NOTIFY
                notify = true;
#endif
            }

            int minutes = int(app->time_remaining / 60.0f);
            float seconds = app->time_remaining - float(minutes * 60);

            print_above(1, "   %02d:%05.02f", minutes, seconds);

            pthread_mutex_unlock(&app->mutex);
        }

#ifdef NOTIFY
        if (notify) {
            const char *msg = "";

            switch (app->pomo) {
            case PomoState::Focus:
                msg = "Time to Focus";
                break;
            case PomoState::Break:
                msg = "Time for a Break";
                break;
            case PomoState::LongBreak:
                msg = "Time for a Long Break";
                break;
            }

            pid_t pid = fork();
            if (pid == 0) {
                execl("/usr/bin/notify-send", "notify-send", msg, NULL);
            } else {
                waitpid(pid, NULL, 0);
            }
        }
#endif
    }

    return NULL;
}

void save_timings(App *app) {
    FILE *file = fopen("pomodoros.txt", "a");

    if (file == NULL) {
        printf("\n\n\nCould now open file 'pomodoros.txt'.\n");
        return;
    }

    dates::datetime_t now = dates::get_current_date();

    char buff[31] = {0};

    dates::write_iso_datetime(now, buff, 30);

    fprintf(file, "%s ", buff);
    fprintf(file, "%.2f ", app->focus_total);
    fprintf(file, "%.2f\n", app->break_total);

    fclose(file);
}

const char* pomo_state_str(PomoState p) {
    switch (p) {
    case PomoState::Focus:
        return "Focus";
    case PomoState::Break:
        return "Break";
    case PomoState::LongBreak:
        return "Long Break";
    }

    return "??";
}

const char* timer_state_str(TimerState t) {
    switch (t) {
    case TimerState::Running:
        return "Running";
    case TimerState::Stopped:
        return "Stopped";
    case TimerState::Paused:
        return "Paused";
    }

    return "??";
}

