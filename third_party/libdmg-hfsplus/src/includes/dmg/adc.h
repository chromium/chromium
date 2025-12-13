#include <stdio.h>
#include <unistd.h>

#define ADC_PLAIN 0x01
#define ADC_2BYTE 0x02
#define ADC_3BYTE 0x03

size_t adc_decompress(size_t in_size, unsigned char *input, size_t avail_size, unsigned char *output, size_t *bytes_written);
int adc_chunk_type(char _byte);
int adc_chunk_size(char _byte);
int adc_chunk_offset(unsigned char *chunk_start);
