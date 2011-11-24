
#include "setup/FileLocationEntry.hpp"

#include "util/load.hpp"
#include "util/log.hpp"
#include "util/storedenum.hpp"

void FileLocationEntry::load(std::istream & is, const InnoVersion & version) {
	
	firstSlice = load_number<uint32_t>(is, version.bits);
	lastSlice = load_number<uint32_t>(is, version.bits);
	if(version < INNO_VERSION(4, 0, 0)) {
		if(firstSlice < 1 || lastSlice < 1) {
			log_warning << "[file location] unexpected disk number: " << firstSlice << " / "
			            << lastSlice;
		} else {
			firstSlice--, lastSlice--;
		}
	}
	
	chunkOffset = load_number<uint32_t>(is);
	
	if(version >= INNO_VERSION(4, 0, 1)) {
		fileOffset = load_number<uint64_t>(is);
	} else {
		fileOffset = 0;
	}
	
	if(version >= INNO_VERSION(4, 0, 0)) {
		file_size = load_number<uint64_t>(is);
		chunkSize = load_number<uint64_t>(is);
	} else {
		file_size = load_number<uint32_t>(is);
		chunkSize = load_number<uint32_t>(is);
	}
	
	if(version >= INNO_VERSION(5, 3, 9)) {
		is.read(checksum.sha1, sizeof(checksum.sha1)), checksum.type = Checksum::Sha1;
	} else if(version >= INNO_VERSION(4, 2, 0)) {
		is.read(checksum.md5, sizeof(checksum.md5)), checksum.type = Checksum::MD5;
	} else if(version >= INNO_VERSION(4, 0, 1)) {
		checksum.crc32 = load_number<uint32_t>(is), checksum.type = Checksum::Crc32;
	} else {
		checksum.adler32 = load_number<uint32_t>(is), checksum.type = Checksum::Adler32;
	}
	
	if(version.bits == 16) {
		
		int32_t date = load_number<int32_t>(is); // milliseconds?
		
		// TODO this seems to be off by a few years:
		// expected ~ 2000-04-18, got 1991-07-28
		
		timestamp.tv_sec = date;
		timestamp.tv_nsec = 0;
		
	} else {
		
		int64_t filetime = load_number<int64_t>(is);
		
		static const int64_t FILETIME_OFFSET = 0x19DB1DED53E8000l;
		if(filetime < FILETIME_OFFSET) {
			log_warning << "[file location] unexpected filetime: " << filetime;
		}
		filetime -= FILETIME_OFFSET;
		
		timestamp.tv_sec = std::time_t(filetime / 10000000);
		timestamp.tv_nsec = int32_t(filetime % 10000000) * 100;
	}
	
	fileVersionMS = load_number<uint32_t>(is);
	fileVersionLS = load_number<uint32_t>(is);
	
	options = 0;
	
	stored_flag_reader<Options> flags(is);
	
	flags.add(VersionInfoValid);
	flags.add(VersionInfoNotValid);
	if(version >= INNO_VERSION(2, 0, 17) && version < INNO_VERSION(4, 0, 1)) {
		flags.add(BZipped);
	}
	if(version >= INNO_VERSION(4, 0, 10)) {
		flags.add(TimeStampInUTC);
	}
	if(version >= INNO_VERSION(4, 1, 0)) {
		flags.add(IsUninstallerExe);
	}
	if(version >= INNO_VERSION(4, 1, 8)) {
		flags.add(CallInstructionOptimized);
	}
	if(version >= INNO_VERSION(4, 2, 0)) {
		flags.add(Touch);
	}
	if(version >= INNO_VERSION(4, 2, 2)) {
		flags.add(ChunkEncrypted);
	}
	if(version >= INNO_VERSION(4, 2, 5)) {
		flags.add(ChunkCompressed);
	} else {
		options |= ChunkCompressed;
	}
	if(version >= INNO_VERSION(5, 1, 13)) {
		flags.add(SolidBreak);
	}
	
	options |= flags;
	
	if(options & BZipped) {
		options |= ChunkCompressed;
	}
}

ENUM_NAMES(FileLocationEntry::Options, "File Location Option",
	"version info valid",
	"version info not valid",
	"timestamp in UTC",
	"is uninstaller exe",
	"call instruction optimized",
	"touch",
	"chunk encrypted",
	"chunk compressed",
	"solid break",
	"bzipped",
)