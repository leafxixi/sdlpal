//
// Copyright (c) 2016, Pal Lockheart <ex@palx.org>.
// All rights reserved.
// Modified by Lou Yihua <louyihua@21cn.com> with unicode support, 2015.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _UWP_FAKEIO_H
#define _UWP_FAKEIO_H

FILE *fopen_uwp(const char *filename, const char *mode);
long ftell_uwp(FILE *fp);
int fseek_uwp(FILE *fp, long offset, int whence);
void rewind_uwp(FILE *fp);
size_t fread_uwp(void *ptr, size_t size, size_t nitems, FILE *fp);
char *fgets_uwp(char *ptr, size_t size, FILE *fp);
int fgetc_uwp(FILE *fp);
size_t fwrite_uwp(const void *ptr, size_t size, size_t nitems, FILE *fp);
int fclose_uwp(FILE *fp);

#define fopen fopen_uwp
#define ftell ftell_uwp
#define fseek fseek_uwp
//#define rewind(fp) rewind_uwp
#define fread fread_uwp
#define fwrite fwrite_uwp
#define fgets fgets_uwp
#define fgetc fgetc_uwp
#define fclose fclose_uwp

#endif //_UWP_FAKEIO_H