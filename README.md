# g_s_l
genaric_scripting_language

```
    // COMMAND BRAKEDOWN
    //                                     0x00 -- do nothing (hehe nop ftw)
    //                                     0x01 -- if vvvvv---- if opperands ----vvvvvv  IF true, execute the line below it, else skip it
    //                                               data_a   data_b |                  opp
    //    Load a const int 0 - 239                 0x(0-E)(0-F)      | 0x10(=), 0x20(!=), 0x30(>) , 0x40(<)
    //                                                               |                    0x31(>=), 0x41(<=)
    //    load from data register                  0xF[reg]          |
    //                                           --------------------^^ IF ^^-------------------
    //-----------------------------------------------------------------
    //                                     OPP      ARG 1         ARG 2
    //    Set a value in the register      0x02 -- location,      value
    //    GOTO                             0x03 -- line number
    //    Incriment register               0x04 -- register
    //    Decrement register               0x05 -- register
    //----------------------(INT MATH ONLY)----------------------------
    //    add                              0x06 -- data a, data b, dest (loc only)
    //    sub                              0x07 -- data a, data b, dest
    //    multiply                         0x08 -- data a, data b, dest
    //    devide                           0x09 -- data a, data b, dest
    //------------------------(DEV STUFF)------------------------------
    //    DUMP                             0xFF -- DUMP REGISTER
    //-----------------------------------------------------------------

    // FUNCTION OVERVIEW
    // The first element of the prog io array will act as the program counter, and will incriment once every line of the program.
    // The Seconed elemnt of the prog io array will be the resault location, resaults of if will be stored here, along with any math that is done.

    // WORKING FUNCTIONS (TESTED)
    // DUMP REGISTER
    // GOTO
    // INC & DEC
    // IF STATEMENTS
    // ADDTION
    // Subtraction
    // MULT
    // DIV
	
/*	
	
Example programs

	volatile unsigned char test_program_0[10][4] = {
    // CMD, ARG,ARG,ARG
    0x02, 0x02, 0xFF, 0,   // 0 |SET Reg2 ->256
    0x09, 0xF2, 0x2, 0x02, // 1 |reg 2 / 2 -> reg2
    0xFF, 0, 0, 0,         // 2 |DUMP
    0x01, 0xF2, 2, 0x40,   // 3 |IF reg2 < 2
    0x03, 0, 0, 0,         // 4 | goto 0 //reset the vals to keep deviding
    0x03, 1, 0, 0,         // 5 |goto 1  //skip setting the value
    0, 0, 0, 0,            // 6 |
    0, 0, 0, 0,            // 7 |
    0, 0, 0, 0,            // 8 |
    0, 0, 0, 0             // 9 |
	};
	
	

volatile unsigned char test_program_0[10][4] = {
    // CMD, ARG,ARG,ARG
    cmd_interpreter::cmds::SRG, 0x02, 0xFF, 0,   // 0 |SET Reg2 ->255
    cmd_interpreter::cmds::DIV, 0xF2, 0x2, 0x02, // 1 |reg2/2 -> reg2
    cmd_interpreter::cmds::DUMP, 0, 0, 0,        // 2 |DUMP
    cmd_interpreter::cmds::IF, 0xF2, 2, 0x40,    // 3 |IF reg2 < 2
    cmd_interpreter::cmds::GOTO, 0, 0, 0,        // 4 | goto 0 //reset the vals to keep deviding
    cmd_interpreter::cmds::GOTO, 1, 0, 0,        // 5 |goto 1  //skip setting the value
    0, 0, 0, 0,                                  // 6 |
    0, 0, 0, 0,                                  // 7 |
    0, 0, 0, 0,                                  // 8 |
    0, 0, 0, 0                                   // 9 |

};
	*/
	
	
	
	PROGRAM
	...				<- PC
	...
	...
	...
	...
	--------------------------
	|0xAF[0]->[5]|0xAF[0]->[5]|
	--------------------------
	0xAF[0] 0XB0[1]
	  |       ^
	  V       |
	 [5]     [6]
	 
	 
----------------------------------------------------
INPUTS: DISABLED                  OUTPUTS: DISABLED
----------------------------------------------------
ADDON: 0x02[0]  0x01[0]
         v        ^
LOCAL:  [5]      [6]




ADDON: 0x02[1 ]  0x02[3 ]  0x02[3 ]  0x02[3 ]  0x02[2 ]
         ^         v
LOCAL:  [1 ]      [2 ]      [3 ]      [4 ]      [10]

-------------------STATUS REGISTERS-----------------
|  LAST RUN TIME: 0000 | RUNNING: T | LOOP_DONE: T |
----------------------------------------------------



-------------------STATUS REGISTERS-----------------
|  LAST RUN TIME: 0 | RUNNING: T | LOOP_DONE: T    |
----------------------------------------------------
```
