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

enum { YMAX = 16, XMAX = 30, MINES = 99 };
enum { FIELD = '#', FLAG = 'P', MINE = 'M', FREE = ' ', WRONG = 'W'};
enum { FLAGKEY = 'a', CLICKKEY = 'd', QUITKEY = 'q', RESTARTKEY = 'r' };
enum { STAGE1, STAGE2, STAGE3 };
enum { GAME_CONT, GAME_WON, GAME_LOST };
enum { STATE_STOPPED, STATE_INIT, STATE_RUNNING };
enum { DRAW_SIMPLE, DRAW_ALL };
enum { SMILEY_NORMAL = ')', SMILEY_CLICK = '/', SMILEY_FLAG = '|', SMILEY_WON = 'D', SMILEY_LOST = '(' };

#define YSTART ((int)(LINES/2)-(int)(YMAX/2))
#define XSTART ((int)(COLS/2)-XMAX)

char field[YMAX][XMAX];     // for printing
int minefield[YMAX][XMAX];  // minestatus
int color[128];             // field colorcodes

// status
int cur_y, cur_x, cur_y_old, cur_x_old;
int flags, cleared;
time_t startTime, currentTime;
char drawing, drawMode, drawSmiley;

typedef int fieldfunc(int stage, int y, int x);

void draw_field()
{
    drawing = 1;
    
    if(drawMode == DRAW_ALL)
        clear();
    
    mvprintw(YSTART-2, XSTART, "%02i/%i", flags, MINES);
    mvprintw(YSTART-2, XSTART+XMAX-1, ":%c", drawSmiley);
    mvprintw(YSTART-2, XSTART+XMAX*2-6, "%02i:%02i", (int)currentTime/60, (int)currentTime%60);
    
    int y, x, c;
    
    if(drawMode == DRAW_ALL)
        for(y=0; y<YMAX; y++)
        {
            move(YSTART+y, XSTART);
            for(x=0; x<XMAX; x++)
            {
                c = field[y][x];
                addch(c | color[(int)c]);
                if(c == FLAG && x<XMAX-1 && field[y][x+1] == FLAG)
                    addch(' ' | color[FLAG]);
                else
                    addch(' ');
            }
        }
    
    c = field[cur_y][cur_x];
    mvaddch(YSTART+cur_y, XSTART+cur_x*2, c | A_REVERSE);
    
    if(drawMode == DRAW_SIMPLE)
    {
        if(cur_x<XMAX-1)
        {
            if(c == FLAG && field[cur_y][cur_x+1] == FLAG)
                addch(' ' | color[FLAG]);
            else
                addch(' ');
        }
        if(cur_x>0)
        {
            if(c == FLAG && field[cur_y][cur_x-1] == FLAG)
                mvaddch(YSTART+cur_y, XSTART+cur_x*2-1, ' ' | color[FLAG]);
            else
                mvaddch(YSTART+cur_y, XSTART+cur_x*2-1, ' ');
        }
        c = field[cur_y_old][cur_x_old];
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
            c += (minefield[y][x] == 9 ? 1 : 0);
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
            field[y][x] = FIELD;
            minefield[y][x] = 0;
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
        } while(minefield[y][x] == 9 ||
            (y >= cur_y-1 && y <= cur_y+1 && x >= cur_x-1 && x <= cur_x+1));
        minefield[y][x] = 9;
    }
    
    for(y=0; y<YMAX; y++) 
        for(x=0; x<XMAX; x++)
            if(minefield[y][x] != 9)
                minefield[y][x] = exec_fieldfunc(y, x, count_mines);
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
            if(field[y][x] == FIELD)
            {
                if(minefield[y][x] == 0)
                    explode_zero(y, x);
                else
                {
                    field[y][x] = (char)(minefield[y][x]+48);
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
    if(field[y][x] == FIELD)
        cleared++;
    field[y][x] = FREE;
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
            c += (field[y][x] == FLAG ? 1 : 0);
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
            if(field[y][x] == FIELD)
            {
                if(minefield[y][x] == 0)
                    explode_zero(y, x);
                else if(minefield[y][x] == 9)
                {
                    clear = 0;
                    field[y][x] = MINE;
                }
                else
                {
                    field[y][x] = (char)(minefield[y][x]+48);
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
    if(field[y][x] == FLAG || minefield[y][x] != exec_fieldfunc(y, x, count_flags))
        return 1;
    else
    {
        if(field[y][x] == FIELD)
            cleared++;
        return exec_fieldfunc(y, x, explode);
    }
}

int gameover(int status)
{
    if(status == GAME_LOST)
    {
        int y, x;
        for(y=0; y<YMAX; y++) 
            for(x=0; x<XMAX; x++)
                if(minefield[y][x] == 9)
                {
                    if(field[y][x] != FLAG)
                        field[y][x] = MINE;
                }
                else
                {
                    if(field[y][x] == FLAG)
                        field[y][x] = WRONG;
                }
        drawMode = DRAW_ALL;
        drawSmiley = SMILEY_LOST;
        draw_field();
        mvprintw(YSTART+YMAX+1, XSTART, "== Game Over ==\n\n");
    }
    else
    {
        drawMode = DRAW_ALL;
        drawSmiley = SMILEY_WON;
        draw_field();
        mvprintw(YSTART+YMAX+1, XSTART, "== Game Solved ==\n\n");
    }
    refresh();
    return status;
}

int click()
{
    drawSmiley = SMILEY_CLICK;
    if(field[cur_y][cur_x] == FLAG)
        return GAME_CONT;
    else if(minefield[cur_y][cur_x] == 9)
    {
        field[cur_y][cur_x] = MINE;
        return gameover(GAME_LOST);
    }
    else
    {
        if(field[cur_y][cur_x] != FIELD)
        {
            drawMode = DRAW_ALL;
            if(!explode_field(cur_y, cur_x))
                return gameover(GAME_LOST);
        }
        else
        {
            int c = minefield[cur_y][cur_x];
            if(c == 0)
            {
                drawMode = DRAW_ALL;
                explode_zero(cur_y, cur_x);
            }
            else
            {
                field[cur_y][cur_x] = (char)(c+48);
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
    if(field[cur_y][cur_x] == FLAG)
    {
        field[cur_y][cur_x] = FIELD;
        flags--;
    }
    else if(field[cur_y][cur_x] == FIELD)
    {
        field[cur_y][cur_x] = FLAG;
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
    color[FIELD]  = COLOR_PAIR(8);
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
            case FLAGKEY:
                if(state != STATE_STOPPED)
                    flag();
                break;
            case CLICKKEY:
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
            case QUITKEY:
                stop_time();
                goto end;
            case RESTARTKEY:
                stop_time();
                goto restart;
        }
        if(state != STATE_STOPPED)
            draw_field();
    }
end:
    endwin();
    return 0;
}

