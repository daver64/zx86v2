/*******************************************************************************
	Copyright 2001, 2002 Georges Menie (<URL snipped>)
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU Lesser General Public License for more details.
	You should have received a copy of the GNU Lesser General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
	*/

/*******************************************************************************
	putchar is the only external dependency for this file,
	if you have a working putchar, just remove the following
	define. If the function should be called something else,
	replace outbyte(c) by your own function call.
	*/
//*******************************************************************************
//  Updated by Daniel D Miller.  Changes to the original Menie code are
//  Copyright 2009-2013 Daniel D Miller
//  All such changes are distributed under the same license as the original,
//  as described above.
//  11/06/09 - adding floating-point support
//  03/19/10 - pad fractional portion of floating-point number with 0s
//  03/30/10 - document the \% bug
//  07/20/10 - Fix a round-off bug in floating-point conversions
//             ( 0.99 with %.1f did not round to 1.0 )
//  10/25/11 - Add support for %+ format (always show + on positive numbers)
//  05/10/13 - Add stringfn() function, which takes a maximum-output-buffer
//             length as an argument.  Similar to snprintf()
//*******************************************************************************
//  BUGS
//  If '%' is included in a format string, in the form \% with the intent
//  of including the percent sign in the output string, this function will
//  mis-handle the data entirely!!
//  Essentially, it will just discard the character following the percent sign.
//  This bug is not easy to fix in the existing code;
//  for now, I'll just try to remember to use %% instead of \% ...
//*******************************************************************************
//  compile command for stand-alone mode (TEST_PRINTF)
//  gcc -Wall -O2 -DTEST_PRINTF -s printf2.c -o printf2.exe
//*******************************************************************************


#include "common.h"
#include "graphics.h"
#include <stdarg.h>

extern int putchar (int c);
typedef  unsigned char  u8 ;
typedef  unsigned int   uint ;

static uint use_leading_plus = 0 ;
static int max_output_len = -1 ;
static int curr_output_len = 0 ;
//extern int strlen( char *str);
//****************************************************************************
static void printchar (char **str, int c)
{
	if (max_output_len >= 0  &&  curr_output_len >= max_output_len)
		return ;
	if (str) 
	{
		**str = (char) c;
		++(*str);
		curr_output_len++ ;
	}
	else 
	{
		curr_output_len++ ;
		os_putc(c);
	}

}

static void fprintchar(char **str, int c, FILE* fp)
{
	if (str) 
	{
		**str = (char) c;
		++(*str);
		curr_output_len++ ;
	}
	else 
	{
		curr_output_len++ ;
		fputc(c,fp);
	}
}

//****************************************************************************
//  This version returns the length of the output string.
//  It is more useful when implementing a walking-string function.
//****************************************************************************
//#ifdef TEST_PRINTF
static const double round_nums[8] = 
{
	0.5,
	0.05,
	0.005,
	0.0005,
	0.00005,
	0.000005,
	0.0000005,
	0.00000005
} ;


static unsigned dbl2stri(char *outbfr, double dbl, unsigned dec_digits)
{
	static char local_bfr[128] ;
	char *output = (outbfr == 0) ? local_bfr : outbfr ;

	//*******************************************
	//  extract negative info
	//*******************************************
	if (dbl < 0.0)
	{
		*output++ = '-' ;
		dbl *= -1.0 ;
	} 
	else
	{
		if (use_leading_plus)
		{
			*output++ = '+' ;
		}

	}

	//  handling rounding by adding .5LSB to the floating-point data
	if (dec_digits < 8) 
	{
		dbl += round_nums[dec_digits] ;
	}

	//**************************************************************************
	//  construct fractional multiplier for specified number of digits.
	//**************************************************************************
	uint mult = 1 ;
	uint idx ;
	for (idx=0; idx < dec_digits; idx++)
		mult *= 10 ;

	// printf("mult=%u\n", mult) ;
	uint wholeNum = (uint) dbl ;
	uint decimalNum = (uint) ((dbl - wholeNum) * mult);

	//*******************************************
	//  convert integer portion
	//*******************************************
	char tbfr[40] ;
	idx = 0 ;
	while (wholeNum != 0) 
	{
		tbfr[idx++] = '0' + (wholeNum % 10) ;
		wholeNum /= 10 ;
	}
	// printf("%.3f: whole=%s, dec=%d\n", dbl, tbfr, decimalNum) ;
	if (idx == 0) 
	{
		*output++ = '0' ;
	} 
	else 
	{
		while (idx > 0) 
		{
			*output++ = tbfr[idx-1] ;  //lint !e771
			idx-- ;
		}
	}
	if (dec_digits > 0)
	{
		*output++ = '.' ;

		//*******************************************
		//  convert fractional portion
		//*******************************************
		idx = 0 ;
		while (decimalNum != 0)
		{
			tbfr[idx++] = '0' + (decimalNum % 10) ;
			decimalNum /= 10 ;
		}
		//  pad the decimal portion with 0s as necessary;
		//  We wouldn't want to report 3.093 as 3.93, would we??
		while (idx < dec_digits)
		{
			tbfr[idx++] = '0' ;
		}
		// printf("decimal=%s\n", tbfr) ;
		if (idx == 0)
		{
			*output++ = '0' ;
		} 
		else
		{
			while (idx > 0)
			{
				*output++ = tbfr[idx-1] ;
				idx-- ;
			}
		}
	}
	*output = 0 ;

	//  prepare output
	output = (outbfr == 0) ? local_bfr : outbfr ;

	return strlen(output) ;

}

//****************************************************************************
#define  PAD_RIGHT   1
#define  PAD_ZERO    2

static int prints(char **out, const char *string, int width, int pad)
{
	int pc = 0, padchar = ' ';
	if (width > 0)
	{
		int len = 0;
		const char *ptr;
		for (ptr = string; *ptr; ++ptr)
			++len;
		if (len >= width)
			width = 0;
		else
			width -= len;
		if (pad & PAD_ZERO)
			padchar = '0';
	}
	if (!(pad & PAD_RIGHT))
	{
		for (; width > 0; --width) 
		{
			printchar (out, padchar);
			++pc;
		}
	}
	for (; *string; ++string)
	{
		printchar (out, *string);
		++pc;
	}
	for (; width > 0; --width)
	{
		printchar (out, padchar);
		++pc;
	}
	return pc;
}

static int fprints(char **out, const char *string, int width, int pad, FILE* fp)
{
	int pc = 0, padchar = ' ';
	if (width > 0)
	{
		int len = 0;
		const char *ptr;
		for (ptr = string; *ptr; ++ptr)
			++len;
		if (len >= width)
			width = 0;
		else
			width -= len;
		if (pad & PAD_ZERO)
			padchar = '0';
	}
	if (!(pad & PAD_RIGHT))
	{
		for (; width > 0; --width) 
		{
			fprintchar (out, padchar, fp);
			++pc;
		}
	}
	for (; *string; ++string)
	{
		fprintchar (out, *string, fp);
		++pc;
	}
	for (; width > 0; --width)
	{
		fprintchar (out, padchar, fp);
		++pc;
	}
	return pc;
}

//****************************************************************************
/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 32
static int printui (char **out, unsigned int i, uint base, int sign, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = false, pc = 0;
	unsigned u = (unsigned) i;
	if (i == 0)
	{
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}
	u=i;
	//if (sign && base == 10 && i < 0)
	//{
	//	neg = 1;
	//	u = (unsigned) -i;
	//}
	//  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) 
	{
		t = u % base;  
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = (char) t + '0';
		u /= base; 
	}
	if (neg)
	{
		if (width && (pad & PAD_ZERO)) 
		{
			printchar (out, '-');
			++pc;
			--width;
		}
		else
		{
			*--s = '-';
		}
	} 
	else
	{
		if (use_leading_plus)
		{
			*--s = '+';
		}
	}
	return pc + prints (out, s, width, pad);
}
static int printi (char **out, int i, uint base, int sign, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	unsigned u = (unsigned) i;
	if (i == 0)
	{
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}
	if (sign && base == 10 && i < 0)
	{
		neg = 1;
		u = (unsigned) -i;
	}
	//  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) 
	{
		t = u % base;  //lint !e573 !e737 !e713 Warning 573: Signed-unsigned mix with divide
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = (char) t + '0';
		u /= base;  //lint !e573 !e737  Signed-unsigned mix with divide
	}
	if (neg)
	{
		if (width && (pad & PAD_ZERO)) 
		{
			printchar (out, '-');
			++pc;
			--width;
		}
		else
		{
			*--s = '-';
		}
	} 
	else
	{
		if (use_leading_plus)
		{
			*--s = '+';
		}
	}
	return pc + prints (out, s, width, pad);
}

static int fprinti (char **out, int i, uint base, int sign, int width, int pad, int letbase, FILE* fp)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	unsigned u = (unsigned) i;
	if (i == 0)
	{
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return fprints (out, print_buf, width, pad,fp);
	}
	if (sign && base == 10 && i < 0)
	{
		neg = 1;
		u = (unsigned) -i;
	}
	//  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) 
	{
		t = u % base;  //lint !e573 !e737 !e713 Warning 573: Signed-unsigned mix with divide
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = (char) t + '0';
		u /= base;  //lint !e573 !e737  Signed-unsigned mix with divide
	}
	if (neg)
	{
		if (width && (pad & PAD_ZERO)) 
		{
			fprintchar (out, '-', fp);
			++pc;
			--width;
		}
		else
		{
			*--s = '-';
		}
	} 
	else
	{
		if (use_leading_plus)
		{
			*--s = '+';
		}
	}
	return pc + fprints (out, s, width, pad, fp);
}

//****************************************************************************
// 64-bit unsigned integer print function
static int printui64 (char **out, uint64_t i, uint base, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, pc = 0;
	if (i == 0)
	{
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}
	//  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (i) 
	{
		t = i % base;
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = (char) t + '0';
		i /= base;
	}
	return pc + prints (out, s, width, pad);
}

// 64-bit signed integer print function  
static int printi64 (char **out, int64_t i, uint base, int sign, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	uint64_t u = (uint64_t) i;
	if (i == 0)
	{
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}
	if (sign && base == 10 && i < 0)
	{
		neg = 1;
		u = (uint64_t) -i;
	}
	//  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) 
	{
		t = u % base;
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = (char) t + '0';
		u /= base;
	}
	if (neg)
	{
		if (width && (pad & PAD_ZERO))
		{
			printchar (out, '-');
			++pc;
			--width;
		}
		else
		{
			*--s = '-';
		}
	}
	return pc + prints (out, s, width, pad);
}

// 64-bit file print functions
static int fprintui64 (char **out, uint64_t i, uint base, int width, int pad, int letbase, FILE* fp)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, pc = 0;
	if (i == 0)
	{
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return fprints (out, print_buf, width, pad, fp);
	}
	//  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (i) 
	{
		t = i % base;
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = (char) t + '0';
		i /= base;
	}
	return pc + fprints (out, s, width, pad, fp);
}

static int fprinti64 (char **out, int64_t i, uint base, int sign, int width, int pad, int letbase, FILE* fp)
{
	char print_buf[PRINT_BUF_LEN];
	char *s;
	int t, neg = 0, pc = 0;
	uint64_t u = (uint64_t) i;
	if (i == 0)
	{
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return fprints (out, print_buf, width, pad, fp);
	}
	if (sign && base == 10 && i < 0)
	{
		neg = 1;
		u = (uint64_t) -i;
	}
	//  make sure print_buf is NULL-term
	s = print_buf + PRINT_BUF_LEN - 1;
	*s = '\0';

	while (u) 
	{
		t = u % base;
		if (t >= 10)
			t += letbase - '0' - 10;
		*--s = (char) t + '0';
		u /= base;
	}
	if (neg)
	{
		if (width && (pad & PAD_ZERO))
		{
			fprintchar (out, '-', fp);
			++pc;
			--width;
		}
		else
		{
			*--s = '-';
		}
	}
	return pc + fprints (out, s, width, pad, fp);
}

//****************************************************************************
static int print (char **out, int *varg)
{
	int post_decimal ;
	int width, pad ;
	unsigned dec_width = 6 ;
	int pc = 0;
	char *format = (char *) (*varg++);
	char scr[2];
	use_leading_plus = 0 ;  //  start out with this clear
	for (; *format != 0; ++format) {
		if (*format == '%')
		{
			dec_width = 6 ;
			++format;
			width = pad = 0;
			if (*format == '\0')
				break;
			if (*format == '%')
				goto out_lbl;
			if (*format == '-')

			{
				++format;
				pad = PAD_RIGHT;
			}
			if (*format == '+')
			{
				++format;
				use_leading_plus = 1 ;
			}
			while (*format == '0')
			{
				++format;
				pad |= PAD_ZERO;
			}
			post_decimal = 0 ;
			if (*format == '.'  ||
					(*format >= '0' &&  *format <= '9')) 
			{

				while (1) 
				{
					if (*format == '.')
					{
						post_decimal = 1 ;
						dec_width = 0 ;
						format++ ;
					}
					else if ((*format >= '0' &&  *format <= '9')) 
					{
						if (post_decimal)
						{
							dec_width *= 10;
							dec_width += (uint) (u8) (*format - '0');
						}
						else
						{
							width *= 10;
							width += *format - '0';
						}
						format++ ;
					}
					else 
					{
						break;
					}
				}
			}
			int is_long_long = 0;  // Track if we have 'll' modifier
			if (*format == 'l') {
				++format;
				if (*format == 'l') {
					++format; // skip second 'l' for 'll' modifier
					is_long_long = 1;
				}
			} else if (*format == 'z') {
				++format;
				// 'z' (size_t) is treated as 32-bit on our platform
			}
			switch (*format) 
			{
				case 's':
					{
						// char *s = *((char **) varg++);   //lint !e740
						char *s = (char *) *varg++ ;  //lint !e740 !e826
						pc += prints (out, s ? s : "(null)", width, pad);
						use_leading_plus = 0 ;  //  reset this flag after printing one value
					}
					break;
				case 'i':
				case 'd':
					if (is_long_long) {
						// 64-bit signed integer
						int64_t val64 = *((int64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += printi64 (out, val64, 10, 1, width, pad, 'a');
					} else {
						pc += printi (out, *varg++, 10, 1, width, pad, 'a');
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'b':
					if (is_long_long) {
						// 64-bit binary (treat as unsigned)
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += printui64 (out, val64, 2, width, pad, 'a');
					} else {
						pc += printi(out,*varg++,2,1,width,pad,'a');
					}
					use_leading_plus = 0;
					break;
				case 'x':
					if (is_long_long) {
						// 64-bit hex lowercase
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += printui64 (out, val64, 16, width, pad, 'a');
					} else {
						pc += printui (out, *varg++, 16, 0, width, pad, 'a');
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'X':
					if (is_long_long) {
						// 64-bit hex uppercase
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += printui64 (out, val64, 16, width, pad, 'A');
					} else {
						pc += printui (out, *varg++, 16, 0, width, pad, 'A');
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'p':
				case 'u':
					if (is_long_long) {
						// 64-bit unsigned decimal
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += printui64 (out, val64, 10, width, pad, 'a');
					} else {
						pc += printui (out, *varg++, 10, 0, width, pad, 'a');
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'c':
					/* char are converted to int then pushed on the stack */
					scr[0] = (char) *varg++;
					scr[1] = '\0';
					pc += prints (out, scr, width, pad);
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;

				case 'g':
				case 'f':
					{
						// http://wiki.debian.org/ArmEabiPort#Structpackingandalignment
						// Stack alignment
						//
						// The ARM EABI requires 8-byte stack alignment at public function entry points,
						// compared to the previous 4-byte alignment.
#ifdef USE_NEWLIB
						char *cptr = (char *) varg ;  //lint !e740 !e826  convert to double pointer
						uint caddr = (uint) cptr ;
						if ((caddr & 0xF) != 0) {
							cptr += 4 ;
						}
						double *dblptr = (double *) cptr ;  //lint !e740 !e826  convert to double pointer
#else
						double *dblptr = (double *) varg ;  //lint !e740 !e826  convert to double pointer
#endif
						double dbl = *dblptr++ ;   //  increment double pointer
						varg = (int *) dblptr ;    //lint !e740  copy updated pointer back to base pointer
						char bfr[81] ;
						// unsigned slen =
						dbl2stri(bfr, dbl, dec_width) ;
						// stuff_talkf("[%s], width=%u, dec_width=%u\n", bfr, width, dec_width) ;
						pc += prints (out, bfr, width, pad);
						use_leading_plus = 0 ;  //  reset this flag after printing one value
					}
					break;

				default:
					printchar (out, '%');
					printchar (out, *format);
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
			}
		}
		else
			// if (*format == '\\') {
			//
			// } else
		{
out_lbl:
			printchar (out, *format);
			++pc;
		}
	}  //  for each char in format string
	if (out) //lint !e850
		**out = '\0';
	return pc;
}

static int ffprint(char **out, int *varg, FILE* fp)
{
	int post_decimal ;
	int width, pad ;
	unsigned dec_width = 6 ;
	int pc = 0;
	char *format = (char *) (*varg++);
	char scr[2];
	use_leading_plus = 0 ;  //  start out with this clear
	for (; *format != 0; ++format) {
		if (*format == '%')
		{
			dec_width = 6 ;
			++format;
			width = pad = 0;
			if (*format == '\0')
				break;
			if (*format == '%')
				goto out_lbl;
			if (*format == '-')

			{
				++format;
				pad = PAD_RIGHT;
			}
			if (*format == '+')
			{
				++format;
				use_leading_plus = 1 ;
			}
			while (*format == '0')
			{
				++format;
				pad |= PAD_ZERO;
			}
			post_decimal = 0 ;
			if (*format == '.'  ||
					(*format >= '0' &&  *format <= '9')) 
			{

				while (1) 
				{
					if (*format == '.')
					{
						post_decimal = 1 ;
						dec_width = 0 ;
						format++ ;
					}
					else if ((*format >= '0' &&  *format <= '9')) 
					{
						if (post_decimal)
						{
							dec_width *= 10;
							dec_width += (uint) (u8) (*format - '0');
						}
						else
						{
							width *= 10;
							width += *format - '0';
						}
						format++ ;
					}
					else 
					{
						break;
					}
				}
			}
			int is_long_long = 0;  // Track if we have 'll' modifier
			if (*format == 'l') {
				++format;
				if (*format == 'l') {
					++format; // skip second 'l' for 'll' modifier
					is_long_long = 1;
				}
			} else if (*format == 'z') {
				++format;
				// 'z' (size_t) is treated as 32-bit on our platform
			}
			switch (*format) 
			{
				case 's':
					{
						// char *s = *((char **) varg++);   //lint !e740
						char *s = (char *) *varg++ ;  //lint !e740 !e826
						pc += fprints (out, s ? s : "(null)", width, pad, fp);
						use_leading_plus = 0 ;  //  reset this flag after printing one value
					}
					break;
				case 'i':
				case 'd':
					if (is_long_long) {
						// 64-bit signed integer
						int64_t val64 = *((int64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += fprinti64 (out, val64, 10, 1, width, pad, 'a', fp);
					} else {
						pc += fprinti (out, *varg++, 10, 1, width, pad, 'a', fp);
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'b':
					if (is_long_long) {
						// 64-bit binary (treat as unsigned)
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += fprintui64 (out, val64, 2, width, pad, 'a', fp);
					} else {
						pc += fprinti(out,*varg++,2,1,width,pad,'a', fp);
					}
					use_leading_plus = 0;
					break;
				case 'x':
					if (is_long_long) {
						// 64-bit hex lowercase
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += fprintui64 (out, val64, 16, width, pad, 'a', fp);
					} else {
						pc += fprinti (out, *varg++, 16, 0, width, pad, 'a', fp);
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'X':
					if (is_long_long) {
						// 64-bit hex uppercase
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += fprintui64 (out, val64, 16, width, pad, 'A', fp);
					} else {
						pc += fprinti (out, *varg++, 16, 0, width, pad, 'A', fp);
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'p':
				case 'u':
					if (is_long_long) {
						// 64-bit unsigned decimal
						uint64_t val64 = *((uint64_t*)varg);
						varg += 2; // advance by 64 bits
						pc += fprintui64 (out, val64, 10, width, pad, 'a', fp);
					} else {
						pc += fprinti (out, *varg++, 10, 0, width, pad, 'a', fp);
					}
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
				case 'c':
					 
					scr[0] = (char) *varg++;
					scr[1] = '\0';
					pc += fprints (out, scr, width, pad, fp);
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
	
			  case 'g':
				case 'f':
					{
						// http://wiki.debian.org/ArmEabiPort#Structpackingandalignment
						// Stack alignment
						//
						// The ARM EABI requires 8-byte stack alignment at public function entry points,
						// compared to the previous 4-byte alignment.
#ifdef USE_NEWLIB
						char *cptr = (char *) varg ;  //lint !e740 !e826  convert to double pointer
						uint caddr = (uint) cptr ;
						if ((caddr & 0xF) != 0) {
							cptr += 4 ;
						}
						double *dblptr = (double *) cptr ;  //lint !e740 !e826  convert to double pointer
#else
						double *dblptr = (double *) varg ;  //lint !e740 !e826  convert to double pointer
#endif
						double dbl = *dblptr++ ;   //  increment double pointer
						varg = (int *) dblptr ;    //lint !e740  copy updated pointer back to base pointer
						char bfr[81] ;
						// unsigned slen =
						dbl2stri(bfr, dbl, dec_width) ;
						// stuff_talkf("[%s], width=%u, dec_width=%u\n", bfr, width, dec_width) ;
						pc += fprints (out, bfr, width, pad, fp);
						use_leading_plus = 0 ;  //  reset this flag after printing one value
					}
					break;

				default:
					fprintchar (out, '%', fp);
					fprintchar (out, *format, fp);
					use_leading_plus = 0 ;  //  reset this flag after printing one value
					break;
			}
		}
		else
			// if (*format == '\\') {
			//
			// } else
		{
out_lbl:
			fprintchar (out, *format,fp);
			++pc;
		}
	}  //  for each char in format string
	if (out) //lint !e850
		**out = '\0';
	return pc;
}



int printf(const char *format, ...)
{
	//spin_lock(&print_lock);
	max_output_len = -1 ;
	curr_output_len = 0 ;
	int *varg = (int *) (char *) (&format);
	
	int result = print(0, varg);
	//spin_unlock(&print_lock);
	return result;
}
/*
int klog_printf(const char *format, ... )
{
	max_output_len = -1;
	curr_output_len = 0;
	FILE *kernel_log_fp=fopen("\\kernel.log","a+");
	int *varg=(int*)(char*)(&format);
	int result = ffprint(0,varg,kernel_log_fp); 
	fclose(kernel_log_fp);
	return result;
}*/
int fprintf(FILE *fp, const char *format, ...)
{
	max_output_len = -1;
	curr_output_len = 0;
	int *varg=(int*)(char*)(&format);
	return ffprint(0,varg,fp);
}

int vfprintf(FILE *fp, const char *format, va_list ap)
{
	max_output_len = -1;
	curr_output_len = 0;
	int *varg = (int *) (char *) &ap;
	return ffprint(0, varg, fp);
}

int termfn(uint max_len, const char *format, ...)
{
	max_output_len = (int) max_len ;
	curr_output_len = 0 ;
	int *varg = (int *) (char *) (&format);
	return print (0, varg);
}  
//****************************************************************************
int stringf (char *out, const char *format, ...)
{
	max_output_len = -1 ;
	curr_output_len = 0 ;
	int *varg = (int *) (char *) (&format);
	return print (&out, varg);
}
int sprintf(char *out, const char *format, ...)
{
	max_output_len = -1 ;
	curr_output_len = 0 ;
	int *varg = (int *) (char *) (&format);
	return print (&out, varg);
}
int snprintf(char *out, size_t len, const char *format, ...)
{
	max_output_len = (int) len ;
	curr_output_len = 0 ;
	int *varg = (int *) (char *) (&format);
	return print (&out, varg);
}

int vsnprintf(char *out, size_t len, const char *format, va_list ap)
{
	max_output_len = (int) len ;
	curr_output_len = 0 ;
	int *varg = (int *) (char *) &ap;
	return print (&out, varg);
}
//****************************************************************************
int stringfn(char *out, uint max_len, const char *format, ...)
{
	max_output_len = (int) max_len ;
	curr_output_len = 0 ;
	int *varg = (int *) (char *) (&format);
	return print (&out, varg);
}

