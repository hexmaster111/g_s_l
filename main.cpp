#include <Arduino.h>

using namespace std;
#define DEBUGGER_OUTPUT Serial
#define DEBUG_SCREEN Serial // What is used for the debugger
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
    const unsigned char LOPDN = 0x0A;

    const unsigned char HALT = 0xFE;
    const unsigned char DUMP = 0xFF;

    const char *NOP_pretty = "NOP ";
    const char *IF_pretty = "IF  ";
    const char *SRG_pretty = "SRG ";
    const char *GOTO_pretty = "GOTO";
    const char *INCR_pretty = "INCR";
    const char *DECR_pretty = "DECR";
    const char *ADD_pretty = "ADD ";
    const char *SUB_pretty = "SUB ";
    const char *MUT_pretty = "MUT ";
    const char *DIV_pretty = "DIV ";
    const char *LOPDN_pretty = "LPDN";
    const char *HALT_pretty = "HALT";
    const char *DUMP_pretty = "DUMP";

    const char valid_commands[] = {NOP, IF, SRG, GOTO, INCR, DECR, ADD, SUB, MUT, DIV, DUMP, LOPDN, HALT};
  }

  const int max_program_length = 16;
  const int max_instruction_args = 4;     // THIS INCLUDES THE INITAL COMMAND
  const int prog_data_register_size = 14; // 16 should be a good amount
  const int cmd_run_delay = 500;          // How offten the interpter should run
  const int max_cpu_usage_time = 1;       // max time that the interupter can keep the CPU buzy

  volatile unsigned char program_space[max_program_length][max_instruction_args]; // dataspace for the program to run
  volatile int program_IO_data_register[prog_data_register_size];                 // Store the programs running data

  namespace debug
  {

    void print_pretty_hex(int num)
    {
      DEBUG_SCREEN.print("0x");
      if (num < 16)
      {
        DEBUG_SCREEN.print("0");
      }
      DEBUG_SCREEN.print(num, HEX);
    }

    void print_command_name(int cmd)
    {
      switch (program_space[cmd][0]) // print the name
      {
      case cmds::NOP:
        DEBUG_SCREEN.print(cmds::NOP_pretty);
        break;

      case cmds::IF:
        DEBUG_SCREEN.print(cmds::IF_pretty);
        break;

      case cmds::SRG:
        DEBUG_SCREEN.print(cmds::SRG_pretty);
        break;

      case cmds::GOTO:
        DEBUG_SCREEN.print(cmds::GOTO_pretty);
        break;

      case cmds::INCR:
        DEBUG_SCREEN.print(cmds::INCR_pretty);
        break;

      case cmds::DECR:
        DEBUG_SCREEN.print(cmds::DECR_pretty);
        break;

      case cmds::ADD:
        DEBUG_SCREEN.print(cmds::ADD_pretty);
        break;

      case cmds::SUB:
        DEBUG_SCREEN.print(cmds::SUB_pretty);
        break;

      case cmds::MUT:
        DEBUG_SCREEN.print(cmds::MUT_pretty);
        break;

      case cmds::DIV:
        DEBUG_SCREEN.print(cmds::DIV_pretty);
        break;

      case cmds::LOPDN:
        DEBUG_SCREEN.print(cmds::LOPDN_pretty);
        break;

      case cmds::DUMP:
        DEBUG_SCREEN.print(cmds::DUMP_pretty);
        break;

      case cmds::HALT:
        DEBUG_SCREEN.print(cmds::HALT_pretty);
        break;

      default:
        DEBUG_SCREEN.print("----");
        break;
      }
    }

  }

  namespace setup // Where the IO setup inforation lives
  {
    enum io_direction // Remote addon to register direction
    {
      REG_INPUT = 0,
      REG_OUTPUT = 1
    };

    struct program_setup // holds pin and dir data for remote devices
    {
      bool pin_used_for_io = false;
      int addon_address = 0;
      int addon_register = 0;
      io_direction dir = REG_INPUT;
    };

    program_setup register_usage[prog_data_register_size]; // The setup that the io function will read
    // NOTE the index for this array is what register its for

    bool load_setup(program_setup setup[prog_data_register_size])
    {
      bool _error = false;
      // check the setup for errors

      for (size_t i = 0; i < prog_data_register_size; i++)
      { // for every setup item

        if (setup[i].pin_used_for_io && (setup[i].addon_address == 0))
        { // pin enabled for IO but no address deffined
          DEBUGGER_OUTPUT.print("SETUP ERROR - register: [");
          DEBUGGER_OUTPUT.print(i);
          DEBUGGER_OUTPUT.println("] enabled for I/O but no address was deffined.");
          _error = true;
        }
      }

      // load the setup into register_usage
      if (!_error)
      {
        for (size_t i = 0; i < prog_data_register_size; i++)
        { // load the user setup into the program array
          register_usage[i].addon_address = setup[i].addon_address;
          register_usage[i].addon_register = setup[i].addon_register;
          register_usage[i].pin_used_for_io = setup[i].pin_used_for_io;
          register_usage[i].dir = setup[i].dir;
        }
      }

      return _error;
    }
  }

  namespace status // Common varables for controlling script execution
  {
    //-----EXECUTION CONTROLL------//
    bool running = false;                       // IF false, the interpter will not run.
    bool dev_dump_before_every_command = false; // Enable to get a register dump every instruction
    bool loop_done = false;                     // Used by the LUPDN command, when set true, the scripts execution will pause untel the next loop time

    unsigned char minimum_error_level_to_print = 0xff; // what level of errors to print

    //------Debugging controll-----//
    bool visual_debug_mode = true; // enable to start the visual debugger
    bool visual_debug_enable_inputs = false;
    bool visual_debug_enable_outputs = false;

    //-----EXECUTION TIME CONTROLL------//
    unsigned long last_cmd_run = 0; // last time that the interupter was able to run

  }

  void update_output_devices()
  {
    for (size_t i = 0; i < prog_data_register_size; i++)
    {
      if (setup::register_usage[i].pin_used_for_io)
      { // if the register is used for io
        if (setup::register_usage[i].dir == setup::REG_OUTPUT)
        { // OUTPUT DEVICE
          //  update the output (if it needs it...)
          //@TODO intigrate this into the remote IO devices
          DEBUGGER_OUTPUT.print("[");
          DEBUGGER_OUTPUT.print(i);
          DEBUGGER_OUTPUT.print("] ");
          DEBUGGER_OUTPUT.print(program_IO_data_register[i]);
          DEBUGGER_OUTPUT.print("R  ->  D");
          DEBUGGER_OUTPUT.println(setup::register_usage[i].addon_address);
        }
      }
    }
  }

  void update_input_devices()
  {

    for (size_t i = 0; i < prog_data_register_size; i++)
    {
      if (setup::register_usage[i].pin_used_for_io)
      { // if the register is used for io
        if (setup::register_usage[i].dir == setup::REG_INPUT)
        { // INPUT DEVICE
          // Read the remote IO device
          // update the register to reflect the IO device
          DEBUGGER_OUTPUT.print("[");
          DEBUGGER_OUTPUT.print(i);
          DEBUGGER_OUTPUT.print("] ");
          DEBUGGER_OUTPUT.print(program_IO_data_register[i]);
          DEBUGGER_OUTPUT.print("R  <-  D");
          DEBUGGER_OUTPUT.println(setup::register_usage[i].addon_address);
        }
      }
    }
  }

  bool load_program(volatile unsigned char prog[max_program_length][max_instruction_args])
  { // check program, then load it into program space
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
        DEBUGGER_OUTPUT.print("Invalid command @ line: ");
        DEBUGGER_OUTPUT.println(i);
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
          program_space[i][x] = prog[i][x]; // Copy in every step of the program
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

  void error_handler(unsigned char err_level, const char *log_msg, int log_val)
  {
    // Thoughts on Error handling
    //  If there is an error, halt execution, and report the error to ~some~ error handler
    // Print out error messages ~prettly~
    if (err_level >= status::minimum_error_level_to_print)
    {
      DEBUGGER_OUTPUT.print("E_LV[ ");
      DEBUGGER_OUTPUT.print(err_level);
      DEBUGGER_OUTPUT.print("]ERR_MSG[ ");
      DEBUGGER_OUTPUT.print(log_msg);
      DEBUGGER_OUTPUT.print("] VAL: ");
      DEBUGGER_OUTPUT.print(log_val);
    }
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
    //    LOOP_DONE                        0x0A -- no args, when a program loop is done, this insturction must be called to allow the main cpu tasks continue
    //    HALT                             0xFE -- no args, Stopts the running program
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
      error_handler(0x01, "PROGRAM COMPLEATE -- NOTHING TO RUN", 0);
      return;
    }

    if (status::dev_dump_before_every_command)
    {
      DEBUGGER_OUTPUT.print(" PC -> ");
      DEBUGGER_OUTPUT.println(program_IO_data_register[0]);

      for (uint8_t i = 1; i < prog_data_register_size; i++)
      {
        DEBUGGER_OUTPUT.print("[");
        DEBUGGER_OUTPUT.print(i);
        DEBUGGER_OUTPUT.print("]");
        DEBUGGER_OUTPUT.print(" -> ");
        DEBUGGER_OUTPUT.println(program_IO_data_register[i]);
      }
    }

    // check what command the current line is

    switch (program_space[program_IO_data_register[0]][0])
    {
    case 0x00: // NOP
      NOP();
      break;

    case cmds::IF: // IF

      // DEBUGGER_OUTPUT.print("A: ");
      // DEBUGGER_OUTPUT.println(get_data_a());
      // DEBUGGER_OUTPUT.print("B: ");
      // DEBUGGER_OUTPUT.println(get_data_b());
      // DEBUGGER_OUTPUT.print("OPP: ");
      // DEBUGGER_OUTPUT.println(program_space[program_IO_data_register[0]][3]);

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
        DEBUGGER_OUTPUT.print("INVALID OPPERAND ON LINE: ");
        DEBUGGER_OUTPUT.println(program_IO_data_register[0]);
        break;
      }

      break; // FI

    case cmds::SRG: // Set register
      program_IO_data_register[program_space[program_IO_data_register[0]][1]] = program_space[program_IO_data_register[0]][2];
      break;

    case cmds::GOTO: // GOTO LOCATION
      program_IO_data_register[0] = program_space[program_IO_data_register[0]][1];
      // DEBUGGER_OUTPUT.print("GOTO LINE # ");
      // DEBUGGER_OUTPUT.println(program_IO_data_register[0]);
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

    case cmds::HALT:
      status::running = false;  // halt the program from running
      status::loop_done = true; // Allow the program to exit right away
      return;                   // We are done running, lets go do something else fun *takes ball* *goes home*
      break;

    case cmds::DUMP: // DUMP PROG_IO_REG
      if (status::visual_debug_mode)
      {
        break;
      }
      DEBUGGER_OUTPUT.println("------------------------");
      DEBUGGER_OUTPUT.print(" PC -> ");
      DEBUGGER_OUTPUT.println(program_IO_data_register[0]);

      for (uint8_t i = 1; i < prog_data_register_size; i++)
      {
        DEBUGGER_OUTPUT.print("[");
        DEBUGGER_OUTPUT.print(i);
        DEBUGGER_OUTPUT.print("]");
        DEBUGGER_OUTPUT.print(" -> ");
        DEBUGGER_OUTPUT.println(program_IO_data_register[i]);
      }
      DEBUGGER_OUTPUT.println("------------------------");
      break;

    case cmds::LOPDN: // LOOP DONE
      status::loop_done = true;
      break;

    default:
      error_handler(0xFF, "Instruction not found", program_space[program_IO_data_register[0]][0]);
      break;
    }

    program_IO_data_register[0]++; // Incriment the program counter

    if (program_IO_data_register[0] >= max_program_length)
    {
      // END THE PROGRAM
      status::running = false;
    }

  } // Intrupter

  void load_program(volatile unsigned char _program[cmd_interpreter::max_program_length][cmd_interpreter::max_instruction_args],
                    cmd_interpreter::setup::program_setup _register_setup[cmd_interpreter::prog_data_register_size])
  {

    // Load program and setup info
    DEBUGGER_OUTPUT.println("LOADING PROGRAM...");
    if (!cmd_interpreter::load_program(_program) && !cmd_interpreter::setup::load_setup(_register_setup))
    {
      DEBUGGER_OUTPUT.println("LOAD DONE, STARTING PROGRAM");

      cmd_interpreter::program_IO_data_register[0] = 0; // reset the program counter to zero

      cmd_interpreter::status::running = true; // Set to true to allow execution
    }
    else
    {
      DEBUGGER_OUTPUT.println("LOAD FAILED");
    }
  }

  void debugger_draw_fraim()
  {
    // dump the program to serial display, with PC having a pointer to the current instruction
    DEBUG_SCREEN.write(0xC); // clear the screen some
    DEBUG_SCREEN.println("--PROG INSTRUCTIONS---|P|-----PROG BRAKE DOWN-------");
    DEBUG_SCREEN.println("----------------------|C|INST|----------------------");
    for (size_t i = 0; i < cmd_interpreter::max_program_length; i++)
    { // BUILD THE DISPLAY LINE BY LINE
      // LINE NUMBER | INST NAME | DATA,DATA,DATA | PC | Command annotation

      if (i <= 9) // make up for the missing 0
      {
        DEBUG_SCREEN.print("0");
      }
      DEBUG_SCREEN.print(i); // print the line number

      DEBUG_SCREEN.print("|");
      debug::print_command_name(i);

      DEBUG_SCREEN.print(" "); // Space between cmd name and data

      for (size_t x = 1; x < cmd_interpreter::max_instruction_args; x++)
      { // prnt the data

        debug::print_pretty_hex(program_space[i][x]);
        DEBUG_SCREEN.print(",");
      }

      if (i == program_IO_data_register[0]) // Draw program counter pointer
      {
        DEBUG_SCREEN.print("<");
      }
      else
      {
        DEBUG_SCREEN.print(" "); // conver to move the pointer
      }

      DEBUG_SCREEN.print("|"); // Bar to seprate the automatic anotation

      // on the line time to anotate the program

      debug::print_command_name(i); // cmd
      DEBUG_SCREEN.print(" ");      // space

      if (cmd_interpreter::program_space[i][0] == cmds::IF)
      {                                                  // if cmd
        if (cmd_interpreter::program_space[i][1] > 0xF0) // arg 1
        {
          // ifcmd loading from a register
          DEBUG_SCREEN.print("[");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][1] - 0xF0); // register number
          DEBUG_SCREEN.print("] ");
        }
        else
        { // user is using a const value
          DEBUG_SCREEN.print(" ");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][1]); // ABS value
          DEBUG_SCREEN.print("  ");
        }

        // opperand
        switch (cmd_interpreter::program_space[i][3])
        {
        case 0x10:
          DEBUG_SCREEN.print("= ");
          break;

        case 0x20:
          DEBUG_SCREEN.print("!=");
          break;

        case 0x30:
          DEBUG_SCREEN.print("> ");
          break;

        case 0x31:
          DEBUG_SCREEN.print(">=");
          break;

        case 0x40:
          DEBUG_SCREEN.print("< ");
          break;

        case 0x41:
          DEBUG_SCREEN.print("<=");
          break;

        default:
          DEBUG_SCREEN.print("IN");
          break;
        }

        if (cmd_interpreter::program_space[i][2] > 0xF0) // arg 2
        {
          // ifcmd loading from a register
          DEBUG_SCREEN.print("[");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][2] - 0xF0); // register number
          DEBUG_SCREEN.print("] ");
        }
        else
        { // user is using a const value
          DEBUG_SCREEN.print(" ");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][2]); // ABS value
          DEBUG_SCREEN.print("  ");
        }
      }
      else if (cmd_interpreter::program_space[i][0] == cmds::GOTO)
      { // if cmd is goto, only print the raw number
        DEBUG_SCREEN.print("->");
        DEBUG_SCREEN.print(cmd_interpreter::program_space[i][1]);
        if (cmd_interpreter::program_space[i][1] <= 9)
        { // Add a space for the missing digit
          DEBUG_SCREEN.print(" ");
        }
      }
      else if (cmd_interpreter::program_space[i][0] == cmds::INCR || cmd_interpreter::program_space[i][0] == cmds::DECR)
      {
        DEBUG_SCREEN.print("[");
        DEBUG_SCREEN.print(cmd_interpreter::program_space[i][1]);
        DEBUG_SCREEN.print("]");
      }
      else if (cmd_interpreter::program_space[i][0] == cmds::ADD ||
               cmd_interpreter::program_space[i][0] == cmds::SUB ||
               cmd_interpreter::program_space[i][0] == cmds::MUT ||
               cmd_interpreter::program_space[i][0] == cmds::DIV)
      {
        if (cmd_interpreter::program_space[i][1] > 0xF0) // arg 1
        {
          // ifcmd loading from a register
          DEBUG_SCREEN.print("[");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][1] - 0xF0); // register number
          DEBUG_SCREEN.print("] ");
        }
        else
        { // user is using a const value
          DEBUG_SCREEN.print(" ");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][1]); // ABS value
          DEBUG_SCREEN.print("  ");
        }

        switch (cmd_interpreter::program_space[i][0])
        {
        case cmds::ADD:
          DEBUG_SCREEN.print("+");
          break;
        case cmds::SUB:
          DEBUG_SCREEN.print("-");
          break;
        case cmds::MUT:
          DEBUG_SCREEN.print("*");
          break;
        case cmds::DIV:
          DEBUG_SCREEN.print("/");
          break;

        default:
          DEBUG_SCREEN.print("?");
          break;
        }

        // Opperation

        if (cmd_interpreter::program_space[i][2] > 0xF0) // arg 1
        {
          // ifcmd loading from a register
          DEBUG_SCREEN.print("[");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][2] - 0xF0); // register number
          DEBUG_SCREEN.print("] ");
        }
        else
        { // user is using a const value
          DEBUG_SCREEN.print(" ");
          DEBUG_SCREEN.print(cmd_interpreter::program_space[i][2]); // ABS value
          DEBUG_SCREEN.print(" ");
        }

        // dest
        DEBUG_SCREEN.print("->");

        DEBUG_SCREEN.print("[");
        DEBUG_SCREEN.print(cmd_interpreter::program_space[i][3]); // ABS value
        DEBUG_SCREEN.print("]");
      }
      else if (cmd_interpreter::program_space[i][0] == cmds::SRG)
      {
        DEBUG_SCREEN.print("[");
        DEBUG_SCREEN.print(cmd_interpreter::program_space[i][1]);
        DEBUG_SCREEN.print("]");
        DEBUG_SCREEN.print("<-");
        DEBUG_SCREEN.print(" ");
        DEBUG_SCREEN.print(cmd_interpreter::program_space[i][2]);
        DEBUG_SCREEN.print(" ");
      }

      DEBUG_SCREEN.println(""); // NEW LINE
    }

    // Print the Current prog io register
    DEBUG_SCREEN.println("-------------------IO REGISTERS---------------------");
    for (size_t i = 0; i < prog_data_register_size; i++)
    { // For every element in the data register
      DEBUG_SCREEN.print(" [");
      if (i <= 9)
      {
        DEBUG_SCREEN.print("0");
      }
      if (i <= 99)
      {
        DEBUG_SCREEN.print("0");
      }

      DEBUG_SCREEN.print(i);
      DEBUG_SCREEN.print("]=");

      if (program_IO_data_register[i] <= 9)
      {
        DEBUG_SCREEN.print("0");
      }
      if (program_IO_data_register[i] <= 99)
      {
        DEBUG_SCREEN.print("0");
      }
      DEBUG_SCREEN.print(program_IO_data_register[i]); // Print value
      if (i == 4 || i == 9)
      {
        DEBUG_SCREEN.println("");
      }
    }
    DEBUG_SCREEN.println(""); // New line after program counter
    DEBUG_SCREEN.println("----------------------IO STATUS---------------------");

    DEBUG_SCREEN.print("INPUTS: ");
    if (status::visual_debug_enable_inputs)
    {
      DEBUG_SCREEN.print(" ENABLED");
    }
    else
    {
      DEBUG_SCREEN.print("DISABLED");
    }

    DEBUG_SCREEN.print("                  ");

    DEBUG_SCREEN.print("OUTPUTS: ");
    if (status::visual_debug_enable_outputs)
    {
      DEBUG_SCREEN.print(" ENABLED");
    }
    else
    {
      DEBUG_SCREEN.print("DISABLED");
    }
    DEBUG_SCREEN.println("");
    DEBUG_SCREEN.println("-------------------IO DEVICE MAP--------------------");

    DEBUG_SCREEN.print("ADDON: ");

    for (size_t i = 0; i < prog_data_register_size; i++)
    {
      if (setup::register_usage[i].pin_used_for_io)
      { // show addon address
        debug::print_pretty_hex(setup::register_usage[i].addon_address);
        DEBUG_SCREEN.print("[");
        DEBUG_SCREEN.print(setup::register_usage[i].addon_register);
        if (setup::register_usage[i].addon_register <= 9)
        {
          DEBUG_SCREEN.print(" ");
        }
        DEBUG_SCREEN.print("]  ");
      }
    }
    DEBUG_SCREEN.println("");
    // Where the pointers will live
    DEBUG_SCREEN.print("         ");

    for (size_t i = 0; i < prog_data_register_size; i++)
    {
      if (setup::register_usage[i].pin_used_for_io)
      { // Arrow to indicate data flow
        if (setup::register_usage[i].dir == setup::REG_INPUT)
        {
          DEBUG_SCREEN.print("v");
        }
        else
        {
          DEBUG_SCREEN.print("^");
        }
        DEBUG_SCREEN.print("         ");
      }
    }

    DEBUG_SCREEN.println("");

    DEBUG_SCREEN.print("LOCAL: ");

    for (size_t i = 0; i < prog_data_register_size; i++)
    {
      if (setup::register_usage[i].pin_used_for_io)
      { // Show loacal register
        DEBUG_SCREEN.print(" [");
        DEBUG_SCREEN.print(i);
        if (i <= 9)
        {
          DEBUG_SCREEN.print(" ");
        }
        DEBUG_SCREEN.print("]     ");
      }
    }

    DEBUG_SCREEN.println("");
    DEBUG_SCREEN.println("-------------------STATUS REGISTERS-----------------");

    DEBUG_SCREEN.print("|  ");
    DEBUG_SCREEN.print("RUNNING: ");
    if (status::running)
    {
      DEBUG_SCREEN.print("T ");
    }
    else
    {
      DEBUG_SCREEN.print("F ");
    }

    DEBUG_SCREEN.println("   |");
    DEBUG_SCREEN.println("----------------------------------------------------");

    if (status::visual_debug_enable_inputs)
    {
      update_input_devices(); // Read input devices
    }

    // cmd_interpreter(); // take a step

    if (status::visual_debug_enable_outputs)
    {
      update_output_devices();
    }
  }

  void run() // Execute the script (call as often as you like)
  {
    if (!status::visual_debug_mode)
    {
      if ((millis() > status::last_cmd_run + cmd_run_delay) && cmd_interpreter::status::running)
      {
        update_input_devices(); // read from remote IO devices

        status::loop_done = false;

        unsigned long start_time = millis();

        while (!status::loop_done)
        {
          cmd_interpreter(); // Take a single step

          if (millis() > start_time + max_cpu_usage_time)
          { // If the program seems hung up, stop running and give the main program some CPU time
            status::loop_done = true;
            error_handler(0x0F, "VM EXECUTION TIME EXCETED, MAKE SURE YOUR PROGRAM CALLES LOPDN OFFTEN ENOUGH", 0);
          }
        }
        update_output_devices();         // Write to the remote IO devices
        status::last_cmd_run = millis(); // Wait some time before going more(perhaps we should try and keep this executing at a known frequency)
      }
    }
    else
    {
      error_handler(0xFE, "IN DEBUGGING MODE", 0);
    }
  }
} // Namespace

cmd_interpreter::setup::program_setup test_program_0_setup[cmd_interpreter::prog_data_register_size];

void setup_program_0()
{
  test_program_0_setup[1].pin_used_for_io = true;
  test_program_0_setup[1].addon_address = 0x02;
  test_program_0_setup[1].addon_register = 1;
  test_program_0_setup[1].dir = cmd_interpreter::setup::REG_OUTPUT;

  test_program_0_setup[2].pin_used_for_io = true;
  test_program_0_setup[2].addon_address = 0x02;
  test_program_0_setup[2].addon_register = 99;
  test_program_0_setup[2].dir = cmd_interpreter::setup::REG_INPUT;
}

volatile unsigned char test_program_0[cmd_interpreter::max_program_length][cmd_interpreter::max_instruction_args] = {
    // CMD,ARG,  ARG,  ARG
    0x02, 0x02, 0xFF, 0,    // 0 |SET Reg2 ->256
    0x09, 0xF2, 0x2, 0x02,  // 1 |reg 2 / 2 -> reg2
    0x0A, 0, 0, 0,          // 2 |DUMP
    0x01, 0xF2, 2, 0x40,    // 3 |IF reg2 < 2
    0xFE, 0, 0, 0,          // 4 | goto 0 //reset the vals to keep deviding
    0x03, 1, 0, 0,          // 5 |goto 1  //skip setting the value
    0xFF, 0, 0, 0,          // 6 |-------------Debugging tests below
    0x01, 0x02, 0xF2, 0x10, // if
    0x04, 0x02, 0, 0,       // incr
    0x05, 0x02, 0, 0,       //
    0x0A, 0, 0, 0           //
};

volatile unsigned char test_program_1[cmd_interpreter::max_program_length][cmd_interpreter::max_instruction_args] = {
    // CMD,ARG,  ARG,  ARG
    0x04, 0x03, 0x00, 0x00, // 00 | [3]++
    0x03, 0x00, 0x00, 0x00, // 01 | GOTO 0
    0x00, 0x00, 0x00, 0x00, // 02 |
    0x00, 0x00, 0x00, 0x00, // 03 |
    0x00, 0x00, 0x00, 0x00, // 04 |
};

void setup()
{
  DEBUGGER_OUTPUT.begin(115200);
  DEBUG_SCREEN.begin(115200);
  DEBUGGER_OUTPUT.println("--G.S.L. STARTED--");
  // Setup the pins
  setup_program_0();                                                   // There is likely a better way to do this, but irl this will be on the fly loading, sooooo..... probbably good enough
  cmd_interpreter::load_program(test_program_0, test_program_0_setup); // Load program
}

bool one_shot = false;

void loop()
{
  cmd_interpreter::run();                 // Call to run the interpter normaly with debug mode disabled
  cmd_interpreter::debugger_draw_fraim(); // Call to draw a fraim to the display
  cmd_interpreter::cmd_interpreter();     // Call to take a debugging step
  delay(500);

  if (millis() > 15000 && !one_shot)
  {
    one_shot = true;
    Serial.println("!!!LOADING NEW PROGRAM!!!!");
    delay(5000);
    cmd_interpreter::load_program(test_program_1, test_program_0_setup); // Load program
    delay(5000);
  }
}