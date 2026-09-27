#include "common.h"
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <errno.h>

/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

static volatile const double Infinity = INFINITY;
static volatile const double Tiny = 0x1p-1022;

// double2 represents a number equal to d0 + d1, with |d1| <= 1/2 ULP(d0).
typedef struct
{
	double d0, d1;
} double2;

// Return a * b, given |a|, |b| < 2**970.
static inline double2 Mul112(double a, double b)
{
	static const double c = 0x1p27 + 1;

	double
		ap = a * c,
		bp = b * c,
		a0 = (a - ap) + ap, b0 = (b - bp) + bp,
		a1 = a - a0, b1 = b - b0,
		d0 = a * b,
		d1 = a0 * b0 - d0 + a0 * b1 + a1 * b0 + a1 * b1;
	return (double2){d0, d1};
}

// Return a + b with relative error below 2**-103 given |b| < |a|.
static inline double2 Add212RightSmaller(double2 a, double b)
{
	double
		c0 = a.d0 + b,
		c1 = a.d0 - c0 + b + a.d1,
		d0 = c0 + c1,
		d1 = c0 - d0 + c1;
	return (double2){d0, d1};
}

/*	Return a + b with relative error below 2**-103 and then rounded to
	double given |b| < |a|.
*/
static inline double Add221RightSmaller(double2 a, double2 b)
{
	double
		c0 = a.d0 + b.d0,
		c1 = a.d0 - c0 + b.d0 + b.d1 + a.d1,
		d0 = c0 + c1;
	return d0;
}

/*	Return approximately a * b - 1 given |a|, |b| < 2**970 and a * b is
	very near 1.
*/
static inline double Mul121Special(double a, double2 b)
{
	static const double c = 0x1p27 + 1;

	double
		ap = a * c,
		bp = b.d0 * c,
		a0 = (a - ap) + ap, b0 = (b.d0 - bp) + bp,
		a1 = a - a0, b1 = b.d0 - b0,
		m1 = a0 * b0 - 1 + a0 * b1 + a1 * b0 + a1 * b1 + a * b.d1;
	return m1;
}

// Return a * b with relative error below 2**-102 given |a|, |b| < 2**970.
static inline double2 Mul222(double2 a, double2 b)
{
	static const double c = 0x1p27 + 1;

	double
		ap = a.d0 * c,
		bp = b.d0 * c,
		a0 = (a.d0 - ap) + ap, b0 = (b.d0 - bp) + bp,
		a1 = a.d0 - a0, b1 = b.d0 - b0,
		m0 = a.d0 * b.d0,
		m1 = a0 * b0 - m0 + a0 * b1 + a1 * b0 + a1 * b1 + (a.d0 * b.d1 + a.d1 * b.d0),
		d0 = m0 + m1,
		d1 = m0 - d0 + m1;
	return (double2){d0, d1};
}

// Return arcsine(x) given |x| <= .5, with the same properties as asin.
static double Center(double x)
{
	if (-0x1.7137449123ef5p-26 <= x && x <= +0x1.7137449123ef5p-26)
		return -0x1p-1022 < x && x < +0x1p-1022
				   // Generate underflow and inexact and return x.
				   ? x - x * x
				   // Generate inexact and return x.
				   : x * (Tiny + 1);

	static const double p03 = 0.1666666666666558995379880;
	static const double p05 = 0.0750000000029696112392353;
	static const double p07 = 0.0446428568582815922683933;
	static const double p09 = 0.0303819580081956423799529;
	static const double p11 = 0.0223717830666671020710108;
	static const double p13 = 0.0173593516996479249428647;
	static const double p15 = 0.0138885410156894774969889;
	static const double p17 = 0.0121483892822292648695383;
	static const double p19 = 0.0066153165197009078340075;
	static const double p21 = 0.0192942786775238654913582;
	static const double p23 = -0.0158620440988475212803145;
	static const double p25 = 0.0316658385792867081040808;

	// Square x.
	double x2 = x * x;

	return ((((((((((((
						  +p25) *
						  x2 +
					  p23) *
						 x2 +
					 p21) *
						x2 +
					p19) *
					   x2 +
				   p17) *
					  x2 +
				  p15) *
					 x2 +
				 p13) *
					x2 +
				p11) *
				   x2 +
			   p09) *
				  x2 +
			  p07) *
				 x2 +
			 p05) *
				x2 +
			p03) *
			   x2 * x +
		   x;
}

// Return arcsine(x) given .5 < x, with the same properties as asin.
static double Tail(double x)
{
	if (1 <= x)
		return 1 == x
				   // If x is 1, generate inexact and return Pi/2 rounded down.
				   ? 0x3.243f6a8885a308d313198a2e03707344ap-1 + Tiny
				   // If x is outside the domain, generate invalid and return NaN.
				   : Infinity - Infinity;

	static const double p01 = 0.2145993335526539017488949;
	static const double p02 = -0.0890259194305537131666744;
	static const double p03 = 0.0506659694457588602631748;
	static const double p04 = -0.0331919619444009606270380;
	static const double p05 = 0.0229883479552557203133368;
	static const double p06 = -0.0156746038587246716524035;
	static const double p07 = 0.0097868293573384001221447;
	static const double p08 = -0.0052049731575223952626203;
	static const double p09 = 0.0021912255981979442677477;
	static const double p10 = -0.0006702485124770180942917;
	static const double p11 = 0.0001307564187657962919394;
	static const double p12 = -0.0000121189820098929624806;

	double polynomial = ((((((((((((
									   +p12) *
									   x +
								   p11) *
									  x +
								  p10) *
									 x +
								 p09) *
									x +
								p08) *
								   x +
							   p07) *
								  x +
							  p06) *
								 x +
							 p05) *
								x +
							p04) *
							   x +
						   p03) *
							  x +
						  p02) *
							 x +
						 p01) *
						x;

#if UseLongDouble
	static const long double HalfPi = 0x3.243f6a8885a308d313198a2ep-1L;
	static const long double p00 = -1.5707961988153774692344105L;

	return HalfPi + sqrtl(1 - x) * (polynomial + p00);
#else  // #if UseLongDouble
	static const double2
		HalfPi = {0x1.921fb54442d18p+0, 0x1.1a62633145c07p-54},
		p00 = {-0x1.921fb31e97d96p0, +0x1.eab77149ad27cp-54};

	// Estimate 1 / sqrt(1-x).
	double e = 1 / sqrt(1 - x);

	double2 ex = Mul112(e, 1 - x);	   // e * x.
	double e2x = Mul121Special(e, ex); // e**2 * x - 1.

	// Set t0 to an improved approximation of sqrt(1-x) with Newton-Raphson.
	double2 t0 = Add212RightSmaller(ex, ex.d0 * -.5 * e2x);

	// Calculate pi/2 + sqrt(1-x) * p(x).
	return Add221RightSmaller(HalfPi, Mul222(
										  t0,
										  Add212RightSmaller(p00, polynomial)));
#endif // #if UseLongDouble
}

// See documentation above.
double asin(double x)
{
	if (x < -.5)
		return -Tail(-x);
	else if (x <= .5)
		return Center(x);
	else
		return +Tail(+x);
}

float asinf(float x)
{
	return (float)asin((double)x);
}
