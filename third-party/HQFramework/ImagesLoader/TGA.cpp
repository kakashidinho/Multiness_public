/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "Bitmap.h"
#include "ImgByteStream.h"
#include <string.h>
#include <new>


int Bitmap::LoadTGA()
{
	hq_short16 x,y,w,h;
	if (!this->pTemp)
		return IMG_FAIL_FILE_NOT_EXIST;
	if (this->pTemp[1]!=0)//=0 là không dùng dạng indexed color map
		return IMG_FAIL_NOT_SUPPORTED;
	hq_ubyte8 encode=this->pTemp[2];//dạng mã hóa lưu ở byte thứ 3
	if(encode >11)
		return IMG_FAIL_NOT_SUPPORTED;
	memcpy(&x,&this->pTemp[8],2);//X origin
	memcpy(&y,&this->pTemp[10],2);//Y origin
	memcpy(&w,&this->pTemp[12],2);//width
	memcpy(&h,&this->pTemp[14],2);//height
	
	if(w<1||h<1)
		return IMG_FAIL_BAD_FORMAT;

	if(this->pTemp[17]&0xc0)//2 bit thứ 7 và 8 của byte thứ 18 không là 0=>bad format
		return IMG_FAIL_BAD_FORMAT;
	this->width=(hq_uint32)w;
	this->height=(hq_uint32)h;

	this->bits=(hq_short16) this->pTemp[16];
	this->imgSize=(hq_uint32)(this->bits)/8*(this->width)*(this->height);

	this->CheckOriginTGA();

	switch(bits)
	{
	case 24:
		if (layout == LAYOUT_BGR)
			format = FMT_B8G8R8;
		else
			format=FMT_R8G8B8;
		break;
	case 8:
		format=FMT_L8;
		break;
	case 16:
		format=FMT_A8L8;
		break;
	case 32:
		if (layout == LAYOUT_BGR)
			format = FMT_A8B8G8R8;
		else
			format=FMT_A8R8G8B8;
		break;
	default:
		format=FMT_UNKNOWN;
		return IMG_FAIL_NOT_SUPPORTED;
	}

	switch(encode)//dạng mã hóa
	{
	case 2:case 3://raw data ,không nén
		return LoadTGARawData();
	case 10:case 11://nén RLE 
		return LoadTGARLEData();
	default:
		return IMG_FAIL_NOT_SUPPORTED;
	}
}

int Bitmap::LoadTGARawData()
{
	try{	
		this->pData=new hq_ubyte8[this->imgSize];
	}
	catch(std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}

	int offset=(int)this->pTemp[0]+18;//độ dời đến vị trí bắt đầu của data pixel đầu tiên bằng giá trị lưu ở byte đầu tiên + 18 bytes (độ lớn header);
	
	switch (this->layout)
	{
	case LAYOUT_BGR:
		if (this->bits == 24 || this->bits == 32)
		{
			size_t pixelSize = this->bits / 8;
			hqubyte8* des = this->pData;
			hqubyte8* src = this->pTemp + offset;
			for (hquint32 i = 0 ; i < width ; ++i )
				for (hquint32 j = 0 ; j < height ; ++j )
				{
					des[0] = src[2];
					des[1] = src[1];
					des[2] = src[0];
					if (this->bits == 32)
						des[3] = src[3];

					des += pixelSize;
					src += pixelSize;
				}

			break;
		}
	default:
		memcpy(this->pData,&this->pTemp[offset],this->imgSize);
		break;
	}
	return IMG_OK;
}
int Bitmap::LoadTGARLEData()//load pixel data ở dạng nén RLE
{
	bool flipRGB = (this->bits == 24 || this->bits == 32) && this->layout == LAYOUT_BGR;
	//độ lớn (bytes) của 1 pixel data
	int pixelSize=(int)this->pTemp[16]/8;
	hq_ubyte8 *cur;

	try{
		this->pData=new hq_ubyte8[this->imgSize];
	}
	catch(std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}

	int offset=(int)this->pTemp[0]+18;//độ dời đến vị trí bắt đầu của data pixel đầu tiên bằng giá trị lưu ở byte đầu tiên + 18 bytes;
	cur=(hq_ubyte8*)this->pTemp+offset;
	
	hq_uint32 index=0;
	while (index<this->imgSize)
	{
		if(*cur&0x80)//bit thứ 8 là 1=>dạng run length packet
		{
			int length=((*cur)-127);//độ dài mà pixel sẽ lặp lại
			cur++;//trỏ đến byte tiếp theo
			//copy pixel "length" lần vào dữ liệu ra
			for(int j=0;j<length;++j)
			{
				if (flipRGB)
				{
					hqubyte8* des = this->pData + index;
					des[0] = cur[2];
					des[1] = cur[1];
					des[2] = cur[0];
					if (this->bits == 32)
						des[3] = cur[3];
				}
				else
					memcpy((this->pData)+index,cur,pixelSize);
				index+=pixelSize;
			}
			cur+=pixelSize;
		}
		else {//dạng raw packet

			int num=*cur+1;//giá trị của byte này là số lượng pixels tiếp theo trừ 1
			
			cur++;//trỏ đến byte tiếp theo
			//copy "num" pixels tiếp theo vào dữ liệu ra
			for(int j=0;j<num;++j)
			{
				if (flipRGB)
				{
					hqubyte8* des = this->pData + index;
					des[0] = cur[2];
					des[1] = cur[1];
					des[2] = cur[0];
					if (this->bits == 32)
						des[3] = cur[3];
				}
				else
					memcpy((this->pData)+index,cur,pixelSize);
				index+=pixelSize;
				cur+=pixelSize;
			}
		}
	}
	return IMG_OK;
}
void Bitmap::CheckOriginTGA()
{
	//check bit thứ 5 và 6 ở byte thứ 18
	int flag=(int)((this->pTemp[17]&0x30)>>4);
	switch(flag)
	{
	case 0:
		this->origin = ORIGIN_BOTTOM_LEFT;
		break;
	case 1:
		this->origin = ORIGIN_BOTTOM_RIGHT;
		break;
	case 2:
		this->origin = ORIGIN_TOP_LEFT;
		break;
	case 3:
		this->origin = ORIGIN_TOP_RIGHT;
		break;
	default:
		this->origin = ORIGIN_BOTTOM_LEFT;
	}
}
