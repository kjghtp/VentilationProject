/*
 * ABBcontroller.h
 *
 *  Created on: 10.3.2017
 *      Author: Tommi
 */

#ifndef ABBCONTROLLER_H_
#define ABBCONTROLLER_H_

#include <cstring>
#include <cstdio>

#include "ModbusMaster.h"
#include "I2C.h"
#include "ITM_write.h"
#include "LiquidCrystal.h"
#include "DigitalIoPin.h"

class ABBcontroller {
public:
	ABBcontroller();
	bool autoMeasure();
	bool manualMeasure();
	virtual ~ABBcontroller();
	bool getMode();
	void printRegister(uint16_t reg);
	bool setFrequency(uint16_t freq);
	int measureAndCompare();
	bool startAbb();
	bool stopAbb();
	void printData();
	// UI functions
	void drawUserInterface(); // näin alkuun ~
	void readUserinput();

private:
	bool autoMode;
	ModbusMaster *node;
	//I2C i2c;
	uint16_t frequency;
	uint16_t pasc;
	int tickLimit; //One tick is 100ms

	// User interface
	LiquidCrystal* lcd;
	enum userInterfaceStates { menu, automaticMode, manualMode, endOfEnum };
	int userInterfaceState, selection;
	enum interfaceControls { ok, left, right };
	DigitalIoPin *switch1Ok, *switch2Left, *switch3Right;

};

#endif /* ABBCONTROLLER_H_ */
