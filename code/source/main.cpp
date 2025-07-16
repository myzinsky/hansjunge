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

	sprintf(buffer, "cpu: %.3f s", cpuTime);
	Halib::DrawText(buffer, 300, 10);

	sprintf(buffer, "lcd: %.3f s", lcdTime);
	Halib::DrawText(buffer, 300, 20);

	float newTime = Halib::GetTimeSinceStartup();
	sprintf(buffer, "tot: %.3f s", newTime - cpuTime);
	cpuTime = newTime;
	Halib::DrawText(buffer, 300, 40);

	//cpuTime = 0;
	//lcdTime = 0;
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

	lcd_write_control(128);
	Halib::SetVSynchronization(false);

    int r=0;

	float oldTime = Halib::GetTimeSinceStartup();
	float time;
	//This is your game loop. The program should never leave it.
	while(!Halib::GetShouldClose()) 
	{
		cpu_cycle();
		/*time = Halib::GetTimeSinceStartup();
		float delta = time - oldTime;
		cpuTime += delta;
		oldTime = time;*/
		lcd_cycle();
		/*time = Halib::GetTimeSinceStartup();
		delta = time - oldTime;
		lcdTime += delta;
		oldTime = time;*/
	}
	
	return 0;
}
