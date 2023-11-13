#ifndef C_ONSOLE
#define C_ONSOLE

/* Simple c library for text coloring */
/* Only tested on windows. */

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

enum Color
{
    BLACK,
    NAVY,
    GREEN,
    TEAL,
    MAROON,
    PURPLE,
    OLIVE,
    SILVER,
    GREY,
    BLUE,
    LIME,
    AQUA,
    RED,
    PINK,
    YELLOW,
    WHITE
};

struct Pair
{
    unsigned short x;
    unsigned short y;
};

void clear_console();
void reset_console();
void set_console_title(char *title);
void set_fg_color(enum Color color);
void set_bg_color(enum Color color);
void set_colors(enum Color foreground, enum Color background);
void set_cursor_position(struct Pair position);
struct Pair get_console_dimensions();

void set_console_dimensions(struct Pair dimensions);

#ifdef C_ONSOLE_IMPLEMENTATION
#ifdef _WIN32

static WORD windows_get_defaults(HANDLE console_h)
{
    static WORD defaults = 0;
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (!defaults && GetConsoleScreenBufferInfo(console_h, &info))
    {
        defaults = info.wAttributes;
    }

    return defaults;
}

static VOID windows_set_colors(INT fg, INT bg)
{
    HANDLE console_h = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD defaults = windows_get_defaults(console_h);
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (fg == bg && bg == -1)
    {
        SetConsoleTextAttribute(console_h, defaults);
        return;
    }

    if (!GetConsoleScreenBufferInfo(console_h, &info))
    {
        return;
    }

    if (fg != -1)
    {
        info.wAttributes &= ~(info.wAttributes & 0x0f);
        info.wAttributes |= (WORD)fg;
    }

    if (bg != -1)
    {
        info.wAttributes &= ~(info.wAttributes & 0xf0);
        info.wAttributes |= (WORD)bg;
    }

    SetConsoleTextAttribute(console_h, info.wAttributes);
}

static int fg_color_to_attr(enum Color color)
{
    switch (color)
    {
    case RED:
        return FOREGROUND_RED;
    case GREEN:
        return FOREGROUND_GREEN;
    case BLUE:
        return FOREGROUND_BLUE;
    case YELLOW:
        return FOREGROUND_RED | FOREGROUND_GREEN;
    case OLIVE:
        return FOREGROUND_RED | FOREGROUND_GREEN;
    case PURPLE:
        return FOREGROUND_RED | FOREGROUND_BLUE;
    case WHITE:
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    case BLACK:
        return 0;
    default:
        return -1;
    }
}

#else

static void nix_set_colors(enum Color foreground, enum Color background)
{
    if (foreground == background && background == -1)
    {
        printf("\033[0m");
        return;
    }

    if (foreground != -1)
    {
        printf("\033[%sm", nix_fg_color_to_str(foreground));
    }
}

static char *nix_fg_color_to_str(enum Color color)
{
    switch (color)
    {
    case RED:
        return "31";
    case GREEN:
        return "32";
    case BLUE:
        return "34";
    case YELLOW:
        return "33";
    case OLIVE:
        return "33";
    case PURPLE:
        return "35";
    default:
        return "0";
    }
}

#endif

void clear_console()
{
#ifdef _WIN32

    HANDLE console_h = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD defaults = windows_get_defaults(console_h);
    CONSOLE_SCREEN_BUFFER_INFO info;
    long unsigned int written;

    if (!GetConsoleScreenBufferInfo(console_h, &info))
    {
        return;
    }

    FillConsoleOutputAttribute(console_h, defaults, info.dwSize.X * info.dwSize.Y, (COORD){0, 0}, &written);
    FillConsoleOutputCharacter(console_h, ' ', info.dwSize.X * info.dwSize.Y, (COORD){0, 0}, &written);
    SetConsoleCursorPosition(console_h, (COORD){0, 0});

#else

    printf("\033[2J\033[1;1H");

#endif
}

void set_console_title(char *title)
{
#ifdef _WIN32
    SetConsoleTitle(title);
#else
    printf("\033]0;%s\007", title);
#endif
}

void set_fg_color(enum Color color)
{
    set_colors(color, -1);
}

void set_bg_color(enum Color color)
{
    set_colors(-1, color);
}

void set_colors(enum Color foreground, enum Color background)
{
#ifdef _WIN32
    windows_set_colors(fg_color_to_attr(foreground), fg_color_to_attr(background));
#else
    nix_set_colors(foreground, background);
#endif
}

void set_cursor_position(struct Pair position)
{
#ifdef _WIN32
    HANDLE console_h = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = {position.x, position.y};
    SetConsoleCursorPosition(console_h, coord);
#else
    printf("\033[%d;%dH", position.y, position.x);
#endif
}

struct Pair get_console_dimensions()
{
#ifdef _WIN32
    HANDLE console_h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (!GetConsoleScreenBufferInfo(console_h, &info))
    {
        return (struct Pair){0, 0};
    }

    return (struct Pair){info.dwSize.X, info.dwSize.Y};
#else
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    return (struct Pair){w.ws_col, w.ws_row};
#endif
}

void reset_console()
{
    set_colors(-1, -1);
    set_cursor_position((struct Pair){0, 0});

#ifdef _WIN32
    HANDLE console_h = GetStdHandle(STD_OUTPUT_HANDLE);
    WORD defaults = windows_get_defaults(console_h);
    SetConsoleTextAttribute(console_h, defaults);
#else
    printf("\033[0m");
#endif
}

void set_console_dimensions(struct Pair dimensions)
{
#ifdef _WIN32
    HANDLE console_h = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = {dimensions.x, dimensions.y};
    SMALL_RECT windowSize = {0, 0, dimensions.x - 1, dimensions.y - 1};
    SetConsoleScreenBufferSize(console_h, coord);
    SetConsoleWindowInfo(console_h, 1, &windowSize);
#else
    printf("\033[8;%d;%dt", dimensions.y, dimensions.x); // unreliable
#endif
}

#endif // C_ONSOLE_IMPLEMENTATION

#endif // C_ONSOLE
