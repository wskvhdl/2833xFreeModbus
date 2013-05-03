/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id: portserial.c,v 1.1 2006/08/22 21:35:13 wolti Exp $
 */

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include "debug.h"

/* ----------------------- static functions ---------------------------------*/
interrupt void prvvUARTTxReadyISR( void );
interrupt void prvvUARTRxISR( void );

/* ----------------------- Start implementation -----------------------------*/
void vMBPortSerialEnable( BOOL xRxEnable, BOOL xTxEnable )
{
	/* If xRXEnable enable serial receive interrupts. If xTxENable enable
	 * transmitter empty interrupts.
	 */
	ENTER_CRITICAL_SECTION(  );
	if(xRxEnable){
		//SciaRegs.SCICTL1.bit.RXENA = 1;
		SciaRegs.SCICTL2.bit.RXBKINTENA =1;
	}
	else {
		//SciaRegs.SCICTL1.bit.RXENA = 0;
		SciaRegs.SCICTL2.bit.RXBKINTENA =0;
	}

	if(xTxEnable){
		//SciaRegs.SCICTL1.bit.TXENA = 1;
		SciaRegs.SCICTL2.bit.TXINTENA =1;
		SciaRegs.SCICTL2.bit.TXEMPTY = 0;
		SciaRegs.SCICTL1.bit.SWRESET = 0;
		SciaRegs.SCICTL1.bit.SWRESET = 1;
		//IFR = 0x0000;
	}
	else{
		//SciaRegs.SCICTL1.bit.TXENA = 0;
		SciaRegs.SCICTL2.bit.TXINTENA =0;
	}
	EXIT_CRITICAL_SECTION(  );
}

BOOL xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity )
{
	(void)ucPORT;

	ENTER_CRITICAL_SECTION(  );
	PieVectTable.SCIRXINTA = &prvvUARTRxISR;
	PieVectTable.SCITXINTA = &prvvUARTTxReadyISR;

	SciaRegs.SCICCR.all =0x0007;	// 1 stop bit,  No loopback
									// No parity,8 char bits,
									// async mode, idle-line protocol
	SciaRegs.SCICTL1.all =0x0003;  // enable TX, RX, internal SCICLK,
								  // Disable RX ERR, SLEEP, TXWAKE
	SciaRegs.SCIFFTX.bit.SCIFFENA = 0;
	//UCDATABITS SETTINGS -------------------------------------------
	switch(ucDataBits){
		case 8:
			SciaRegs.SCICCR.bit.SCICHAR = 0x7;
			break;
		case 7:
			SciaRegs.SCICCR.bit.SCICHAR = 0x6;
			break;
		default:
			SciaRegs.SCICCR.bit.SCICHAR = 0x7;
	}

	//EPARITY SETTINGS ----------------------------------------------
	switch(eParity){
		case MB_PAR_EVEN:
			SciaRegs.SCICCR.bit.PARITYENA = 1;
			SciaRegs.SCICCR.bit.PARITY = 1;
			break;
		case MB_PAR_ODD:
			SciaRegs.SCICCR.bit.PARITYENA = 1;
			SciaRegs.SCICCR.bit.PARITY = 0;
			break;
		case MB_PAR_NONE:
			SciaRegs.SCICCR.bit.PARITYENA = 0;
			break;
		default:
			SciaRegs.SCICCR.bit.PARITYENA = 0;
	}

	//ULBAUDRATE SETTINGS -------------------------------------------
	#if (CPU_FRQ_150MHZ)
	SciaRegs.SCIHBAUD    =0x0001;  // 9600 baud @LSPCLK = 37.5MHz.
	SciaRegs.SCILBAUD    =0x00E7;
	#endif
	#if (CPU_FRQ_100MHZ)
	SciaRegs.SCIHBAUD    =0x0001;  // 9600 baud @LSPCLK = 20MHz.
	SciaRegs.SCILBAUD    =0x0044;


	#endif

	PieCtrlRegs.PIECTRL.bit.ENPIE = 1;   // Enable the PIE block
	PieCtrlRegs.PIEIER9.bit.INTx1=1;     // PIE Group 9, int1
	PieCtrlRegs.PIEIER9.bit.INTx2=1;     // PIE Group 9, INT2

	//IER = 0x100;	// Enable CPU INT
	IER |= M_INT9;	// Enable CPU INT
	SciaRegs.SCICTL1.all =0x0023;     // Relinquish SCI from Reset
	vMBPortSerialEnable( FALSE, FALSE );
	EXIT_CRITICAL_SECTION(  );
	return TRUE;
}

BOOL xMBPortSerialPutByte( CHAR ucByte )
{
	/* Put a byte in the UARTs transmit buffer. This function is called
	 * by the protocol stack if pxMBFrameCBTransmitterEmpty( ) has been
	 * called. */

	SciaRegs.SCITXBUF=ucByte;
	return TRUE;
}

BOOL xMBPortSerialGetByte( CHAR * pucByte )
{
	/* Return the byte in the UARTs receive buffer. This function is called
	 * by the protocol stack after pxMBFrameCBByteReceived( ) has been called.
	 */
	*pucByte = SciaRegs.SCIRXBUF.bit.RXDT;
	return TRUE;
}

/* Create an interrupt handler for the transmit buffer empty interrupt
 * (or an equivalent) for your target processor. This function should then
 * call pxMBFrameCBTransmitterEmpty( ) which tells the protocol stack that
 * a new character can be sent. The protocol stack will then call
 * xMBPortSerialPutByte( ) to send the character.
 */
interrupt void prvvUARTTxReadyISR( void )
{
	static unsigned int uiCnt = 0;
	PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ack
	#if (TEST == TEST_TX)
	if(uiCnt++ < 10)
		xMBPortSerialPutByte('a');
	else
		vMBPortSerialEnable(false,false);
	#elif (TEST == NO_TEST)
	pxMBFrameCBTransmitterEmpty(  );
	#endif
}

/* Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
interrupt void prvvUARTRxISR( void )
{
	PieCtrlRegs.PIEACK.all|=0x100;       // Issue PIE ack
	#if (TEST == TEST_RX)
	char cByte;
	(void) xMBPortSerialGetByte(&cByte);
	#else
	pxMBFrameCBByteReceived(  );
	#endif
}
