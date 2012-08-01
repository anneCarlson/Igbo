/*
* Copyright (c) 2009, Helios (helios.vmg@gmail.com)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice,
*       this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY HELIOS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
* EVENT SHALL HELIOS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/

typedef unsigned char uchar;

#define BOM8A 0xEF
#define BOM8B 0xBB
#define BOM8C 0xBF

/*
string: a UTF-8-encoded C string (nul terminated)
Return value: a wchar_t C string.

The function handles memory allocation on its own.

Limitations: Only handles the range [U+0000;U+FFFF], higher code points are
changed to '?'.

Assumptions: sizeof(wchar_t)>=2
*/
wchar_t *UTF8_to_WChar(const char *string){
	long b=0,
		c=0;
	if ((uchar)string[0]==BOM8A && (uchar)string[1]==BOM8B && (uchar)string[2]==BOM8C)
		string+=3;
	for (const char *a=string;*a;a++)
		if (((uchar)*a)<128 || (*a&192)==192)
			c++;
	wchar_t *res=new wchar_t[c+1];
	res[c]=0;
	for (uchar *a=(uchar*)string;*a;a++){
		if (!(*a&128))
			//Byte represents an ASCII character. Direct copy will do.
			res[b]=*a;
		else if ((*a&192)==128)
			//Byte is the middle of an encoded character. Ignore.
			continue;
		else if ((*a&224)==192)
			//Byte represents the start of an encoded character in the range
			//U+0080 to U+07FF
			res[b]=((*a&31)<<6)|a[1]&63;
		else if ((*a&240)==224)
			//Byte represents the start of an encoded character in the range
			//U+07FF to U+FFFF
			res[b]=((*a&15)<<12)|((a[1]&63)<<6)|a[2]&63;
		else if ((*a&248)==240){
			//Byte represents the start of an encoded character beyond the
			//U+FFFF limit of 16-bit integers
			res[b]='?';
		}
		b++;
	}
	return res;
}

//Do not call me.
long getUTF8size(const wchar_t *string){
	if (!string)
		return 0;
	long res=0;
	for (;*string;string++){
		if (*string<0x80)
			res++;
		else if (*string<0x800)
			res+=2;
		else
			res+=3;
	}
	return res;
}

/*
string: a wchar_t C string (nul terminated)
Return value: a UTF-8-encoded C string.

The function handles memory allocation on its own.

Limitations: Only handles the range [U+0000;U+FFFF], higher code points are
changed to '?'.

Assumptions: sizeof(wchar_t)>=2
*/
char *WChar_to_UTF8(const wchar_t *string){
	long fSize=getUTF8size(string);
	char *res=new char[fSize+1];
	res[fSize]=0;
	if (!string)
		return res;
	long b=0;
	for (;*string;string++,b++){
		if (*string<0x80)
			res[b]=(char)*string;
		else if (*string<0x800){
			res[b++]=(*string>>6)|192;
			res[b]=*string&63|128;
		}else{
			res[b++]=(*string>>12)|224;
			res[b++]=((*string&4095)>>6)|128;
			res[b]=*string&63|128;
		}
	}
	return res;
}
