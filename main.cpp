#include <Arduino.h>

using namespace std;

namespace cmd_interpreter
{

  const int max_program_length = 10;
  const int max_instruction_args = 4; // THIS INCLUDES THE INITAL COMMAND
  const int prog_data_register_size = 10;

  unsigned char program_space[max_program_length][max_instruction_args]; // dataspace for the program to run
  int program_IO_data_register[prog_data_register_size];

  bool running = false;

  // Check the program and copy it into program space
  bool check_program()
  {
    // check that all commands are valid
    //    check that the args seem resinable

    return 0; // happy program
  }

  bool load_program(unsigned char prog[max_program_length][max_instruction_args])
  {
    for (uint8_t i = 0; i < max_program_length; i++)
    {
      for (uint8_t x = 0; x < max_instruction_args; x++)
      {
        program_space[x][i] = prog[x][i];
      }
    }

    return 0;
  }

  void cmd_interpreter()
  {
    // COMMAND BRAKEDOWN
    //                                     0x00 -- do nothing (hehe nop ftw)
    //                                     0x01 -- if vvvvv---- if opperands ----vvvvvv  IF true, execute the line below it, else skip it
    //                                                     data_a   data_b |                  opp
    //    Load a const int 0 - 15                  0x0(0-F)          | 0x10(=), 0x20(!=), 0x30(>) , 0x40(<)
    //                                                               |                    0x31(>=), 0x41(<=)
    //    load from data register                  0x1[reg]          |
    //                                           --------------------^^ IF ^^-------------------
    //    Set a value in the register      0x02 -- arg 1 is location, arg 2 is value, val spesfied the same way as an if statement
    //    GOTO                             0x03 -- ARG 1 is "line number" or element number
    //
    //                                     0xFF -- DUMP REGISTER

    // FUNCTION OVERVIEW
    // The first element of the prog io array will act as the program counter, and will incriment once every line of the program.
    // The Seconed elemnt of the prog io array will be the resault location, resaults of if will be stored here, along with any math that is done.

    //WORKING FUNCTIONS (TESTED)
    //DUMP REGISTER
    //  WORKS AS EXPECTED
    //GOTO
    //  WORKS AS EXPECTED


    if (!running)
    {
      Serial.println("PROGRAM COMPLEATE -- NOTHING TO RUN");
      return;
    }

    // temp vars for use with commands
    int reg_0;
    int reg_1;
    int reg_2;

    // check what command the current line is

    switch (program_space[program_IO_data_register[0]][0])
    {
    case 0x00: // NOP
      NOP();
      break;

    case 0x01: // IF
      // this one will happon later >.<
      break;

    case 0x02:                                               // Set register
      reg_0 = program_space[program_IO_data_register[0]][1]; // ARG 1  location
      reg_1 = program_space[program_IO_data_register[0]][2]; // ARG 2  value
      program_IO_data_register[reg_0] = reg_1;
      break;

    case 0x03: // GOTO LOCATION
      program_IO_data_register[0] = program_space[program_IO_data_register[0]][1];

      Serial.print("GOTO LINE # ");
      Serial.println(program_IO_data_register[0]);
      return; // WE DONT WANT TO INC THE PC
      break;

    case 0xFF: // DUMP PROG_IO_REG
      Serial.print(" PC -> ");
      Serial.println(program_IO_data_register[0]);
      Serial.print(" RL -> ");
      Serial.println(program_IO_data_register[1]);

      for (uint8_t i = 2; i < prog_data_register_size; i++)
      {
        Serial.print("[");
        Serial.print(i);
        Serial.print("]");
        Serial.print(" -> ");
        Serial.println(program_IO_data_register[i]);
      }
      break;

    default:
      Serial.print(program_space[program_IO_data_register[0]][0]);
      Serial.println(" : NOT FOUND");
      break;
    }

    program_IO_data_register[0]++; // Incriment the program counter

    if (program_IO_data_register[0] >= max_program_length)
    {
      // END THE PROGRAM
      running = false;
    }

  } // Intrupter
} // Namespace

unsigned char test_program_0[10][4] = {
    // CMD, ARG,ARG,ARG
    0xFF, 0, 0, 0, // 0 | DUMP
    0x03, 0, 0, 0, // 1 | GOTO LINE 0
    0, 0, 0, 0,    // 2 |
    0, 0, 0, 0,    // 3 |
    0, 0, 0, 0,    // 4 |
    0, 0, 0, 0,    // 5 |
    0, 0, 0, 0,    // 6 |
    0, 0, 0, 0,    // 7 |
    0, 0, 0, 0,    // 8 |
    0, 0, 0, 0     // 9 |
};

unsigned char test_program_1[4][10] = {
    // CMD, ARG,ARG,ARG
    0x01, 0, 0x0F, 0x40, // 0 | if 0 < 16
    0x03, 0, 0, 0,       // 1 |   --if true execute this line, a false will skip this line (goto line 0)
    0, 0, 0, 0,          // 2 |
    0, 0, 0, 0,          // 3 |
    0, 0, 0, 0,          // 4 |
    0, 0, 0, 0,          // 5 |
    0, 0, 0, 0,          // 6 |
    0, 0, 0, 0,          // 7 |
    0, 0, 0, 0,          // 8 |
    0, 0, 0, 0           // 9 |
};

void setup()
{
  Serial.begin(115200);
  Serial.println("--ESP CMD EATER STARTED--");
  if (!cmd_interpreter::load_program(test_program_0))
  {
    Serial.println("LOAD DONE");
  }
  else
  {
    Serial.println("LOAD FAILED");
  }

  cmd_interpreter::running = true; // Set to true to allow execution
}

void loop()
{
  Serial.println("-------------------");
  cmd_interpreter::cmd_interpreter(); // Take a single step
  Serial.println("-------------------");
  delay(1000);
}