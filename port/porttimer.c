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
 * File: $Id: porttimer.c,v 1.1 2006/08/22 21:35:13 wolti Exp $
 */

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- static functions ---------------------------------*/
interrupt void prvvTIMERExpiredISR( void );

/* ----------------------- Start implementation -----------------------------*/
BOOL
xMBPortTimersInit( USHORT usTim1Timerout50us )
{
	// Interrupts that are used in this example are re-mapped to
	// ISR functions found within this file.
	EALLOW;  // This is needed to write to EALLOW protected registers
	PieVectTable.TINT0 = &prvvTIMERExpiredISR;
	EDIS;    // This is needed to disable write to EALLOW protected registers	

	// Initialize the Device Peripheral. This function can be found in DSP2833x_CpuTimers.c
	InitCpuTimers();

	#if (CPU_FRQ_150MHZ)
	// Configure CPU-Timer 0 to interrupt every second:
	// 150MHz CPU Freq, 1 second Period (in uSeconds)
	ConfigCpuTimer(&CpuTimer0, 150, usTim1Timerout50us);
	#endif

	#if (CPU_FRQ_100MHZ)
	// Configure CPU-Timer 0 to interrupt every second:
	// 100MHz CPU Freq, 1 second Period (in uSeconds)
	ConfigCpuTimer(&CpuTimer0, 100, usTim1Timerout50us);
	#endif

	// To ensure precise timing, use write-only instructions to write to the entire register. Therefore, if any
	// of the configuration bits are changed in ConfigCpuTimer and InitCpuTimers (in DSP2833x_CpuTimers.h), the
	// below settings must also be updated.
	//CpuTimer0Regs.TCR.all = 0x4009;	// Use write-only instruction to set TSS bit = 1
	CpuTimer0Regs.TCR.all = 0xC011;

	// Enable CPU int1 which is connected to CPU-Timer 0
	IER |= M_INT1;
	//IER |= M_INT13;
	//IER |= M_INT14;

	// Enable TINT0 in the PIE: Group 1 interrupt 7
	PieCtrlRegs.PIEIER1.bit.INTx7 = 1;

	// Enable global Interrupts and higher priority real-time debug events:
	EINT;   // Enable Global interrupt INTM
	ERTM;   // Enable Global realtime interrupt DBGM

    return TRUE;
}


void vMBPortTimersEnable(  )
{
    /* Enable the timer with the timeout passed to xMBPortTimersInit( ) */
	CpuTimer0Regs.TIM.all = 0x0;	//Reset counter

	CpuTimer0Regs.TCR.all = 0xC001; //TIF = 1; TIE = 1; TSS = 0
	//CpuTimer0Regs.TCR.bit.TIF = 1;
	//CpuTimer0Regs.TCR.bit.TIE = 1;
	//CpuTimer0Regs.TCR.bit.TSS = 0;
}

void vMBPortTimersDisable(  )
{
    /* Disable any pending timers. */
	CpuTimer0Regs.TCR.all = 0x0011; // TSS = 1; TIF = 0; TIE = 0
	//CpuTimer0Regs.TCR.bit.TIF = 0;
	//CpuTimer0Regs.TCR.bit.TIE = 0;
	//CpuTimer0Regs.TCR.bit.TSS = 1;
}

/* Create an ISR which is called whenever the timer has expired. This function
 * must then call pxMBPortCBTimerExpired( ) to notify the protocol stack that
 * the timer has expired.
 */
interrupt void prvvTIMERExpiredISR( void )
{
	CpuTimer0.InterruptCount++;
    ( void )pxMBPortCBTimerExpired(  );
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;

    CpuTimer0Regs.TCR.bit.TIF = 1;
}

