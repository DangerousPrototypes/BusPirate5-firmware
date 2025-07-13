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
 * buffers.c - allocation and freeing of internal MP3 decoder buffers
 *
 * All memory allocation for the codec is done in this file, so if you don't want 
 *  to use other the default system malloc() and free() for heap management this is 
 *  the only file you'll need to change.
 **************************************************************************************/

#include "coder.h"

#ifndef _WIN32
#ifdef DEMO_HELIX_FOOTPRINT
#include "dv_debug_usart.h"
#endif
#endif

/**************************************************************************************
 * Function:    ClearBuffer
 *
 * Description: fill buffer with 0's
 *
 * Inputs:      pointer to buffer
 *              number of bytes to fill with 0
 *
 * Outputs:     cleared buffer
 *
 * Return:      none
 *
 * Notes:       slow, platform-independent equivalent to memset(buf, 0, nBytes)
 **************************************************************************************/
#ifndef MPDEC_ALLOCATOR
static void ClearBuffer(void *buf, int nBytes)
{
	int i;
	unsigned char *cbuf = (unsigned char *)buf;

	for (i = 0; i < nBytes; i++)
		cbuf[i] = 0;

	return;
}
#endif

/**************************************************************************************
 * Function:    AllocateBuffers
 *
 * Description: allocate all the memory needed for the MP3 decoder
 *
 * Inputs:      none
 *
 * Outputs:     none
 *
 * Return:      pointer to MP3DecInfo structure (initialized with pointers to all 
 *                the internal buffers needed for decoding, all other members of 
 *                MP3DecInfo structure set to 0)
 *
 * Notes:       if one or more mallocs fail, function frees any buffers already
 *                allocated before returning
 **************************************************************************************/
MP3DecInfo *AllocateBuffers(void)
{
	MP3DecInfo *mp3DecInfo;
#ifdef MPDEC_ALLOCATOR
	FrameHeader *fh;
	SideInfo *si;
	ScaleFactorInfo *sfi;
	HuffmanInfo *hi;
	DequantInfo *di;
	IMDCTInfo *mi;
	SubbandInfo *sbi;

	mp3DecInfo = (MP3DecInfo *)MPDEC_ALLOCATOR(sizeof(MP3DecInfo));
	if (!mp3DecInfo)
	{
#ifndef _WIN32
#ifdef DEMO_HELIX_FOOTPRINT
	  sprintf(COPY_DEBUG_BUFFER,"mp3DecInfo size: %d\n", (int)sizeof(MP3DecInfo));
    DV_DEBUG_USART_Trace( COPY_DEBUG_BUFFER );
#endif
#endif
		return 0;
	}

	hi =  (HuffmanInfo *)     MPDEC_ALLOCATOR(sizeof(HuffmanInfo));
	sbi = (SubbandInfo *)     MPDEC_ALLOCATOR(sizeof(SubbandInfo));
	mi =  (IMDCTInfo *)       MPDEC_ALLOCATOR(sizeof(IMDCTInfo));
	di =  (DequantInfo *)     MPDEC_ALLOCATOR(sizeof(DequantInfo));
	si =  (SideInfo *)        MPDEC_ALLOCATOR(sizeof(SideInfo));
	sfi = (ScaleFactorInfo *) MPDEC_ALLOCATOR(sizeof(ScaleFactorInfo));
	fh =  (FrameHeader *)     MPDEC_ALLOCATOR(sizeof(FrameHeader));

	if (!fh || !si || !sfi || !hi || !di || !mi || !sbi) {
#ifndef _WIN32
#ifdef DEMO_HELIX_FOOTPRINT
    sprintf(COPY_DEBUG_BUFFER,"mp3DecInfo:%d[%d] | fh:%d[%d] | si:%d[%d] \
      | sfi:%d[%d] | hi:%d[%d] | di:%d[%d] | mi:%d[%d] | sbi:%d[%d]\n",
      (int)mp3DecInfo, (int)sizeof(MP3DecInfo), (int)fh, (int)sizeof(FrameHeader),
      (int)si, (int)sizeof(SideInfo), (int)sfi, (int)sizeof(ScaleFactorInfo),
      (int)hi, (int)sizeof(HuffmanInfo), (int)di, (int)sizeof(DequantInfo),
      (int)mi, (int)sizeof(IMDCTInfo), (int)sbi, (int)sizeof(SubbandInfo) );
    DV_DEBUG_USART_Trace( COPY_DEBUG_BUFFER );
#endif
#endif
		FreeBuffers(mp3DecInfo);	// safe to call - only frees memory that was successfully allocated
		return 0;
	}
#else

	// Buffers:
	static char s_mp3DecInfo[sizeof(MP3DecInfo)];
	static char fh[sizeof(FrameHeader)];
	static char si[sizeof(SideInfo)];
	static char sfi[sizeof(ScaleFactorInfo)];
	static char hi[sizeof(HuffmanInfo)];
	static char di[sizeof(DequantInfo)];
	static char mi[sizeof(IMDCTInfo)];
	static char sbi[sizeof(SubbandInfo)];

	mp3DecInfo = (MP3DecInfo *)s_mp3DecInfo;
	ClearBuffer(mp3DecInfo, sizeof(MP3DecInfo));

	/* important to do this - DSP primitives assume a bunch of state variables are 0 on first use */
	ClearBuffer(fh,  sizeof(FrameHeader));
	ClearBuffer(si,  sizeof(SideInfo));
	ClearBuffer(sfi, sizeof(ScaleFactorInfo));
	ClearBuffer(hi,  sizeof(HuffmanInfo));
	ClearBuffer(di,  sizeof(DequantInfo));
	ClearBuffer(mi,  sizeof(IMDCTInfo));
	ClearBuffer(sbi, sizeof(SubbandInfo));

#endif

	mp3DecInfo->FrameHeaderPS =     (void *)fh;
	mp3DecInfo->SideInfoPS =        (void *)si;
	mp3DecInfo->ScaleFactorInfoPS = (void *)sfi;
	mp3DecInfo->HuffmanInfoPS =     (void *)hi;
	mp3DecInfo->DequantInfoPS =     (void *)di;
	mp3DecInfo->IMDCTInfoPS =       (void *)mi;
	mp3DecInfo->SubbandInfoPS =     (void *)sbi;

#ifndef _WIN32
#ifdef DEMO_HELIX_FOOTPRINT
	sprintf(COPY_DEBUG_BUFFER, "Total decoder malloc size: %d\n",
					(int)(sizeof(MP3DecInfo) + sizeof(FrameHeader) + sizeof(SideInfo)
					+ sizeof(ScaleFactorInfo) + sizeof(HuffmanInfo) + sizeof(DequantInfo)
					+ sizeof(IMDCTInfo) + sizeof(SubbandInfo)));
	DV_DEBUG_USART_Trace( COPY_DEBUG_BUFFER );
#endif
#endif

	return mp3DecInfo;
}

#ifdef MPDEC_FREE
#define SAFE_FREE(x)	{if (x)	MPDEC_FREE(x);	(x) = 0;}	/* helper macro */
#else
#define SAFE_FREE(x)    { (x) = 0; }
#endif

/**************************************************************************************
 * Function:    FreeBuffers
 *
 * Description: frees all the memory used by the MP3 decoder
 *
 * Inputs:      pointer to initialized MP3DecInfo structure
 *
 * Outputs:     none
 *
 * Return:      none
 *
 * Notes:       safe to call even if some buffers were not allocated (uses SAFE_FREE)
 **************************************************************************************/
void FreeBuffers(MP3DecInfo *mp3DecInfo)
{
	if (!mp3DecInfo)
		return;

	SAFE_FREE(mp3DecInfo->FrameHeaderPS);
	SAFE_FREE(mp3DecInfo->SideInfoPS);
	SAFE_FREE(mp3DecInfo->ScaleFactorInfoPS);
	SAFE_FREE(mp3DecInfo->HuffmanInfoPS);
	SAFE_FREE(mp3DecInfo->DequantInfoPS);
	SAFE_FREE(mp3DecInfo->IMDCTInfoPS);
	SAFE_FREE(mp3DecInfo->SubbandInfoPS);

	SAFE_FREE(mp3DecInfo);
}
