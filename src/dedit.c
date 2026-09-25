#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINES 200
#define MAX_COLS  256

char lines[MAX_LINES][MAX_COLS];
int  nlines = 0;

int load_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
	{
        perror(path);
        return -1;
    	}


    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_COLS, fp) != NULL) 
	{
        int len = strlen(lines[nlines]);
        if (len > 0 && lines[nlines][len - 1] == '\n')
            lines[nlines][len - 1] = '\0';
        nlines++;
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s <filename>\n", argv[0]);
        return 1;
    }

    if (load_file(argv[1]) == -1)
        return 1;

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int cy = 0, cx = 0;

    for (int i = 0; i < nlines && i < rows; i++)
        mvaddnstr(i, 0, lines[i], cols);

    move(cy, cx);
    refresh();

    int ch;
    while ((ch = getch()) != 17)
	{
        if (ch == KEY_UP && cy > 0)
            cy--;
        else if (ch == KEY_DOWN && cy < rows - 1)
            cy++;
        else if (ch == KEY_LEFT && cx > 0)
            cx--;
        else if (ch == KEY_RIGHT && cx < cols - 1)
            cx++;
        else if (ch >= 32 && ch <= 126)
	{
            mvaddch(cy, cx, ch);
            if (cx < cols - 1)
                cx++;
        }

        move(cy, cx);
        refresh();
    }

    endwin();
    return 0;
}
