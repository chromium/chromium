#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <dmg/dmg.h>

void flipUDIFChecksum(UDIFChecksum* o, char out) {
    
    if (out) {
        if (o->bitness == 32) // crc32 checksums are integers
        {
            FLIPENDIAN(o->data[0]);
        }
        FLIPENDIAN(o->bitness);
        FLIPENDIAN(o->type);
    } else {
        FLIPENDIAN(o->type);
        FLIPENDIAN(o->bitness);
        if (o->bitness == 32) // crc32 checksums are integers
        {
            FLIPENDIAN(o->data[0]);
        }
    }
}

void readUDIFChecksum(AbstractFile* file, UDIFChecksum* o) {
  int i;
  
  o->type = readUInt32(file);
  o->bitness = readUInt32(file);
  
  for(i = 0; i < 32; i++) {
    o->data[i] = readUInt32(file);
  }
}

void writeUDIFChecksum(AbstractFile* file, UDIFChecksum* o) {
  int i;
  
  writeUInt32(file, o->type);
  writeUInt32(file, o->bitness);  
  
  for(i = 0; i < 32; i++) {
    writeUInt32(file, o->data[i]);
  }
}

void readUDIFID(AbstractFile* file, UDIFID* o) {
  o->data4 = readUInt32(file); FLIPENDIAN(o->data4);
  o->data3 = readUInt32(file); FLIPENDIAN(o->data3);
  o->data2 = readUInt32(file); FLIPENDIAN(o->data2);
  o->data1 = readUInt32(file); FLIPENDIAN(o->data1);
}

void writeUDIFID(AbstractFile* file, UDIFID* o) {
  FLIPENDIAN(o->data4); writeUInt32(file, o->data4); FLIPENDIAN(o->data4);
  FLIPENDIAN(o->data3); writeUInt32(file, o->data3); FLIPENDIAN(o->data3);
  FLIPENDIAN(o->data2); writeUInt32(file, o->data2); FLIPENDIAN(o->data2);
  FLIPENDIAN(o->data1); writeUInt32(file, o->data1); FLIPENDIAN(o->data1);
}

void readUDIFResourceFile(AbstractFile* file, UDIFResourceFile* o) {
  o->fUDIFSignature = readUInt32(file);
  
  ASSERT(o->fUDIFSignature == 0x6B6F6C79, "readUDIFResourceFile - signature incorrect");
  
  o->fUDIFVersion = readUInt32(file);
  o->fUDIFHeaderSize = readUInt32(file);
  o->fUDIFFlags = readUInt32(file);
  
  o->fUDIFRunningDataForkOffset = readUInt64(file);
  o->fUDIFDataForkOffset = readUInt64(file);
  o->fUDIFDataForkLength = readUInt64(file);
  o->fUDIFRsrcForkOffset = readUInt64(file);
  o->fUDIFRsrcForkLength = readUInt64(file);
  
  o->fUDIFSegmentNumber = readUInt32(file);
  o->fUDIFSegmentCount = readUInt32(file);
  readUDIFID(file, &(o->fUDIFSegmentID));
  
  readUDIFChecksum(file, &(o->fUDIFDataForkChecksum));
  
  o->fUDIFXMLOffset = readUInt64(file);
  o->fUDIFXMLLength = readUInt64(file);
  
  ASSERT(file->read(file, &(o->reserved1), 0x78) == 0x78, "fread");
  
  readUDIFChecksum(file, &(o->fUDIFMasterChecksum));
  
  o->fUDIFImageVariant = readUInt32(file);
  o->fUDIFSectorCount = readUInt64(file);
  
  o->reserved2 = readUInt32(file);
  o->reserved3 = readUInt32(file);
  o->reserved4 = readUInt32(file);
}

void writeUDIFResourceFile(AbstractFile* file, UDIFResourceFile* o) {
  writeUInt32(file, o->fUDIFSignature);
  writeUInt32(file, o->fUDIFVersion);
  writeUInt32(file, o->fUDIFHeaderSize);
  writeUInt32(file, o->fUDIFFlags);
  
  writeUInt64(file, o->fUDIFRunningDataForkOffset);
  writeUInt64(file, o->fUDIFDataForkOffset);
  writeUInt64(file, o->fUDIFDataForkLength);
  writeUInt64(file, o->fUDIFRsrcForkOffset);
  writeUInt64(file, o->fUDIFRsrcForkLength);
  
  writeUInt32(file, o->fUDIFSegmentNumber);
  writeUInt32(file, o->fUDIFSegmentCount);
  writeUDIFID(file, &(o->fUDIFSegmentID));
  
  writeUDIFChecksum(file, &(o->fUDIFDataForkChecksum));
  
  writeUInt64(file, o->fUDIFXMLOffset);
  writeUInt64(file, o->fUDIFXMLLength);
  
  ASSERT(file->write(file, &(o->reserved1), 0x78) == 0x78, "fwrite");
  
  writeUDIFChecksum(file, &(o->fUDIFMasterChecksum));
  
  writeUInt32(file, o->fUDIFImageVariant);
  writeUInt64(file, o->fUDIFSectorCount);
  
  writeUInt32(file, o->reserved2);
  writeUInt32(file, o->reserved3);
  writeUInt32(file, o->reserved4);
}

