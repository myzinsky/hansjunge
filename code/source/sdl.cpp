#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include <unistd.h>
#include <Hall/Hall.h>
#include <iostream>

static unsigned int frames;
static int button_start, button_select;
static int button_a, button_b;
static int button_down, button_up, button_left, button_right;
static int button_debug, button_quit;

int sdl_update(void)
{
    Hall::ControllerState controller = Hall::GetController(0);

    button_a = Hall::GetA(controller);
    button_b = Hall::GetB(controller);
    button_select = Hall::GetSelect(controller);
    button_start = Hall::GetStart(controller);
    button_up = Hall::GetUp(controller);
    button_down = Hall::GetDown(controller);
    button_left = Hall::GetLeft(controller);
    button_right = Hall::GetRight(controller); 

	return 0;
}

unsigned int sdl_get_buttons(void)
{
	return (button_start*8) | (button_select*4) | (button_b*2) | button_a;
}

unsigned int sdl_get_directions(void)
{
	return (button_down*8) | (button_up*4) | (button_left*2) | button_right;
}

