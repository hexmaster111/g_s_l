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
    const unsigned char LOPDN = 0x0A;
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
    const char *DUMP_pretty = "DUMP";

    const char valid_commands[] = {NOP, IF, SRG, GOTO, INCR, DECR, ADD, SUB, MUT, DIV, DUMP, LOPDN};
  }

  const int max_program_length = 11;
  const int max_instruction_args = 4;    // THIS INCLUDES THE INITAL COMMAND
  const int prog_data_register_size = 4; // 16 should be a good amount
  const int cmd_run_delay = 500;         // How offten the interpter should run
  const int max_cpu_usage_time = 1;      // max time that the interupter can keep the CPU buzy

  volatile unsigned char program_space[max_program_length][max_instruction_args]; // dataspace for the program to run
  volatile int program_IO_data_register[prog_data_register_size];                 // Store the programs running data

  namespace debug
  {

    void print_pretty_hex(int num)
    {
      Serial.print("0x");
      if (num < 16)
      {
        Serial.print("0");
      }
      Serial.print(num, HEX);
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
          Serial.print("[");
          Serial.print(i);
          Serial.print("] ");
          Serial.print(program_IO_data_register[i]);
          Serial.print("R  ->  D");
          Serial.println(setup::register_usage[i].addon_address);
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
          Serial.print("[");
          Serial.print(i);
          Serial.print("] ");
          Serial.print(program_IO_data_register[i]);
          Serial.print("R  <-  D");
          Serial.println(setup::register_usage[i].addon_address);
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

  void error_handler(unsigned char err_level, const char *log_msg, int log_val)
  {
    // Thoughts on Error handling
    //  If there is an error, halt execution, and report the error to ~some~ error handler
    // Print out error messages ~prettly~
    if (err_level >= status::minimum_error_level_to_print)
    {
      Serial.print("E_LV[ ");
      Serial.print(err_level);
      Serial.print("]ERR_MSG[ ");
      Serial.print(log_msg);
      Serial.print("] VAL: ");
      Serial.print(log_val);
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
      if (status::visual_debug_mode)
      {
        break;
      }
      Serial.println("------------------------");
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
      Serial.println("------------------------");
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
    Serial.println("LOADING PROGRAM...");
    if (!cmd_interpreter::load_program(_program) && !cmd_interpreter::setup::load_setup(_register_setup))
    {
      Serial.println("LOAD DONE, STARTING PROGRAM");

      cmd_interpreter::status::running = true; // Set to true to allow execution
    }
    else
    {
      Serial.println("LOAD FAILED");
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
            Serial.println("BRAKE!! cpu time exceaded...\n make sure you are using the LOPDN command\n if you are sure the prog is running correctly, try upping max_cpu_usage_time");
          }
        }
        update_output_devices();         // Write to the remote IO devices
        status::last_cmd_run = millis(); // Wait some time before going more(perhaps we should try and keep this executing at a known frequency)
      }
    }
    else
    { // Welcome to the deugging mode!

      // dump the program to serial display, with PC having a pointer to the current instruction
      Serial.write(0xC); // clear the screen some
      Serial.println("---------------------");
      for (size_t i = 0; i < cmd_interpreter::max_program_length; i++)
      { // for every instruction
        // Print the name
        switch (program_space[i][0]) // print the name
        {
        case cmds::NOP:
          Serial.print(cmds::NOP_pretty);
          break;

        case cmds::IF:
          Serial.print(cmds::IF_pretty);
          break;

        case cmds::SRG:
          Serial.print(cmds::SRG_pretty);
          break;

        case cmds::GOTO:
          Serial.print(cmds::GOTO_pretty);
          break;

        case cmds::INCR:
          Serial.print(cmds::INCR_pretty);
          break;

        case cmds::DECR:
          Serial.print(cmds::DECR_pretty);
          break;

        case cmds::ADD:
          Serial.print(cmds::ADD_pretty);
          break;

        case cmds::SUB:
          Serial.print(cmds::SUB_pretty);
          break;

        case cmds::MUT:
          Serial.print(cmds::MUT_pretty);
          break;

        case cmds::DIV:
          Serial.print(cmds::DIV_pretty);
          break;

        case cmds::LOPDN:
          Serial.print(cmds::LOPDN_pretty);
          break;

        case cmds::DUMP:
          Serial.print(cmds::DUMP_pretty);
          break;

        default:
          Serial.print("----");
          break;
        }

        Serial.print(" "); // Space between cmd name and data

        for (size_t x = 1; x < cmd_interpreter::max_instruction_args; x++)
        { // prnt the data

          debug::print_pretty_hex(program_space[i][x]);
          Serial.print(",");
        }

        if (i == program_IO_data_register[0]) // Draw program counter pointer
        {
          Serial.print(" <---");
        }

        Serial.println(""); // NEW LINE
      }

      // Print the Current prog io register
      Serial.println("------------");
      Serial.println("IO REGISTERS");
      for (size_t i = 0; i < prog_data_register_size; i++)
      { // For every element in the data register
        Serial.print("  [");
        Serial.print(i);
        Serial.print("]=");
        Serial.print(program_IO_data_register[i]); // Print value
      }
      Serial.println(""); // New line after program counter

      if (status::visual_debug_enable_inputs)
      {
        update_input_devices(); // Read input devices
      }

      cmd_interpreter(); // take a step

      if (status::visual_debug_enable_outputs)
      {
        update_output_devices();
      }
      delay(1000); // debugging delay
    }
  }
} // Namespace

cmd_interpreter::setup::program_setup test_program_0_setup[cmd_interpreter::prog_data_register_size];

void setup_program_0()
{
  test_program_0_setup[1].pin_used_for_io = true;
  test_program_0_setup[1].addon_address = 0x02;
  test_program_0_setup[1].addon_register = 0;
  test_program_0_setup[1].dir = cmd_interpreter::setup::REG_OUTPUT;

  test_program_0_setup[2].pin_used_for_io = true;
  test_program_0_setup[2].addon_address = 0x01;
  test_program_0_setup[2].addon_register = 0;
  test_program_0_setup[2].dir = cmd_interpreter::setup::REG_INPUT;
}

volatile unsigned char test_program_0[cmd_interpreter::max_program_length][cmd_interpreter::max_instruction_args] = {
    // CMD,ARG,  ARG,  ARG
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

void setup()
{
  Serial.begin(115200);
  Serial.println("--G.S.L. STARTED--");
  // Setup the pins
  setup_program_0();                                                   // There is likely a better way to do this, but irl this will be on the fly loading, sooooo..... probbably good enough
  cmd_interpreter::load_program(test_program_0, test_program_0_setup); // Load program
}

void loop()
{
  cmd_interpreter::run();
}
