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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ncurses.h>

enum { YMAX = 16, XMAX = 16, MINES = 40 };
enum { FIELD = '#', FLAG = 'P', MINE = 'M', FREE = '_', CURSOR = 'X', WRONG = 'W'};
enum { FLAGKEY = 'a', CLICKKEY = 'd', QUITKEY = 'q', RESTARTKEY = 'r' };
enum { STAGE1, STAGE2, STAGE3 };
enum { GAME_CONT, GAME_WON, GAME_LOST };
enum { SHOW_CURSOR, SHOW_NO_CURSOR };

#define YSTART ((int)(LINES/2)-(int)(YMAX/2))
#define XSTART ((int)(COLS/2)-XMAX)

char field[YMAX][XMAX];     // for printing
int minefield[YMAX][XMAX];  // minestatus

// status
int cur_y, cur_x;
int flags, cleared;

typedef int fieldfunc(int stage, int y, int x);

void drawfield(int cursormode)
{
    clear();
    
    char tmp = field[cur_y][cur_x];
    if(cursormode == SHOW_CURSOR)
        field[cur_y][cur_x] = CURSOR;
    
    mvprintw(YSTART-2, XSTART, "%i/%i", flags, MINES);
    
    int y, x;
    for(y=0; y<YMAX; y++)
    {
        move(YSTART+y, XSTART);
        for(x=0; x<XMAX; x++)
            printw("%c ", field[y][x]);
    }
    field[cur_y][cur_x] = tmp;
    
    refresh();
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
int countmines(int stage, int y, int x)
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

void fieldgen()
{
    int y, x, m;
    for(y=0; y<YMAX; y++)
        for(x=0; x<XMAX; x++)
        {
            field[y][x] = FIELD;
            minefield[y][x] = 0;
        }
    
    for(m=0; m<MINES; m++)
    {
        do {
            y = rand()%YMAX;
            x = rand()%XMAX;
        } while(minefield[y][x] == 9);
        minefield[y][x] = 9;
    }
    
    for(y=0; y<YMAX; y++) 
        for(x=0; x<XMAX; x++)
            if(minefield[y][x] != 9)
                minefield[y][x] = exec_fieldfunc(y, x, countmines);
}

void explodezero(int y, int x);

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
                    explodezero(y, x);
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

void explodezero(int y, int x) {
    if(field[y][x] == FIELD)
        cleared++;
    field[y][x] = FREE;
    exec_fieldfunc(y, x, zero);
}

// fieldfunc
int countflags(int stage, int y, int x)
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
                    explodezero(y, x);
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

int explodefield(int y, int x)
{
    if(field[y][x] == FLAG || minefield[y][x] != exec_fieldfunc(y, x, countflags))
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
        drawfield(SHOW_NO_CURSOR);
        mvprintw(YSTART+YMAX+1, XSTART, "== Game Over ==\n\n");
    }
    else
    {
        drawfield(SHOW_NO_CURSOR);
        mvprintw(YSTART+YMAX+1, XSTART, "== Game Solved ==\n\n");
    }
    refresh();
    return status;
}

int click()
{
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
            if(!explodefield(cur_y, cur_x))
                return gameover(GAME_LOST);
        }
        else
        {
            int c = minefield[cur_y][cur_x];
            if(c == 0)
                explodezero(cur_y, cur_x);
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

void setcurpos(int dir)
{
    int ynum = 0, xnum = 0;
    switch(dir)
    {
        case KEY_DOWN:  ynum =  1; break;
        case KEY_UP:    ynum = -1; break;
        case KEY_RIGHT: xnum =  1; break;
        case KEY_LEFT:  xnum = -1; break;
    }
    cur_y = mod(cur_y+ynum, YMAX);
    cur_x = mod(cur_x+xnum, XMAX);
}

int main(int argc, char* args[])
{
    initscr();
    keypad(stdscr, 1);
    noecho();
    curs_set(0);
    
    srand(time(0));
    
    int c, clicked, running;
    
restart:
    cur_y = (int)YMAX/2;
    cur_x = (int)XMAX/2;
    flags = 0;
    cleared = 0;

    fieldgen();
    drawfield(SHOW_CURSOR);
    
    running = 1;
    
    while(1)
    {
        c = getch();
        clicked = 0;
        switch(c)
        {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_RIGHT:
            case KEY_LEFT:
                if(running)
                    setcurpos(c);
                break;
            case FLAGKEY:
                if(running)
                {
                    flag();
                    clicked = 1;
                }
                break;
            case CLICKKEY: 
                if(running)
                {
                    if(click() != GAME_CONT)
                        running = 0;
                    else
                        clicked = 1;
                }
                break;
            case QUITKEY:
                goto end;
            case RESTARTKEY:
                goto restart;
        }
        if(running)
            drawfield(clicked ? SHOW_NO_CURSOR : SHOW_CURSOR);
    }
end:
    endwin();
    return 0;
}

