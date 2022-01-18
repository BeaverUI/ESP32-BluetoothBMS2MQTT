#ifndef mydatatypes_H_
#define mydatatypes_H_

#define BMS_MAX_CELLS 20

typedef struct
{
	byte start;
	byte type;
	byte status;
	byte dataLen;
} bmsPacketHeaderStruct;

typedef struct
{
	uint16_t Volts; // unit 1mV
	int32_t Amps;   // unit 1mA
	int32_t Watts;   // unit 1W
	uint16_t CapacityRemainAh;
	uint8_t CapacityRemainPercent; //unit 1%
	uint16_t Temp1;				   //unit 0.1C
	uint16_t Temp2;				   //unit 0.1C
	uint16_t BalanceCodeLow;
	uint16_t BalanceCodeHigh;
	uint8_t MosfetStatus;
	
} packBasicInfoStruct;

typedef struct
{
	uint8_t NumOfCells;
	uint16_t CellVolt[BMS_MAX_CELLS]; //cell 1 has index 0 :-/
	uint16_t CellMax;
	uint16_t CellMin;
	uint16_t CellDiff; // difference between highest and lowest
	uint16_t CellAvg;
} packCellInfoStruct;

#endif /* mydatatypes_H_ */
