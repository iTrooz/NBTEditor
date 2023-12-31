#include "NBTReader.h"
#include "NBTCompound.h"
#include "File/ByteBuffer.h"
#include "File/GzipByteReader.h"
#include "File/MemoryByteReader.h"
#include "File/MemoryByteWriter.h"
#include "File/WriteBuffer.h"
#include "File/GzipByteWriter.h"
#include "File/FileByteWriter.h"
#include "Exception/NBTException.h"
#include "Exception/FileException.h"
#include "Exception/StreamOverflowException.h"
#include "NBTHelper.h"
#include <iostream>
#include <fstream>

using namespace std;

namespace NBT {
	NBTCompound* NBTReader::LoadFromData(Byte* data, uint length, NBTFileType* outType) {
		if (length > 1 && data[0] == NbtCompound) {
			// Maybe the file is uncompressed - try to load it without gzip compression
			std::cout << "Try to load the nbt file without compression." << std::endl;

			File::MemoryByteReader reader(data, length);
			File::ByteBuffer buffer(&reader);

			try {
				NBTCompound* compound = LoadFromUncompressedStream(&buffer);
				if (outType != NULL)
					*outType = NbtUncompressed;
				return compound;
			} catch (const Exception::Exception& ex) {
				std::cout << "Loading without compression failed: " << ex.GetMessage().c_str() << std::endl;
			}
		}

		File::GzipByteReader reader(data, length);
		File::ByteBuffer buffer(&reader);

		if (outType != NULL)
			*outType = NbtGzipCompressed;

		return LoadFromUncompressedStream(&buffer);
	}

	NBTCompound* NBTReader::LoadFromUncompressedStream(File::ByteBuffer* buffer) {
		Byte rootType = buffer->ReadByte();
		if (rootType != NbtCompound)
			throw Exception::NBTException("Invalid root tag, must be a compound: " + std::to_string(rootType));
		buffer->ReadString();

		NBTCompound* rootCompound = new NBTCompound();
		rootCompound->Read(buffer);

		return rootCompound;
	}

	void NBTReader::SaveToWriter(File::ByteWriter &byteWriter, NBTCompound* compound) {
		File::GzipByteWriter* gzipWriter = new File::GzipByteWriter(&byteWriter);

		try {
			File::WriteBuffer buffer(gzipWriter);
			buffer.WriteByte(NbtCompound);
			buffer.WriteString("");

			compound->Write(&buffer);
			gzipWriter->Finish();
		} catch (const Exception::Exception& ex) {
			delete gzipWriter;
			throw ex;
		}

		delete gzipWriter;
	}

	void NBTReader::SaveToWriterUncompressed(File::ByteWriter &byteWriter, NBTCompound* compound) {
		File::WriteBuffer writer(&byteWriter);
		writer.WriteByte(NbtCompound);
		writer.WriteString("");

		compound->Write(&writer);
	}

	NBTCompound* NBTReader::LoadRegionFile(const char* filePath) {
		ifstream ifs(filePath, ios::binary | ios::ate);
		if (!ifs)
			throw Exception::FileException(strerror(errno), filePath, errno);

		uint length = ifs.tellg();
		ifs.seekg(0, ios::beg);

		// Need to be in heap because the stack is too small
		Byte* bytes = new Byte[length];
		ifs.read((char*)bytes, length);
		ifs.close();

		NBTCompound* compound = LoadRegionData(bytes, length);
		delete[] bytes;
		return compound;
	}

	NBTCompound* NBTReader::LoadRegionData(Byte* data, uint length) {
		File::MemoryByteReader reader(data, length);
		File::ByteBuffer buffer(&reader);

		RegionChunkInformation chunks[1024];
		for (int i = 0; i < 1024; i++) {
			jint offset = buffer.ReadThreeBytesInt();
			chunks[i].roundedSize = buffer.ReadByte();
			chunks[i].relX = i % 32;
			chunks[i].relZ = (int) (i / 32.0);
			chunks[i].offset = offset;
		}

		// Read chunk last modified dates
		for (int i = 0; i < 1024; i++) {
			chunks[i].lastChange = buffer.ReadInt();
		}

		NBTCompound* rootCompound = new NBTCompound();

		// Read chunk data
		for (int i = 0; i < 1024; i++) {
			RegionChunkInformation chunkInfo = chunks[i];
			if (chunkInfo.offset == 0)
				continue;

			uint offset = (chunkInfo.offset - 2) * 4096 + 8192;
			if (offset + 3 >= length)
				throw Exception::StreamOverflowException(offset, 3, length);

			uint chunkSize = (((data[offset] & 0x0F) << 24) | ((data[offset + 1] & 0xFF) << 16) | ((data[offset + 2] & 0xFF) << 8) | (data[offset + 3] & 0xFF)) - 1;
			if (offset + 5 + chunkSize >= length)
				throw Exception::StreamOverflowException(offset + 5, chunkSize, length);

			Byte compressionFormat = data[offset + 4];
			if (compressionFormat != 2)
				throw Exception::Exception(QString("Chunk %1,%2 has unknown compression format %3, only 2 (zlib) is supported.").arg(chunkInfo.relX).arg(chunkInfo.relZ).arg(compressionFormat).toStdString());

			File::GzipByteReader chunkReader(data + offset + 5, chunkSize, false);
			File::ByteBuffer chunkBuffer(&chunkReader);
			NBTCompound* compound = LoadFromUncompressedStream(&chunkBuffer);

			if (compound->FindName("LastChange") == NULL) {
				NBTEntry* lastChangeEntry = new NBTEntry("LastChange", NbtInt);
				lastChangeEntry->value = new int(chunkInfo.lastChange);
				compound->AddEntry(lastChangeEntry);
			}

			NBTEntry* entry = new NBTEntry(QString::number(chunkInfo.relX) + ", " + QString::number(chunkInfo.relZ), NbtCompound);
			entry->value = compound;
			rootCompound->AddEntry(entry);
		}

		return rootCompound;
	}

	void NBTReader::SaveRegionToWriter(File::ByteWriter &byteWriter, NBTCompound* compound) {
		File::WriteBuffer writer(&byteWriter);

		Byte fillBytes[4096];
		std::fill(fillBytes, fillBytes + 4096, 0);

		File::MemoryByteWriter chunkDataWriter;
		RegionChunkInformation chunks[1024];

		for (int i = 0; i < 1024; i++) {
			uint offset = chunkDataWriter.GetOffset();
			int relX = i % 32;
			int relZ = (int) (i / 32.0);

			NBTEntry* chunkEntry = compound->FindName(QString::number(relX) + ", " + QString::number(relZ));
			NBTCompound* chunkCompound = (chunkEntry == NULL ? NULL : NBTHelper::GetCompound(*chunkEntry));
			if (chunkCompound == NULL || chunkCompound->GetEntries().empty()) {
				chunks[i].lastChange = 0;
				chunks[i].offset = 0;
				chunks[i].roundedSize = 0;
				continue;
			}

			// Needed for removing of the LastChange entry
			NBTCompound chunkCompoundCopy(chunkCompound);

			NBTEntry* lastChange = chunkCompound->FindName("LastChange");
			if (lastChange != NULL) {
				chunks[i].lastChange = NBTHelper::GetInt(*lastChange);
				chunkCompoundCopy.RemoveEntry(lastChange);
			} else {
				chunks[i].lastChange = 0;
			}

			// Write placeholder for chunk size and compression format
			chunkDataWriter.WriteBytes(fillBytes, 4);
			chunkDataWriter.WriteByte(2);  // Zlib compression

			// Write chunk nbt data
			{
				File::GzipByteWriter gzipWriter(&chunkDataWriter, false);
				File::WriteBuffer buffer(&gzipWriter);

				buffer.WriteByte(NbtCompound);
				buffer.WriteString("");
				chunkCompoundCopy.Write(&buffer);
				gzipWriter.Finish();
			}

			// Override pre-written placeholder with the correct chunk size
			{
				uint newOffset = chunkDataWriter.GetOffset();
				int chunkSize = (newOffset - offset) - 4;
				int kilobyteSize = (newOffset - offset) / 4096 + 1;

				chunkDataWriter.SetBufferByte(offset, (chunkSize >> 24) & 0x0F);
				chunkDataWriter.SetBufferByte(offset + 1, (chunkSize >> 16) & 0xFF);
				chunkDataWriter.SetBufferByte(offset + 2, (chunkSize >> 8) & 0xFF);
				chunkDataWriter.SetBufferByte(offset + 3, chunkSize & 0xFF);

				chunks[i].roundedSize = kilobyteSize;
				chunks[i].offset = offset / 4096 + 2;

				// Fill up to 4096 bytes
				int rest = newOffset % 4096;
				if (rest != 0)
					chunkDataWriter.WriteBytes(fillBytes, 4096 - rest);
			}
		}

		for (int i = 0; i < 1024; i++) {
			RegionChunkInformation chunkInfo = chunks[i];
			writer.WriteThreeBytesInt(chunkInfo.offset);
			writer.WriteByte(chunkInfo.roundedSize);
		}
		for (int i = 0; i < 1024; i++)
			writer.WriteInt(chunks[i].lastChange);

		// Add chunkDataWriter data to file stream
		NBT::NBTArray<Byte> chunkBytes = chunkDataWriter.GetByteArray();
		writer.WriteBytes(chunkBytes.array, chunkBytes.length);
	}

}