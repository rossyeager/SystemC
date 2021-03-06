/*

You have to add a tlm_initiator_socket to the initiator, a tlm_target_socket to the memory,
and bind the two sockets together at the top level.

You also have to provide dummy implementations of the remaining forward and backward
interface methods. (They already exist; you only have to uncomment them.)

*/

#include "systemc"
using namespace sc_core;
using namespace sc_dt;
using namespace std;

#include "tlm.h"

// Initiator module generating generic payload transactions

struct Initiator: sc_module, tlm::tlm_bw_transport_if<>
{
  tlm::tlm_initiator_socket<> init_socket;

  SC_CTOR(Initiator)
  {
    SC_THREAD(thread_process);
    init_socket.bind(*this);
  }

  void thread_process()
  {
    // TLM-2 generic payload transaction, reused across calls to b_transport
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    //    init_socket->b_transport(trans,delay);
     
    // Generate a random sequence of reads and writes
    for (int i = 0; i < 64; i += 4)
    {
      tlm::tlm_command cmd = static_cast<tlm::tlm_command>(rand() % 2);
      if (cmd == tlm::TLM_WRITE_COMMAND) data = i;

      // Initialize 8 out of the 10 attributes, byte_enable_length and extensions being unused
      trans.set_command( cmd );
      trans.set_address( i );
      trans.set_data_ptr( reinterpret_cast<unsigned char*>(&data) );
      trans.set_data_length( 4 );
      trans.set_streaming_width( 4 ); // = data_length to indicate no streaming
      trans.set_byte_enable_ptr( 0 ); // 0 indicates unused
      trans.set_dmi_allowed( false ); // Mandatory initial value
      trans.set_response_status( tlm::TLM_INCOMPLETE_RESPONSE ); // Mandatory initial value

      init_socket->b_transport( trans, delay );  // Blocking transport call

      // Initiator obliged to check response status and delay
      if ( trans.is_response_error() )
        SC_REPORT_ERROR("TLM-2", trans.get_response_string().c_str());

      cout << "Completed " << (cmd ? "write" : "read") << ", addr = " << hex << i
           << ", data = " << hex << data << ", time " << sc_time_stamp()
           << " delay = " << delay << endl;
    }
  }
 
 
  // TLM-2 backward non-blocking transport method
  virtual tlm::tlm_sync_enum nb_transport_bw( tlm::tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase, sc_time& delay )
  {
    // Dummy method
    return tlm::TLM_ACCEPTED;
  }

  // TLM-2 backward DMI method
  virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range,
                                         sc_dt::uint64 end_range)
  {
    // Dummy method
  }
 

  // Internal data buffer used by initiator with generic payload
  int data;
};


// Target module representing a simple memory

struct Memory: sc_module, tlm::tlm_fw_transport_if<>
{
  enum { SIZE = 256 };
  int mem[SIZE];

  tlm::tlm_target_socket<> targ_socket;

  SC_CTOR(Memory)
  {
    targ_socket.bind(*this);
    // Initialize memory with random data
    for (int i = 0; i < SIZE; i++)
      mem[i] = rand() % 256;
  }

  // TLM-2 blocking transport method
  virtual void b_transport( tlm::tlm_generic_payload& trans, sc_time& delay )
  {
    tlm::tlm_command cmd = trans.get_command();
    sc_dt::uint64    adr = trans.get_address();
    unsigned char*   ptr = trans.get_data_ptr();
    unsigned int     len = trans.get_data_length();
    unsigned char*   byt = trans.get_byte_enable_ptr();
    unsigned int     wid = trans.get_streaming_width();

    // Obliged to check address range and check for unsupported features,
    //   i.e. byte enables, streaming, and bursts
    // Can ignore DMI hint and extensions
    // Using the SystemC report handler is an acceptable way of signalling an error

    if (adr >= sc_dt::uint64(SIZE) * 4 || adr % 4 || byt != 0 || len > 4 || wid < len) 
    {
      //SC_REPORT_ERROR("TLM-2", "Target does not support given generic payload transaction");
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
    else 
    {
         // Obliged to implement read and write commands
   	if ( cmd == tlm::TLM_READ_COMMAND )
    	      memcpy(ptr, &mem[adr/4], len);
	else if ( cmd == tlm::TLM_WRITE_COMMAND )
	      memcpy(&mem[adr/4], ptr, len);

	    // Obliged to set response status to indicate successful completion
    	trans.set_response_status( tlm::TLM_OK_RESPONSE );
    }
  }

  
  // TLM-2 forward non-blocking transport method
  virtual tlm::tlm_sync_enum nb_transport_fw( tlm::tlm_generic_payload& trans,
                                              tlm::tlm_phase& phase, sc_time& delay )
  {
    // Dummy method (not TLM-2.0 compliant)
    return tlm::TLM_ACCEPTED;
  }

  // TLM-2 forward DMI method
  virtual bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                                  tlm::tlm_dmi& dmi_data)
  {
    // Dummy method
    return false;
  }

  // TLM-2 debug transport method
  virtual unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
  {
    // Dummy method
    return 0;
  }
  
};


SC_MODULE(Top)
{
  Initiator *initiator;
  Memory    *memory;

  SC_CTOR(Top)
  {
    // Instantiate components
    initiator = new Initiator("initiator");
    memory    = new Memory   ("memory");

    initiator->init_socket.bind(memory->targ_socket);
  }

};


int sc_main(int argc, char* argv[])
{
  Top top("top");
  sc_start();
  return 0;
}
