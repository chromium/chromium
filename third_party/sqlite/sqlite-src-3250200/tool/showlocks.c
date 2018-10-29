/*
** This file implements a simple command-line utility that shows all of the
** Posix Advisory Locks on a file.
**
** Usage:
**
**     showlocks FILENAME
**
** To compile:  gcc -o showlocks showlocks.c
*/
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

/* This utility only looks for locks in the first 2 billion bytes */
#define MX_LCK 2147483647

/*
** Print all locks on the inode of "fd" that occur in between
** lwr and upr, inclusive.
*/
static int showLocksInRange(int fd, off_t lwr, off_t upr){
  int cnt = 0;
  struct flock x;

  x.l_type = F_WRLCK;
  x.l_whence = SEEK_SET;
  x.l_start = lwr;
  x.l_len = upr-lwr;
  fcntl(fd, F_GETLK, &x);
  if( x.l_type==F_UNLCK ) return 0;
  printf("start: %-12d len: %-5d pid: %-5d type: %s\n",
       (int)x.l_start, (int)x.l_len,
       x.l_pid, x.l_type==F_WRLCK ? "WRLCK" : "RDLCK");
  cnt++;
  if( x.l_start>lwr ){
    cnt += showLocksInRange(fd, lwr, x.l_start-1);
  }
  if( x.l_start+x.l_len<upr ){
    cnt += showLocksInRange(fd, x.l_start+x.l_len+1, upr);
  }
  return cnt;
}

int main(int argc, char **argv){
  int fd;
  int cnt;

  if( argc!=2 ){
    fprintf(stderr, "Usage: %s FILENAME\n", argv[0]);
    return 1;
  }
  fd = open(argv[1], O_RDWR, 0);
  if( fd<0 ){
    fprintf(stderr, "%s: cannot open %s\n", argv[0], argv[1]);
    return 1;
  }
  cnt = showLocksInRange(fd, 0, MX_LCK);
  if( cnt==0 ) printf("no locks\n");  
  close(fd);
  return 0;
}
