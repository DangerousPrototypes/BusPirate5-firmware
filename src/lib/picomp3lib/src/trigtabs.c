/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: RCSL 1.0/RPSL 1.0 
 *  
 * Portions Copyright (c) 1995-2002 RealNetworks, Inc. All Rights Reserved. 
 *      
 * The contents of this file, and the files included with this file, are 
 * subject to the current version of the RealNetworks Public Source License 
 * Version 1.0 (the "RPSL") available at 
 * http://www.helixcommunity.org/content/rpsl unless you have licensed 
 * the file under the RealNetworks Community Source License Version 1.0 
 * (the "RCSL") available at http://www.helixcommunity.org/content/rcsl, 
 * in which case the RCSL will apply. You may also obtain the license terms 
 * directly from RealNetworks.  You may not use this file except in 
 * compliance with the RPSL or, if you have a valid RCSL with RealNetworks 
 * applicable to this file, the RCSL.  Please see the applicable RPSL or 
 * RCSL for the rights, obligations and limitations governing use of the 
 * contents of the file.  
 *  
 * This file is part of the Helix DNA Technology. RealNetworks is the 
 * developer of the Original Code and owns the copyrights in the portions 
 * it created. 
 *  
 * This file, and the files included with this file, is distributed and made 
 * available on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND REALNETWORKS HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS 
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * 
 * Technology Compatibility Kit Test Suite(s) Location: 
 *    http://www.helixcommunity.org/content/tck 
 * 
 * Contributor(s): 
 *  
 * ***** END LICENSE BLOCK ***** */ 

/**************************************************************************************
 * Fixed-point MP3 decoder
 * Jon Recker (jrecker@real.com), Ken Cooke (kenc@real.com)
 * June 2003
 *
 * trigtabs.c - global ROM tables for pre-calculated trig coefficients
 **************************************************************************************/

// constants in RAM are not significantly faster

#include "coder.h"
#include <stdint.h>

/* post-IMDCT window, win[blockType][i]
 * format = Q31
 * Fused sin window with final stage of IMDCT
 * includes 1/sqrt(2) scaling, since we scale by sqrt(2) in dequant in order
 *   for fast IMDCT36 to be usable
 * 
 * 	for(i=0;i<9;i++)   win[0][i] = sin(pi/36 *(i+0.5));
 * 	for(i=9;i<36;i++)  win[0][i] = -sin(pi/36 *(i+0.5));
 * 
 * 	for(i=0;i<9;i++)   win[1][i] = sin(pi/36 *(i+0.5));
 * 	for(i=9;i<18;i++)  win[1][i] = -sin(pi/36 *(i+0.5));
 * 	for(i=18;i<24;i++) win[1][i] = -1;
 * 	for(i=24;i<30;i++) win[1][i] = -sin(pi/12 *(i+0.5-18));
 * 	for(i=30;i<36;i++) win[1][i] = 0;
 * 
 * 	for(i=0;i<6;i++)   win[3][i] = 0;
 * 	for(i=6;i<9;i++)   win[3][i] = sin(pi/12 *(i+0.5-6));
 * 	for(i=9;i<12;i++)  win[3][i] = -sin(pi/12 *(i+0.5-6));
 * 	for(i=12;i<18;i++) win[3][i] = -1;
 * 	for(i=18;i<36;i++) win[3][i] = -sin(pi/36*(i+0.5));
 * 
 * 	for(i=0;i<3;i++)   win[2][i] = sin(pi/12*(i+0.5));
 * 	for(i=3;i<12;i++)  win[2][i] = -sin(pi/12*(i+0.5));
 * 	for(i=12;i<36;i++) win[2][i] = 0;
 * 
 * 	for (i = 0; i < 4; i++) {
 * 		if (i == 2) {
 * 			win[i][8]  *= cos(pi/12 * (0+0.5));
 * 			win[i][9]  *= cos(pi/12 * (0+0.5));
 * 			win[i][7]  *= cos(pi/12 * (1+0.5));
 * 			win[i][10] *= cos(pi/12 * (1+0.5));
 * 			win[i][6]  *= cos(pi/12 * (2+0.5));
 * 			win[i][11] *= cos(pi/12 * (2+0.5));
 * 			win[i][0]  *= cos(pi/12 * (3+0.5));
 * 			win[i][5]  *= cos(pi/12 * (3+0.5));
 * 			win[i][1]  *= cos(pi/12 * (4+0.5));
 * 			win[i][4]  *= cos(pi/12 * (4+0.5));
 * 			win[i][2]  *= cos(pi/12 * (5+0.5));
 * 			win[i][3]  *= cos(pi/12 * (5+0.5));
 * 		} else {
 * 			for (j = 0; j < 9; j++) {
 * 				win[i][8-j] *= cos(pi/36 * (17-j+0.5));
 * 				win[i][9+j] *= cos(pi/36 * (17-j+0.5));
 * 			}
 * 			for (j = 0; j < 9; j++) {
 * 				win[i][18+8-j] *= cos(pi/36 * (j+0.5));
 * 				win[i][18+9+j] *= cos(pi/36 * (j+0.5));
 * 			}
 * 		}
 * 	}
 *	for (i = 0; i < 4; i++)
 *		for (j = 0; j < 36; j++)
 * 			win[i][j] *= 1.0 / sqrt(2);
 */
const int imdctWin[4][36] = {
	{
	(int32_t)0x02aace8b, (int32_t)0x07311c28, (int32_t)0x0a868fec, (int32_t)0x0c913b52, (int32_t)0x0d413ccd, (int32_t)0x0c913b52, (int32_t)0x0a868fec, (int32_t)0x07311c28, 
	(int32_t)0x02aace8b, (int32_t)0xfd16d8dd, (int32_t)0xf6a09e66, (int32_t)0xef7a6275, (int32_t)0xe7dbc161, (int32_t)0xe0000000, (int32_t)0xd8243e9f, (int32_t)0xd0859d8b, 
	(int32_t)0xc95f619a, (int32_t)0xc2e92723, (int32_t)0xbd553175, (int32_t)0xb8cee3d8, (int32_t)0xb5797014, (int32_t)0xb36ec4ae, (int32_t)0xb2bec333, (int32_t)0xb36ec4ae, 
	(int32_t)0xb5797014, (int32_t)0xb8cee3d8, (int32_t)0xbd553175, (int32_t)0xc2e92723, (int32_t)0xc95f619a, (int32_t)0xd0859d8b, (int32_t)0xd8243e9f, (int32_t)0xe0000000, 
	(int32_t)0xe7dbc161, (int32_t)0xef7a6275, (int32_t)0xf6a09e66, (int32_t)0xfd16d8dd, 
	},
	{
	(int32_t)0x02aace8b, (int32_t)0x07311c28, (int32_t)0x0a868fec, (int32_t)0x0c913b52, (int32_t)0x0d413ccd, (int32_t)0x0c913b52, (int32_t)0x0a868fec, (int32_t)0x07311c28, 
	(int32_t)0x02aace8b, (int32_t)0xfd16d8dd, (int32_t)0xf6a09e66, (int32_t)0xef7a6275, (int32_t)0xe7dbc161, (int32_t)0xe0000000, (int32_t)0xd8243e9f, (int32_t)0xd0859d8b, 
	(int32_t)0xc95f619a, (int32_t)0xc2e92723, (int32_t)0xbd44ef14, (int32_t)0xb831a052, (int32_t)0xb3aa3837, (int32_t)0xafb789a4, (int32_t)0xac6145bb, (int32_t)0xa9adecdc, 
	(int32_t)0xa864491f, (int32_t)0xad1868f0, (int32_t)0xb8431f49, (int32_t)0xc8f42236, (int32_t)0xdda8e6b1, (int32_t)0xf47755dc, (int32_t)0x00000000, (int32_t)0x00000000, 
	(int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, 
	},
	{
	(int32_t)0x07311c28, (int32_t)0x0d413ccd, (int32_t)0x07311c28, (int32_t)0xf6a09e66, (int32_t)0xe0000000, (int32_t)0xc95f619a, (int32_t)0xb8cee3d8, (int32_t)0xb2bec333, 
	(int32_t)0xb8cee3d8, (int32_t)0xc95f619a, (int32_t)0xe0000000, (int32_t)0xf6a09e66, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, 
	(int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, 
	(int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, 
	(int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, 
	},
	{
	(int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x00000000, (int32_t)0x028e9709, (int32_t)0x04855ec0, 
	(int32_t)0x026743a1, (int32_t)0xfcde2c10, (int32_t)0xf515dc82, (int32_t)0xec93e53b, (int32_t)0xe4c880f8, (int32_t)0xdd5d0b08, (int32_t)0xd63510b7, (int32_t)0xcf5e834a, 
	(int32_t)0xc8e6b562, (int32_t)0xc2da4105, (int32_t)0xbd553175, (int32_t)0xb8cee3d8, (int32_t)0xb5797014, (int32_t)0xb36ec4ae, (int32_t)0xb2bec333, (int32_t)0xb36ec4ae, 
	(int32_t)0xb5797014, (int32_t)0xb8cee3d8, (int32_t)0xbd553175, (int32_t)0xc2e92723, (int32_t)0xc95f619a, (int32_t)0xd0859d8b, (int32_t)0xd8243e9f, (int32_t)0xe0000000, 
	(int32_t)0xe7dbc161, (int32_t)0xef7a6275, (int32_t)0xf6a09e66, (int32_t)0xfd16d8dd, 
	},
};

/* indexing = [mid-side off/on][intensity scale factor]
 * format = Q30, range = [0.0, 1.414]
 *
 * mid-side off: 
 *   ISFMpeg1[0][i] = tan(i*pi/12) / [1 + tan(i*pi/12)]  (left scalefactor)
 *                  =      1       / [1 + tan(i*pi/12)]  (right scalefactor)
 *
 * mid-side on: 
 *   ISFMpeg1[1][i] = sqrt(2) * ISFMpeg1[0][i]
 *
 * output L = ISFMpeg1[midSide][isf][0] * input L
 * output R = ISFMpeg1[midSide][isf][1] * input L
 *
 * obviously left scalefactor + right scalefactor = 1 (m-s off) or sqrt(2) (m-s on)
 *   so just store left and calculate right as 1 - left 
 *  (can derive as right = ISFMpeg1[x][6] - left)
 *
 * if mid-side enabled, multiply joint stereo scale factors by sqrt(2)
 *   - we scaled whole spectrum by 1/sqrt(2) in Dequant for the M+S/sqrt(2) in MidSideProc
 *   - but the joint stereo part of the spectrum doesn't need this, so we have to undo it
 *
 * if scale factor is and illegal intensity position, this becomes a passthrough
 *   - gain = [1, 0] if mid-side off, since L is coded directly and R = 0 in this region
 *   - gain = [1, 1] if mid-side on, since L = (M+S)/sqrt(2), R = (M-S)/sqrt(2)
 *     - and since S = 0 in the joint stereo region (above NZB right) then L = R = M * 1.0
 */
const int ISFMpeg1[2][7] = {
	{(int32_t)0x00000000, (int32_t)0x0d8658ba, (int32_t)0x176cf5d0, (int32_t)0x20000000, (int32_t)0x28930a2f, (int32_t)0x3279a745, (int32_t)0x40000000},
	{(int32_t)0x00000000, (int32_t)0x13207f5c, (int32_t)0x2120fb83, (int32_t)0x2d413ccc, (int32_t)0x39617e16, (int32_t)0x4761fa3d, (int32_t)0x5a827999}
};

/* indexing = [intensity scale on/off][mid-side off/on][intensity scale factor]
 * format = Q30, range = [0.0, 1.414]
 *
 * if (isf == 0)                 kl = 1.0             kr = 1.0
 * else if (isf & (int32_t)0x01 == (int32_t)0x01)  kl = i0^((isf+1)/2), kr = 1.0
 * else if (isf & (int32_t)0x01 == (int32_t)0x00)  kl = 1.0,            kr = i0^(isf/2)
 *
 * if (intensityScale == 1)      i0 = 1/sqrt(2)       = (int32_t)0x2d413ccc (Q30)
 * else                          i0 = 1/sqrt(sqrt(2)) = (int32_t)0x35d13f32 (Q30)
 *
 * see comments for ISFMpeg1 (just above) regarding scaling, sqrt(2), etc.
 *
 * compress the MPEG2 table using the obvious identities above...
 * for isf = [0, 1, 2, ... 30], let sf = table[(isf+1) >> 1] 
 *   - if isf odd,  L = sf*L,     R = tab[0]*R
 *   - if isf even, L = tab[0]*L, R = sf*R
 */
const int ISFMpeg2[2][2][16] = {
{
	{
		/* intensityScale off, mid-side off */
		(int32_t)0x40000000, (int32_t)0x35d13f32, (int32_t)0x2d413ccc, (int32_t)0x260dfc14, (int32_t)0x1fffffff, (int32_t)0x1ae89f99, (int32_t)0x16a09e66, (int32_t)0x1306fe0a, 
		(int32_t)0x0fffffff, (int32_t)0x0d744fcc, (int32_t)0x0b504f33, (int32_t)0x09837f05, (int32_t)0x07ffffff, (int32_t)0x06ba27e6, (int32_t)0x05a82799, (int32_t)0x04c1bf82,
	},
	{
		/* intensityScale off, mid-side on */
		(int32_t)0x5a827999, (int32_t)0x4c1bf827, (int32_t)0x3fffffff, (int32_t)0x35d13f32, (int32_t)0x2d413ccc, (int32_t)0x260dfc13, (int32_t)0x1fffffff, (int32_t)0x1ae89f99, 
		(int32_t)0x16a09e66, (int32_t)0x1306fe09, (int32_t)0x0fffffff, (int32_t)0x0d744fcc, (int32_t)0x0b504f33, (int32_t)0x09837f04, (int32_t)0x07ffffff, (int32_t)0x06ba27e6, 
	},
},
{
	{
		/* intensityScale on, mid-side off */
		(int32_t)0x40000000, (int32_t)0x2d413ccc, (int32_t)0x20000000, (int32_t)0x16a09e66, (int32_t)0x10000000, (int32_t)0x0b504f33, (int32_t)0x08000000, (int32_t)0x05a82799, 
		(int32_t)0x04000000, (int32_t)0x02d413cc, (int32_t)0x02000000, (int32_t)0x016a09e6, (int32_t)0x01000000, (int32_t)0x00b504f3, (int32_t)0x00800000, (int32_t)0x005a8279, 
	},
		/* intensityScale on, mid-side on */
	{
		(int32_t)0x5a827999, (int32_t)0x3fffffff, (int32_t)0x2d413ccc, (int32_t)0x1fffffff, (int32_t)0x16a09e66, (int32_t)0x0fffffff, (int32_t)0x0b504f33, (int32_t)0x07ffffff, 
		(int32_t)0x05a82799, (int32_t)0x03ffffff, (int32_t)0x02d413cc, (int32_t)0x01ffffff, (int32_t)0x016a09e6, (int32_t)0x00ffffff, (int32_t)0x00b504f3, (int32_t)0x007fffff, 
	}
}
};

/* indexing = [intensity scale on/off][left/right]
 * format = Q30, range = [0.0, 1.414]
 *
 * illegal intensity position scalefactors (see comments on ISFMpeg1)
 */
const int ISFIIP[2][2] = {
	{(int32_t)0x40000000, (int32_t)0x00000000}, /* mid-side off */
	{(int32_t)0x40000000, (int32_t)0x40000000}, /* mid-side on */
};

const unsigned char uniqueIDTab[8] = {(int32_t)0x5f, (int32_t)0x4b, (int32_t)0x43, (int32_t)0x5f, (int32_t)0x5f, (int32_t)0x4a, (int32_t)0x52, (int32_t)0x5f};

/* anti-alias coefficients - see spec Annex B, table 3-B.9 
 *   csa[0][i] = CSi, csa[1][i] = CAi
 * format = Q31
 */
const int csa[8][2] = {
	{(int32_t)0x6dc253f0, (int32_t)0xbe2500aa}, 
	{(int32_t)0x70dcebe4, (int32_t)0xc39e4949},
	{(int32_t)0x798d6e73, (int32_t)0xd7e33f4a},
	{(int32_t)0x7ddd40a7, (int32_t)0xe8b71176},
	{(int32_t)0x7f6d20b7, (int32_t)0xf3e4fe2f},
	{(int32_t)0x7fe47e40, (int32_t)0xfac1a3c7}, 
	{(int32_t)0x7ffcb263, (int32_t)0xfe2ebdc6}, 
	{(int32_t)0x7fffc694, (int32_t)0xff86c25d}, 
};

/* format = Q30, range = [0.0981, 1.9976]
 *
 * n = 16;
 * k = 0;
 * for(i=0; i<5; i++, n=n/2) {
 *   for(p=0; p<n; p++, k++) {
 *     t = (PI / (4*n)) * (2*p + 1);
 *     coef32[k] = 2.0 * cos(t);
 *   }
 * }
 * coef32[30] *= 0.5;	/ *** for initial back butterfly (i.e. two-point DCT) *** /
 */
const int coef32[31] = {
	(int32_t)0x7fd8878d, (int32_t)0x7e9d55fc, (int32_t)0x7c29fbee, (int32_t)0x78848413, (int32_t)0x73b5ebd0, (int32_t)0x6dca0d14, (int32_t)0x66cf811f, (int32_t)0x5ed77c89, 
	(int32_t)0x55f5a4d2, (int32_t)0x4c3fdff3, (int32_t)0x41ce1e64, (int32_t)0x36ba2013, (int32_t)0x2b1f34eb, (int32_t)0x1f19f97b, (int32_t)0x12c8106e, (int32_t)0x0647d97c, 
	(int32_t)0x7f62368f, (int32_t)0x7a7d055b, (int32_t)0x70e2cbc6, (int32_t)0x62f201ac, (int32_t)0x5133cc94, (int32_t)0x3c56ba70, (int32_t)0x25280c5d, (int32_t)0x0c8bd35e, 
	(int32_t)0x7d8a5f3f, (int32_t)0x6a6d98a4, (int32_t)0x471cece6, (int32_t)0x18f8b83c, (int32_t)0x7641af3c, (int32_t)0x30fbc54d, (int32_t)0x2d413ccc, 
};

/* format = Q30, right shifted by 12 (sign bits only in top 12 - undo this when rounding to short)
 *   this is to enable early-terminating multiplies on ARM
 * range = [-1.144287109, 1.144989014]
 * max gain of filter (per output sample) ~= 2.731
 *
 * new (properly sign-flipped) values 
 *  - these actually are correct to 32 bits, (floating-pt coefficients in spec
 *      chosen such that only ~20 bits are required)
 *
 * Reordering - see table 3-B.3 in spec (appendix B)
 *
 * polyCoef[i] = 
 *   D[ 0, 32, 64, ... 480],   i = [  0, 15]
 *   D[ 1, 33, 65, ... 481],   i = [ 16, 31]
 *   D[ 2, 34, 66, ... 482],   i = [ 32, 47]
 *     ...
 *   D[15, 47, 79, ... 495],   i = [240,255]
 *
 * also exploits symmetry: D[i] = -D[512 - i], for i = [1, 255]
 * 
 * polyCoef[256, 257, ... 263] are for special case of sample 16 (out of 0)
 *   see PolyphaseStereo() and PolyphaseMono()
 */
const int polyCoef[264] = {
	/* shuffled vs. original from 0, 1, ... 15 to 0, 15, 2, 13, ... 14, 1 */
	(int32_t)0x00000000, (int32_t)0x00000074, (int32_t)0x00000354, (int32_t)0x0000072c, (int32_t)0x00001fd4, (int32_t)0x00005084, (int32_t)0x000066b8, (int32_t)0x000249c4,
	(int32_t)0x00049478, (int32_t)0xfffdb63c, (int32_t)0x000066b8, (int32_t)0xffffaf7c, (int32_t)0x00001fd4, (int32_t)0xfffff8d4, (int32_t)0x00000354, (int32_t)0xffffff8c,
	(int32_t)0xfffffffc, (int32_t)0x00000068, (int32_t)0x00000368, (int32_t)0x00000644, (int32_t)0x00001f40, (int32_t)0x00004ad0, (int32_t)0x00005d1c, (int32_t)0x00022ce0,
	(int32_t)0x000493c0, (int32_t)0xfffd9960, (int32_t)0x00006f78, (int32_t)0xffffa9cc, (int32_t)0x0000203c, (int32_t)0xfffff7e4, (int32_t)0x00000340, (int32_t)0xffffff84,
	(int32_t)0xfffffffc, (int32_t)0x00000060, (int32_t)0x00000378, (int32_t)0x0000056c, (int32_t)0x00001e80, (int32_t)0x00004524, (int32_t)0x000052a0, (int32_t)0x00020ffc,
	(int32_t)0x000491a0, (int32_t)0xfffd7ca0, (int32_t)0x00007760, (int32_t)0xffffa424, (int32_t)0x00002080, (int32_t)0xfffff6ec, (int32_t)0x00000328, (int32_t)0xffffff74,
	(int32_t)0xfffffffc, (int32_t)0x00000054, (int32_t)0x00000384, (int32_t)0x00000498, (int32_t)0x00001d94, (int32_t)0x00003f7c, (int32_t)0x00004744, (int32_t)0x0001f32c,
	(int32_t)0x00048e18, (int32_t)0xfffd6008, (int32_t)0x00007e70, (int32_t)0xffff9e8c, (int32_t)0x0000209c, (int32_t)0xfffff5ec, (int32_t)0x00000310, (int32_t)0xffffff68,
	(int32_t)0xfffffffc, (int32_t)0x0000004c, (int32_t)0x0000038c, (int32_t)0x000003d0, (int32_t)0x00001c78, (int32_t)0x000039e4, (int32_t)0x00003b00, (int32_t)0x0001d680,
	(int32_t)0x00048924, (int32_t)0xfffd43ac, (int32_t)0x000084b0, (int32_t)0xffff990c, (int32_t)0x00002094, (int32_t)0xfffff4e4, (int32_t)0x000002f8, (int32_t)0xffffff5c,
	(int32_t)0xfffffffc, (int32_t)0x00000044, (int32_t)0x00000390, (int32_t)0x00000314, (int32_t)0x00001b2c, (int32_t)0x0000345c, (int32_t)0x00002ddc, (int32_t)0x0001ba04,
	(int32_t)0x000482d0, (int32_t)0xfffd279c, (int32_t)0x00008a20, (int32_t)0xffff93a4, (int32_t)0x0000206c, (int32_t)0xfffff3d4, (int32_t)0x000002dc, (int32_t)0xffffff4c,
	(int32_t)0xfffffffc, (int32_t)0x00000040, (int32_t)0x00000390, (int32_t)0x00000264, (int32_t)0x000019b0, (int32_t)0x00002ef0, (int32_t)0x00001fd4, (int32_t)0x00019dc8,
	(int32_t)0x00047b1c, (int32_t)0xfffd0be8, (int32_t)0x00008ecc, (int32_t)0xffff8e64, (int32_t)0x00002024, (int32_t)0xfffff2c0, (int32_t)0x000002c0, (int32_t)0xffffff3c,
	(int32_t)0xfffffff8, (int32_t)0x00000038, (int32_t)0x0000038c, (int32_t)0x000001bc, (int32_t)0x000017fc, (int32_t)0x0000299c, (int32_t)0x000010e8, (int32_t)0x000181d8,
	(int32_t)0x0004720c, (int32_t)0xfffcf09c, (int32_t)0x000092b4, (int32_t)0xffff894c, (int32_t)0x00001fc0, (int32_t)0xfffff1a4, (int32_t)0x000002a4, (int32_t)0xffffff2c,
	(int32_t)0xfffffff8, (int32_t)0x00000034, (int32_t)0x00000380, (int32_t)0x00000120, (int32_t)0x00001618, (int32_t)0x00002468, (int32_t)0x00000118, (int32_t)0x00016644,
	(int32_t)0x000467a4, (int32_t)0xfffcd5cc, (int32_t)0x000095e0, (int32_t)0xffff8468, (int32_t)0x00001f44, (int32_t)0xfffff084, (int32_t)0x00000284, (int32_t)0xffffff18,
	(int32_t)0xfffffff8, (int32_t)0x0000002c, (int32_t)0x00000374, (int32_t)0x00000090, (int32_t)0x00001400, (int32_t)0x00001f58, (int32_t)0xfffff068, (int32_t)0x00014b14,
	(int32_t)0x00045bf0, (int32_t)0xfffcbb88, (int32_t)0x00009858, (int32_t)0xffff7fbc, (int32_t)0x00001ea8, (int32_t)0xffffef60, (int32_t)0x00000268, (int32_t)0xffffff04,
	(int32_t)0xfffffff8, (int32_t)0x00000028, (int32_t)0x0000035c, (int32_t)0x00000008, (int32_t)0x000011ac, (int32_t)0x00001a70, (int32_t)0xffffded8, (int32_t)0x00013058,
	(int32_t)0x00044ef8, (int32_t)0xfffca1d8, (int32_t)0x00009a1c, (int32_t)0xffff7b54, (int32_t)0x00001dfc, (int32_t)0xffffee3c, (int32_t)0x0000024c, (int32_t)0xfffffef0,
	(int32_t)0xfffffff4, (int32_t)0x00000024, (int32_t)0x00000340, (int32_t)0xffffff8c, (int32_t)0x00000f28, (int32_t)0x000015b0, (int32_t)0xffffcc70, (int32_t)0x0001161c,
	(int32_t)0x000440bc, (int32_t)0xfffc88d8, (int32_t)0x00009b3c, (int32_t)0xffff7734, (int32_t)0x00001d38, (int32_t)0xffffed18, (int32_t)0x0000022c, (int32_t)0xfffffedc,
	(int32_t)0xfffffff4, (int32_t)0x00000020, (int32_t)0x00000320, (int32_t)0xffffff1c, (int32_t)0x00000c68, (int32_t)0x0000111c, (int32_t)0xffffb92c, (int32_t)0x0000fc6c,
	(int32_t)0x00043150, (int32_t)0xfffc708c, (int32_t)0x00009bb8, (int32_t)0xffff7368, (int32_t)0x00001c64, (int32_t)0xffffebf4, (int32_t)0x00000210, (int32_t)0xfffffec4,
	(int32_t)0xfffffff0, (int32_t)0x0000001c, (int32_t)0x000002f4, (int32_t)0xfffffeb4, (int32_t)0x00000974, (int32_t)0x00000cb8, (int32_t)0xffffa518, (int32_t)0x0000e350,
	(int32_t)0x000420b4, (int32_t)0xfffc5908, (int32_t)0x00009b9c, (int32_t)0xffff6ff4, (int32_t)0x00001b7c, (int32_t)0xffffead0, (int32_t)0x000001f4, (int32_t)0xfffffeac,
	(int32_t)0xfffffff0, (int32_t)0x0000001c, (int32_t)0x000002c4, (int32_t)0xfffffe58, (int32_t)0x00000648, (int32_t)0x00000884, (int32_t)0xffff9038, (int32_t)0x0000cad0,
	(int32_t)0x00040ef8, (int32_t)0xfffc425c, (int32_t)0x00009af0, (int32_t)0xffff6ce0, (int32_t)0x00001a88, (int32_t)0xffffe9b0, (int32_t)0x000001d4, (int32_t)0xfffffe94,
	(int32_t)0xffffffec, (int32_t)0x00000018, (int32_t)0x0000028c, (int32_t)0xfffffe04, (int32_t)0x000002e4, (int32_t)0x00000480, (int32_t)0xffff7a90, (int32_t)0x0000b2fc,
	(int32_t)0x0003fc28, (int32_t)0xfffc2c90, (int32_t)0x000099b8, (int32_t)0xffff6a3c, (int32_t)0x00001988, (int32_t)0xffffe898, (int32_t)0x000001bc, (int32_t)0xfffffe7c,
	(int32_t)0x000001a0, (int32_t)0x0000187c, (int32_t)0x000097fc, (int32_t)0x0003e84c, (int32_t)0xffff6424, (int32_t)0xffffff4c, (int32_t)0x00000248, (int32_t)0xffffffec, 
};

