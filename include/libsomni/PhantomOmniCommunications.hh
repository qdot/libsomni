#ifndef LIBSOMNI_PHANTOMOMNICOMMUNICATIONS_H
#define LIBSOMNI_PHANTOMOMNICOMMUNICATIONS_H

#include <string>

#ifdef LIBSOMNI_OSX
#else
#include "libraw1394/raw1394.h"
#endif

namespace libsomni {
	class PhantomOmniCommunications
	{
#ifdef LIBSOMNI_OSX
#else
		raw1394handle_t device_;
#endif
	public:
		PhantomOmniCommunications();
		~PhantomOmniCommunications();

		bool find();
		bool open();
		bool close();
		bool ping();
		bool startIsoStream();
		bool stopIsoStream();
		void getSerial(std::string& serial_str);
		bool writeBlocking(const uint64_t addr, uint32_t size, const char* q);
		bool readBlocking(const uint64_t addr, uint32_t size, const char* q);
	};
}

#endif
