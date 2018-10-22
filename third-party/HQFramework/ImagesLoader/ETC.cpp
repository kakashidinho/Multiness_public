/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "ETC.h"
#include <string.h>
#include "ImgLoader.h"

/*----------etc pack code-----------*/

#define CLAMP(ll,x,ul) (((x)<(ll)) ? (ll) : (((x)>(ul)) ? (ul) : (x)))
#define GETBITS(source, size, startpos)  (( (source) >> ((startpos)-(size)+1) ) & ((1<<(size)) -1))
#define GETBITSHIGH(source, size, startpos)  (( (source) >> (((startpos)-32)-(size)+1) ) & ((1<<(size)) -1))
#define CHANNEL(img,width,x,y,index) img[3*(y*width+x)+index]
#define FLIP_EACH2BITS(byte) ((byte & 0x11) << 1 | (byte & 0x22) >> 1 | (byte & 0x44) << 1 | (byte & 0x88) >> 1)
#define FLIP4BITS_2HALFBYTE(byte) ((byte & 0x11) << 3 | (byte & 0x22) << 1 | (byte & 0x44) >> 1 | (byte & 0x88) >> 3)
#define FLIP2BITS_2HALFBYTE(byte) ((byte & 0x11) << 1 | (byte & 0x22) >> 1 | (byte & 0xcc))
#define SWAP(a,b,temp) temp=a; a=b ; b=temp;
#define SWAP_HALFBYTE(byte) ((byte & 0xf) << 4 | (byte & 0xf0) >> 4)
#define FLIP_ELE2BITS(byte) ((byte & 0x3) << 6 | (byte & 0xc) << 2 | (byte & 0x30) >> 2 | (byte & 0xc0 >> 6))
#define FLIP_ELE2BITS_HALFBYTE(byte) ((byte & 0x3) << 2 | (byte & 0xc) >> 2 | (byte & 0xf0))


typedef hqubyte8 uint8;

static hqint32 unscramble[4] = {2, 3, 1, 0};

static hqint32 compressParams[16][4] = {{-8, -2,  2, 8}, {-8, -2,  2, 8}, {-17, -5, 5, 17}, {-17, -5, 5, 17}, {-29, -9, 9, 29}, {-29, -9, 9, 29}, {-42, -13, 13, 42}, {-42, -13, 13, 42}, {-60, -18, 18, 60}, {-60, -18, 18, 60}, {-80, -24, 24, 80}, {-80, -24, 24, 80}, {-106, -33, 33, 106}, {-106, -33, 33, 106}, {-183, -47, 47, 183}, {-183, -47, 47, 183}};

static void DecompressBlockDiffFlip(hquint32 block_part1, hquint32 block_part2, uint8 *img,hqint32 width,hqint32 height,hqint32 startx,hqint32 starty, bool flipRGB)
{
	uint8 avg_color[3], enc_color1[3], enc_color2[3];
    hqbyte8 diff[3];
	hqint32 table;
	hqint32 index,shift;
	hqint32 r,g,b;
	hqint32 diffbit;
	hqint32 flipbit;
	hquint32 pixel_indices_MSB, pixel_indices_LSB;
	hqint32 x,y;
	hquint32 redChannel = flipRGB? 0 : 2;
	hquint32 blueChannel = 2 - redChannel;

	diffbit = (GETBITSHIGH(block_part1, 1, 33));
	flipbit = (GETBITSHIGH(block_part1, 1, 32));

    if( !diffbit )
	{

		// We have diffbit = 0.

		// First decode left part of block.
		avg_color[0]= GETBITSHIGH(block_part1, 4, 63);
		avg_color[1]= GETBITSHIGH(block_part1, 4, 55);
		avg_color[2]= GETBITSHIGH(block_part1, 4, 47);

		// Here, we should really multiply by 17 instead of 16. This can
		// be done by just copying the four lower bits to the upper ones
		// while keeping the lower bits.
		avg_color[0] |= (avg_color[0] <<4);
		avg_color[1] |= (avg_color[1] <<4);
		avg_color[2] |= (avg_color[2] <<4);

		table = GETBITSHIGH(block_part1, 3, 39) << 1;

		
			
		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift = 0;
			for(x=startx; x<startx+2; x++)
			{
				for(y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];
					
					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}


				}
			}
		}
		else
		{
			// We should flip
			shift = 0;
			for(x=startx; x<startx+4; x++)
			{
				for(y=starty; y<starty+2; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

 					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}
				}
				shift+=2;
			}
		}

		// Now decode other part of block. 
		avg_color[0]= GETBITSHIGH(block_part1, 4, 59);
		avg_color[1]= GETBITSHIGH(block_part1, 4, 51);
		avg_color[2]= GETBITSHIGH(block_part1, 4, 43);

		// Here, we should really multiply by 17 instead of 16. This can
		// be done by just copying the four lower bits to the upper ones
		// while keeping the lower bits.
		avg_color[0] |= (avg_color[0] <<4);
		avg_color[1] |= (avg_color[1] <<4);
		avg_color[2] |= (avg_color[2] <<4);

		table = GETBITSHIGH(block_part1, 3, 36) << 1;
		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift=8;
			for(x=startx+2; x<startx+4; x++)
			{
				for(y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

 					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}

				}
			}
		}
		else
		{
			// We should flip
			shift=2;
			for(x=startx; x<startx+4; x++)
			{
				for(y=starty+2; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

 					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}

				}
				shift += 2;
			}
		}

	}
	else
	{
		// We have diffbit = 1. 

//      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34  33  32 
//      ---------------------------------------------------------------------------------------------------
//     | base col1    | dcol 2 | base col1    | dcol 2 | base col 1   | dcol 2 | table  | table  |diff|flip|
//     | R1' (5 bits) | dR2    | G1' (5 bits) | dG2    | B1' (5 bits) | dB2    | cw 1   | cw 2   |bit |bit |
//      ---------------------------------------------------------------------------------------------------
// 
// 
//     c) bit layout in bits 31 through 0 (in both cases)
// 
//      31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3   2   1  0
//      --------------------------------------------------------------------------------------------------
//     |       most significant pixel index bits       |         least significant pixel index bits       |  
//     | p| o| n| m| l| k| j| i| h| g| f| e| d| c| b| a| p| o| n| m| l| k| j| i| h| g| f| e| d| c | b | a |
//      --------------------------------------------------------------------------------------------------      


		// First decode left part of block.
		enc_color1[0]= GETBITSHIGH(block_part1, 5, 63);
		enc_color1[1]= GETBITSHIGH(block_part1, 5, 55);
		enc_color1[2]= GETBITSHIGH(block_part1, 5, 47);


		// Expand from 5 to 8 bits
		avg_color[0] = (enc_color1[0] <<3) | (enc_color1[0] >> 2);
		avg_color[1] = (enc_color1[1] <<3) | (enc_color1[1] >> 2);
		avg_color[2] = (enc_color1[2] <<3) | (enc_color1[2] >> 2);


		table = GETBITSHIGH(block_part1, 3, 39) << 1;
			
		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift = 0;
			for(x=startx; x<startx+2; x++)
			{
				for(y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

 					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}


				}
			}
		}
		else
		{
			// We should flip
			shift = 0;
			for(x=startx; x<startx+4; x++)
			{
				for(y=starty; y<starty+2; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

 					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}
				}
				shift+=2;
			}
		}


		// Now decode right part of block. 


		diff[0]= GETBITSHIGH(block_part1, 3, 58);
		diff[1]= GETBITSHIGH(block_part1, 3, 50);
	    diff[2]= GETBITSHIGH(block_part1, 3, 42);

		enc_color2[0]= enc_color1[0] + diff[0];
		enc_color2[1]= enc_color1[1] + diff[1];
		enc_color2[2]= enc_color1[2] + diff[2];

		// Extend sign bit to entire byte. 
		diff[0] = (diff[0] << 5);
		diff[1] = (diff[1] << 5);
		diff[2] = (diff[2] << 5);
		diff[0] = diff[0] >> 5;
		diff[1] = diff[1] >> 5;
		diff[2] = diff[2] >> 5;

		//  Calculale second color
		enc_color2[0]= enc_color1[0] + diff[0];
		enc_color2[1]= enc_color1[1] + diff[1];
		enc_color2[2]= enc_color1[2] + diff[2];

		// Expand from 5 to 8 bits
		avg_color[0] = (enc_color2[0] <<3) | (enc_color2[0] >> 2);
		avg_color[1] = (enc_color2[1] <<3) | (enc_color2[1] >> 2);
		avg_color[2] = (enc_color2[2] <<3) | (enc_color2[2] >> 2);


		table = GETBITSHIGH(block_part1, 3, 36) << 1;
		pixel_indices_MSB = GETBITS(block_part2, 16, 31);
		pixel_indices_LSB = GETBITS(block_part2, 16, 15);

		if( (flipbit) == 0 )
		{
			// We should not flip
			shift=8;
			for(x=startx+2; x<startx+4; x++)
			{
				for(y=starty; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

 					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}

				}
			}
		}
		else
		{
			// We should flip
			shift=2;
			for(x=startx; x<startx+4; x++)
			{
				for(y=starty+2; y<starty+4; y++)
				{
					index  = ((pixel_indices_MSB >> shift) & 1) << 1;
					index |= ((pixel_indices_LSB >> shift) & 1);
					shift++;
					index=unscramble[index];

 					if (x < width && y < height)
					{
 						r=CHANNEL(img,width,x,y,redChannel)  =CLAMP(0,avg_color[0]+compressParams[table][index],255);
 						g=CHANNEL(img,width,x,y,1)=CLAMP(0,avg_color[1]+compressParams[table][index],255);
 						b=CHANNEL(img,width,x,y,blueChannel) =CLAMP(0,avg_color[2]+compressParams[table][index],255);
					}
				}
				shift += 2;
			}
		}
	}
}
static void Read_big_endian_4byte_word(const hqubyte8* bytes, unsigned int *pValue)
{
	hquint32 block;
	block = 0;

	block |= bytes[0];
	block = block << 8;
	block |= bytes[1];
	block = block << 8;
	block |= bytes[2];
	block = block << 8;
	block |= bytes[3];

	*pValue = block;
}

/*----------end etc pack code-----------*/
void FlipFullBlockETC1(hq_ubyte8 * pBlock)//flip vertical
{
//		31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3   2   1  0
//      --------------------------------------------------------------------------------------------------
//     |       most significant pixel index bits       |         least significant pixel index bits       |  
//     | p| o| n| m| l| k| j| i| h| g| f| e| d| c| b| a| p| o| n| m| l| k| j| i| h| g| f| e| d| c | b | a |
//      --------------------------------------------------------------------------------------------------

#if FOR_GLES_SPEC
/*
	
	|a   |e   |i   |m   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |b   |f   |j   |n   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |c   |g   |k   |o   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |d   |h   |l   |p   |
    |    |    |    |    |
*/
	
	for (int i = 4 ; i < 8 ; ++i)
		pBlock[i] = FLIP4BITS_2HALFBYTE(pBlock[i]);
#else
/*
flipbit = 0 :				flipbit = 1 :
	|a   |e   |i   |m   |		|a   |c   |e   |g   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |b   |f   |j   |n   |		|b   |d   |f   |h   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ----
    |c   |g   |k   |o   |		|i   |k   |m   |o   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |d   |h   |l   |p   |		|j   |l   |n   |p   |
    |    |    |    |    |		|    |    |    |    |
*/
	hqubyte8 flipbit = pBlock[3] & 0x1;
	if (flipbit)
	{
		hqubyte8 temp;
		SWAP(pBlock[4], pBlock[5], temp);
		SWAP(pBlock[6], pBlock[7], temp);

		for (int i = 4 ; i < 8 ; ++i)
			pBlock[i] = FLIP_EACH2BITS(pBlock[i]);
	}
	else
	{
		for (int i = 4 ; i < 8 ; ++i)
			pBlock[i] = FLIP4BITS_2HALFBYTE(pBlock[i]);
	}
#endif

}

void FlipHalfBlockETC1(hq_ubyte8 * pBlock)//flip vertical
{
//		31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3   2   1  0
//      --------------------------------------------------------------------------------------------------
//     |       most significant pixel index bits       |         least significant pixel index bits       |  
//     | p| o| n| m| l| k| j| i| h| g| f| e| d| c| b| a| p| o| n| m| l| k| j| i| h| g| f| e| d| c | b | a |
//      --------------------------------------------------------------------------------------------------
#if FOR_GLES_SPEC
/*
	|a   |e   |i   |m   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |b   |f   |j   |n   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |c   |g   |k   |o   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |d   |h   |l   |p   |
    |    |    |    |    |
*/
	for (int i = 4 ; i < 8 ; ++i)
		pBlock[i] = FLIP2BITS_2HALFBYTE(pBlock[i]);
#else
/*
flipbit = 0 :				flipbit = 1 :
	|a   |e   |i   |m   |		|a   |c   |e   |g   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |b   |f   |j   |n   |		|b   |d   |f   |h   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ----
    |c   |g   |k   |o   |		|i   |k   |m   |o   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |d   |h   |l   |p   |		|j   |l   |n   |p   |
    |    |    |    |    |		|    |    |    |    |
*/
	hqubyte8 flipbit = pBlock[3] & 0x1;
	if (flipbit)
	{
		pBlock[5] = FLIP_EACH2BITS(pBlock[5]);
	}
	else
	{
		for (int i = 4 ; i < 8 ; ++i)
			pBlock[i] = FLIP2BITS_2HALFBYTE(pBlock[i]);
	}

#endif
}

void FlipHFullBlockETC1(hq_ubyte8 * pBlock)//flip horizontal
{
//		31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3   2   1  0
//      --------------------------------------------------------------------------------------------------
//     |       most significant pixel index bits       |         least significant pixel index bits       |  
//     | p| o| n| m| l| k| j| i| h| g| f| e| d| c| b| a| p| o| n| m| l| k| j| i| h| g| f| e| d| c | b | a |
//      --------------------------------------------------------------------------------------------------
#if FOR_GLES_SPEC
/*
	|a   |e   |i   |m   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |b   |f   |j   |n   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |c   |g   |k   |o   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |d   |h   |l   |p   |
    |    |    |    |    |
*/
	hqubyte8 temp;
	SWAP(pBlock[4], pBlock[5], temp);
	SWAP(pBlock[6], pBlock[7], temp);


	for (int i = 4 ; i < 8 ; ++i)
		pBlock[i] = SWAP_HALFBYTE(pBlock[i]);
#else
/*
flipbit = 0 :				flipbit = 1 :
	|a   |e   |i   |m   |		|a   |c   |e   |g   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |b   |f   |j   |n   |		|b   |d   |f   |h   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ----
    |c   |g   |k   |o   |		|i   |k   |m   |o   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |d   |h   |l   |p   |		|j   |l   |n   |p   |
    |    |    |    |    |		|    |    |    |    |
*/
	hqubyte8 flipbit = pBlock[3] & 0x1;
	if (flipbit)
	{
		for (int i = 4 ; i < 8 ; ++i)
		{
			pBlock[i] = FLIP_ELE2BITS(pBlock[i]);
		}
	}
	else
	{
		hqubyte8 temp;
		SWAP(pBlock[4], pBlock[5], temp);
		SWAP(pBlock[6], pBlock[7], temp);


		for (int i = 4 ; i < 8 ; ++i)
			pBlock[i] = SWAP_HALFBYTE(pBlock[i]);
	}
#endif
}

void FlipHHalfBlockETC1(hq_ubyte8 * pBlock)//flip horizontal
{
//		31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3   2   1  0
//      --------------------------------------------------------------------------------------------------
//     |       most significant pixel index bits       |         least significant pixel index bits       |  
//     | p| o| n| m| l| k| j| i| h| g| f| e| d| c| b| a| p| o| n| m| l| k| j| i| h| g| f| e| d| c | b | a |
//      --------------------------------------------------------------------------------------------------
#if FOR_GLES_SPEC
/*
	|a   |e   |i   |m   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |b   |f   |j   |n   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |c   |g   |k   |o   |
    |    |    |    |    |
     ---- ---- ---- ---- 
    |d   |h   |l   |p   |
    |    |    |    |    |
*/
	pBlock[5] = SWAP_HALFBYTE(pBlock[5]);
	pBlock[7] = SWAP_HALFBYTE(pBlock[7]);
#else
/*
flipbit = 0 :				flipbit = 1 :
	|a   |e   |i   |m   |		|a   |c   |e   |g   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |b   |f   |j   |n   |		|b   |d   |f   |h   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ----
    |c   |g   |k   |o   |		|i   |k   |m   |o   |
    |    |    |    |    |		|    |    |    |    |
     ---- ---- ---- ----		 ---- ---- ---- ---- 
    |d   |h   |l   |p   |		|j   |l   |n   |p   |
    |    |    |    |    |		|    |    |    |    |
*/
	hqubyte8 flipbit = pBlock[3] & 0x1;
	if (flipbit)
	{
		for (int i = 4 ; i < 8 ; ++i)
			pBlock[i] = FLIP_ELE2BITS_HALFBYTE(pBlock[i]);
	}
	else
	{
		pBlock[5] = SWAP_HALFBYTE(pBlock[5]);
		pBlock[7] = SWAP_HALFBYTE(pBlock[7]);
	}
#endif
}

void SwapBlockETC1(hq_ubyte8 *pBlock1,hq_ubyte8 *pBlock2)
{
	hq_ubyte8 temp[8];
	memcpy(temp,pBlock1,8);
	memcpy(pBlock1,pBlock2,8);
	memcpy(pBlock2,temp,8);
}

void DecodeETC1(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB )
{
	hquint32 numBlocksPerRow = (width + 3) / 4;
	hquint32 numBlocksPerCol = (height + 3) / 4;
	hquint32 block_part1, block_part2;
	const hqubyte8 *pBlock = encodeBlocks;

	for (hquint32 blockY = 0; blockY < numBlocksPerCol ; ++blockY)
	{
		for (hquint32 blockX = 0; blockX < numBlocksPerRow ; ++blockX)
		{
			//read block info
			Read_big_endian_4byte_word(pBlock, &block_part1);
			Read_big_endian_4byte_word(pBlock + 4, &block_part2);
			DecompressBlockDiffFlip(block_part1, block_part2, decodeData, width, height, blockX * 4, blockY * 4, flipRGB);
			pBlock += 8;//next block
		}
	}
}
