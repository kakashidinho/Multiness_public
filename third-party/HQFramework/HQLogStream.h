/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_LOG_STREAM_H
#define HQ_LOG_STREAM_H

#include "HQPlatformDef.h"

class HQLogStream
{
protected:
	virtual ~HQLogStream(){ };
public:

	virtual void Close()  = 0;// this method should destroy the object
	virtual void Flush() {};
	virtual void Log(const char *tag, const char * message) = 0;
};

#endif
