#include <Arduino.h>

using namespace std;

namespace cmd_interpreter
{

  namespace cmds
  {

    const unsigned char NOP = 0x00;
    const unsigned char IF = 0x01;
    const unsigned char SRG = 0x02;
    const unsigned char GOTO = 0x03;
    const unsigned char INCR = 0x04;
    const unsigned char DECR = 0x05;
    const unsigned char ADD = 0x06;
    const unsigned char SUB = 0x07;
    const unsigned char MUT = 0x08;
    const unsigned char DIV = 0x09;
    const unsigned char DUMP = 0xFF;

    const char valid_commands[] = {NOP, IF, SRG, GOTO, INCR, DECR, ADD, SUB, MUT, DIV, DUMP};
  }

  const int max_program_length = 10;
  const int max_instruction_args = 4; // THIS INCLUDES THE INITAL COMMAND
  const int prog_data_register_size = 10;

  volatile unsigned char program_space[max_program_length][max_instruction_args]; // dataspace for the program to run
  volatile int program_IO_data_register[prog_data_register_size];                 // Store the programs running data

  namespace setup
  {
    struct program_setup
    {
      bool pin_used_for_io = false;
      int addon_address = 0;
      int addon_register = 0;
    };

    program_setup register_usage[prog_data_register_size]; // The setup that the io function will read

    bool load_setup(program_setup setup[prog_data_register_size])
    {
      bool _error = false;
      // check the setup for errors

      for (size_t i = 0; i < prog_data_register_size; i++)
      { // for every setup item

        if (setup[i].pin_used_for_io && (setup[i].addon_address == 0))
        { // pin enabled for IO but no address deffined
          Serial.print("SETUP ERROR - register: [");
          Serial.print(i);
          Serial.println("] enabled for I/O but no address was deffined.");
          _error = true;
        }
      }

      // load the setup into register_usage
      if (!_error)
      {
        for (size_t i = 0; i < prog_data_register_size; i++)
        { //load the user setup into the program array
          register_usage[i].addon_address = setup[i].addon_address;
          register_usage[i].addon_register = setup[i].addon_register;
          register_usage[i].pin_used_for_io = setup[i].pin_used_for_io;
        }
      }

      return _error;
    }
  }

  namespace status
  {
    bool running = false;
    bool dev_dump_before_every_command = false;

  }

  // Thoughts on IO
  //   In usage, a function will refrence the IO registers peroticly to set pre-deffined output devices based on the value of the registers,
  // This will need to be fleshed out more later

  // check program, then load it into program space
  bool load_program(volatile unsigned char prog[max_program_length][max_instruction_args])
  {
    bool load_error = false;

    // check that all commands are valid
    for (uint8_t i = 0; i < max_program_length; i++)
    { // for every instuction in the program
      bool _found = false;
      for (unsigned char x = 0; x <= sizeof(cmds::valid_commands) / sizeof(cmds::valid_commands[1]); x++)
      { // go through the all valid instructions
        if (prog[i][0] == cmds::valid_commands[x])
        { // if the cmd is valid
          _found = true;
        }
      }
      if (!_found)
      {
        Serial.print("Invalid command @ line: ");
        Serial.println(i);
        load_error = true;
      }
    }

    if (!load_error)
    { // if no load error

      // Load in the program
      for (uint8_t i = 0; i < max_program_length; i++)
      {
        for (uint8_t x = 0; x < max_instruction_args; x++)
        {
          program_space[x][i] = prog[x][i]; // Copy in every step of the program
        }
      }
    }

    return load_error;
  }

  unsigned char get_data_a()
  {
    unsigned char _ret;

    if (program_space[program_IO_data_register[0]][1] >= 0xF0)
    { // IF data is larger then F0 we will pull from a register

      _ret = program_space[program_IO_data_register[0]][1] - 0xF0; // register to read
      _ret = program_IO_data_register[_ret];
    }
    else
    { // We are reading in a raw value
      _ret = program_space[program_IO_data_register[0]][1];
    }

    return _ret;
  }

  unsigned char get_data_b()
  {
    unsigned char _ret;
    if (program_space[program_IO_data_register[0]][2] >= 0xF0)
    { // IF data is larger then F0 we will pull from a register

      _ret = program_space[program_IO_data_register[0]][2] - 0xF0; // register to read
      _ret = program_IO_data_register[_ret];
    }
    else
    { // Reading in a raw value
      _ret = program_space[program_IO_data_register[0]][2];
    }

    return _ret;
  }

  void error_handler(uint8_t err_level, const char *log_msg)
  {
    // Thoughts on Error handling
    //  If there is an error, halt execution, and report the error to ~some~ error handler
    // Print out error messages ~prettly~
  }

  void cmd_interpreter()
  {
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

        !! Note thease two are the same program, one is just using the pre-deffined and the other is just working with the vals directly!!

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

    if (!status::running)
    {
      Serial.println("PROGRAM COMPLEATE -- NOTHING TO RUN");
      return;
    }

    if (status::dev_dump_before_every_command)
    {
      Serial.print(" PC -> ");
      Serial.println(program_IO_data_register[0]);

      for (uint8_t i = 1; i < prog_data_register_size; i++)
      {
        Serial.print("[");
        Serial.print(i);
        Serial.print("]");
        Serial.print(" -> ");
        Serial.println(program_IO_data_register[i]);
      }
    }

    // check what command the current line is

    switch (program_space[program_IO_data_register[0]][0])
    {
    case 0x00: // NOP
      NOP();
      break;

    case cmds::IF: // IF

      // Serial.print("A: ");
      // Serial.println(get_data_a());
      // Serial.print("B: ");
      // Serial.println(get_data_b());
      // Serial.print("OPP: ");
      // Serial.println(program_space[program_IO_data_register[0]][3]);

      switch (program_space[program_IO_data_register[0]][3]) // Opperand
      {
      case 0x10: // =

        if (get_data_a() == get_data_b())
        {
          // Do nothing, let PC inc and read next line
        }
        else
        {
          // Skip the next line(inc pc once here and once at end)
          program_IO_data_register[0]++;
        }

        break;

      case 0x20: // !=

        if (get_data_a() != get_data_b())
        {
          // Do nothing
        }
        else
        {
          // Skip next line
          program_IO_data_register[0]++;
        }

        break;

      case 0x30: // >
        if (get_data_a() > get_data_b())
        {
          // Do nothing
        }
        else
        {
          // Skip next line
          program_IO_data_register[0]++;
        }
        break;

      case 0x40: // <

        if (get_data_a() < get_data_b())
        {
          // Do nothing
        }
        else
        {
          // Skip next line
          program_IO_data_register[0]++;
        }
        break;

      case 0x31: // >=
        if (get_data_a() <= get_data_b())
        {
          // Do nothing
        }
        else
        {
          // Skip next line
          program_IO_data_register[0]++;
        }
        break;

      case 0x41: // <=
        if (get_data_a() <= get_data_b())
        {
          // Do nothing
        }
        else
        {
          // Skip next line
          program_IO_data_register[0]++;
        }
        break;

      default:
        status::running = false;
        Serial.print("INVALID OPPERAND ON LINE: ");
        Serial.println(program_IO_data_register[0]);
        break;
      }

      break; // FI

    case cmds::SRG: // Set register
      program_IO_data_register[program_space[program_IO_data_register[0]][1]] = program_space[program_IO_data_register[0]][2];
      break;

    case cmds::GOTO: // GOTO LOCATION
      program_IO_data_register[0] = program_space[program_IO_data_register[0]][1];
      // Serial.print("GOTO LINE # ");
      // Serial.println(program_IO_data_register[0]);
      return; // WE DONT WANT TO INC THE PC
      break;

    case cmds::INCR: // inc
      program_IO_data_register[program_space[program_IO_data_register[0]][1]]++;
      break;

    case cmds::DECR: // dec
      program_IO_data_register[program_space[program_IO_data_register[0]][1]]--;
      break;

    case cmds::ADD: // add
      // data a, data b, dest
      program_IO_data_register[program_space[program_IO_data_register[0]][3]] = get_data_a() + get_data_b();
      break;

    case cmds::SUB: // sub
                    // data a, data b, dest
      program_IO_data_register[program_space[program_IO_data_register[0]][3]] = get_data_b() - get_data_a();
      break;

    case cmds::MUT: // mut
                    // data a, data b, dest
      program_IO_data_register[program_space[program_IO_data_register[0]][3]] = get_data_a() * get_data_b();
      break;

    case cmds::DIV: // div
                    // data a, data b, dest
      program_IO_data_register[program_space[program_IO_data_register[0]][3]] = get_data_a() / get_data_b();
      break;

    case cmds::DUMP: // DUMP PROG_IO_REG
      Serial.print(" PC -> ");
      Serial.println(program_IO_data_register[0]);

      for (uint8_t i = 1; i < prog_data_register_size; i++)
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
      status::running = false;
    }

  } // Intrupter
} // Namespace

cmd_interpreter::setup::program_setup test_program_0_setup[cmd_interpreter::prog_data_register_size];

void setup_program_0()
{
  test_program_0_setup[9].pin_used_for_io = true;
  test_program_0_setup[9].addon_address = 0x15;
  test_program_0_setup[9].addon_register = 0;
}

volatile unsigned char test_program_0[10][4] = {
    // CMD,ARG,  ARG,  ARG
    0x02, 0x02, 0xFF, 0x00, // 0 |SET Reg2 ->256
    0x09, 0xF2, 0x02, 0x02, // 1 |reg 2 / 2 -> reg2
    0xFF, 0x00, 0x00, 0x00, // 2 |DUMP
    0x01, 0xF2, 0x02, 0x40, // 3 |IF reg2 < 2
    0x03, 0x00, 0x00, 0x00, // 4 | goto 0 //reset the vals to keep deviding
    0x03, 0x01, 0x00, 0x00, // 5 |goto 1  //skip setting the value
    0x00, 0x00, 0x00, 0x00, // 6 |
    0x00, 0x00, 0x00, 0x00, // 7 |
    0x00, 0x00, 0x00, 0x00, // 8 |
    0x00, 0x00, 0x00, 0x00  // 9 |
};

void setup()
{
  Serial.begin(115200);
  Serial.println("--ESP CMD EATER STARTED--");
  // Setup the pins
  setup_program_0();

  // Load program and setup info
  if (!cmd_interpreter::load_program(test_program_0) && !cmd_interpreter::setup::load_setup(test_program_0_setup))
  {
    Serial.println("LOAD DONE, STARTING PROGRAM");

    cmd_interpreter::status::running = true; // Set to true to allow execution
  }
  else
  {
    Serial.println("LOAD FAILED");
  }
}

unsigned long last_cmd_run = 0;
const int cmd_run_delay = 500;

void loop()
{
  if ((millis() > last_cmd_run + cmd_run_delay) && cmd_interpreter::status::running)
  {
    // run the commands
    cmd_interpreter::cmd_interpreter(); // Take a single step

    last_cmd_run = millis();
  }
}
