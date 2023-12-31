#include <algorithm>
#include "NBT/NBTCompound.h"
#include "File/ByteBuffer.h"
#include "NBTTag.h"
#include "NBTHelper.h"
#include "Exception/NBTException.h"

namespace NBT {

	NBTCompound::NBTCompound() : noFree(false) {
	}

	NBTCompound::NBTCompound(NBTCompound* oldCompound) : noFree(true) {
		for (auto& entry : oldCompound->entries) {
			entries.push_back(entry);
		}
	}

	NBTCompound::~NBTCompound() {
		if (!noFree)
			Free();
	}

	void NBTCompound::Free() {
		for (auto& entry : entries) {
			delete entry;
		}

		entries.clear();
	}

	void NBTCompound::Read(File::ByteBuffer* buffer) {
		NBTType type = static_cast<NBTType>(buffer->ReadByte());
		while (type != NbtEnd) {
			QString elemName = buffer->ReadString();
			NBTEntry* entry = new NBTEntry(elemName, type);

			const NBTTag* tag = NBTHelper::GetTagByType(type);
			if (tag == NULL)
				throw Exception::NBTException("Unknown nbt element type: " + std::to_string(type));
			entry->value = tag->Read(buffer);

			entries.push_back(entry);

			// Read type of next element
			type = static_cast<NBTType>(buffer->ReadByte());
		}
	}

	void NBTCompound::Write(File::WriteBuffer* buffer) {
		for (auto& entry : entries) {
			buffer->WriteByte(entry->type);
			buffer->WriteString(entry->name);

			const NBTTag* tag = NBTHelper::GetTagByType(entry->type);
			tag->Write(buffer, *entry);
		}

		buffer->WriteByte(NbtEnd);
	}

	NBTEntry* NBTCompound::FindName(QString name) {
		for (auto& entry : entries) {
			if (entry->name == name) {
				return entry;
			}
		}
		return NULL;
	}

	const std::vector<NBTEntry*>& NBTCompound::GetEntries() {
		return entries;
	}

	void NBTCompound::AddEntry(NBTEntry* entry, int position) {
		if (position == -1)
			position = entries.size();

		entries.insert(entries.begin() + position, entry);
	}

	bool NBTCompound::RemoveEntry(NBTEntry* entry) {
		std::vector<NBTEntry*>::iterator pos = std::find(entries.begin(), entries.end(), entry);
		if (pos == entries.end())
			return false;

		entries.erase(pos);
		return true;
	}

	void* NBTTagCompound::Read(File::ByteBuffer* buffer) const {
		NBTCompound* compound = new NBTCompound();
		compound->Read(buffer);
		return compound;
	}

	void NBTTagCompound::Write(WriteBuffer* buffer, NBTEntry& entry) const {
		NBTCompound* compound = GetData(entry);
		compound->Write(buffer);
	}

}