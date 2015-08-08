/*
* Adplug - Replayer for many OPL2/OPL3 audio file formats.
* Copyright (C) 1999 - 2005 Simon Peter, <dn.tlp@gmx.net>, et al.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
* demuopl.h - Emulated OPL using DOSBOX's emulator, by Wei Mingzhi
*             <whistler_wmz@users.sf.net>.
*/

#ifndef H_ADPLUG_DBEMUOPL
#define H_ADPLUG_DBEMUOPL

#include "opl.h"
#include "dbopl.h"

class CDBemuopl : public Copl
{
public:
	CDBemuopl(int rate, bool bit16, bool usestereo);
	~CDBemuopl();

	void update(short *buf, int samples);

	// template methods
	void write(int reg, int val);

	void init();

protected:
	DBOPL::Chip chip;
	int32_t* buffer;
	int rate, maxlen;
	bool use16bit, stereo;

	static bool _inited;

	void update_opl3(short *buf, int samples);
	void update_opl2(short *buf, int samples);
};

#endif
