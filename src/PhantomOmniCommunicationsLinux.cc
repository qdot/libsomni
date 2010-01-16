#include "libsomni/PhantomOmniCommunications.hh"

namespace libsomni
{
	static enum raw1394_iso_disposition 
	my_iso_packet_handler(raw1394handle_t handle, unsigned char *data, 
						  unsigned int length, unsigned char channel,
						  unsigned char tag, unsigned char sy, unsigned int cycle, 
						  unsigned int dropped)
	{
        int ret;
        static unsigned int counter = 0;

		for(int i = 0; i < 40; ++i)
		{
			printf("%02x ", data[i]);
		}
		printf("\n");

//		printf("%02x %02x %d\n", data[14], data[15], (short)(data[14] | data[15] << 8));
        return RAW1394_ISO_OK;
	}

	inline void endian_swap(unsigned short& x)
	{
		x = (x>>8) | 
			(x<<8);
	}

	inline void endian_swap(unsigned int& x)
	{
		x = (x>>24) | 
			((x<<8) & 0x00FF0000) |
			((x>>8) & 0x0000FF00) |
			(x<<24);
	}

	void pack_serial_number(uint32_t serial_num, const char* serial_str)
	{
		//Serial Translation
		uint32_t a = 0x80000000;
		uint32_t b;
		uint32_t c;
		uint32_t d;
		uint32_t e;
		uint32_t f;

		sscanf(serial_str, "%1u%02u%02u%1u%05u", &b, &c, &d, &e, &f);
		
		b = b & 0x7;
		b = b << 0x17;

		a = 0x8007ffff & a;

		c = c & 0xf;
		c = c << 0x13;

		a = a | b;
		a = a | c;

		a = a & 0x83f807ff;
		d = d & 0x1f;
		d = d << 0xe;

		e = e & 0x7;
		e = e << 0xb;

		a = a | d;
		a = a | e;

		a = a & 0xfffff801;
		f = 0x3ff & f;
		f = f + f;
		a = a | f;

		serial_num = a;

	}

	PhantomOmniCommunications::PhantomOmniCommunications() :
		device_(raw1394_new_handle())
	{		
	}
	
	bool PhantomOmniCommunications::find()
	{
		struct raw1394_portinfo pinf[16];
		int i, numcards;
		int retval;
		numcards = raw1394_get_port_info(device_, pinf, 16);
		if (numcards < 0) {
			perror("couldn't get card info");
			return false;
		} else {
			printf("%d card(s) found\n", numcards);
		}
		
		if (!numcards) {
			printf("No available 1394 cards!\n");
			return false;
		}
		
		for (i = 0; i < numcards; i++) {
			printf("  nodes on bus: %2d, card name: %s\n", pinf[i].nodes,
				   pinf[i].name);
		}
        
		if (raw1394_set_port(device_, 0) < 0) {
			perror("couldn't set port");
			return false;
		}

		// There's something at 0x1006000c, but I don't know what it is.
		// fw_search_for_1394device_ reads it before it tries to read a serial number
		// I guess this might be a info node or something?
		/*
		  unsigned long long a = 0x1006000c;
		  retval = raw1394_read(device_, 0xffc0 | 0, a, 4, &buffer);
		  if (retval < 0) {
		  printf("failed with error");
		  } else {
		  printf("completed with value 0x%16llx\n", guid);
		  }
		
	
		  unsigned long long b = 0x10060010;
		  retval = raw1394_read(device_, 0xffc0 | 0, b, 4, &buffer2);
		  if (retval < 0) {
		  printf("failed with error");
		  } else {
		  printf("completed with value 0x%08x\n", buffer2);
		  }
		*/
/*
  printf("using first card found: %d nodes on bus, local ID is %d, IRM is %d\n",
  raw1394_get_nodecount(handle),
  raw1394_get_local_id(handle) & 0x3f,
  raw1394_get_irm_id(handle) & 0x3f);
*/
		return true;
	}

	bool PhantomOmniCommunications::open()
	{
		//Notifying channels (I have no idea what a channel is or why I'm notifying it)
		int retval;
		char val[1];
		unsigned long long notify1 = 0x1000;
		val[0] = 1;		
		writeBlocking(notify1, 1, val);

		unsigned long long notify2 = 0x1001;
		val[0] = 0;
		writeBlocking(notify2, 1, val);
		
		//Apparently "opens" the device
		unsigned long long dev_open = 0x00020010;
		uint32_t open_val = 0xf80f0000;
		retval = writeBlocking(dev_open, 4, (char*)&open_val);
		return true;
	}

	bool PhantomOmniCommunications::close()
	{
		return true;
	}

	bool PhantomOmniCommunications::ping()
	{
		return true;
	}

	bool PhantomOmniCommunications::startIsoStream()
	{
		//IMPORTANT: This is the byte that turns the haptics iso stream on 
		unsigned long long haptics = 0x1087;
		char turn_on_stream[1] = {0x08};
		writeBlocking(haptics, 1, turn_on_stream);
		raw1394_iso_recv_init(device_, my_iso_packet_handler, 1000, 64, 0, RAW1394_DMA_DEFAULT, -1);
		raw1394_iso_xmit_init(device_, NULL, 1000, 64, 0, RAW1394_ISO_SPEED_400, -1);	
		raw1394_iso_recv_start(device_, -1, -1, 0);
		raw1394_iso_xmit_start(device_, -1, -1);
		return true;
	}

	bool PhantomOmniCommunications::stopIsoStream()
	{
		//IMPORTANT: This is the byte that turns the haptics iso stream on 
		unsigned long long haptics = 0x1087;
		char turn_off_stream[1] = {0};
		writeBlocking(haptics, 1, turn_off_stream);
		raw1394_iso_stop(device_);
		return true;
	}

	void PhantomOmniCommunications::runLoop()
	{
		raw1394_loop_iterate(device_);
	}

	void PhantomOmniCommunications::getSerial(std::string& serial_str)
	{
	}

	bool PhantomOmniCommunications::writeBlocking(const uint64_t addr, uint32_t size, const char* q)
	{		
		int retval = raw1394_write(device_, 0xffc0 | 0, addr, size, q);
		if (retval < 0) {
			printf("failed with error");
			return false;
		}
		return true;
	}

	bool PhantomOmniCommunications::readBlocking(const uint64_t addr, uint32_t size, const char* q)
	{
		int retval = raw1394_read(device_, 0xffc0 | 0, addr, 4, q);
		if (retval < 0) {
			printf("failed with error");
			return false;
		}
		return true;
	}

}
