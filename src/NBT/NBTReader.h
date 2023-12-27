#include "NBTCompound.h"
#include "File/ByteWriter.h"

namespace NBT {
	class NBTReader {
	public:
		static NBTCompound* LoadFromData(Byte* data, uint length, NBTFileType* outType = NULL);
		static NBTCompound* LoadFromUncompressedStream(File::ByteBuffer* buffer);

		static void SaveToWriter(File::ByteWriter &byteWriter, NBTCompound* compound);
		static void SaveToWriterUncompressed(File::ByteWriter &byteWriter, NBTCompound* compound);

		static NBTCompound* LoadRegionFile(const char* filePath);
		static NBTCompound* LoadRegionData(Byte* data, uint length);

		static void SaveRegionToWriter(File::ByteWriter &byteWriter, NBTCompound* compound);
	};
}