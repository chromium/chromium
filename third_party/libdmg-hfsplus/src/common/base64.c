#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dmg/dmg.h>

unsigned char* decodeBase64(char* toDecode, size_t* dataLength) {
  uint8_t buffer[4];
  uint8_t charsInBuffer;
  unsigned char* curChar;
  unsigned char* decodeBuffer;
  unsigned int decodeLoc;
  unsigned int decodeBufferSize;
  uint8_t bytesToDrop;
  
  curChar = (unsigned char*) toDecode;
  charsInBuffer = 0;
  
  decodeBufferSize = 100;
  decodeLoc = 0;
  decodeBuffer = (unsigned char*) malloc(decodeBufferSize);
  
  bytesToDrop = 0;
  
  while((*curChar) != '\0') {
    if((*curChar) >= 'A' && (*curChar) <= 'Z') {
      buffer[charsInBuffer] = (*curChar) - 'A';
      charsInBuffer++;
    }
    
    if((*curChar) >= 'a' && (*curChar) <= 'z') {
      buffer[charsInBuffer] = ((*curChar) - 'a') + ('Z' - 'A' + 1);
      charsInBuffer++;
    }
    
    if((*curChar) >= '0' && (*curChar) <= '9') {
      buffer[charsInBuffer] = ((*curChar) - '0') + ('Z' - 'A' + 1)  + ('z' - 'a' + 1);
      charsInBuffer++;
    }
    
    if((*curChar) == '+') {
      buffer[charsInBuffer] = ('Z' - 'A' + 1)  + ('z' - 'a' + 1)  + ('9' - '0' + 1);
      charsInBuffer++;
    }
    
    if((*curChar) == '/') {
      buffer[charsInBuffer] = ('Z' - 'A' + 1)  + ('z' - 'a' + 1)  + ('9' - '0' + 1) + 1;
      charsInBuffer++;
    }
    
    if((*curChar) == '=') {
      bytesToDrop++;
    }
    
    if(charsInBuffer == 4) {
      charsInBuffer = 0;
      
      if((decodeLoc + 3) >= decodeBufferSize) {
        decodeBufferSize <<= 1;
        decodeBuffer = (unsigned char*) realloc(decodeBuffer, decodeBufferSize);
      }
      decodeBuffer[decodeLoc] = ((buffer[0] << 2) & 0xFC) + ((buffer[1] >> 4) & 0x3F);
      decodeBuffer[decodeLoc + 1] = ((buffer[1] << 4) & 0xF0) + ((buffer[2] >> 2) & 0x0F);
      decodeBuffer[decodeLoc + 2] = ((buffer[2] << 6) & 0xC0) + (buffer[3] & 0x3F);
      
      decodeLoc += 3;
      buffer[0] = 0;
      buffer[1] = 0;
      buffer[2] = 0;
      buffer[3] = 0;
    }

    curChar++;
  }
  
  if(bytesToDrop != 0) {  
    if((decodeLoc + 3) >= decodeBufferSize) {
      decodeBufferSize <<= 1;
      decodeBuffer = (unsigned char*) realloc(decodeBuffer, decodeBufferSize);
    }
    
    decodeBuffer[decodeLoc] = ((buffer[0] << 2) & 0xFC) | ((buffer[1] >> 4) & 0x3F);
    
    if(bytesToDrop <= 2)
      decodeBuffer[decodeLoc + 1] = ((buffer[1] << 4) & 0xF0) | ((buffer[2] >> 2) & 0x0F);
      
    if(bytesToDrop <= 1)
      decodeBuffer[decodeLoc + 2] = ((buffer[2] << 6) & 0xC0) | (buffer[3] & 0x3F);
    
    *dataLength = decodeLoc + 3 - bytesToDrop;
  } else {
    *dataLength = decodeLoc;
  }
  
  return decodeBuffer;
}

void writeBase64(AbstractFile* file, unsigned char* data, size_t dataLength, int tabLength, int width) {
  char* buffer;
  buffer = convertBase64(data, dataLength, tabLength, width);
  file->write(file, buffer, strlen(buffer));
  free(buffer);
}

#define CHECK_BUFFER_SIZE() \
    if(pos == bufferSize) { \
      bufferSize <<= 1; \
      buffer = (unsigned char*) realloc(buffer, bufferSize); \
    }

#define CHECK_LINE_END_STRING() \
    CHECK_BUFFER_SIZE() \
    if(width == lineLength) { \
      buffer[pos++] = '\n'; \
      CHECK_BUFFER_SIZE() \
      for(j = 0; j < tabLength; j++) { \
        buffer[pos++] = '\t'; \
        CHECK_BUFFER_SIZE() \
      } \
      lineLength = 0; \
    } else { \
      lineLength++; \
    }

char* convertBase64(unsigned char* data, size_t dataLength, int tabLength, int width) {
  const char* dictionary = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  
  unsigned char* buffer;
  size_t pos;
  size_t bufferSize;
  int i, j;
  int lineLength;
  
  bufferSize = 100;
  buffer = (unsigned char*) malloc(bufferSize);
  pos = 0;
  lineLength = 0;
   
  for(i = 0; i < tabLength; i++) {
    CHECK_BUFFER_SIZE()
    buffer[pos++] = '\t';
  }
  i = 0;
  while(dataLength >= 3) {
    dataLength -= 3;
    buffer[pos++] = dictionary[(data[i] >> 2) & 0x3F];
    CHECK_LINE_END_STRING();
    buffer[pos++] = dictionary[(((data[i] << 4) & 0x30) | ((data[i+1] >> 4) & 0x0F)) & 0x3F];
    CHECK_LINE_END_STRING();
    buffer[pos++] = dictionary[(((data[i+1] << 2) & 0x3C) | ((data[i+2] >> 6) & 0x03)) & 0x03F];
    CHECK_LINE_END_STRING();
    buffer[pos++] = dictionary[data[i+2] & 0x3F];
    CHECK_LINE_END_STRING();
    i += 3;
  }

  if(dataLength == 2) {
    buffer[pos++] = dictionary[(data[i] >> 2) & 0x3F];
    CHECK_LINE_END_STRING();
    buffer[pos++] = dictionary[(((data[i] << 4) & 0x30) | ((data[i+1] >> 4) & 0x0F)) & 0x3F];
    CHECK_LINE_END_STRING();
    buffer[pos++] = dictionary[(data[i+1] << 2) & 0x3C];
    CHECK_LINE_END_STRING();
    buffer[pos++] = '=';
  } else if(dataLength == 1) {
    buffer[pos++] = dictionary[(data[i] >> 2) & 0x3F];
    CHECK_LINE_END_STRING();
    buffer[pos++] = dictionary[(data[i] << 4) & 0x30];
    CHECK_LINE_END_STRING();
    buffer[pos++] = '=';
    CHECK_LINE_END_STRING();
    buffer[pos++] = '=';
  }
  
  CHECK_BUFFER_SIZE();
  buffer[pos++] = '\n';
  
  CHECK_BUFFER_SIZE();
  buffer[pos++] = '\0';
  
  return (char*) buffer;
}
