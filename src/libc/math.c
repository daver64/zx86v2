#include "common.h"
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <errno.h>

long labs(long i)
{
	return i < 0 ? -i : i;
}

int abs(int i)
{
	return i < 0 ? -i : i;
}

double exp(double x)
{
	asm("fldl2e ; fmulp ; f2xm1" : "+t"(x));
	return x + 1;
}

float expf(float x)
{
	asm("fldl2e ; fmulp ; f2xm1" : "+t"(x));
	return x + 1;
}

long double expl(long double x)
{
	asm("fldl2e ; fmulp ; f2xm1" : "+t"(x));
	return x + 1;
}

double exp2(double x)
{
	asm("f2xm1" : "+t"(x));
	return x + 1;
}

float exp2f(float x)
{
	asm("f2xm1" : "+t"(x));
	return x + 1;
}

long double exp2l(long double x)
{
	asm("f2xm1" : "+t"(x));
	return x + 1;
}

double expm1(double x)
{
	return exp(x) - 1;
}

float expm1f(float x)
{
	return expf(x) - 1;
}

long double expm1l(long double x)
{
	return expl(x) - 1;
}

double ldexp(double x, int exp)
{
	return x * (1 << exp);
}

float ldexpf(float x, int exp)
{
	return x * (1 << exp);
}

long double ldexpl(long double x, int exp)
{
	return x * (1 << exp);
}

double log(double x)
{
	double ln2;
	asm("fldln2" : "=t"(ln2));
	return ln2 * log2(x);
}

float logf(float x)
{
	float ln2;
	asm("fldln2" : "=t"(ln2));
	return ln2 * log2f(x);
}

long double logl(long double x)
{
	long double ln2;
	asm("fldln2" : "=t"(ln2));
	return ln2 * log2l(x);
}

double log10(double x)
{
	double log10_2;
	asm("fldlg2" : "=t"(log10_2));
	return log10_2 * log2(x);
}

float log10f(float x)
{
	double log10_2;
	asm("fldlg2" : "=t"(log10_2));
	return log10_2 * log2f(x);
}

long double log10l(long double x)
{
	double log10_2;
	asm("fldlg2" : "=t"(log10_2));
	return log10_2 * log2l(x);
}

double log1p(double x)
{
	log(1 + x);
}

float log1pf(float x)
{
	logf(1 + x);
}

long double log1pl(long double x)
{
	logl(1 + x);
}

double log2(double x)
{
	asm("fld1 ; fxch ; fyl2x" : "+t"(x));
	return x;
}

float log2f(float x)
{
	asm("fld1 ; fxch ; fyl2x" : "+t"(x));
	return x;
}

long double log2l(long double x)
{
	asm("fld1 ; fxch ; fyl2x" : "+t"(x));
	return x;
}

double cbrt(double x)
{
	return pow(x, 1. / 3.);
}

double fabs(double x)
{
	asm("fabs" : "+t"(x));
	return x;
}

double hypot(double x, double y)
{
	return sqrt(x * x + y * y);
}

float hypotf(float x, float y)
{
	return sqrt(x * x + y * y);
}

long double hypotl(long double x, long double y)
{
	return sqrt(x * x + y * y);
}

double pow(double x, double y)
{
	return exp(y * log(x));
}

float powf(float x, float y)
{
	return exp(y * log(x));
}

long double powl(long double x, long double y)
{
	return exp(y * log(x));
}

double sqrt(double x)
{
	asm("fsqrt" : "+t"(x));
	return x;
}

float sqrtf(float x)
{
	asm("fsqrt" : "+t"(x));
	return x;
}

long double sqrtl(long double x)
{
	asm("fsqrt" : "+t"(x));
	return x;
}



double sin(double x)
{
	asm("fsin" : "+t"(x));
	return x;
}
double cos(double x)
{
	asm("fcos" : "+t"(x));
	return x;
}
double tan(double x)
{
	asm("fptan" : "+t"(x));
	return x;
}
double atan(double x)
{
	asm("fpatan" : "+t"(x));
	return x;
}
float atanf(float x)
{
	x = atan((double)x);
	return x;
}
// long int				strtol		(const char* nptr, char** endptr, int base);
////unsigned long int		strtou
#ifndef pi
#define pi (3.1415926)
#endif
/*float atan2f(float a, float b)
{
	float atan2val;
	if (b > 0)
	{
		atan2val = atan(a/b);
	}
	else if ((b < 0) && (a >= 0) )
	{
		atan2val = atan(a/b) + pi;
	}
	else if ((b < 0) && (a < 0))
	{
		atan2val = atan(a/b) - pi;
	}
	else if ((b == 0) && (a > 0))
	{
		atan2val = pi / 2;
	}
	else if ((b == 0) && (a < 0))
	{
		atan2val = 0 - (pi / 2 );
	}
	else if ((b == 0) && (a == 0))
	{
		atan2val = 1000;               //represents undefined
	}
	return atan2val;
}*/
float atan2f(float y, float x)
{
	// http://pubs.opengroup.org/onlinepubs/009695399/functions/atan2.html
	// Volkan SALMA

	const float ONEQTR_PI = M_PI / 4.0;
	const float THRQTR_PI = 3.0 * M_PI / 4.0;
	float r, angle;
	float abs_y = fabs(y) + 1e-10f; // kludge to prevent 0/0 condition
	if (x < 0.0f)
	{
		r = (x + abs_y) / (abs_y - x);
		angle = THRQTR_PI;
	}
	else
	{
		r = (x - abs_y) / (x + abs_y);
		angle = ONEQTR_PI;
	}
	angle += (0.1963f * r * r - 0.9817f) * r;
	if (y < 0.0f)
		return (-angle); // negate if in quad III or IV
	else
		return (angle);
}

double atan2(double y, double x)
{
	return atan2f(y, x);
}
unsigned long int strtoul(const char *nptr, char **endptr, int base)
{
	const char *s = nptr;
	unsigned long acc;
	int c;
	unsigned long cutoff;
	int neg = 0, any, cutlim;
	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	do
	{
		c = *s++;
	} while (isspace(c));

	if (c == '-')
	{
		neg = 1;
		c = *s++;
	}
	else if (c == '+')
		c = *s++;

	if ((base == 0 || base == 16) &&
		c == '0' && (*s == 'x' || *s == 'X'))
	{
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? -(unsigned long)ULONG_MIN : ULONG_MAX;
	cutlim = cutoff % (unsigned long)base;
	cutoff /= (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++)
	{
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else
		{
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0)
	{
		acc = neg ? LONG_MIN : LONG_MAX;
		errno = ERANGE;
	}
	else if (neg)
		acc = -acc;

	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}
int strtol(const char *nptr, char **endptr, int base)
{
	const char *s = nptr;
	unsigned long acc;
	int c;
	unsigned long cutoff;
	int neg = 0, any, cutlim;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	do
	{
		c = *s++;
	} while (isspace(c));
	if (c == '-')
	{
		neg = 1;
		c = *s++;
	}
	else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
		c == '0' && (*s == 'x' || *s == 'X'))
	{
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
	cutlim = cutoff % (unsigned long)base;
	cutoff /= (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++)
	{
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else
		{
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0)
	{
		acc = neg ? LONG_MIN : LONG_MAX;
		errno = ERANGE;
	}
	else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}
long long strtoll(const char *nptr, char **endptr, int base)
{
	/*	const char *s = nptr;
			int acc;
			int c;
			int cutoff;
			int neg = 0, any, cutlim;


			do {
			c = *s++;
			} while (isspace(c));
			if (c == '-') {
			neg = 1;
			c = *s++;
			} else if (c == '+')
			c = *s++;
			if ((base == 0 || base == 16) &&
			c == '0' && (*s == 'x' || *s == 'X')) {
			c = s[1];
			s += 2;
			base = 16;
			}
			if (base == 0)
			base = c == '0' ? 8 : 10;


			cutoff = neg ? -(unsigned long long)LLONG_MIN : LLONG_MAX;
			cutlim = cutoff % (unsigned long long)base;
			cutoff /= (unsigned long long)base;
			for (acc = 0, any = 0;; c = *s++) {
			if (isdigit(c))
			c -= '0';
			else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
			else
			break;
			if (c >= base)
			break;
			if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
			else {
			any = 1;
			acc *= base;
			acc += c;
			}
			}
			if (any < 0) {
			acc = neg ? LLONG_MIN : LLONG_MAX;
			errno = ERANGE;
			} else if (neg)
			acc = -acc;
			if (endptr != 0)
	 *endptr = (char *) (any ? s - 1 : nptr);
	 return (acc);*/

	return 0;
}
long int atol(const char *c)
{
	return (long int)atoi(c);
}
int atoi(const char *c)
{
	int previous_result = 0, result = 0;
	int multiplier = 1;

	if (c && *c == '-')
	{
		multiplier = -1;
		c++;
	}
	else
	{
		multiplier = 1;
	}
	while (*c)
	{
		if (*c < '0' || *c > '9')
		{
			return result * multiplier;
		}
		result *= 10;
		if (result < previous_result)
		{
			// errno = 9;
			return (INT_MIN);
		}
		else
		{
			previous_result *= 10;
		}
		result += *c - '0';

		if (result < previous_result)
		{
			// errno = 9;
			return (INT_MIN);
		}
		else
		{
			previous_result += *c - '0';
		}
		c++;
	}
	return (result * multiplier);
}

double atof(const char *s)
{
	double a = 0.0;
	int e = 0;
	int c;
	double sign_multiplier = 1.0;
	if (*s == '-')
	{
		s++;
		sign_multiplier = -1.0;
	}
	else if (*s == '+')
	{
		s++;
	}
	while ((c = *s++) != '\0' && isdigit(c))
	{
		a = a * 10.0 + (c - '0');
	}
	if (c == '.')
	{
		while ((c = *s++) != '\0' && isdigit(c))
		{
			a = a * 10.0 + (c - '0');
			e = e - 1;
		}
	}
	if (c == 'e' || c == 'E')
	{
		int sign = 1;
		int i = 0;
		c = *s++;
		if (c == '+')
			c = *s++;
		else if (c == '-')
		{
			c = *s++;
			sign = -1;
		}
		while (isdigit(c))
		{
			i = i * 10 + (c - '0');
			c = *s++;
		}
		e += i * sign;
	}
	while (e > 0)
	{
		a *= 10.0;
		e--;
	}
	while (e < 0)
	{
		a *= 0.1;
		e++;
	}
	return (a * sign_multiplier);
}

double ceil(double x)
{
	uint_least16_t cw;
	asm volatile("fstcw %0" : : "m"(cw) : "memory");
	cw |= 0x800;
	cw &= ~0x400;
	asm volatile("fldcw %0" : : "m"(cw));
	asm("frndint" : "+t"(x));
	return x;
}

double floor(double x)
{
	uint_least16_t cw;
	asm volatile("fstcw %0" : : "m"(cw) : "memory");
	cw |= 0x400;
	cw &= ~0x800;
	asm volatile("fldcw %0" : : "m"(cw));
	asm("frndint" : "+t"(x));
	return x;
}

double remainder(double x, double y)
{
	double result;
	asm("fldl %2 ; fldl %1 ; fprem ; fxch ; fincstp" : "=t"(result) : "m"(x), "m"(y));
	return result;
}

double remquo(double x, double y, int *quo)
{
	*quo = (int)x / (int)y;
	return remainder(x, y);
}

double copysign(double x, double y)
{
	double magnitude = fabs(x);
	return signbit(y) ? -magnitude : magnitude;
}

double fdim(double x, double y)
{
	return (x > y) ? (x - y) : 0;
}

float fdimf(float x, float y)
{
	return (x > y) ? (x - y) : 0;
}

long double fdiml(long double x, long double y)
{
	return (x > y) ? (x - y) : 0;
}

double fmax(double x, double y)
{
	return (x > y) ? x : y;
}

float fmaxf(float x, float y)
{
	return (x > y) ? x : y;
}

long double fmaxl(long double x, long double y)
{
	return (x > y) ? x : y;
}

double fmin(double x, double y)
{
	return (x < y) ? x : y;
}

float fminf(float x, float y)
{
	return (x < y) ? x : y;
}

long double fminl(long double x, long double y)
{
	return (x < y) ? x : y;
}

double fma(double x, double y, double z)
{
	return x * y + z;
}

float fmaf(float x, float y, float z)
{
	return x * y + z;
}

long double fmal(long double x, long double y, long double z)
{
	return x * y + z;
}

ldiv_t ldiv(long int numer, long int denom)
{
	ldiv_t result;
	result.quot = numer / denom;
	result.rem = numer % denom;
	if (numer >= 0 && result.rem < 0)
	{
		++result.quot;
		result.rem -= denom;
	}
	return result;
}

uint64_t __udivmoddi4(uint64_t num, uint64_t den, uint64_t *rem_p)
{
	uint64_t quot = 0, qbit = 1;

	if (den == 0)
	{
		return 1 / ((unsigned)den); /* Intentional divide by zero, without
																   triggering a compiler warning which
																   would abort the build */
	}

	/* Left-justify denominator and count shift */
	while ((int64_t)den >= 0)
	{
		den <<= 1;
		qbit <<= 1;
	}

	while (qbit)
	{
		if (den <= num)
		{
			num -= den;
			quot += qbit;
		}
		den >>= 1;
		qbit >>= 1;
	}

	if (rem_p)
		*rem_p = num;

	return quot;
}

uint64_t __moddi3(uint64_t a, uint64_t b)
{
	const int bits_in_dword_m1 = (int)(sizeof(uint64_t) * CHAR_BIT) - 1;
	uint64_t s = b >> bits_in_dword_m1; // s = b < 0 ? -1 : 0
	b = (b ^ s) - s;					// negate if s == -1
	s = a >> bits_in_dword_m1;			// s = a < 0 ? -1 : 0
	a = (a ^ s) - s;					// negate if s == -1
	uint64_t r;
	__udivmoddi4(a, b, &r);
	return ((uint64_t)r ^ s) - s; // negate if s == -1
}

uint64_t __divdi3(uint64_t a, uint64_t b)
{
	const int bits_in_dword_m1 = (int)(sizeof(uint64_t) * CHAR_BIT) - 1;
	uint64_t s_a = a >> bits_in_dword_m1;					// s_a = a < 0 ? -1 : 0
	uint64_t s_b = b >> bits_in_dword_m1;					// s_b = b < 0 ? -1 : 0
	a = (a ^ s_a) - s_a;									// negate if s_a == -1
	b = (b ^ s_b) - s_b;									// negate if s_b == -1
	s_a ^= s_b;												// sign of quotient
	return (__udivmoddi4(a, b, (uint64_t *)0) ^ s_a) - s_a; // negate if s_a == -1
}

uint64_t __divmoddi4(uint64_t a, uint64_t b, uint64_t *rem)
{
	uint64_t d = __divdi3(a, b);
	*rem = a - (d * b);
	return d;
}
double trunc(double x)
{
	if (x < 0)
		return -floor(-x);
	return floor(x);
}
double fmod(double x, double y)
{
	return x = trunc(x / y) * y;
}
double modf(double x, double *iptr)
{
	union
	{
		double f;
		uint64_t i;
	} u = {x};
	uint64_t mask;
	int e = (int)(u.i >> 52 & 0x7ff) - 0x3ff;

	/* no fractional part */
	if (e >= 52)
	{
		*iptr = x;
		if (e == 0x400 && u.i << 12 != 0) /* nan */
			return x;
		u.i &= 1ULL << 63;
		return u.f;
	}

	/* no integral part*/
	if (e < 0)
	{
		u.i &= 1ULL << 63;
		*iptr = u.f;
		return x;
	}

	mask = -1ULL >> 12 >> e;
	if ((u.i & mask) == 0)
	{
		*iptr = x;
		u.i &= 1ULL << 63;
		return u.f;
	}
	u.i &= ~mask;
	*iptr = u.f;
	return x - u.f;
}
