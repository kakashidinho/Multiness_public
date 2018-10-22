/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "3STC_DXT.h"
#include <string.h>
#include "ImgLoader.h"

#define LittleEndian

//utility macros
#define SwapByte4(a) (((a & 0xc0)>> 6) | ((a & 0x30)>> 2) | ((a & 0x0c)<< 2) | ((a & 0x03)<< 6))
#define SwapHalfByte4(a) (((a & 0x0c)>> 2) | ((a & 0x03)<< 2) | (a & 0xf0))
#define SwapByte2(a) (((a & 0xf0)>> 4) | ((a & 0x0f)<< 4))
#define Swap24bit3(a) (((a & 0xe00e00)>> 9) | ((a & 0x1c01c0)>> 3) | ((a & 0x038038)<< 3) | ((a & 0x007007)<< 9))
#define SwapHalf24bit3(a) ((a & 0xfc0fc0) | ((a & 0x038038)>> 3) | ((a & 0x007007)<< 3))
#define ARGB32(r,g,b,a) (((r & 0xff) << 16) | ((g & 0xff) << 8) |\
							(b & 0xff) | ((a & 0xff) << 24))
#define ABGR32(r,g,b,a) ARGB32(b,g,r,a)
#define XRGB32(r,g,b) ARGB32(r,g,b,0xff)
#define XBGR32(r,g,b) ABGR32(r,g,b,0xff)

//DXT1
void FlipFullBlockDXT1(hq_ubyte8 * pBlock)
{
//1 block của DXT1 có 8 byte
//byte 0->1 =>color0
//byte 2->3 =>color1
//byte 4->7 =>mỗi byte là 1 dòng
//để đảo block ta đảo 4 dòng trong byte 4->7

	hq_ubyte8 temp;
	temp=pBlock[4];
	pBlock[4]=pBlock[7];
	pBlock[7]=temp;

	temp=pBlock[5];
	pBlock[5]=pBlock[6];
	pBlock[6]=temp;
}

void FlipHalfBlockDXT1(hq_ubyte8 * pBlock)
{
	hq_ubyte8 temp;
	temp=pBlock[4];
	pBlock[4]=pBlock[5];
	pBlock[5]=temp;
}

void FlipHFullBlockDXT1(hq_ubyte8 * pBlock)
{
//1 block của DXT1 có 8 byte
//byte 0->1 =>color0
//byte 2->3 =>color1
//byte 4->7 =>mỗi byte là 1 hàng trong bộ 4x4
	
	pBlock[4]=SwapByte4(pBlock[4]);
	pBlock[5]=SwapByte4(pBlock[5]);
	pBlock[6]=SwapByte4(pBlock[6]);
	pBlock[7]=SwapByte4(pBlock[7]);
}

void FlipHHalfBlockDXT1(hq_ubyte8 * pBlock)
{
	pBlock[4]=SwapHalfByte4(pBlock[4]);
	pBlock[5]=SwapHalfByte4(pBlock[5]);
	pBlock[6]=SwapHalfByte4(pBlock[6]);
	pBlock[7]=SwapHalfByte4(pBlock[7]);
}

void SwapBlockDXT1(hq_ubyte8 *pBlock1,hq_ubyte8 *pBlock2)
{
	hq_ubyte8 temp[8];
	memcpy(temp,pBlock1,8);
	memcpy(pBlock1,pBlock2,8);
	memcpy(pBlock2,temp,8);
}

void DecodeDXT1(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB)
{
	hq_uint32 color[4];//bảng màu
	hq_ushort16 color0_16, color1_16;//16 bits version of color0 and color1
	hq_ubyte8 r[2],g[2],b[2];
	
	hq_ubyte8 *pBlock=(hq_ubyte8*)encodeBlocks;
	hq_uint32 *pCur;
	hq_uint32 nWidthBlocks=(width+3)/4;//số block trên 1 hàng
	hq_uint32 nHeightBlocks=(height+3)/4;//số block trên 1 cột
	hq_uint32 w,h;//tọa độ pixel trong data đã giải mã
	for(hq_uint32 nH=0;nH<nHeightBlocks;++nH)//với từng hàng block
		for(hq_uint32 nW=0;nW<nWidthBlocks;++nW)//với từng cột block
		{
			h=nH*4;
			w=nW*4;
			pCur = (hq_uint32*)decodeData+(h*width+w);

			//bảng màu
			
			memcpy(&color0_16,pBlock,2);//color0
			//get color0 components
			r[0]= (hqubyte8) (GetRfromRGB16(color0_16) / 31.f * 255.f);
			g[0]= (hqubyte8) (GetGfromRGB16(color0_16) / 63.f * 255.f);
			b[0]= (hqubyte8) (GetBfromRGB16(color0_16) / 31.f * 255.f);


			memcpy(&color1_16,pBlock+2,2);//color1
			//get color1 components
			r[1]= (hqubyte8) (GetRfromRGB16(color1_16) / 31.f * 255.f);
			g[1]= (hqubyte8) (GetGfromRGB16(color1_16) / 63.f * 255.f);
			b[1]= (hqubyte8) (GetBfromRGB16(color1_16) / 31.f * 255.f);

			if (flipRGB)//A8B8G8R8
			{
				color[0] = XBGR32(r[0], g[0], b[0]);
				color[1] = XBGR32(r[1], g[1], b[1]);

				if(color0_16 > color1_16)
				{
					color[2]=XBGR32((2*r[0]+r[1])/3,(2*g[0]+g[1])/3,(2*b[0]+b[1])/3);//color2=(2*color0+color1)/3
					color[3]=XBGR32((2*r[1]+r[0])/3,(2*g[1]+g[0])/3,(2*b[1]+b[0])/3);//color3=(color0+2*color1)/3
				}
				else
				{
					color[2]=XBGR32((r[0]+r[1])/2,(g[0]+g[1])/2,(b[0]+b[1])/2);//color2=(color0+color1)/2
					color[3]=0x00000000;//transparent black 
				}
			}
			else//A8R8G8B8
			{
				color[0] = XRGB32(r[0], g[0], b[0]);
				color[1] = XRGB32(r[1], g[1], b[1]);

				if(color0_16 > color1_16)
				{
					color[2]=XRGB32((2*r[0]+r[1])/3,(2*g[0]+g[1])/3,(2*b[0]+b[1])/3);//color2=(2*color0+color1)/3
					color[3]=XRGB32((2*r[1]+r[0])/3,(2*g[1]+g[0])/3,(2*b[1]+b[0])/3);//color3=(color0+2*color1)/3
				}
				else
				{
					color[2]=XRGB32((r[0]+r[1])/2,(g[0]+g[1])/2,(b[0]+b[1])/2);//color2=(color0+color1)/2
					color[3]=0x00000000;//transparent black 
				}
			}

			pBlock+=4;//đến địa chỉ chứa bảng lookup


			for(hq_uint32 i=0;i<4 && h < (height-i);++i)
			{
				*(pCur+i*width)=color[pBlock[i] & 0x3];
				if((w+1)<width)//chưa ra khỏi đường bao chiều rộng
					*(pCur+i*width+1)=color[(pBlock[i] & 0xc)>> 2];
				if((w+2)<width)//chưa ra khỏi đường bao chiều rộng
					*(pCur+i*width+2)=color[(pBlock[i] & 0x30)>> 4];
				if((w+3)<width)//chưa ra khỏi đường bao chiều rộng
					*(pCur+i*width+3)=color[(pBlock[i] & 0xc0)>> 6];
			}

			pBlock+=4;//next block
		}
}

//DXT3
void FlipFullBlockDXT3(hq_ubyte8 * pBlock)
{
//1 block của DXT3 có 16 byte
//byte 0->7 =>cứ 2 byte là 1 dòng dành cho kênh alpha
//byte 8-15 =>giống DXT1


	hq_ubyte8 temp;
	temp=pBlock[0];
	pBlock[0]=pBlock[6];
	pBlock[6]=temp;

	temp=pBlock[1];
	pBlock[1]=pBlock[7];
	pBlock[7]=temp;

	temp=pBlock[2];
	pBlock[2]=pBlock[4];
	pBlock[4]=temp;

	temp=pBlock[3];
	pBlock[3]=pBlock[5];
	pBlock[5]=temp;

	FlipFullBlockDXT1(pBlock+8);
}

void FlipHalfBlockDXT3(hq_ubyte8 * pBlock)
{
	hq_ubyte8 temp;
	temp=pBlock[0];
	pBlock[0]=pBlock[2];
	pBlock[2]=temp;

	temp=pBlock[1];
	pBlock[1]=pBlock[3];
	pBlock[3]=temp;

	FlipHalfBlockDXT1(pBlock+8);
}


void FlipHFullBlockDXT3(hq_ubyte8 * pBlock)
{
//1 block của DXT3 có 16 byte
//byte 0->7 =>cứ 2 byte là 1 hàng trong bộ 4x4 dành cho kênh alpha
//byte 8-15 =>giống DXT1
	
	hq_ubyte8 temp;
	temp=SwapByte2(pBlock[0]);
	pBlock[0]=SwapByte2(pBlock[1]);
	pBlock[1]=temp;

	temp=SwapByte2(pBlock[2]);
	pBlock[2]=SwapByte2(pBlock[3]);
	pBlock[3]=temp;

	temp=SwapByte2(pBlock[4]);
	pBlock[4]=SwapByte2(pBlock[5]);
	pBlock[5]=temp;

	temp=SwapByte2(pBlock[6]);
	pBlock[6]=SwapByte2(pBlock[7]);
	pBlock[7]=temp;

	FlipHFullBlockDXT1(pBlock+8);
}

void FlipHHalfBlockDXT3(hq_ubyte8 * pBlock)
{
#ifdef LittleEndian
	pBlock[0]=SwapByte2(pBlock[0]);
	pBlock[2]=SwapByte2(pBlock[2]);
	pBlock[4]=SwapByte2(pBlock[4]);
	pBlock[6]=SwapByte2(pBlock[6]);
#else
	pBlock[1]=SwapByte2(pBlock[1]);
	pBlock[3]=SwapByte2(pBlock[3]);
	pBlock[5]=SwapByte2(pBlock[5]);
	pBlock[7]=SwapByte2(pBlock[7]);
#endif
	FlipHHalfBlockDXT1(pBlock+8);
}

void DecodeDXT3(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB)
{
	hq_ushort16 color[2];//bảng màu
	hq_float32 r[4],g[4],b[4];
	
	hquint32 redChannel = flipRGB? 0 : 2;
	hquint32 blueChannel = 2 - redChannel;

	hq_ubyte8 *pBlock=(hq_ubyte8*)encodeBlocks;
	hq_ubyte8 *pAlpha;
	hq_ubyte8 *pCur;
	hq_uint32 nWidthBlocks=(width+3)/4;//số block trên 1 hàng
	hq_uint32 nHeightBlocks=(height+3)/4;//số block trên 1 cột
	hq_uint32 w,h;//tọa độ pixel trong data đã giải mã

	hq_uint32 rowSize=width*4;//độ lớn (bytes) 1 hàng trong dữ liệu giải nén
	for(hq_uint32 nH=0;nH<nHeightBlocks;++nH)//với từng hàng block
		for(hq_uint32 nW=0;nW<nWidthBlocks;++nW)//với từng cột block
		{
			pAlpha=pBlock;//địa chỉ bảng giá trị alpha

			h=nH*4;
			w=nW*4;
			pCur=decodeData+(h*width+w)*4;

			//đến bảng màu
			pBlock+=8;
			memset(color,0,2*sizeof(hq_short16));
			//bảng màu
			memcpy(&color[0],pBlock,2);//color0
			memcpy(&color[1],pBlock+2,2);//color1
			//get color components, range 0.0f->1.0f
			r[0]=GetRfromRGB16(color[0])/31.0f;
			g[0]=GetGfromRGB16(color[0])/63.0f;
			b[0]=GetBfromRGB16(color[0])/31.0f;

			r[1]=GetRfromRGB16(color[1])/31.0f;
			g[1]=GetGfromRGB16(color[1])/63.0f;
			b[1]=GetBfromRGB16(color[1])/31.0f;
			
			//color2=(2*color0+color1)/3
			r[2]=(2*r[0]+r[1])/3.0f;
			g[2]=(2*g[0]+g[1])/3.0f;
			b[2]=(2*b[0]+b[1])/3.0f;
			//color3=(2*color1+color0)/3
			r[3]=(2*r[1]+r[0])/3.0f;
			g[3]=(2*g[1]+g[0])/3.0f;
			b[3]=(2*b[1]+b[0])/3.0f;

			pBlock+=4;//đến địa chỉ chứa bảng lookup

			for(hq_uint32 i=0;i<4 && h < (height-i);++i)
			{
				hq_uint32 index= pBlock[i] & 0x3;//chỉ mục màu của pixel
				hq_ubyte8 *pColor=pCur+i*rowSize;
				//alpha
				pColor[3]=(hq_ubyte8)((hq_uint32)((pAlpha[2*i] & 0xf)*17.0f) & 0xff); //17.0f= 255/15
				//red
				pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
				//green
				pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
				//blue
				pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				if((w+1)<width)//chưa ra khỏi đường bao chiều rộng
				{
					index= (pBlock[i] & 0xc) >> 2;
					pColor+=4;
					//alpha
					pColor[3]=(hq_ubyte8)((hq_uint32)(((pAlpha[2*i] & 0xf0)>> 4)*17.0f) & 0xff);
					//red
					pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
					//green
					pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
					//blue
					pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				}
				if((w+2)<width)//chưa ra khỏi đường bao chiều rộng
				{
					index= (pBlock[i] & 0x30) >> 4;
					pColor+=4;
					//alpha
					pColor[3]=(hq_ubyte8)((hq_uint32)((pAlpha[2*i+1] & 0xf)*17.0f) & 0xff);
					//red
					pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
					//green
					pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
					//blue
					pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				}
				if((w+3)<width)//chưa ra khỏi đường bao chiều rộng
				{
					index= (pBlock[i] & 0xc0) >> 6;
					pColor+=4;
					//alpha
					pColor[3]=(hq_ubyte8)((hq_uint32)(((pAlpha[2*i+1] & 0xf0)>> 4)*17.0f) & 0xff);
					//red
					pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
					//green
					pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
					//blue
					pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				}
			}

			pBlock+=4;//next block
		}
}

//DXT5
void FlipFullBlockDXT5(hq_ubyte8 * pBlock)
{
//1 block DXT5 có 16 byte
//byte 0 =>alpha0
//byte 1 =>alpha1
//byte 2->7 => 1 tập gồm 4x4 bộ 3 bit lookup table dành cho alpha channel của 4x4 pixel
//byte 8-15 =>DXT1

	hq_uint32 row_0_1 = pBlock[2] + 256 * (pBlock[3] + 256 * pBlock[4]);//hàng 0 và hàng 1
	hq_uint32 row_2_3 = pBlock[5] + 256 * (pBlock[6] + 256 * pBlock[7]);//hàng 2 và hàng 3

	hq_uint32 row_1_0 = ((row_0_1 & 0x000fff) << 12) | ((row_0_1 & 0xfff000) >> 12);//đảo hàng 0 và 1

	hq_uint32 row_3_2 = ((row_2_3 & 0x000fff) << 12) | ((row_2_3 & 0xfff000) >> 12);//đảo hàng 2 và 3
	
	pBlock[2] = row_3_2 & 0xff;
	pBlock[3] = (row_3_2 & 0xff00) >> 8;
	pBlock[4] = (row_3_2 & 0xff0000) >> 16;
	pBlock[5] = row_1_0 & 0xff;
	pBlock[6] = (row_1_0 & 0xff00) >> 8;
	pBlock[7] = (row_1_0 & 0xff0000) >> 16;


	FlipFullBlockDXT1(pBlock+8);
}

void FlipHalfBlockDXT5(hq_ubyte8 * pBlock)
{
	hq_uint32 row_0_1 = pBlock[2] + 256 * (pBlock[3] + 256 * pBlock[4]);

	hq_uint32 row_1_0 = ((row_0_1 & 0x000fff) << 12) | ((row_0_1 & 0xfff000) >> 12);
	
	pBlock[2] = row_1_0 & 0xff;
	pBlock[3] = (row_1_0 & 0xff00) >> 8;
	pBlock[4] = (row_1_0 & 0xff0000) >> 16;

	FlipHalfBlockDXT1(pBlock+8);
}

void FlipHFullBlockDXT5(hq_ubyte8 * pBlock)
{
//1 block DXT5 có 16 byte
//byte 0 =>alpha0
//byte 1 =>alpha1
//byte 2->7 => 1 tập gồm 4x4 bộ 3 bit lookup table dành cho alpha channel của 4x4 pixel
//byte 8-15 =>DXT1
	hq_uint32 row_0_1 = pBlock[2] + 256 * (pBlock[3] + 256 * pBlock[4]);//hàng 0 và hàng 1
	hq_uint32 row_2_3 = pBlock[5] + 256 * (pBlock[6] + 256 * pBlock[7]);//hàng 2 và hàng 3

	row_0_1= Swap24bit3(row_0_1);
	row_2_3= Swap24bit3(row_2_3);

	pBlock[2] = row_0_1 & 0xff;
	pBlock[3] = (row_0_1 & 0xff00) >> 8;
	pBlock[4] = (row_0_1 & 0xff0000) >> 16;
	pBlock[5] = row_2_3 & 0xff;
	pBlock[6] = (row_2_3 & 0xff00) >> 8;
	pBlock[7] = (row_2_3 & 0xff0000) >> 16;


	FlipHFullBlockDXT1(pBlock+8);
}

void FlipHHalfBlockDXT5(hq_ubyte8 * pBlock)
{
	hq_uint32 row_0_1 = pBlock[2] + 256 * (pBlock[3] + 256 * pBlock[4]);//hàng 0 và hàng 1
	hq_uint32 row_2_3 = pBlock[5] + 256 * (pBlock[6] + 256 * pBlock[7]);//hàng 2 và hàng 3

	row_0_1= SwapHalf24bit3(row_0_1);
	row_2_3= SwapHalf24bit3(row_2_3);

	pBlock[2] = row_0_1 & 0xff;
	pBlock[3] = (row_0_1 & 0xff00) >> 8;
	pBlock[4] = (row_0_1 & 0xff0000) >> 16;
	pBlock[5] = row_2_3 & 0xff;
	pBlock[6] = (row_2_3 & 0xff00) >> 8;
	pBlock[7] = (row_2_3 & 0xff0000) >> 16;

	FlipHHalfBlockDXT1(pBlock+8);
}

void SwapBlockDXT3_5(hq_ubyte8 *pBlock1,hq_ubyte8 *pBlock2)
{
	hq_ubyte8 temp[16];
	memcpy(temp,pBlock1,16);
	memcpy(pBlock1,pBlock2,16);
	memcpy(pBlock2,temp,16);
}

void DecodeDXT5(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB)
{
	hq_ushort16 color[2];//bảng màu
	hq_float32 r[4],g[4],b[4],a[8];
	hq_uint32 row_0_1,row_2_3;
	hq_ubyte8 indexA[16];//16 chỉ mục trong bảng lookup giá trị alpha
	
	hquint32 redChannel = flipRGB? 0 : 2;
	hquint32 blueChannel = 2 - redChannel;

	hq_ubyte8 *pBlock=(hq_ubyte8*)encodeBlocks;
	hq_ubyte8 *pCur;
	hq_uint32 nWidthBlocks=(width+3)/4;//số block trên 1 hàng
	hq_uint32 nHeightBlocks=(height+3)/4;//số block trên 1 cột
	hq_uint32 w,h;//tọa độ pixel trong data đã giải mã

	hq_uint32 rowSize=width*4;//độ lớn (bytes) 1 hàng trong dữ liệu giải nén
	for(hq_uint32 nH=0;nH<nHeightBlocks;++nH)//với từng hàng block
		for(hq_uint32 nW=0;nW<nWidthBlocks;++nW)//với từng cột block
		{
			h=nH*4;
			w=nW*4;
			pCur=decodeData+(h*width+w)*4;

			a[0]=pBlock[0]/255.0f;//alpha0 ,range 0.0f->1.0f
			a[1]=pBlock[1]/255.0f;//alpha1

			if(a[0] > a[1])
			{
				a[2]=(6*a[0]+a[1])/7.0f;
				a[3]=(5*a[0]+2*a[1])/7.0f;
				a[4]=(4*a[0]+3*a[1])/7.0f;
				a[5]=(3*a[0]+4*a[1])/7.0f;
				a[6]=(2*a[0]+5*a[1])/7.0f;
				a[7]=(a[0]+6*a[1])/7.0f;
			}
			else{
				a[2]=(4*a[0]+a[1])/5.0f;
				a[3]=(3*a[0]+2*a[1])/5.0f;
				a[4]=(2*a[0]+3*a[1])/5.0f;
				a[5]=(a[0]+4*a[1])/5.0f;
				a[6]=0.0f;
				a[7]=1.0f;
			}
			
			//bảng lookup giá trị alpha
			row_0_1 = pBlock[2] + 256 * (pBlock[3] + 256 * pBlock[4]);//hàng 0 và hàng 1
			row_2_3 = pBlock[5] + 256 * (pBlock[6] + 256 * pBlock[7]);//hàng 2 và hàng 3

			//các chỉ mục trong bảng lookup giá trị alpha (mỗi chỉ mục 3 bit)
			indexA[0]=(row_0_1 & 0x7);
			indexA[1]=(row_0_1 & 0x38)>> 3;
			indexA[2]=(row_0_1 & 0x1c0)>> 6;
			indexA[3]=(row_0_1 & 0xe00)>> 9;

			indexA[4]=(row_0_1 & 0x007000)>> 12;
			indexA[5]=(row_0_1 & 0x038000)>> 15;
			indexA[6]=(row_0_1 & 0x1c0000)>> 18;
			indexA[7]=(row_0_1 & 0xe00000)>> 21;

			indexA[8]=(row_2_3 & 0x7);
			indexA[9]=(row_2_3 & 0x38)>> 3;
			indexA[10]=(row_2_3 & 0x1c0)>> 6;
			indexA[11]=(row_2_3 & 0xe00)>> 9;

			indexA[12]=(row_2_3 & 0x007000)>> 12;
			indexA[13]=(row_2_3 & 0x038000)>> 15;
			indexA[14]=(row_2_3 & 0x1c0000)>> 18;
			indexA[15]=(row_2_3 & 0xe00000)>> 21;

			//đến bảng màu
			pBlock+=8;
			memset(color,0,2*sizeof(hq_short16));
			//bảng màu
			memcpy(&color[0],pBlock,2);//color0
			memcpy(&color[1],pBlock+2,2);//color1
			//get color components, range 0.0f->1.0f
			r[0]=GetRfromRGB16(color[0])/31.0f;
			g[0]=GetGfromRGB16(color[0])/63.0f;
			b[0]=GetBfromRGB16(color[0])/31.0f;

			r[1]=GetRfromRGB16(color[1])/31.0f;
			g[1]=GetGfromRGB16(color[1])/63.0f;
			b[1]=GetBfromRGB16(color[1])/31.0f;
			
			//color2=(2*color0+color1)/3
			r[2]=(2*r[0]+r[1])/3.0f;
			g[2]=(2*g[0]+g[1])/3.0f;
			b[2]=(2*b[0]+b[1])/3.0f;
			//color3=(2*color1+color0)/3
			r[3]=(2*r[1]+r[0])/3.0f;
			g[3]=(2*g[1]+g[0])/3.0f;
			b[3]=(2*b[1]+b[0])/3.0f;

			pBlock+=4;//đến địa chỉ chứa bảng lookup

			for(hq_uint32 i=0;i<4 && h < (height-i);++i)
			{
				hq_uint32 index= pBlock[i] & 0x3;
				hq_ubyte8 *pColor=pCur+i*rowSize;
				//alpha
				pColor[3]=(hq_ubyte8)((hq_uint32)(a[indexA[4*i]] * 255.0f) & 0xff);
				//red
				pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
				//green
				pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
				//blue
				pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				if((w+1)<width)//chưa ra khỏi đường bao chiều rộng
				{
					index= (pBlock[i] & 0xc) >> 2;
					pColor+=4;
					//alpha
					pColor[3]=(hq_ubyte8)((hq_uint32)(a[indexA[4*i+1]] *255.0f) & 0xff);
					//red
					pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
					//green
					pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
					//blue
					pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				}
				if((w+2)<width)//chưa ra khỏi đường bao chiều rộng
				{
					index= (pBlock[i] & 0x30) >> 4;
					pColor+=4;
					//alpha
					pColor[3]=(hq_ubyte8)((hq_uint32)(a[indexA[4*i+2]] *255.0f) & 0xff);
					//red
					pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
					//green
					pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
					//blue
					pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				}
				if((w+3)<width)//chưa ra khỏi đường bao chiều rộng
				{
					index= (pBlock[i] & 0xc0) >> 6;
					pColor+=4;
					//alpha
					pColor[3]=(hq_ubyte8)((hq_uint32)(a[indexA[4*i+3]] *255.0f) & 0xff);
					//red
					pColor[redChannel]=(hq_ubyte8)((hq_uint32)(r[index] * 255.0f) & 0xff);
					//green
					pColor[1]=(hq_ubyte8)((hq_uint32)(g[index] * 255.0f) & 0xff);
					//blue
					pColor[blueChannel]=(hq_ubyte8)((hq_uint32)(b[index] * 255.0f) & 0xff);
				}
			}

			pBlock+=4;//next block
		}
}
