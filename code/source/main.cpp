#include "Halib/Graphic.h"
#include "Halib/System.h"
#include <iostream>

#include "rom.h"
#include "lcd.h"
#include "mem.h"
#include "cpu.h"
#include "timer.h"

void Halib::DrawUI() {}

float cpuTime = -1, lcdTime = -1;
Halib::Color backgroundColor = Halib::Color(10, 10, 10, true);
void OnVSync()
{
	char buffer[64];

	float newTime = Halib::GetTimeSinceStartup();
	sprintf(buffer, "Time per frame: %.3f s", newTime - cpuTime);
	cpuTime = newTime;
	Halib::DrawText(buffer, 300, 40);
}


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


	Halib::Clear(backgroundColor);
	Halib::Show();
	Halib::Clear(backgroundColor);
	Halib::Show();

	//lcd_write_control(128);
	Halib::SetVSynchronization(false);

	float oldTime = Halib::GetTimeSinceStartup();
	float time;
	
	while(!Halib::GetShouldClose()) 
	{
		cpu_cycle();
		lcd_cycle();
	}
	
	return 0;
}
