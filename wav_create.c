// Initial revision

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned char byte;
typedef unsigned int word32;

// From https://docs.fileformat.com/audio/wav/
// left channel is first:
//https://web.archive.org/
//	web/20080113195252/http://www.borg.com/~jglatt/tech/wave.htm

byte g_wav_hdr[44] = {
	'R', 'I', 'F', 'F', 0xff, 0xff, 0xff, 0xff,		// 0x00-0x07
		// [4-7]: total file size
	'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',			// 0x08-0x0f
	16, 0, 0, 0, 1, 0, 2, 0,				// 0x10-0x17
		// 16=length of 'fmt ' chunk, 1=PCM, 2=stereo.
	0xff, 0xff, 0xff, 0xff,	0xff, 0xff, 0xff, 0xff,		// 0x18-0x1f
		// [0x18-0x1b]: sample rate; [0x1c-0x1f]: bytes/sec
	4, 0, 16, 0, 'd', 'a', 't', 'a',			// 0x20-0x27
		// [0x20]: sample size bytes, [0x22]: bits per sample
	0xff, 0xff, 0xff, 0xff					// 0x28-0x2b
		// [0x28-0x2b]: bytes of sample data
};

byte g_sine_wave32[32] = {
	0x80, 0x98, 0xb0, 0xc6, 0xd9, 0xe9, 0xf5, 0xfc,
	0xfe, 0xfc, 0xf5, 0xe9, 0xd9, 0xc6, 0xb0, 0x98,
	0x80, 0x67, 0x4f, 0x39, 0x26, 0x16, 0x0a, 0x03,
	0x01, 0x03, 0x0a, 0x16, 0x26, 0x39, 0x4f, 0x67
};

int	g_out_sample_rate = (int)((1020484.32016 / 65.0) + 0.5);
int	g_out_num_channels = 1;
int	g_out_bit_width = 8;		// 1-8 is one byte unsigned, 9-16 signed
int	g_out_mask_bit_width = 16;	// Mask data to this many bits
int	g_out_toggle = 0;

typedef struct {
	const char *str;
	int	*iptr;
	char	**str_ptr;
} Opt_table;

Opt_table g_opt_table[] = {
	{ "-outsr",	&g_out_sample_rate, 0 },
	{ "-outch",	&g_out_num_channels, 0 },
	{ "-outbits",	&g_out_bit_width, 0 },
	{ "-outmaskbits",&g_out_mask_bit_width, 0 },
	{ "-outtog",	&g_out_toggle, 0 },
};

void
die(const char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(1);
}

void
set_word32(byte *bptr, word32 val)
{
	bptr[0] = val & 0xff;
	bptr[1] = (val >> 8) & 0xff;
	bptr[2] = (val >> 16) & 0xff;
	bptr[3] = (val >> 24) & 0xff;
}

void
create_file(const char *str)
{
	byte	hdr[64];
	FILE	*outfile;
	byte	*destptr, *start_destptr, *old_destptr;
	word32	ret, out_mask, out_size_bytes, out_sample_size;
	int	out_samples, off, samp, num_hi, num_lo, out_bytes_sample;
	int	size_mul, toggle;
	int	i, j;

	size_mul = 1;
	if(g_out_bit_width == 1) {
		size_mul = 65;
	}

	out_samples = g_out_sample_rate * 3;		// 3 seconds
	out_bytes_sample = (g_out_bit_width + 7) >> 3;
	out_sample_size = out_bytes_sample * g_out_num_channels;
	out_size_bytes = out_sample_size * out_samples * size_mul;

	destptr = malloc(out_size_bytes + 128);
	start_destptr = destptr;
	out_mask = 0xffff0000ULL >> g_out_mask_bit_width;
	out_mask |= 0xffff0000ULL;

	toggle = 0;
	for(i = 0; i < out_samples; i++) {
		off = destptr - start_destptr;
		if(off >= out_size_bytes) {
			die("Overflow, i:%d, off:%d, max:%d\n", i, off,
							out_size_bytes);
		}
		samp = g_sine_wave32[i & 31] & (out_mask >> 8);
		num_hi = 1;
		num_lo = 0;
		if(g_out_bit_width == 1) {
			num_hi = (samp * 65) / 255;
			if(num_hi > 65) {
				die("samp:%d led to num_hi:%d\n", samp,
								num_hi);
			}
			num_lo = 65 - num_hi;
			if(g_out_toggle) {
				if(!toggle) {
					if(samp >= 39) {
						// Do "lo" first
						num_lo = num_hi;
						num_hi = 65 - num_lo;
						samp = 0;
					} else {
						samp = 0xff;
					}
				} else {
					if(samp >= 39) {
						// Do "lo" first
						num_lo = num_hi;
						num_hi = 65 - num_lo;
						samp = 0;
					} else {
						samp = 0xff;
					}
				}
			} else {
				samp = 0xff;
			}
		}

		if((num_hi > 65) || (num_lo > 65)) {
			printf("For i:%d, num_hi:%d, num_lo:%d\n", i, num_hi,
								num_lo);
		}
		old_destptr = destptr;
		for(j = 0; j < num_hi; j++) {
			*destptr++ = samp;
		}
		samp = samp ^ 0xff;
		for(j = 0; j < num_lo; j++) {
			*destptr++ = samp;
		}
		if(num_hi) {
			toggle = ~toggle;
		}
		if(num_lo) {
			toggle = ~toggle;
		}
		if((destptr - old_destptr) > 65) {
			printf("i:%d diff:%d\n", i,
						(int)(destptr - old_destptr));
		}
	}

	outfile = fopen(str, "wb");
	if(!outfile) {
		die("Couldn't open %s for output\n", str);
	}

	memcpy(&hdr[0], &g_wav_hdr[0], 44);
	set_word32(&hdr[4], out_size_bytes + 44 - 8);
	set_word32(&hdr[24], g_out_sample_rate * size_mul);
	set_word32(&hdr[28], g_out_sample_rate * out_sample_size * size_mul);
	set_word32(&hdr[40], out_size_bytes);
	hdr[20] = 1;				// Uncompressed data
	hdr[22] = g_out_num_channels;
	hdr[32] = out_sample_size;
	hdr[34] = g_out_bit_width;
	if(g_out_bit_width == 1) {
		hdr[34] = 8;
	}

	ret = fwrite(&hdr[0], 1, 44, outfile);
	if(ret != 44) {
		printf("fwrite of hdr 44 failed:%d\n", ret);
	}
	fwrite(start_destptr, 1, out_size_bytes, outfile);
	fclose(outfile);

	printf("Done with %s\n", str);
}

int
parse_arg(int argc, char **argv, int pos)
{
	char	**str_ptr;
	int	*iptr;
	int	num, num_opts;
	int	i;

	num_opts = sizeof(g_opt_table) / sizeof(g_opt_table[0]);
	for(i = 0; i < num_opts; i++) {
		if(strcmp(argv[pos], g_opt_table[i].str) != 0) {
			continue;
		}
		// Option match
		iptr = g_opt_table[i].iptr;
		if((pos + 1) >= argc) {
			die("Option %s requires arg\n", argv[pos]);
		}
		if(iptr) {
			num = strtoul(argv[pos+1], 0, 0);
			*iptr = num;
			printf("Set %s to %d\n", argv[pos], num);
			return pos + 2;
		}
		str_ptr = g_opt_table[i].str_ptr;
		if(str_ptr) {
			*str_ptr = argv[pos + 1];
			printf("Set %s to %s\n", argv[pos], argv[pos + 1]);
			return pos + 2;
		}
	}
	return pos;
}

int
main(int argc, char **argv)
{
	int	pos, new_pos;

	pos = 1;
	while(pos < argc) {
		new_pos = parse_arg(argc, argv, pos);
		if(new_pos == pos) {
			// Not a valid option, so it must be the output file
			create_file(argv[pos]);
			new_pos++;
		}
		pos = new_pos;
	}
	return 0;
}

