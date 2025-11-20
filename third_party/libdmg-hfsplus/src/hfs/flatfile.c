#include <stdlib.h>
#include <hfs/hfsplus.h>

static int flatFileRead(io_func* io, off_t location, size_t size, void *buffer) {
  FILE* file;
  file = (FILE*) io->data;
  
  if(size == 0) {
    return TRUE;
  }
  
  //printf("%d %d\n", location, size); fflush(stdout);
  
  if(fseeko(file, location, SEEK_SET) != 0) {
    perror("fseek");
    return FALSE;
  }
   
  if(fread(buffer, size, 1, file) != 1) {
    perror("fread");
    return FALSE;
  } else {
    return TRUE;
  }
}

static int flatFileWrite(io_func* io, off_t location, size_t size, void *buffer) {
  FILE* file;
  
  /*int i;
  
  printf("write: %lld %d - ", location, size); fflush(stdout);
  
  for(i = 0; i < size; i++) {
    printf("%x ", ((unsigned char*)buffer)[i]);
    fflush(stdout);
  }
  printf("\n"); fflush(stdout);*/
  
  if(size == 0) {
    return TRUE;
  }
  
  file = (FILE*) io->data;
  
  if(fseeko(file, location, SEEK_SET) != 0) {
    perror("fseek");
    return FALSE;
  }
  
  if(fwrite(buffer, size, 1, file) != 1) {
    perror("fwrite");
    return FALSE;
  } else {
    return TRUE;
  }
 
  return TRUE;
}

static void closeFlatFile(io_func* io) {
  FILE* file;
  
  file = (FILE*) io->data;
  
  fclose(file);
  free(io);
}

io_func* openFlatFile(const char* fileName) {
  io_func* io;
  
  io = (io_func*) malloc(sizeof(io_func));
  io->data = fopen(fileName, "rb+");
  
  if(io->data == NULL) {
    perror("fopen");
    return NULL;
  }
  
  io->read = &flatFileRead;
  io->write = &flatFileWrite;
  io->close = &closeFlatFile;
  
  return io;
}

io_func* openFlatFileRO(const char* fileName) {
  io_func* io;
  
  io = (io_func*) malloc(sizeof(io_func));
  io->data = fopen(fileName, "rb");
  
  if(io->data == NULL) {
    perror("fopen");
    return NULL;
  }
  
  io->read = &flatFileRead;
  io->write = &flatFileWrite;
  io->close = &closeFlatFile;
  
  return io;
}
