#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINES 1000
#define MAX_COLS  256

char lines[MAX_LINES][MAX_COLS];
int  nlines = 0;

const char *filename;
int  dirty = 0;
int  quit_armed = 0;

int  top = 0;
int  fy = 0, fx = 0;
int  rows, cols;
char status_msg[128];

int load_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return -1;
    }
    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_COLS, fp) != NULL) {
        int len = strlen(lines[nlines]);
        if (len > 0 && lines[nlines][len - 1] == '\n')
            lines[nlines][len - 1] = '\0';
        nlines++;
    }
    fclose(fp);
    if (nlines == 0) {
        lines[0][0] = '\0';
        nlines = 1;
    }
    return 0;
}

int save_file(void)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
        return -1;
    if (nlines == 1 && lines[0][0] == '\0') {
    } else {
        for (int i = 0; i < nlines; i++) {
            fputs(lines[i], fp);
            fputc('\n', fp);
        }
    }
    fclose(fp);
    return 0;
}

void insert_char(int ch)
{
    int len = strlen(lines[fy]);
    if (len >= MAX_COLS - 1)
        return;
    memmove(&lines[fy][fx + 1], &lines[fy][fx], len - fx + 1);
    lines[fy][fx] = (char)ch;
    fx++;
    dirty = 1;
}

void backspace(void)
{
    int len = strlen(lines[fy]);
    if (fx > 0) {
        memmove(&lines[fy][fx - 1], &lines[fy][fx], len - fx + 1);
        fx--;
        dirty = 1;
    } else if (fy > 0) {
        int prev = strlen(lines[fy - 1]);
        if (prev + len < MAX_COLS) {
            strcat(lines[fy - 1], lines[fy]);
            for (int i = fy; i < nlines - 1; i++)
                strcpy(lines[i], lines[i + 1]);
            nlines--;
            fy--;
            fx = prev;
            dirty = 1;
        }
    }
}

void delete_char(void)
{
    int len = strlen(lines[fy]);
    if (fx < len) {
        memmove(&lines[fy][fx], &lines[fy][fx + 1], len - fx);
        dirty = 1;
    } else if (fy < nlines - 1) {
        int next = strlen(lines[fy + 1]);
        if (len + next < MAX_COLS) {
            strcat(lines[fy], lines[fy + 1]);
            for (int i = fy + 1; i < nlines - 1; i++)
                strcpy(lines[i], lines[i + 1]);
            nlines--;
            dirty = 1;
        }
    }
}

void insert_newline(void)
{
    if (nlines >= MAX_LINES)
        return;
    int len = strlen(lines[fy]);

    for (int i = nlines; i > fy + 1; i--)
        strcpy(lines[i], lines[i - 1]);
    nlines++;

    if (fx >= len) {
        lines[fy + 1][0] = '\0';
    } else {
        strcpy(lines[fy + 1], &lines[fy][fx]);
        lines[fy][fx] = '\0';
    }
    fy++;
    fx = 0;
    dirty = 1;
}

void scroll_to_cursor(void)
{
    int visible = rows - 1;
    if (fy < top)
        top = fy;
    if (fy >= top + visible)
        top = fy - visible + 1;
    if (top < 0)
        top = 0;
}

void draw_screen(void)
{
    int visible = rows - 1;
    scroll_to_cursor();

    erase();

    for (int i = 0; i < visible; i++) {
        int line = top + i;
        if (line < nlines)
            mvaddnstr(i, 0, lines[line], cols);
    }

    char buf[256];
    if (status_msg[0] != '\0')
        snprintf(buf, sizeof buf, "%s", status_msg);
    else
        snprintf(buf, sizeof buf, "[EDIT] %s | %s | %d:%d", filename,
                 dirty ? "Modified" : "Saved", fy + 1, fx + 1);
    attron(A_REVERSE);
    mvaddnstr(rows - 1, 0, buf, cols);
    clrtoeol();
    attroff(A_REVERSE);

    move(fy - top, fx < cols ? fx : cols - 1);
    refresh();
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s <filename>\n", argv[0]);
        return 1;
    }
    if (load_file(argv[1]) == -1)
        return 1;
    filename = argv[1];

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    getmaxyx(stdscr, rows, cols);

    draw_screen();

    int ch;
    while (1) {
        ch = getch();
        status_msg[0] = '\0';

        if (ch == 17) {
            if (dirty && !quit_armed) {
                quit_armed = 1;
                strcpy(status_msg,
                       "Unsaved changes! Press Ctrl-Q again to quit without saving.");
            } else {
                break;
            }
        } else {
            quit_armed = 0;
            if (ch == 19) {
                if (save_file() == 0) {
                    dirty = 0;
                    strcpy(status_msg, "Saved.");
                } else {
                    snprintf(status_msg, sizeof status_msg,
                             "Save failed: %s", strerror(errno));
                }
            } else if (ch == KEY_UP) {
                if (fy > 0) fy--;
            } else if (ch == KEY_DOWN) {
                if (fy < nlines - 1) fy++;
            } else if (ch == KEY_LEFT) {
                if (fx > 0) fx--;
            } else if (ch == KEY_RIGHT) {
                if (fx < (int)strlen(lines[fy])) fx++;
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                backspace();
            } else if (ch == KEY_DC) {
                delete_char();
            } else if (ch == 10 || ch == 13 || ch == KEY_ENTER) {
                insert_newline();
            } else if (ch == KEY_RESIZE) {
                getmaxyx(stdscr, rows, cols);
            } else if (ch >= 32 && ch <= 126) {
                insert_char(ch);
            }

            if (fx > (int)strlen(lines[fy]))
                fx = (int)strlen(lines[fy]);
        }

        draw_screen();
    }

    endwin();
    return 0;
}
