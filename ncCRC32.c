#include "ncCRC32.h"

uint32_t ComputeCRC32(uint8_t *data, int size)
{
	if (size < 4)
		return 0;

	uint32_t crc = ~((*data << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
	data += 4;
	size -= 4;
	for (int i = 0; i < size; i = i + 1)
	{
		uint8_t v = *data++;
		for (int bit = 0; bit < 8; bit++)
		{
			if ((crc & 0x80000000) == 0)
				crc = (crc * 2) ^ (v >> 7);
			else
				crc = (crc * 2) ^ (v >> 7) ^ 0x4c11db7;
			v *= 2;
		}
	}
	crc = ~crc;

	return crc;
}
