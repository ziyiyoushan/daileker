#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINES 1000
#define MAX_COLS  256
#define MAX_UNDO  100
#define MAX_WORD  256
#define MAX_MATCH 512
#define MAX_CLIP  (MAX_LINES * (MAX_COLS + 1))

char lines[MAX_LINES][MAX_COLS];
int  nlines = 0;

const char *filename;
int  dirty = 0;
int  quit_armed = 0;
int  done = 0;

int  top = 0;
int  fy = 0, fx = 0;
int  rows, cols;
char status_msg[128];

char snap_lines[MAX_UNDO + 1][MAX_LINES][MAX_COLS];
int  snap_nlines[MAX_UNDO + 1];
int  snap_count = 0;
int  snap_pos = 0;
int  snap_base = 0;

char clip[MAX_CLIP];
int  clip_len = 0;
int  sel_on = 0;
int  sel_ay = 0, sel_ax = 0;

enum { MODE_EDIT, MODE_SEARCH, MODE_R_INPUT, MODE_R_WORD, MODE_R_WALK };
int  mode = MODE_EDIT;

char query[MAX_WORD]; int qlen = 0;
char repl[MAX_WORD];  int rlen = 0;

int  s_org_y = 0, s_org_x = 0;
int  match_on = 0;
int  match_y = 0, match_x = 0;

int  cys[MAX_MATCH], cxs[MAX_MATCH];
int  prompt_cx = 0;

char bigbuf[MAX_CLIP + 2 * MAX_COLS + 2];

int load_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror(path);
        return -1;
    }
    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_COLS, fp) != NULL) {
        int len = (int)strlen(lines[nlines]);
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

void snap_store(int idx)
{
    int slot = (snap_base + idx) % (MAX_UNDO + 1);
    snap_nlines[slot] = nlines;
    for (int i = 0; i < nlines; i++)
        strcpy(snap_lines[slot][i], lines[i]);
}

void snap_restore(int idx)
{
    int slot = (snap_base + idx) % (MAX_UNDO + 1);
    nlines = snap_nlines[slot];
    for (int i = 0; i < nlines; i++)
        strcpy(lines[i], snap_lines[slot][i]);
}

void snap_push(void)
{
    if (snap_pos + 1 < snap_count)
        snap_count = snap_pos + 1;
    if (snap_count == MAX_UNDO + 1) {
        snap_base = (snap_base + 1) % (MAX_UNDO + 1);
        snap_count--;
        snap_pos--;
    }
    snap_store(snap_count);
    snap_count++;
    snap_pos = snap_count - 1;
}

void do_undo(void)
{
    if (snap_pos <= 0) {
        strcpy(status_msg, "Nothing to undo");
        return;
    }
    snap_pos--;
    snap_restore(snap_pos);
    sel_on = 0;
    if (fy >= nlines) fy = nlines - 1;
    if (fy < 0) fy = 0;
    if (fx > (int)strlen(lines[fy])) fx = (int)strlen(lines[fy]);
    dirty = 1;
    strcpy(status_msg, "Undo");
}

void do_redo(void)
{
    if (snap_pos >= snap_count - 1) {
        strcpy(status_msg, "Nothing to redo");
        return;
    }
    snap_pos++;
    snap_restore(snap_pos);
    sel_on = 0;
    if (fy >= nlines) fy = nlines - 1;
    if (fy < 0) fy = 0;
    if (fx > (int)strlen(lines[fy])) fx = (int)strlen(lines[fy]);
    dirty = 1;
    strcpy(status_msg, "Redo");
}


void clamp_cursor(void)
{
    if (fy >= nlines) fy = nlines - 1;
    if (fy < 0) fy = 0;
    if (fx > (int)strlen(lines[fy])) fx = (int)strlen(lines[fy]);
    if (fx < 0) fx = 0;
}

void order_sel(int *sy, int *sx, int *ey, int *ex)
{
    if (sel_ay < fy || (sel_ay == fy && sel_ax <= fx)) {
        *sy = sel_ay; *sx = sel_ax;
        *ey = fy;     *ex = fx;
    } else {
        *sy = fy;     *sx = fx;
        *ey = sel_ay; *ex = sel_ax;
    }
}

void sel_shift(int dy, int dx)
{
    if (!sel_on) {
        sel_on = 1;
        sel_ay = fy;
        sel_ax = fx;
    }
    if (dx > 0) {
        if (fx < (int)strlen(lines[fy]))
            fx++;
        else if (fy < nlines - 1) {
            fy++;
            fx = 0;
        }
    } else if (dx < 0) {
        if (fx > 0)
            fx--;
        else if (fy > 0) {
            fy--;
            fx = (int)strlen(lines[fy]);
        }
    } else if (dy > 0) {
        if (fy < nlines - 1) fy++;
    } else if (dy < 0) {
        if (fy > 0) fy--;
    }
    clamp_cursor();
}

int sel_extract(char *buf, int cap)
{
    int sy, sx, ey, ex, n = 0;
    order_sel(&sy, &sx, &ey, &ex);
    for (int y = sy; y <= ey; y++) {
        int len = (int)strlen(lines[y]);
        int c0 = (y == sy) ? sx : 0;
        int c1 = (y == ey) ? ex : len;
        if (c0 < 0) c0 = 0;
        if (c1 > len) c1 = len;
        for (int c = c0; c < c1 && n < cap - 1; c++)
            buf[n++] = lines[y][c];
        if (y < ey && n < cap - 1)
            buf[n++] = '\n';
    }
    buf[n] = '\0';
    return n;
}

void do_copy(void)
{
    if (!sel_on) {
        strcpy(status_msg, "No selection");
        return;
    }
    clip_len = sel_extract(clip, MAX_CLIP);
    if (clip_len == 0) {
        strcpy(status_msg, "Nothing selected");
        sel_on = 0;
        return;
    }
    snprintf(status_msg, sizeof status_msg, "Copied %d char(s)", clip_len);
    sel_on = 0;
}

void do_cut(void)
{
    int sy, sx, ey, ex;
    if (!sel_on) {
        strcpy(status_msg, "No selection");
        return;
    }
    clip_len = sel_extract(clip, MAX_CLIP);
    if (clip_len == 0) {
        strcpy(status_msg, "Nothing selected");
        sel_on = 0;
        return;
    }

    order_sel(&sy, &sx, &ey, &ex);
    if (sy == ey) {
        int len = (int)strlen(lines[sy]);
        memmove(&lines[sy][sx], &lines[sy][ex], len - ex + 1);
    } else {
        int tail = (int)strlen(&lines[ey][ex]);
        if (sx + tail >= MAX_COLS) {
            strcpy(status_msg, "Cut failed: line too long");
            sel_on = 0;
            return;
        }
        lines[sy][sx] = '\0';
        strcat(lines[sy], &lines[ey][ex]);
        for (int i = ey + 1; i < nlines; i++)
            strcpy(lines[i - (ey - sy)], lines[i]);
        nlines -= (ey - sy);
    }
    fy = sy;
    fx = sx;
    clamp_cursor();
    sel_on = 0;
    dirty = 1;
    snap_push();
    snprintf(status_msg, sizeof status_msg, "Cut %d char(s)", clip_len);
}

void do_paste(void)
{
    if (clip_len == 0) {
        strcpy(status_msg, "Clipboard empty");
        return;
    }

    int len = (int)strlen(lines[fy]);
    if (fx > len) fx = len;
    int rest_len = len - fx;

    int total = fx + clip_len + rest_len;
    if (total + 1 > (int)sizeof bigbuf) {
        strcpy(status_msg, "Paste failed: too long");
        return;
    }
    memcpy(bigbuf, lines[fy], fx);
    memcpy(bigbuf + fx, clip, clip_len);
    memcpy(bigbuf + fx + clip_len, lines[fy] + fx, rest_len);
    bigbuf[total] = '\0';

    int k = 1, seg_len = 0;
    for (int i = 0; i < total; i++) {
        if (bigbuf[i] == '\n') {
            k++;
            seg_len = 0;
        } else if (++seg_len >= MAX_COLS) {
            strcpy(status_msg, "Paste failed: line too long");
            return;
        }
    }
    if (nlines - 1 + k > MAX_LINES) {
        strcpy(status_msg, "Paste failed: too many lines");
        return;
    }

    int fy0 = fy;
    for (int i = nlines - 1; i > fy0; i--)
        strcpy(lines[i + k - 1], lines[i]);

    int row = fy0, pos = 0;
    for (int seg = 0; seg < k; seg++) {
        int p = pos;
        while (p < total && bigbuf[p] != '\n') p++;
        int n = p - pos;
        memcpy(lines[row], bigbuf + pos, n);
        lines[row][n] = '\0';
        row++;
        pos = p + 1;
    }
    nlines += k - 1;

    int off = fx + clip_len, st = 0;
    for (int seg = 0; seg < k; seg++) {
        int p = st;
        while (p < total && bigbuf[p] != '\n') p++;
        int n = p - st;
        if (off <= st + n) {
            fy = fy0 + seg;
            fx = off - st;
            break;
        }
        st = p + 1;
    }
    clamp_cursor();
    dirty = 1;
    snap_push();
    strcpy(status_msg, "Pasted");
}


void insert_char(int ch)
{
    int len = (int)strlen(lines[fy]);
    if (len >= MAX_COLS - 1)
        return;
    memmove(&lines[fy][fx + 1], &lines[fy][fx], len - fx + 1);
    lines[fy][fx] = (char)ch;
    fx++;
    dirty = 1;
    snap_push();
}

void backspace(void)
{
    int len = (int)strlen(lines[fy]);
    if (fx > 0) {
        memmove(&lines[fy][fx - 1], &lines[fy][fx], len - fx + 1);
        fx--;
        dirty = 1;
        snap_push();
    } else if (fy > 0) {
        int prev = (int)strlen(lines[fy - 1]);
        if (prev + len < MAX_COLS) {
            strcat(lines[fy - 1], lines[fy]);
            for (int i = fy; i < nlines - 1; i++)
                strcpy(lines[i], lines[i + 1]);
            nlines--;
            fy--;
            fx = prev;
            dirty = 1;
            snap_push();
        }
    }
}

void delete_char(void)
{
    int len = (int)strlen(lines[fy]);
    if (fx < len) {
        memmove(&lines[fy][fx], &lines[fy][fx + 1], len - fx);
        dirty = 1;
        snap_push();
    } else if (fy < nlines - 1) {
        int next = (int)strlen(lines[fy + 1]);
        if (len + next < MAX_COLS) {
            strcat(lines[fy], lines[fy + 1]);
            for (int i = fy + 1; i < nlines - 1; i++)
                strcpy(lines[i], lines[i + 1]);
            nlines--;
            dirty = 1;
            snap_push();
        }
    }
}

void insert_newline(void)
{
    if (nlines >= MAX_LINES)
        return;
    int len = (int)strlen(lines[fy]);

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
    snap_push();
}

int collect_matches(void)
{
    int n = 0;
    if (qlen == 0)
        return 0;
    for (int y = 0; y < nlines && n < MAX_MATCH; y++) {
        int len = (int)strlen(lines[y]);
        for (int x = 0; x + qlen <= len && n < MAX_MATCH; x++)
            if (strncmp(&lines[y][x], query, qlen) == 0) {
                cys[n] = y;
                cxs[n] = x;
                n++;
            }
    }
    return n;
}

int match_first_from(int oy, int ox, int *my, int *mx)
{
    int n = collect_matches();
    for (int i = 0; i < n; i++)
        if (cys[i] > oy || (cys[i] == oy && cxs[i] >= ox)) {
            *my = cys[i];
            *mx = cxs[i];
            return 1;
        }
    return 0;
}

int match_next_after(int cy, int cx, int *my, int *mx)
{
    int n = collect_matches();
    for (int i = 0; i < n; i++)
        if (cys[i] > cy || (cys[i] == cy && cxs[i] > cx)) {
            *my = cys[i];
            *mx = cxs[i];
            return 1;
        }
    return 0;
}

int match_prev_before(int cy, int cx, int *my, int *mx)
{
    int n = collect_matches();
    for (int i = n - 1; i >= 0; i--)
        if (cys[i] < cy || (cys[i] == cy && cxs[i] < cx)) {
            *my = cys[i];
            *mx = cxs[i];
            return 1;
        }
    return 0;
}
void match_show(int y, int x)
{
    match_on = 1;
    match_y = y;
    match_x = x;
    fy = y;
    fx = x;
}

int apply_replace(int y, int x)
{
    int len = (int)strlen(lines[y]);
    if (len - qlen + rlen >= MAX_COLS) {
        strcpy(status_msg, "Line too long, skipped");
        return 0;
    }
    memmove(&lines[y][x + rlen], &lines[y][x + qlen], len - x - qlen + 1);
    if (rlen > 0)
        memcpy(&lines[y][x], repl, rlen);
    dirty = 1;
    return 1;
}

void walk_yes(void)
{
    int my, mx;
    if (!match_on)
        return;
    if (!apply_replace(match_y, match_x))
        return;
    snap_push();
    if (match_first_from(match_y, match_x + rlen, &my, &mx)) {
        match_show(my, mx);
    } else {
        strcpy(status_msg, "Replaced. No more matches.");
        mode = MODE_EDIT;
        match_on = 0;
    }
}

void walk_next(void)
{
    int my, mx;
    if (match_next_after(match_y, match_x, &my, &mx))
        match_show(my, mx);
    else
        strcpy(status_msg, "No more matches");
}

void walk_prev(void)
{
    int my, mx;
    if (match_prev_before(match_y, match_x, &my, &mx))
        match_show(my, mx);
    else
        strcpy(status_msg, "No previous match");
}
void walk_all(void)
{
    int cnt = 0;
    int y = match_y, x = match_x;
    int ry, rx;
    while (match_first_from(y, x, &ry, &rx)) {
        if (!apply_replace(ry, rx))
            break;
        cnt++;
        y = ry;
        x = rx + rlen;
    }
    if (cnt > 0) {
        snap_push();
        snprintf(status_msg, sizeof status_msg, "Replaced %d occurrence(s).", cnt);
    } else {
        strcpy(status_msg, "Nothing replaced.");
    }
    mode = MODE_EDIT;
    match_on = 0;
    clamp_cursor();
}

int visible_lines(void)
{
    return (mode == MODE_EDIT) ? rows - 1 : rows - 2;
}

void scroll_to_cursor(void)
{
    int vis = visible_lines();
    if (vis < 1) vis = 1;
    if (fy < top)
        top = fy;
    if (fy >= top + vis)
        top = fy - vis + 1;
    if (top < 0)
        top = 0;
    if (top > nlines - vis && nlines > vis)
        top = nlines - vis;
    if (top < 0)
        top = 0;
}

void draw_text_line(int screen_y, int line)
{
    const char *s = lines[line];
    int len = (int)strlen(s);
    int a = -1, b = -1;

    if (match_on && mode != MODE_EDIT && line == match_y) {
        a = match_x;
        b = match_x + qlen;
    } else if (sel_on && mode == MODE_EDIT) {
        int sy, sx, ey, ex;
        order_sel(&sy, &sx, &ey, &ex);
        if (line >= sy && line <= ey) {
            a = (line == sy) ? sx : 0;
            b = (line == ey) ? ex : len;
            if (b < a) b = a;
            if (a == b) a = b = -1;
        }
    }

    if (a < 0 || a >= cols) {
        mvaddnstr(screen_y, 0, s, cols);
        return;
    }
    if (b > len) b = len;
    if (b > cols) b = cols;
    if (a > b) a = b;

    if (a > 0)
        mvaddnstr(screen_y, 0, s, a);
    if (b > a) {
        attron(A_REVERSE);
        mvaddnstr(screen_y, a, s + a, b - a);
        attroff(A_REVERSE);
    }
    if (b < len && cols - b > 0)
        mvaddnstr(screen_y, b, s + b, cols - b);
}

void draw_input_line(void)
{
    int y = rows - 2;
    char buf[1024];

    if (mode == MODE_SEARCH) {
        snprintf(buf, sizeof buf, "Search: %s", query);
        prompt_cx = 8 + qlen;
    } else if (mode == MODE_R_INPUT) {
        snprintf(buf, sizeof buf, "Replace: %s", query);
        prompt_cx = 9 + qlen;
    } else if (mode == MODE_R_WORD) {
        snprintf(buf, sizeof buf, "Replace '%s' with: %s", query, repl);
        prompt_cx = 17 + qlen + rlen;
    } else {
        snprintf(buf, sizeof buf,
                 "Replace '%s' with '%s'   Y=replace N=skip A=all Enter/Ctrl-P=move",
                 query, repl);
        prompt_cx = 0;
    }
    if (prompt_cx > cols - 1) prompt_cx = cols - 1;
    if (prompt_cx < 0) prompt_cx = 0;

    move(y, 0);
    clrtoeol();
    mvaddnstr(y, 0, buf, cols);
}

void draw_screen(void)
{
    int vis = visible_lines();
    if (vis < 1) vis = 1;
    scroll_to_cursor();

    erase();

    for (int i = 0; i < vis; i++) {
        int line = top + i;
        if (line < nlines)
            draw_text_line(i, line);
    }

    if (mode != MODE_EDIT)
        draw_input_line();

    char buf[512];
    if (status_msg[0] != '\0') {
        snprintf(buf, sizeof buf, "%s", status_msg);
    } else {
        switch (mode) {
        case MODE_SEARCH:
            snprintf(buf, sizeof buf,
                     "[SEARCH] type to edit word; Enter=find/next, Ctrl-P=prev, Esc=exit");
            break;
        case MODE_R_INPUT:
            snprintf(buf, sizeof buf, "[REPLACE] type search word; Enter=OK, Esc=exit");
            break;
        case MODE_R_WORD:
            snprintf(buf, sizeof buf, "[REPLACE] type replacement; Enter=start, Esc=exit");
            break;
        case MODE_R_WALK:
            snprintf(buf, sizeof buf,
                     "[REPLACE] Y=replace N=skip A=all Enter/Ctrl-P=move Esc=exit");
            break;
        default:
            snprintf(buf, sizeof buf, "[EDIT] %s | %s | %d:%d", filename,
                     dirty ? "Modified" : "Saved", fy + 1, fx + 1);
        }
    }
    attron(A_REVERSE);
    mvaddnstr(rows - 1, 0, buf, cols);
    clrtoeol();
    attroff(A_REVERSE);

    if (mode == MODE_EDIT || mode == MODE_R_WALK) {
        int cx = fx;
        if (cx > cols - 1) cx = cols - 1;
        move(fy - top, cx);
    } else {
        move(rows - 2, prompt_cx);
    }
    refresh();
}

void enter_search(void)
{
    mode = MODE_SEARCH;
    qlen = 0;
    query[0] = '\0';
    match_on = 0;
    sel_on = 0;
    s_org_y = fy;
    s_org_x = fx;
}

void search_step(int dir)
{
    int my, mx;
    if (qlen == 0) {
        strcpy(status_msg, "Empty search word");
        return;
    }
    if (dir == 0) {
        if (!match_on) {
            if (match_first_from(s_org_y, s_org_x, &my, &mx))
                match_show(my, mx);
            else
                strcpy(status_msg, "No match");
        } else {
            if (match_next_after(match_y, match_x, &my, &mx))
                match_show(my, mx);
            else
                strcpy(status_msg, "No more matches");
        }
    } else {
        if (match_on && match_prev_before(match_y, match_x, &my, &mx))
            match_show(my, mx);
        else
            strcpy(status_msg, "No previous match");
    }
}

void search_key(int ch)
{
    if (ch == 27 || ch == 17) {
        mode = MODE_EDIT;
        match_on = 0;
        clamp_cursor();
        return;
    }
    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        if (qlen > 0)
            query[--qlen] = '\0';
        match_on = 0;
        return;
    }
    if (ch == 10 || ch == 13 || ch == KEY_ENTER) {
        search_step(0);
        return;
    }
    if (ch == 16) {
        search_step(-1);
        return;
    }
    if (ch >= 32 && ch <= 126) {
        if (qlen < MAX_WORD - 1) {
            query[qlen++] = (char)ch;
            query[qlen] = '\0';
        }
        match_on = 0;
        return;
    }
}

void enter_replace(void)
{
    mode = MODE_R_INPUT;
    qlen = 0;
    rlen = 0;
    query[0] = '\0';
    repl[0] = '\0';
    match_on = 0;
    sel_on = 0;
    s_org_y = fy;
    s_org_x = fx;
}

void r_input_key(int ch)
{
    if (ch == 27 || ch == 17) {
        mode = MODE_EDIT;
        return;
    }
    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        if (qlen > 0)
            query[--qlen] = '\0';
        return;
    }
    if (ch == 10 || ch == 13 || ch == KEY_ENTER) {
        if (qlen == 0) {
            strcpy(status_msg, "Empty search word");
            return;
        }
        rlen = 0;
        repl[0] = '\0';
        mode = MODE_R_WORD;
        return;
    }
    if (ch >= 32 && ch <= 126) {
        if (qlen < MAX_WORD - 1) {
            query[qlen++] = (char)ch;
            query[qlen] = '\0';
        }
        return;
    }
}

void r_word_key(int ch)
{
    if (ch == 27 || ch == 17) {
        mode = MODE_EDIT;
        return;
    }
    if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
        if (rlen > 0)
            repl[--rlen] = '\0';
        return;
    }
    if (ch == 10 || ch == 13 || ch == KEY_ENTER) {
        int my, mx;
        if (match_first_from(s_org_y, s_org_x, &my, &mx)) {
            match_show(my, mx);
            mode = MODE_R_WALK;
        } else {
            strcpy(status_msg, "No match");
        }
        return;
    }
    if (ch >= 32 && ch <= 126) {
        if (rlen < MAX_WORD - 1) {
            repl[rlen++] = (char)ch;
            repl[rlen] = '\0';
        }
        return;
    }
}

void walk_key(int ch)
{
    if (ch == 27 || ch == 3 || ch == 17) {
        mode = MODE_EDIT;
        match_on = 0;
        clamp_cursor();
        return;
    }
    if (ch == 10 || ch == 13 || ch == KEY_ENTER) {
        walk_next();
        return;
    }
    if (ch == 16) {
        walk_prev();
        return;
    }
    if (ch == 'y' || ch == 'Y') {
        walk_yes();
        return;
    }
    if (ch == 'n' || ch == 'N') {
        walk_next();
        return;
    }
    if (ch == 'a' || ch == 'A') {
        walk_all();
        return;
    }
}


void edit_key(int ch)
{
    if (ch == 17) {
        if (dirty && !quit_armed) {
            quit_armed = 1;
            strcpy(status_msg,
                   "Unsaved changes! Press Ctrl-Q again to quit without saving.");
        } else {
            done = 1;
        }
        return;
    }
    quit_armed = 0;

    if (ch == 19) {
        if (save_file() == 0) {
            dirty = 0;
            strcpy(status_msg, "Saved.");
        } else {
            snprintf(status_msg, sizeof status_msg,
                     "Save failed: %s", strerror(errno));
        }
        return;
    }

    if (ch == 6)  { enter_search();  return; }
    if (ch == 18) { enter_replace(); return; }
    if (ch == 3)  { do_copy();  return; }
    if (ch == 24) { do_cut();   return; }
    if (ch == 22) { do_paste(); return; }
    if (ch == 26) { do_undo();  return; }
    if (ch == 25) { do_redo();  return; }

    if (ch == KEY_SLEFT)  { sel_shift(0, -1); return; }
    if (ch == KEY_SRIGHT) { sel_shift(0, +1); return; }
    if (ch == KEY_SR)     { sel_shift(-1, 0); return; }
    if (ch == KEY_SF)     { sel_shift(+1, 0); return; }

    if (ch == 27) {
        sel_on = 0;
        return;
    }

    sel_on = 0;

    if (ch == KEY_UP) {
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
    } else if (ch >= 32 && ch <= 126) {
        insert_char(ch);
    }

    /* 列号不能超过行尾（比如从长行移到短行之后） */
    if (fx > (int)strlen(lines[fy]))
        fx = (int)strlen(lines[fy]);
}

/* ==================== 主程序 ==================== */

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s <filename>\n", argv[0]);
        return 1;
    }
    if (load_file(argv[1]) == -1)
        return 1;
    filename = argv[1];

    initscr();              /* 接管终端 */
    raw();                  /* Ctrl 键原样传给 getch() */
    noecho();               /* 不自动显示按下的键 */
    keypad(stdscr, TRUE);   /* 方向键变成长键码 */
    set_escdelay(50);       /* Esc 不用等默认的 1 秒，手感和测试都更好 */

    getmaxyx(stdscr, rows, cols);

    snap_store(0);          /* 撤回的"零点"：刚打开文件的状态 */
    snap_count = 1;
    snap_pos = 0;

    draw_screen();

    while (!done) {
        int ch = getch();
        status_msg[0] = '\0';       /* 上一条临时消息只显示一轮 */

        if (ch == KEY_RESIZE) {     /* 窗口被缩放 */
            getmaxyx(stdscr, rows, cols);
        } else if (mode == MODE_EDIT) {
            edit_key(ch);
        } else if (mode == MODE_SEARCH) {
            search_key(ch);
        } else if (mode == MODE_R_INPUT) {
            r_input_key(ch);
        } else if (mode == MODE_R_WORD) {
            r_word_key(ch);
        } else {
            walk_key(ch);
        }

        draw_screen();
    }

    endwin();               /* 归还终端（必须有） */
    return 0;
}
