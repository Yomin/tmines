/*
 * Copyright (c) 2012 Martin Rödel aka Yomin
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <time.h>
#include <ncurses.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

enum { GAME_MODE_BEGINNER, GAME_MODE_ADVANCED, GAME_MODE_PROFI, GAME_MODE_CUSTOM };
enum { GAME_MODE_DEFAULT = GAME_MODE_PROFI };
enum { HIDDEN = '#', FLAG = 'P', MINE = 'M', FREE = ' ', WRONG = 'W'};
enum { KEY_FLAG = 'a', KEY_CLICK = 'd', KEY_QUIT = 'q', KEY_RSTART = 'r'};
enum { KEY_BEGINNER = '1', KEY_ADVANCED = '2', KEY_PROFI = '3', KEY_CUSTOM = '4'};
enum { STAGE1, STAGE2, STAGE3 };
enum { GAME_CONT, GAME_WON, GAME_LOST };
enum { STATE_STOPPED, STATE_INIT, STATE_RUNNING };
enum { DRAW_SIMPLE, DRAW_ALL };
enum { SMILEY_NORMAL = ')', SMILEY_CLICK = '/', SMILEY_FLAG = '|', SMILEY_WON = 'D', SMILEY_LOST = '(' };

#define YMAX   gameModeSettings[game_mode][0]
#define XMAX   gameModeSettings[game_mode][1]
#define MINES  gameModeSettings[game_mode][2]
#define YSTART ((int)(LINES/2)-(int)(YMAX/2))
#define XSTART ((int)(COLS/2)-XMAX)

#define FIELD(y, x)     field[y*lines+x]
#define MINEFIELD(y, x) minefield[y*lines+x]

char *field;                // for printing
int *minefield;             // minestatus
int color[128];             // field colorcodes
int gameModeSettings[4][3]; // ymax, xmax, mines per game mode

// status
int game_mode, lines;
int cur_y, cur_x, cur_y_old, cur_x_old;
int flags, cleared;
time_t startTime, currentTime;
char drawing, drawMode, drawSmiley;

typedef int fieldfunc(int stage, int y, int x);

void draw_field()
{
    drawing = 1;
    
    mvprintw(YSTART-2, XSTART, "%02i/%02i", flags, MINES);
    mvprintw(YSTART-2, XSTART+XMAX-1, ":%c", drawSmiley);
    mvprintw(YSTART-2, XSTART+XMAX*2-6, "%02i:%02i", (int)currentTime/60, (int)currentTime%60);
    
    int y, x, c;
    
    if(drawMode == DRAW_ALL)
        for(y=0; y<YMAX; y++)
        {
            move(YSTART+y, XSTART);
            for(x=0; x<XMAX; x++)
            {
                c = FIELD(y, x);
                addch(c | color[(int)c]);
                if(c == FLAG && x<XMAX-1 && FIELD(y, x+1) == FLAG)
                    addch(' ' | color[FLAG]);
                else
                    addch(' ');
            }
        }
    
    c = FIELD(cur_y, cur_x);
    mvaddch(YSTART+cur_y, XSTART+cur_x*2, c | A_REVERSE);
    
    if(drawMode == DRAW_SIMPLE)
    {
        if(cur_x<XMAX-1)
        {
            if(c == FLAG && FIELD(cur_y, cur_x+1) == FLAG)
                addch(' ' | color[FLAG]);
            else
                addch(' ');
        }
        if(cur_x>0)
        {
            if(c == FLAG && FIELD(cur_y, cur_x-1) == FLAG)
                mvaddch(YSTART+cur_y, XSTART+cur_x*2-1, ' ' | color[FLAG]);
            else
                mvaddch(YSTART+cur_y, XSTART+cur_x*2-1, ' ');
        }
        c = FIELD(cur_y_old, cur_x_old);
        mvaddch(YSTART+cur_y_old, XSTART+cur_x_old*2, c | color[(int)c]);
    }
    
    refresh();
    
    drawing = 0;
}

int mod(int m, int n)
{
    while(m<0)
        m += n;
    return m%n;
}

int exec_fieldfunc(int y, int x, fieldfunc func)
{
    func(STAGE1, y, x);
    
    int i, xx, yy;
    for(i=0; i<8; i++)
    {
        switch(i)
        {
            case 0: { yy = y-1; xx = x-1; } break;
            case 1: { yy = y-1; xx = x;   } break;
            case 2: { yy = y-1; xx = x+1; } break;
            case 3: { yy = y;   xx = x-1; } break;
            case 4: { yy = y;   xx = x+1; } break;
            case 5: { yy = y+1; xx = x-1; } break;
            case 6: { yy = y+1; xx = x;   } break;
            case 7: { yy = y+1; xx = x+1; } break;
        }
        if(yy>=0 && xx>=0 && yy<YMAX && xx<XMAX) func(STAGE2, yy, xx);
    }
    return func(STAGE3, y, x);
}

// fieldfunc
int count_mines(int stage, int y, int x)
{
    static int c;
    switch(stage)
    {
        case STAGE1:
            c = 0;
            break;
        case STAGE2:
            c += (MINEFIELD(y, x) == 9 ? 1 : 0);
            break;
        case STAGE3:
            return c;
    }
    return -1;
}

void reset_field()
{
    int y, x;
    for(y=0; y<YMAX; y++)
        for(x=0; x<XMAX; x++)
        {
            FIELD(y, x) = HIDDEN;
            MINEFIELD(y, x) = 0;
        }
}

void gen_field()
{
    int y, x, m;
    
    for(m=0; m<MINES; m++)
    {
        do {
            y = rand()%YMAX;
            x = rand()%XMAX;
        } while(MINEFIELD(y, x) == 9 ||
            (y >= cur_y-1 && y <= cur_y+1 && x >= cur_x-1 && x <= cur_x+1));
        MINEFIELD(y, x) = 9;
    }
    
    for(y=0; y<YMAX; y++) 
        for(x=0; x<XMAX; x++)
            if(MINEFIELD(y, x) != 9)
                MINEFIELD(y, x) = exec_fieldfunc(y, x, count_mines);
}

void explode_zero(int y, int x);

// fieldfunc
int zero(int stage, int y, int x)
{
    switch(stage)
    {
        case STAGE1:
            break;
        case STAGE2:
            if(FIELD(y, x) == HIDDEN)
            {
                if(MINEFIELD(y, x) == 0)
                    explode_zero(y, x);
                else
                {
                    FIELD(y, x) = (char)(MINEFIELD(y, x)+48);
                    cleared++;
                }
            }
            break;
        case STAGE3:
            break;
    }
    return -1;
}

void explode_zero(int y, int x) {
    if(FIELD(y, x) == HIDDEN)
        cleared++;
    FIELD(y, x) = FREE;
    exec_fieldfunc(y, x, zero);
}

// fieldfunc
int count_flags(int stage, int y, int x)
{
    static int c;
    switch(stage) {
        case STAGE1:
            c = 0;
            break;
        case STAGE2:
            c += (FIELD(y, x) == FLAG ? 1 : 0);
            break;
        case STAGE3:
            return c;
    }
    return -1;
}

// fieldfunc
int explode(int stage, int y, int x)
{
    static int clear;
    switch(stage)
    {
        case STAGE1:
            clear = 1;
            break;
        case STAGE2:
            if(FIELD(y, x) == HIDDEN)
            {
                if(MINEFIELD(y, x) == 0)
                    explode_zero(y, x);
                else if(MINEFIELD(y, x) == 9)
                {
                    clear = 0;
                    FIELD(y, x) = MINE;
                }
                else
                {
                    FIELD(y, x) = (char)(MINEFIELD(y, x)+48);
                    cleared++;
                }
             }
             break;
        case STAGE3:
            return clear;
    }
    return -1;
}

int explode_field(int y, int x)
{
    if(FIELD(y, x) == FLAG || MINEFIELD(y, x) != exec_fieldfunc(y, x, count_flags))
        return 1;
    else
    {
        if(FIELD(y, x) == HIDDEN)
            cleared++;
        return exec_fieldfunc(y, x, explode);
    }
}

int gameover(int status)
{
    int y, x;
    for(y=0; y<YMAX; y++) 
        for(x=0; x<XMAX; x++)
            if(MINEFIELD(y, x) == 9)
            {
                if(FIELD(y, x) != FLAG)
                    FIELD(y, x) = MINE;
            }
            else
            {
                if(FIELD(y, x) == FLAG)
                    FIELD(y, x) = WRONG;
            }
    drawMode = DRAW_ALL;
    drawSmiley = status == GAME_LOST ? SMILEY_LOST : SMILEY_WON;
    draw_field();
    refresh();
    return status;
}

int click()
{
    drawSmiley = SMILEY_CLICK;
    if(FIELD(cur_y, cur_x) == FLAG)
        return GAME_CONT;
    else if(MINEFIELD(cur_y, cur_x) == 9)
    {
        FIELD(cur_y, cur_x) = MINE;
        return gameover(GAME_LOST);
    }
    else
    {
        if(FIELD(cur_y, cur_x) != HIDDEN)
        {
            drawMode = DRAW_ALL;
            if(!explode_field(cur_y, cur_x))
                return gameover(GAME_LOST);
        }
        else
        {
            int c = MINEFIELD(cur_y, cur_x);
            if(c == 0)
            {
                drawMode = DRAW_ALL;
                explode_zero(cur_y, cur_x);
            }
            else
            {
                FIELD(cur_y, cur_x) = (char)(c+48);
                cleared++;
            }
        }
        if(cleared == YMAX*XMAX-MINES)
            return gameover(GAME_WON);
        return GAME_CONT;
    }
}

void flag()
{
    drawSmiley = SMILEY_FLAG;
    if(FIELD(cur_y, cur_x) == FLAG)
    {
        FIELD(cur_y, cur_x) = HIDDEN;
        flags--;
    }
    else if(FIELD(cur_y, cur_x) == HIDDEN)
    {
        FIELD(cur_y, cur_x) = FLAG;
        flags++;
    }
}

void set_curpos(int dir)
{
    int ynum = 0, xnum = 0;
    switch(dir)
    {
        case KEY_DOWN:  ynum =  1; break;
        case KEY_UP:    ynum = -1; break;
        case KEY_RIGHT: xnum =  1; break;
        case KEY_LEFT:  xnum = -1; break;
    }
    cur_y_old = cur_y;
    cur_x_old = cur_x;
    cur_y = mod(cur_y+ynum, YMAX);
    cur_x = mod(cur_x+xnum, XMAX);
}

void handle_signal(int signal)
{
    currentTime = difftime(time(0), startTime);
    if(!drawing)
    {
        mvprintw(YSTART-2, XSTART+XMAX-1, ":%c", drawSmiley);
        mvprintw(YSTART-2, XSTART+XMAX*2-6, "%02i:%02i", (int)currentTime/60, (int)currentTime%60);
        refresh();
    }
}

int input(const char* msg)
{
    char buf[256];
    while(1)
    {
        printw("%s", msg);
        getnstr(buf, 256);
        if(strspn(buf, "0123456789") == strlen(buf))
            return atoi(buf);
    }
}

void start_time()
{
    struct itimerval timerval;
    timerval.it_interval.tv_sec = 1;
    timerval.it_interval.tv_usec = 0;
    timerval.it_value.tv_sec = 1;
    timerval.it_value.tv_usec = 0;
    startTime = time(0);
    currentTime = 0;
    setitimer(ITIMER_REAL, &timerval, 0);
}

void stop_time()
{
    struct itimerval timerval;
    timerval.it_interval.tv_sec = 0;
    timerval.it_interval.tv_usec = 0;
    timerval.it_value.tv_sec = 0;
    timerval.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timerval, 0);
}

int main(int argc, char* args[])
{
    initscr();
    keypad(stdscr, 1);
    noecho();
    curs_set(0);
    
    field = malloc(LINES*COLS*sizeof(char));
    minefield = malloc(LINES*COLS*sizeof(int));
    lines = LINES;
    
    start_color();
    init_pair( 1, COLOR_BLACK, COLOR_BLACK);
    init_pair( 2, COLOR_RED, COLOR_BLACK);
    init_pair( 3, COLOR_GREEN, COLOR_BLACK);
    init_pair( 4, COLOR_YELLOW, COLOR_BLACK);
    init_pair( 5, COLOR_BLUE, COLOR_BLACK);
    init_pair( 6, COLOR_MAGENTA, COLOR_BLACK);
    init_pair( 7, COLOR_CYAN, COLOR_BLACK);
    init_pair( 8, COLOR_WHITE, COLOR_BLACK);
    init_pair( 9, COLOR_BLACK, COLOR_RED);
    init_pair(10, COLOR_BLACK, COLOR_YELLOW);
    color[FREE]   = COLOR_PAIR(1);
    color[HIDDEN] = COLOR_PAIR(8);
    color[FLAG]   = COLOR_PAIR(10);
    color[MINE]   = COLOR_PAIR(2);
    color[WRONG]  = COLOR_PAIR(9);
    color['1']    = COLOR_PAIR(5);
    color['2']    = COLOR_PAIR(3);
    color['3']    = COLOR_PAIR(2);
    color['4']    = COLOR_PAIR(4);
    color['5']    = COLOR_PAIR(7);
    color['6']    = COLOR_PAIR(6);
    color['7']    = COLOR_PAIR(6);
    color['8']    = COLOR_PAIR(6);
    
    gameModeSettings[GAME_MODE_BEGINNER][0] = 8;
    gameModeSettings[GAME_MODE_BEGINNER][1] = 8;
    gameModeSettings[GAME_MODE_BEGINNER][2] = 10;
    gameModeSettings[GAME_MODE_ADVANCED][0] = 16;
    gameModeSettings[GAME_MODE_ADVANCED][1] = 16;
    gameModeSettings[GAME_MODE_ADVANCED][2] = 40;
    gameModeSettings[GAME_MODE_PROFI][0] = 16;
    gameModeSettings[GAME_MODE_PROFI][1] = 30;
    gameModeSettings[GAME_MODE_PROFI][2] = 99;
    
    game_mode = GAME_MODE_DEFAULT;
    
    srand(time(0));
    
    signal(SIGALRM, handle_signal);
    
    drawing = 0;
    
    int c, state;
    
restart:
    cur_y = (int)YMAX/2;
    cur_x = (int)XMAX/2;
    flags = 0;
    cleared = 0;
    currentTime = 0;
    
    clear();
    reset_field();
    drawMode = DRAW_ALL;
    drawSmiley = SMILEY_NORMAL;
    draw_field();
    
    state = STATE_INIT;
    
    while(1)
    {
        drawMode = DRAW_SIMPLE;
        drawSmiley = SMILEY_NORMAL;
        c = getch();
        switch(c)
        {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_RIGHT:
            case KEY_LEFT:
                if(state != STATE_STOPPED)
                    set_curpos(c);
                break;
            case KEY_FLAG:
                if(state != STATE_STOPPED)
                    flag();
                break;
            case KEY_CLICK:
                switch(state)
                {
                    case STATE_INIT:
                        gen_field();
                        start_time();
                        state = STATE_RUNNING;
                    case STATE_RUNNING:
                        if(click() != GAME_CONT)
                        {
                            stop_time();
                            state = STATE_STOPPED;
                        }
                }
                break;
            case KEY_RSTART:
                stop_time();
                goto restart;
            case KEY_QUIT:
                stop_time();
                goto end;
            case KEY_BEGINNER:
                stop_time();
                game_mode = GAME_MODE_BEGINNER;
                goto restart;
            case KEY_ADVANCED:
                stop_time();
                game_mode = GAME_MODE_ADVANCED;
                goto restart;
            case KEY_PROFI:
                stop_time();
                game_mode = GAME_MODE_PROFI;
                goto restart;
            case KEY_CUSTOM:
                stop_time();
                game_mode = GAME_MODE_CUSTOM;
                clear();
                echo();
                curs_set(1);
                YMAX = input("height: ");
                XMAX = input("width: ");
                MINES = input("mines: ");
                noecho();
                curs_set(0);
                goto restart;
        }
        if(state != STATE_STOPPED)
            draw_field();
    }
end:
    free(field);
    free(minefield);
    endwin();
    return 0;
}

