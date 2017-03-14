/*
 * ABBcontroller.cpp
 *
 *  Created on: 10.3.2017
 *      Author: Tommi
 */

#include "ABBcontroller.h"
#include <cstring>
#include <cstdio>

static volatile int counter;
static volatile uint32_t systicks;
static volatile int counterAutoMeasure;
static volatile bool flagAutoMeasure;

static volatile bool flagSlowAlert;
//static uint16_t
static uint16_t pascLimit = 120;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief	Handle interrupt from SysTick timer
 * @return	Nothing
 */
void SysTick_Handler(void)
{
	systicks++;
	if(counter > 0) counter--;

	counterAutoMeasure++;
	if (counterAutoMeasure >= 3000) {
		counterAutoMeasure = 0;
		flagAutoMeasure = true;
	}
}
#ifdef __cplusplus
}
#endif

void Sleep(int ms)
{
	counter = ms;
	while(counter > 0) {
		__WFI();
	}
}

/* this function is required by the modbus library */
uint32_t millis() {
	return systicks;
}



ABBcontroller::ABBcontroller() {
	autoMode = true;
	node = new ModbusMaster(2);
	//I2C i2c = new I2C(0, 100000);
	frequency = 5000;
	pasc = 20;
	tickLimit = 300;
	oneStep = 250;

	// ------------------------- USER INTRERFACE
	// Initializing the LCD
	DigitalIoPin* pin1 = new DigitalIoPin(0, 8, false);
	DigitalIoPin* pin2 = new DigitalIoPin(1, 6, false);
	DigitalIoPin* pin3 = new DigitalIoPin(1, 8, false);
	DigitalIoPin* pin4 = new DigitalIoPin(0, 5, false);
	DigitalIoPin* pin5 = new DigitalIoPin(0, 6, false);
	DigitalIoPin* pin6 = new DigitalIoPin(0, 7, false);
	lcd = new LiquidCrystal(pin1, pin2, pin3, pin4, pin5, pin6);

	// Setting default state for user interface
	userInterfaceState = menu;
	selection = automaticMode;

	// Initializing switches
	switch1Ok = new DigitalIoPin(0, 17, true, true, true);
	switch2Left = new DigitalIoPin(1, 11, true, true, true);
	switch3Right = new DigitalIoPin(1, 9, true, true, true);

	drawUserInterface();

	frequencyTemp = 50; pascTemp = 50;
	//
}

bool ABBcontroller::startAbb(){
	node->begin(9600); // set transmission rate - other parameters are set inside the object and can't be changed here

	printRegister( 3); // for debugging

	node->writeSingleRegister(0, 0x0406); // prepare for starting

	printRegister(3); // for debugging

	Sleep(1000); // give converter some time to set up
	// note: we should have a startup state machine that check converter status and acts per current status
	//       but we take the easy way out and just wait a while and hope that everything goes well

	printRegister(3); // for debugging

	node->writeSingleRegister(0, 0x047F); // set drive to start mode

	printRegister(3); // for debugging

	Sleep(1000); // give converter some time to set up
	// note: we should have a startup state machine that check converter status and acts per current status
	//       but we take the easy way out and just wait a while and hope that everything goes well

	printRegister(3); // for debugging
	return true;
}

void ABBcontroller::printRegister(uint16_t reg){
	uint8_t result;
	// slave: read 16-bit registers starting at reg to RX buffer
	result = node->readHoldingRegisters(reg, 1);

	// do something with data if read is successful
	if (result == node->ku8MBSuccess)
	{
		printf("R%d=%04X\n", reg, node->getResponseBuffer(0));
	}
	else {
		printf("R%d=???\n", reg);
	}

}

bool ABBcontroller::setFrequency(uint16_t freq){
	uint8_t result;
	int ctr;
	bool atSetpoint;
	const int delay = 500;

	node->writeSingleRegister(1, freq); // set motor frequency

	printf("Set freq = %d\n", freq/40); // for debugging

	// wait until we reach set point or timeout occurs
	ctr = 0;
	atSetpoint = false;
	do {
		Sleep(delay);
		// read status word
		result = node->readHoldingRegisters(3, 1);
		// check if we are at setpoint
		if (result == node->ku8MBSuccess) {
			if(node->getResponseBuffer(0) & 0x0100) atSetpoint = true;
		}
		ctr++;
	} while(ctr < 20 && !atSetpoint);

	printf("Elapsed: %d\n", ctr * delay); // for debugging
	frequency = freq;
	return atSetpoint;

}

bool ABBcontroller::getMode(){
	return ABBcontroller::autoMode;
}

bool ABBcontroller::manualMeasure(){
	if (!flagAutoMeasure) {
		return false;
	}else {
		flagAutoMeasure = false;
		ABBcontroller::setFrequency(frequency);
		return true;
	}
}


bool ABBcontroller::autoMeasure(){
	if (!flagAutoMeasure) {
		return false;
	} else {
		flagAutoMeasure = false;
		//uint16_t inc;
		int i = 0;
		int j = 0;
		uint8_t result;

		if (ABBcontroller::measureAndCompare() == 1 && (frequency -oneStep) >= 1000){

			//while(ABBcontroller::measureAndCompare() == 1){
			// slave: read (2) 16-bit registers starting at register 102 to RX buffer
			j = 0;
			do {
				result = node->readHoldingRegisters(102, 2);
				j++;
			} while(j < 3 && result != node->ku8MBSuccess);
			// note: sometimes we don't succeed on first read so we try up to threee times
			// if read is successful print frequency and current (scaled values)
			if (result == node->ku8MBSuccess) {
				printf("F=%4d, I=%4d  (ctr=%d)\n", node->getResponseBuffer(0), node->getResponseBuffer(1),j);
			}
			else {
				printf("ctr=%d\n",j);
			}
			// lippu tähän

			//Sleep(3000);
			i++;
			if(i >= 20) {
				i=0;
			}
			frequency= frequency-oneStep;
			ABBcontroller::setFrequency(frequency);

			//}
		} else if (ABBcontroller::measureAndCompare() == -1&& (frequency +oneStep) <= 20000) {

			//while(ABBcontroller::measureAndCompare() == -1){
			// slave: read (2) 16-bit registers starting at register 102 to RX buffer
			j = 0;
			do {
				result = node->readHoldingRegisters(102, 2);
				j++;
			} while(j < 3 && result != node->ku8MBSuccess);
			// note: sometimes we don't succeed on first read so we try up to threee times
			// if read is successful print frequency and current (scaled values)
			if (result == node->ku8MBSuccess) {
				printf("F=%4d, I=%4d  (ctr=%d)\n", node->getResponseBuffer(0), node->getResponseBuffer(1),j);
			}
			else {
				printf("ctr=%d\n",j);
			}

			//Sleep(3000);
			i++;
			if(i >= 20) {
				i=0;
			}
			frequency+=oneStep;
			ABBcontroller::setFrequency(frequency);
			//}
		} else {

		}


		return true;

	}
}
int ABBcontroller::measureAndCompare(){
	I2C i2c(0, 100000);

	uint8_t pressureData[3];
	uint8_t readPressureCmd = 0xF1;
	int16_t pressure = 0;

	//ITM_write("täs");
	if (i2c.transaction(0x40, &readPressureCmd, 1, pressureData, 3)) {
		/* Output temperature. */
		pressure = (pressureData[0] << 8) | pressureData[1];
		DEBUGOUT("Pressure read over I2C is %.1f Pa\r\n",	pressure/240.0);
	}
	else {
		DEBUGOUT("Error reading pressure.\r\n");
	}
	//Sleep(100);
	pressure = pressure/240.0;

	int comparison;
	if(pressure >= pasc){
		comparison = pressure - pasc;
	} else {
		comparison = pasc - pressure;
	}

	if (comparison < 2){
		oneStep = 10;
	} else if (comparison < 5){
		oneStep = 250;
	} else if (comparison > 15){
		oneStep = 2000;
	}else if (comparison > 10){
		oneStep = 1000;
	} else {
		oneStep = 500;
	}


	if (pressure == pasc){
		return 0;
	} else if (pressure > pasc){
		return 1;
	} else {
		return -1;
	}
}


void ABBcontroller::drawUserInterface() {
	char testbuffer[30];
	snprintf ( testbuffer, 100, "state: %d selection: %d \n", userInterfaceState, selection);
	ITM_write(testbuffer);
	lcd->clear();
	lcd->setCursor(0,0);

	char buffer [33];
	std::string tempString = "";

	switch(userInterfaceState) {
	case menu:
		if (selection == automaticMode) {
			lcd->print("Automatic Mode");

		} else if (selection == manualMode) {
			lcd->print("Manual Mode");
		}
		break;

	case automaticMode:
		// upper line
		lcd->print("Set pressure(?):");
		//lower line
		lcd->setCursor(0,1);
		itoa(pascTemp, buffer, 10);
		tempString = std::string(buffer);
		lcd->print(tempString);
		break;

	case manualMode:
		// upper line
		lcd->print("Set fan RPM:");
		// lower line
		lcd->setCursor(0,1);
		itoa(frequencyTemp, buffer, 10);
		tempString = std::string(buffer);
		lcd->print(tempString);
		break;

	default:
		ITM_write("ei näin! \n");
		break;
	}
}

void ABBcontroller::readUserinput() {
	int userInput = 5;

	if (switch1Ok->read()) {
		userInput = ok;
	} else if (switch2Left->read()) {
		userInput = left;
	} else if (switch3Right->read()) {
		userInput = right;
	} else {
		return;
	}

	if (userInterfaceState == menu) { // MENU
		if (userInput == ok) {
			userInterfaceState = selection;
			// temppien settaus  tähän
		} else if (userInput == left || userInput == right) { // toggle between automatic and manual mode
			if (selection == automaticMode) {
				selection = manualMode;
			} else selection = automaticMode;
		}

	} else if (userInterfaceState == manualMode) { // Freq
		switch (userInput) {
		case ok:
			frequency = (20000/100)*frequencyTemp;
			userInterfaceState = menu;
			autoMode = false;
			break;

		case left:
			frequencyTemp -= 10;
			if (frequencyTemp <= 0) frequencyTemp = 100;
			break;

		case right:
			frequencyTemp += 10;
			if (frequencyTemp >= 100) frequencyTemp = 0;
			break;

		default:
			ITM_write("error'\n");
			break;
		}
	} else if (userInterfaceState == automaticMode) { // Pasc
		switch (userInput) {
		case ok:
			pasc = pascTemp;
			userInterfaceState = menu;
			autoMode = true;
			break;

		case left:
			pascTemp -= 5;
			if (pascTemp <= 1) pascTemp = pascLimit;
			break;

		case right:
			pascTemp += 5;
			if (pascTemp >= pascLimit) pascTemp = 1;
			break;

		default:
			ITM_write("error'\n");
			break;
		}
	} else {
		ITM_write("ei tänne\n");
	}
	drawUserInterface();
	Sleep(200); // poista tää
}

ABBcontroller::~ABBcontroller() {
	// TODO Auto-generated destructor stub
}

