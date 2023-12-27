#pragma once
#include "Exception.h"

namespace NBT {
	struct NBTEntry;
}

namespace Exception {
	class NBTException : public Exception {
	public:
		explicit NBTException(const char* message): Exception(message)
		{
		}

		explicit NBTException(const std::string& message): Exception(message)
		{
		}
	};
}