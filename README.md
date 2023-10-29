# Cppomo
A simple pomodoro terminal application

## About

- Only depends on libc
- Uses libnotify for notifications 
- Requires a terminal emulator that supports scape codes to work properly
- Saves break and focus times in a local file called pomodoros.txt

## Compilation and Usage

```bash
$ g++ -std=c++20 cppomo -DNOTIFY -DDEBUG -Wall -Wextra -Wpedantic -Werror -lpthread 
$ ./cppomo
```

The flags -DNOTIFY and -DDEBUG are optional.

If the state is stopped use the command **start**, and if it is paused use **continue**.
At any time is possible to use the commands **focus**, **break** or **lbreak** to select another pomodoro state.
