
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

char	*g_out_filename = "out.wav";
int	g_out_sample_rate = (int)((1020484.32016 / 65.0) + 0.5);
int	g_out_num_channels = 2;
int	g_out_bit_width = 16;		// 1-8 is one byte unsigned, 9-16 signed
int	g_out_mask_bit_width = 16;	// Mask data to this many bits
int	g_pwm_width = 1;
int	g_pwm_use_toggle = 0;
int	g_out_scale = 0;

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
	{ "-out",	0, &g_out_filename, },
	{ "-outscale",	&g_out_scale, 0, },
	{ "-pwm",	&g_pwm_width, 0, },
	{ "-pwmtoggle",	&g_pwm_use_toggle, 0, },
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

int
get_one_sample(byte *bufptr, double dscale, int pos, int ch, word32 size_bytes,
		int num_samples, int num_ch, int bytes_per_sample)
{
	double	first_dx, first_frac_dx, last_dx, last_frac_dx, dsum;
	int	x, last_x, samp, new, off;
	int	i;

	first_dx = dscale * pos; 
	first_frac_dx = 1.0 - (first_dx - (int)first_dx);
	last_dx = dscale * (pos + 1);
	last_frac_dx = last_dx - (int)last_dx;

	if(ch >= num_ch) {
		ch = 0;
	}
	x = (int)first_dx;
	last_x = (int)last_dx;
	if(x == last_x) {
		last_frac_dx = last_dx - first_dx;
	}
	dsum = 0;
	if(x < 10) {
		printf("x:%d pos:%d, first_dx:%f last_dx:%f dscale:%f\n",
			x, pos, first_dx, last_dx, dscale);
	}
	for(i = x; i <= last_x; i++) {
		if(i >= num_samples) {
			break;
		}
		off = (i * num_ch * bytes_per_sample) + (ch * bytes_per_sample);
		if((off + bytes_per_sample) > size_bytes) {
			break;
		}
		samp = bufptr[off];
		if(bytes_per_sample == 1) {
			samp = (samp << 8) - 32768;	// Turn unsigned->signed
		} else {
			samp = samp + (bufptr[off + 1] << 8);
			if(samp & 0x8000) {
				samp = samp - 0x10000;
			}
		}
		if(i == last_x) {
			first_frac_dx = last_frac_dx;
		}
		dsum += samp * first_frac_dx;
		if(x < 10) {
			printf(" i:%d first_frac_dx:%f, dsum:%f samp:%d\n", i,
				first_frac_dx, dsum, samp);
		}
		first_frac_dx = 1.0;
	}
	dsum = dsum / dscale;
	if(x < 10) {
		printf("Final value:%f\n", dsum);
	}
	new = (int)dsum;
	if(new < -32768) {
		new = -32768;
	} else if(new > 32767) {
		new = 32767;
	}
	return new;
}

int
pwm_out(byte *destptr, int toggle)
{
	int	samp, new_samp, num_hi, num_lo;
	int	i;

	samp = *destptr;
	num_hi = (samp * g_pwm_width) / 255;
	num_lo = g_pwm_width - num_hi;
	new_samp = 0xff;
	if(g_pwm_use_toggle) {
		// Default for non-toggle is always do HI first then LO
		//  Toggle means sometimes speaker is left in current state
		if(toggle) {		// was high at entry to sample
			if(samp >= 27) {
				// Do "lo" first
				num_lo = num_hi;
				num_hi = g_pwm_width - num_lo;
				new_samp = 0;
			}
		} else {		// was low at entry to sample
			if(samp >= 39) {
				// Do "lo" first
				num_lo = num_hi;
				num_hi = g_pwm_width - num_lo;
				new_samp = 0;
			}
		}
	}
	if((num_hi > g_pwm_width) || (num_lo > g_pwm_width)) {
		die("samp:0x%02x led to num_hi:%d, num_lo:%d\n", samp, num_hi,
								num_lo);
	}
	for(i = 0; i < num_hi; i++) {
		*destptr++ = new_samp;
	}
	if(num_hi) {
		toggle = !toggle;
	}
	new_samp = new_samp ^ 0xff;
	for(i = 0; i < num_lo; i++) {
		*destptr++ = new_samp;
	}
	if(num_lo) {
		toggle = !toggle;
	}

	return toggle;
}

void
do_file(const char *str)
{
	byte	hdr[64];
	FILE	*infile, *outfile;
	byte	*bufptr, *destptr, *start_destptr;
	double	dscale, dnew_rate;
	word32	size_bytes, samp_rate, ret, in_fmt, in_bits_sample, out_mask;
	word32	in_sample_size, in_samples, out_size_bytes, out_sample_size;
	int	c, num, new, num_0s, out_samples, off, samp, out_channels;
	int	in_channels, in_bytes_sample, out_bytes_sample, toggle;
	int	i, j;

	infile = fopen(str, "rb");
	if(infile == 0) {
		die("Could not open: %s\n", str);
	}

	num = fread(&hdr[0], 1, 44, infile);
	if(num != 44) {
		die("Couldn't read header of %s: %d\n", str, num);
	}

	for(i = 0; i < 44; i++) {
		c = g_wav_hdr[i];
		if(c == 0xff) {
			continue;
		}
		if((i == 0x16) || (i == 0x20) || (i == 0x22)) {
			// Ignore num_channels, samp_size, samp_bits
			continue;
		}
		if(c != hdr[i]) {
			printf("WAV header incorrect in %s.  [0x%02x]=%02x\n",
						str, i, hdr[i]);
			exit(1);
		}
	}

	samp_rate = (hdr[27] << 24) | (hdr[26] << 16) | (hdr[25] << 8) |
								hdr[24];
	size_bytes = (hdr[43] << 24) | (hdr[42] << 16) | (hdr[41] << 8) |
								hdr[40];
	in_channels = hdr[22];
	in_fmt = hdr[20];
	if(in_fmt != 1) {
		die("In %s, hdr[20] (wFormatTag)=%02x, must be 1\n", str,
									in_fmt);
	}
	in_bits_sample = hdr[34];
	in_sample_size = hdr[32];	// 4 bytes for stereo 16-bit PCM, etc.
	if((in_sample_size < 1) || (in_sample_size > 16)) {
		die("In %s, hdr[32] (wBlockAlign)=%02x, unsupported\n", str,
							in_sample_size);
	}
	in_bytes_sample = (in_bits_sample + 7) >> 3;

	in_samples = size_bytes / in_sample_size;

	printf("samp_rate:%d, size:%d, ch:%d sampsize:%d b:%d samples:%d\n",
		samp_rate, size_bytes, in_channels, in_sample_size,
		in_bits_sample, in_samples);
	if((samp_rate < 4000) || (samp_rate > 2000000)) {
		printf("samp_rate out of range: %d\n", samp_rate);
		exit(1);
	}
	if((size_bytes > (1 << 29)) || (size_bytes < 10)) {
		printf("size:%d too big\n", size_bytes);
		exit(1);
	}

	bufptr = malloc(size_bytes + 128);
	if(!bufptr) {
		printf("Could not allocate %d bytes of memory\n", size_bytes);
		exit(1);
	}
	ret = fread(bufptr, 1, size_bytes, infile);
	if(ret != size_bytes) {
		printf("Tried to read %d from %s, got %d\n", size_bytes, str,
								ret);
		exit(1);
	}
	fclose(infile);

	dnew_rate = (double)g_out_sample_rate;
	dscale = (double)samp_rate / dnew_rate;
	if(g_pwm_width > 1) {
		g_out_num_channels = 1;
		if(g_out_bit_width > 8) {
			g_out_bit_width = 8;
		}
	}
	if(g_out_num_channels < 0) {
		g_out_num_channels = in_channels;
	}
	out_channels = g_out_num_channels;
	if(g_out_bit_width < 0) {
		g_out_bit_width = in_bits_sample;
	}
	out_samples = (int)(in_samples / dscale);
	out_bytes_sample = (g_out_bit_width + 7) >> 3;
	out_sample_size = out_bytes_sample * g_out_num_channels;
	out_size_bytes = out_sample_size * out_samples * g_pwm_width;

	destptr = malloc(out_size_bytes + 128);
	start_destptr = destptr;
	out_mask = 0xffff0000ULL >> g_out_mask_bit_width;
	out_mask |= 0xffff0000ULL;

	num_0s = 20;
	toggle = 0;
	for(i = 0; i < out_samples; i++) {
		off = destptr - start_destptr;
		if(off >= out_size_bytes) {
			die("Overflow, i:%d, off:%d, max:%d\n", i, off,
							out_size_bytes);
		}
		for(j = 0; j < out_channels; j++) {
			samp = get_one_sample(bufptr, dscale, i, j, size_bytes,
				in_samples, in_channels, in_bytes_sample);
			// samp is in the range -32768 to +32767
			//dsamp = dsamp * 2.0;
			new = samp;
			if((out_channels == 1) && (in_channels > 1)) {
				samp = get_one_sample(bufptr, dscale, i, 1,
					size_bytes, in_samples, in_channels,
					in_bytes_sample);
				new = (new + samp) / 2;
			}
			new = new & out_mask;
			if(out_bytes_sample == 1) {
				new = (new + 32768);	// Now range 0...65535
				new = new / 256;
				if(new < 0) {
					new = 0;
				} else if(new > 255) {
					new = 255;
				}
				if(g_out_scale) {
					new = (new * g_out_scale) / 255;
				}
				if(samp == 0) {
					num_0s++;
					if(num_0s > 20) {
						new = 0;
					}
				} else {
					num_0s = 0;
				}
				*destptr++ = new;
			} else {
				*destptr++ = new & 0xff;
				*destptr++ = ((word32)new >> 8) & 0xff;
			}
		}
		if(g_pwm_width > 1) {
			toggle = pwm_out(destptr - 1, toggle);
			destptr = destptr - 1 + g_pwm_width;
		}
	}

	outfile = fopen(g_out_filename, "wb");
	if(!outfile) {
		die("Couldn't open %s for output\n", g_out_filename);
	}

	memcpy(&hdr[0], &g_wav_hdr[0], 44);
	set_word32(&hdr[4], out_size_bytes + 44 - 8);
	set_word32(&hdr[24], g_out_sample_rate * g_pwm_width);
	set_word32(&hdr[28], g_out_sample_rate * out_sample_size * g_pwm_width);
	set_word32(&hdr[40], out_size_bytes);
	hdr[20] = 1;				// Uncompressed data
	hdr[22] = g_out_num_channels;
	hdr[32] = out_sample_size;
	hdr[34] = g_out_bit_width;

	ret = fwrite(&hdr[0], 1, 44, outfile);
	if(ret != 44) {
		printf("fwrite of hdr 44 failed:%d\n", ret);
	}
	fwrite(start_destptr, 1, out_size_bytes, outfile);
	fclose(outfile);

	printf("Done with %s -> %s\n", str, g_out_filename);
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
			// Not a valid option, so it must be the input file
			do_file(argv[pos]);
			new_pos++;
		}
		pos = new_pos;
	}
	return 0;
}

