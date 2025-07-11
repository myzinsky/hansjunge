#include "Halib/Graphic.h"
#include "Halib/System.h"
#include <iostream>

#include "rom.h"
#include "lcd.h"
#include "mem.h"
#include "cpu.h"
#include "timer.h"

int main() 
{
    std::cout << std::endl << "Prepare Gameboy" << std::endl <<std::endl;

    if(!rom_load("assets/tetris.gb")) {
        std::cout << "Cannot find game" << std::endl;
        return -1;
    }

    std::cout << "ROM OK" << std::endl;

	Halib::Init();
	Halib::SetTargetFramerate(60);

    if(!lcd_init()) {
        return -1;
    }

    std::cout << "LCD OK" << std::endl;

    mem_init();
	printf("Mem OK!\n");

	cpu_init();
	printf("CPU OK!\n");


	Halib::Clear(Halib::Color(5, 5, 5, true));
	Halib::Show();
	Halib::Clear(Halib::Color(5, 5, 5, true));
	Halib::Show();

	lcd_write_control(128);

    int r=0;

	//This is your game loop. The program should never leave it.
	while(!Halib::GetShouldClose()) 
	{
		cpu_cycle();
		lcd_cycle();
	}
	
	return 0;
}
