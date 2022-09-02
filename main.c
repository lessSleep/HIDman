#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "CH559.h"
#include "util.h"
#include "USBHost.h"
#include "uart.h"
#include "ps2.h"

SBIT(LED, 0x90, 6);

void mTimer0Interrupt(void) __interrupt(1)
{
	TH0 = 0xff;
	TL0 = 0xB8;
	PS2ProcessPort(PORT_KEY);
	PS2ProcessPort(PORT_MOUSE);
}

void main()
{
	unsigned char s;
	uint8_t keyindex;
	initClock();
	initUART0(1000000, 1);
	DEBUG_OUT("Startup\n");
	resetHubDevices(0);
	resetHubDevices(1);
	initUSB_Host();

	//port2 setup
	PORT_CFG |= bP2_OC; // open collector
	P2_DIR = 0xff;		// output
	P2_PU = 0x00;		// pull up - change this to 0x00 when we add the 5v pullup

	//timer0 setup
	TMOD = (TMOD & 0xf0) | 0x01; // mode 1 (16bit no auto reload)

	// preload timer
	TH0 = 0xff;
	TL0 = 0xB8;
	TR0 = 1; // start timer0
	ET0 = 1; //enable timer0 interrupt;
	EA = 1;	 // enable all interrupts

	P0_DIR = 0b01110000; // LEDs as output
	P0_PU = 0x00;
	P0 = 0x00; // all lit
	P0 = 0b01110000; // none lit

	DEBUG_OUT("Ready\n");
	//sendProtocolMSG(MSG_TYPE_STARTUP,0, 0x00, 0x00, 0x00, 0);

	OutPort(PORT_KEY, DATA, 1);
	OutPort(PORT_KEY, CLOCK, 1);

	OutPort(PORT_MOUSE, DATA, 1);
	OutPort(PORT_MOUSE, CLOCK, 1);

	while (1)
	{
		if (!(P4_IN & (1 << 6)))
			runBootloader();
		processUart();
		s = checkRootHubConnections();
		pollHIDdevice();

	}
}