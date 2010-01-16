#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/poll.h>

#include "libraw1394/raw1394.h"
#include "libraw1394/csr.h"

#define TESTADDR (CSR_REGISTER_BASE + CSR_CYCLE_TIME)
#define BUFFER 1000
#define PACKET_MAX 64

octlet_t guid;
quadlet_t buffer;
quadlet_t buffer2;

quadlet_t zero = 0;
quadlet_t one = 1;
quadlet_t test = 0x40;

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

union _SERIAL_NUMBER
{
	uint32_t a;
};


class CPHANToMMgr
{
public:
	void PackSerialNumber(_SERIAL_NUMBER* s, const char* serial_str)
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

		(*s).a = a;
	}
};

int my_tag_handler(raw1394handle_t handle, unsigned long tag,
                   raw1394_errcode_t errcode)
{
	int err = raw1394_errcode_to_errno(errcode);

	if (err) {
		printf("failed with error: %s\n", strerror(err));
	} else {
		printf("completed with value 0x%08x\n", buffer);
	}

	return 0;
}

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

int main(int argc, char** argv)
{
	raw1394handle_t handle;
	int i, numcards;
	struct raw1394_portinfo pinf[16];

	tag_handler_t std_handler;
	int retval;
        
	handle = raw1394_new_handle();

	if (!handle) {
		if (!errno) {
			printf("not compatible");
		} else {
			perror("couldn't get handle");
			printf("not loaded");
		}
		exit(1);
	}

	printf("successfully got handle\n");
	printf("current generation number: %d\n", raw1394_get_generation(handle));

	numcards = raw1394_get_port_info(handle, pinf, 16);
	if (numcards < 0) {
		perror("couldn't get card info");
		exit(1);
	} else {
		printf("%d card(s) found\n", numcards);
	}

	if (!numcards) {
		exit(0);
	}

	for (i = 0; i < numcards; i++) {
		printf("  nodes on bus: %2d, card name: %s\n", pinf[i].nodes,
			   pinf[i].name);
	}
        
	if (raw1394_set_port(handle, 0) < 0) {
		perror("couldn't set port");
		exit(1);
	}

	printf("using first card found: %d nodes on bus, local ID is %d, IRM is %d\n",
		   raw1394_get_nodecount(handle),
		   raw1394_get_local_id(handle) & 0x3f,
		   raw1394_get_irm_id(handle) & 0x3f);

	std_handler = raw1394_set_tag_handler(handle, my_tag_handler);
	printf("\nusing standard tag handler and synchronous calls\n");
	raw1394_set_tag_handler(handle, std_handler);
	//for (i = 0; i < pinf[0].nodes; i++) {
	printf("trying to read from node %d...\n", 0);
	fflush(stdout);
	buffer = 0;

	// There's something at 0x1006000c, but I don't know what it is.
	// fw_search_for_1394_device reads it before it tries to read a serial number
	// I guess this might be a info node or something?
	
	unsigned long long a = 0x1006000c;
	retval = raw1394_read(handle, 0xffc0 | 0, a, 4, &buffer);
	if (retval < 0) {
		printf("failed with error");
	} else {
		printf("completed with value 0x%16llx\n", guid);
	}

	
	unsigned long long b = 0x10060010;
	retval = raw1394_read(handle, 0xffc0 | 0, b, 4, &buffer2);
	if (retval < 0) {
		printf("failed with error");
	} else {
		printf("completed with value 0x%08x\n", buffer2);
	}

	fflush(stdout);	
	CPHANToMMgr c;
	_SERIAL_NUMBER d;	
	char s[] = "1111111111111";
	c.PackSerialNumber(&d, s);
	printf("%08x\n", d.a);
	
	_SERIAL_NUMBER n;
	char serial[20];
	endian_swap(buffer2);
	printf("completed with value 0x%08x\n", buffer2);
	//c.UnpackSerialNumber((_SERIAL_NUMBER*)&buffer2, serial);
	printf("Serial: %s\n", serial);

	//Notifying channels (I have no idea what a channel is or why I'm notifying it)
	unsigned long long notify1 = 0x1000;
	retval = raw1394_write(handle, 0xffc0 | 0, notify1, 1, &one);
	unsigned long long notify2 = 0x1001;
	retval = raw1394_write(handle, 0xffc0 | 0, notify2, 1, &zero);

	//Apparently "opens" the device
	unsigned long long dev_open = 0x00020010;
	quadlet_t open_val = 0xf80f0000;
	retval = raw1394_write(handle, 0xffc0 | 0, dev_open, 1, &open_val);

	//IMPORTANT: This is the byte that turns the haptics iso stream on 
	unsigned long long haptics = 0x1087;
	quadlet_t turn_on_stream = 0x08;
	retval = raw1394_write(handle, 0xffc0 | 0, haptics, 1, &turn_on_stream);

	enum raw1394_iso_dma_recv_mode mode = RAW1394_DMA_DEFAULT;	
	raw1394_iso_recv_init(handle, my_iso_packet_handler, BUFFER, PACKET_MAX, 0, mode, -1);
	raw1394_iso_xmit_init(handle, NULL, BUFFER, PACKET_MAX, 0, RAW1394_ISO_SPEED_400, -1);	
	raw1394_iso_xmit_start(handle, -1, -1);
	raw1394_iso_recv_start(handle, -1, -1, 0);
	while (raw1394_loop_iterate(handle) == 0)
	{
		
	}
	//IMPORTANT: This turns the haptics iso stream off
	retval = raw1394_write(handle, 0xffc0 | 0, haptics, 1, &zero);
	raw1394_iso_shutdown(handle);
	raw1394_destroy_handle(handle);

	return 0;
}
