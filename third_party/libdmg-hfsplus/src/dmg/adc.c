#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <dmg/dmg.h>
#include <dmg/adc.h>

size_t adc_decompress(size_t in_size, unsigned char *input, size_t avail_size, unsigned char *output, size_t *bytes_written)
{
	if (in_size == 0)
		return 0;
	bool output_full = false;
	unsigned char *inp = input;
	unsigned char *outp = output;
	int chunk_type;
	int chunk_size;
	int offset;
	int i;

	while (inp - input < in_size) {
		chunk_type = adc_chunk_type(*inp);
		switch (chunk_type) {
		case ADC_PLAIN:
			chunk_size = adc_chunk_size(*inp);
			if (outp + chunk_size - output > avail_size) {
				output_full = true;
				break;
			}
			memcpy(outp, inp + 1, chunk_size);
			inp += chunk_size + 1;
			outp += chunk_size;
			break;

		case ADC_2BYTE:
			chunk_size = adc_chunk_size(*inp);
			offset = adc_chunk_offset(inp);
			if (outp + chunk_size - output > avail_size) {
				output_full = true;
				break;
			}
			if (offset == 0) {
				memset(outp, *(outp - offset - 1), chunk_size);
				outp += chunk_size;
				inp += 2;
			} else {
				for (i = 0; i < chunk_size; i++) {
					memcpy(outp, outp - offset - 1, 1);
					outp++;
				}
				inp += 2;
			}
			break;

		case ADC_3BYTE:
			chunk_size = adc_chunk_size(*inp);
			offset = adc_chunk_offset(inp);
			if (outp + chunk_size - output > avail_size) {
				output_full = true;
				break;
			}
			if (offset == 0) {
				memset(outp, *(outp - offset - 1), chunk_size);
				outp += chunk_size;
				inp += 3;
			} else {
				for (i = 0; i < chunk_size; i++) {
					memcpy(outp, outp - offset - 1, 1);
					outp++;
				}
				inp += 3;
			}
			break;
		}
		if (output_full)
			break;
	}
	*bytes_written = outp - output;
	return inp - input;
}

int adc_chunk_type(char _byte)
{
	if (_byte & 0x80)
		return ADC_PLAIN;
	if (_byte & 0x40)
		return ADC_3BYTE;
	return ADC_2BYTE;
}

int adc_chunk_size(char _byte)
{
	switch (adc_chunk_type(_byte)) {
		case ADC_PLAIN:
		return (_byte & 0x7F) + 1;
	case ADC_2BYTE:
		return ((_byte & 0x3F) >> 2) + 3;
	case ADC_3BYTE:
		return (_byte & 0x3F) + 4;
	}
	return -1;
}

int adc_chunk_offset(unsigned char *chunk_start)
{
	unsigned char *c = chunk_start;
	switch (adc_chunk_type(*c)) {
	case ADC_PLAIN:
		return 0;
	case ADC_2BYTE:
		return ((((unsigned char)*c & 0x03)) << 8) + (unsigned char)*(c + 1);
	case ADC_3BYTE:
		return (((unsigned char)*(c + 1)) << 8) + (unsigned char)*(c + 2);
	}
	return -1;
}
