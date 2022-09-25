/*
	protocol.c
	
	Handles the higher-level parts of the PS/2 protocol
	HID conversion, responding to host commands

*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "CH559.h"
#include "util.h"
#include "USBHost.h"
#include "uart.h"
#include "ps2.h"
#include "data.h"
#include "ps2protocol.h"
#include "menu.h"

// repeatState -
// if positive, we're delaying - count up to repeatDelay then go negative
// if negative, we're repeating - count down to repeatRate then return to -1
// if zero, we ain't repeating
__xdata volatile int16_t RepeatState = 0;

__xdata volatile uint8_t RepeatKey = 0x00;
__xdata int16_t RepeatDelay = 7500;
__xdata int16_t RepeatRate = -1000;

__xdata char lastKeyboardHID[8];

// runs in interupt to keep timings
void RepeatTimer()
{
	if (RepeatState > 0)
		RepeatState++;
	else if (RepeatState < 0)
		RepeatState--;
}
__code int16_t DelayConv[] = {
	3750,
	7500,
	11250,
	15000};

__code int16_t RateConv[] = {
	-500,
	-562,
	-625,
	-688,
	-725,
	-811,
	-877,
	-938,
	-1000,
	-1128,
	-1250,
	-1376,
	-1500,
	-1630,
	-1744,
	-1875,
	-2000,
	-2239,
	-2500,
	-2727,
	-3000,
	-3261,
	-3488,
	-3750,
	-4054,
	-4545,
	-5000,
	-5556,
	-6000,
	-6522,
	-7143,
	-7500};

// Runs in main loop
void HandleRepeats()
{
	if (RepeatState > RepeatDelay)
	{
		SetRepeatState(-1);
	}
	else if (RepeatState < RepeatRate)
	{
		SendKeyboard(HIDtoPS2_Make[RepeatKey]);
		SetRepeatState(-1);
	}
}

void SetKey(uint8_t key, HID_REPORT *report)
{
	report->KeyboardKeyMap[key >> 3] |= 1 << (key & 0x07);
}

void processSeg(HID_SEG *currSeg, HID_REPORT *report)
{
	bool make = 0;
	uint8_t tmp = 0;

	if (currSeg->OutputChannel == MAP_KEYBOARD)
		report->keyboardUpdated = 1;
	else if (currSeg->OutputChannel == MAP_MOUSE)
		report->mouseUpdated = 1;

	if (currSeg->InputType == MAP_TYPE_THRESHOLD_ABOVE && currSeg->value > currSeg->InputParam)
	{
		make = 1;
	}
	else if (currSeg->InputType == MAP_TYPE_THRESHOLD_BELOW && currSeg->value < currSeg->InputParam)
	{
		make = 1;
	}

	else
	{
		make = 0;
	}

	if (make)
	{
		if (currSeg->OutputChannel == MAP_KEYBOARD)
		{
			SetKey(currSeg->OutputControl, report);
		}
		else
		{
			switch (currSeg->OutputControl)
			{
			case MAP_MOUSE_BUTTON1:
				report->nextMousePacket[0] |= 0x01;
				break;
			case MAP_MOUSE_BUTTON2:
				report->nextMousePacket[0] |= 0x02;
				break;
			case MAP_MOUSE_BUTTON3:
				report->nextMousePacket[0] |= 0x04;
				break;
			}
		}
	}
	else if (currSeg->InputType == MAP_TYPE_SCALE)
	{
		if (currSeg->OutputChannel == MAP_MOUSE)
		{

			switch (currSeg->OutputControl)
			{
			// TODO scaling
			case MAP_MOUSE_X:
				//printf("%hd ", currSeg->value);
				if (currSeg->InputParam == 2)
					currSeg->value = (uint8_t)((int8_t)((currSeg->value + 8) >> 4) - 0x08);
				else
					currSeg->value = currSeg->value;
				report->nextMousePacket[1] = currSeg->value;
				report->nextMousePacket[0] = (report->nextMousePacket[0] & 0b11101111) | ((currSeg->value >> 3) & 0b00010000);
				//printf("%hd\n", report->nextMousePacket[1]);
				break;
			case MAP_MOUSE_Y:
				printf("%hd ", currSeg->value);
				if (currSeg->InputParam == 2)
					currSeg->value = (uint8_t)(-((int8_t)((currSeg->value + 8) >> 4) - 0x08));
				else
					currSeg->value = (uint8_t)(-((int8_t)currSeg->value));
				report->nextMousePacket[2] = currSeg->value;
				report->nextMousePacket[0] = (report->nextMousePacket[0] & 0b11011111) | ((currSeg->value >> 2) & 0b00100000);
				printf("%hd\n", report->nextMousePacket[2]);
				break;
			}
		}
	}
	else if (currSeg->InputType == MAP_TYPE_ARRAY)
	{
		if (currSeg->OutputChannel == MAP_KEYBOARD)
		{
			SetKey(currSeg->value, report);
		}
	}
}

__code uint8_t bitMasks[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1F, 0x3F, 0x7F, 0xFF};

bool BitPresent(uint8_t *bitmap, uint8_t bit)
{
	if (bitmap[bit >> 3] & (1 << (bit & 0x07)))
		return 1;
	else
		return 0;
}

bool ParseReport(HID_REPORT_DESC *desc, uint32_t len, uint8_t *report)
{

	HID_REPORT *descReport;
	HID_SEG *currSeg;
	uint8_t *currByte;
	uint32_t tmp;
	/*for (tmp = 0; tmp < (len >> 3); tmp++)
	{
		printf("%x ", report[tmp]);
	}
	printf("\n\n");*/

	if (desc->usesReports)
	{
		// first byte of report will be the report number
		descReport = desc->reports[report[0]];
	}
	else
	{
		descReport = desc->reports[0];
	}

	// sanity check length
	if (descReport->length != len)
	{
		printf("Bad length - %u -> %lu\n", descReport->length, len);
		return 0;
	}

	currSeg = descReport->firstHidSeg;

	// clear key map as all pressed keys should be present in report
	memset(descReport->KeyboardKeyMap, 0, 32);
	memset(descReport->nextMousePacket, 0, 3);
	descReport->nextMousePacket[0] |= 0b00001000;
	// TODO handle segs that are bigger than 8 bits
	while (currSeg != NULL)
	{

		// find byte
		currByte = report + (currSeg->startBit >> 3);

		//currSeg->oldValue = currSeg->value;

		// find bits
		currSeg->value = ((*currByte) >> (currSeg->startBit & 0x07)) // shift bits so lsb of this seg is at bit zero
						 & bitMasks[currSeg->reportSize];			 // mask off the bits according to seg size

		// process em
		// if (currSeg->value != currSeg->oldValue)
		processSeg(currSeg, descReport);

		currSeg = currSeg->next;
	}

	if (descReport->mouseUpdated)
	{
		if (!MenuActive)
		{
			SendMouse3(descReport->nextMousePacket[0], descReport->nextMousePacket[1], descReport->nextMousePacket[2]);
			descReport->mouseUpdated = 0;
		}
	}
	if (descReport->keyboardUpdated)
	{
		for (uint8_t c = 0; c < 255; c++)
		{
			if (BitPresent(descReport->KeyboardKeyMap, c) && !BitPresent(descReport->oldKeyboardKeyMap, c))
			{
				if (MenuActive)
					Menu_Press_Key(c);
				else
				{
					// Make
					if (c <= 0x67)
					{
						SendKeyboard(HIDtoPS2_Make[c]);
						RepeatKey = c;
						SetRepeatState(1);
					}
					else if (c >= 0xE0 && c <= 0xE7)
					{
						SendKeyboard(ModtoPS2_MAKE[c - 0xE0]);
					}
				}
			}
			else if (!BitPresent(descReport->KeyboardKeyMap, c) && BitPresent(descReport->oldKeyboardKeyMap, c))
			{
				if (!MenuActive)
				{
					// break
					if (c <= 0x67)
					{
						// if the key we just released is the one that's repeating then stop
						if (c == RepeatKey)
						{
							RepeatKey = 0;
							SetRepeatState(0);
						}

						// Pause has no break for some reason
						if (c == 0x48)
							continue;

						SendKeyboard(HIDtoPS2_Break[c]);
					}
					else if (c >= 0xE0 && c <= 0xE7)
					{
						SendKeyboard(ModtoPS2_BREAK[c - 0xE0]);
					}
				}
			}
		}

		memcpy(descReport->oldKeyboardKeyMap, descReport->KeyboardKeyMap, 32);
		descReport->keyboardUpdated = 0;
	}
}

void TypematicDefaults()
{
	RepeatDelay = DelayConv[0x01]; // 500ms
	RepeatRate = RateConv[0x0B];   // 10.9cps
}

void HandleReceived(uint8_t port)
{
	if (port == PORT_KEY)
	{
		// see if this is a command byte - these should override any state we're already in so check these first
		switch (ports[PORT_KEY].recvout)
		{

		// set LEDs
		case 0xED:
			SimonSaysSendKeyboard(KEY_ACK);
			ports[PORT_KEY].recvstate = R_LEDS;
			break;

		// Echo
		case 0xEE:
			SimonSaysSendKeyboard(KEY_ECHO);
			ports[PORT_KEY].recvstate = R_IDLE;
			break;

		// Set/Get scancode set
		case 0xF0:
			SimonSaysSendKeyboard(KEY_ACK);
			ports[PORT_KEY].recvstate = R_SCANCODESET;
			break;

		// ID
		case 0xF2:
			SimonSaysSendKeyboard(KEY_ACK);
			SimonSaysSendKeyboard(KEY_ID);
			ports[PORT_KEY].recvstate = R_IDLE;
			break;

		// set repeat
		case 0xF3:
			SimonSaysSendKeyboard(KEY_ACK);
			ports[PORT_KEY].recvstate = R_REPEAT;
			break;

		// Enable (TODO : actually do this)
		case 0xF4:
			SimonSaysSendKeyboard(KEY_ACK);
			ports[PORT_KEY].recvstate = R_IDLE;
			break;

		// Disable (TODO : actually do this)
		case 0xF5:
			SimonSaysSendKeyboard(KEY_ACK);
			TypematicDefaults();
			ports[PORT_KEY].recvstate = R_IDLE;
			break;

		// Set Default
		case 0xF6:
			SimonSaysSendKeyboard(KEY_ACK);
			TypematicDefaults();
			ports[PORT_KEY].recvstate = R_IDLE;
			break;

		// Set All Keys whatever (TODO : Actually do this)
		case 0xF7:
		case 0xF8:
		case 0xF9:
		case 0xFA:
			SimonSaysSendKeyboard(KEY_ACK);
			ports[PORT_KEY].recvstate = R_IDLE;
			break;

		// Set specific Keys whatever (TODO : Actually do this)
		case 0xFB:
		case 0xFC:
		case 0xFD:
			SimonSaysSendKeyboard(KEY_ACK);
			ports[PORT_KEY].recvstate = R_KEYLIST;
			break;

		// resend - TODO figure out how to deal with this
		case 0xFE:
			SimonSaysSendKeyboard(KEY_ACK);
			break;

		// reset
		case 0xFF:
			SimonSaysSendKeyboard(KEY_ACK);
			TypematicDefaults();
			SimonSaysSendKeyboard(KEY_BATCOMPLETE);
			ports[PORT_KEY].recvstate = R_IDLE;
			break;

		// not a command byte - this is fine if we're in another state, otherwise we send an error
		default:
			switch (ports[PORT_KEY].recvstate)
			{

			case R_LEDS:
				// TODO blinkenlights
				SimonSaysSendKeyboard(KEY_ACK);
				ports[PORT_KEY].recvstate = R_IDLE;
				break;

			case R_REPEAT:
				SimonSaysSendKeyboard(KEY_ACK);
				// Bottom 5 bits are repeate rate
				RepeatRate = RateConv[ports[PORT_KEY].recvout & 0x1F];

				// top 3 bits are delay time
				RepeatDelay = DelayConv[(ports[PORT_KEY].recvout >> 5) & 0x03];

				ports[PORT_KEY].recvstate = R_IDLE;
				break;

			case R_SCANCODESET:
				SimonSaysSendKeyboard(KEY_ACK);

				// if request is zero then PC is expecting us to send current scan code set number#
				// TODO : allow changing scancode set and reporting correctly
				if (ports[PORT_KEY].recvout == 0)
					SimonSaysSendKeyboard(KEY_SCANCODE_2);

				ports[PORT_KEY].recvstate = R_IDLE;
				break;

			case R_KEYLIST:
				// Keep sending ACKs until we get a valid command scancode (which will be dealt with by the previous section)
				// TODO : actually log these keys and apply their settings appropriately
				// (not that I've ever seen any programs actually require this)
				SimonSaysSendKeyboard(KEY_ACK);
				break;

			case R_IDLE:
			default:
				// this means we receieved a non-command byte when we are expecting one, send error
				SimonSaysSendKeyboard(KEY_ERROR);
				ports[PORT_KEY].recvstate = R_IDLE;
				break;
			}
		}

		// If we're not expecting more stuff,
		// unlock the send buffer so main loop can send stuff again
		if (ports[PORT_KEY].recvstate == R_IDLE)
		{
			ports[PORT_KEY].sendDisabled = 0;
		}
	}

	else if (port == PORT_MOUSE)
	{
		switch (ports[PORT_KEY].recvstate)
		{
		case R_IDLE:

			switch (ports[port].recvout)
			{
			case 0xE9:							// Status Request
				SimonSaysSendMouse1(0xFA);		// ACK
				SimonSaysSendMouse3(0b00100000, // Stream Mode, Scaling 1:1, Enabled, No buttons pressed
									0x02,		// Resolution 4 counts/mm
									100);		// Sample rate 100/sec
				break;

			// ID
			case 0xF2:
				SimonSaysSendMouse1(0xFA); // ACK
				SimonSaysSendMouse1(0x00); // Standard mouse
				break;

			// Reset
			case 0xFF:
				SimonSaysSendMouse1(0xFA); // ACK
				SimonSaysSendMouse1(0xAA); // POST OK
				SimonSaysSendMouse1(0x00); // Squeek Squeek I'm a mouse
				break;

			// unimplemented command
			case 0xE6: // Set Scaling 1:1
			case 0xE7: // Set Scaling 2:1
			case 0xE8: // Set Resolution
			case 0xEA: // Set Stream Mode
			case 0xEB: // Read Data
			case 0xEC: // Reset Wrap Mode
			case 0xEE: // Set Wrap Mode
			case 0xF0: // Remote Mode
			case 0xF3: // Set Sample Rate
			case 0xF4: // Enable Reporting
			case 0xF5: // Disable Reporting
			case 0xF6: // Set Defaults
			case 0xFE: // Resend
			default:   // argument from command?

				SimonSaysSendMouse1(0xFA); // Just smile and nod
				break;
			}
		}
		// If we're not expecting more stuff,
		// unlock the send buffer so main loop can send stuff again
		if (ports[PORT_MOUSE].recvstate == R_IDLE)
		{
			ports[PORT_MOUSE].sendDisabled = 0;
		}
	}
}