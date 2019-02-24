// Unicode.cpp
//
// Copyright (C) 2000 by Jerry Lozano
//
//

extern "C" {
#include <NTDDK.h>
}

#define max(a,b) ((a>b)?a:b)

#include "Unicode.h"

void CUString::Init() {
	uStr.Length = 0;
	uStr.MaximumLength = 0;
	uStr.Buffer = NULL;
	aType = Empty;
}

CUString::CUString(const char* pAnsiString) {
	ANSI_STRING str;
	RtlInitAnsiString(&str, pAnsiString);
	uStr.MaximumLength = (USHORT) max(32, RtlAnsiStringToUnicodeSize(&str) );
	uStr.Buffer = (PWSTR)
		ExAllocatePoolWithTag(PagedPool, uStr.MaximumLength, 1633);
	aType = FromPaged;
	RtlAnsiStringToUnicodeString(&uStr, &str, FALSE);
}

CUString::CUString(PCWSTR pWideString) {
	RtlInitUnicodeString(&uStr, pWideString);
	aType = FromCode;
}

CUString::~CUString() {
	Free();
}

void CUString::Free() {
	if (aType == FromPaged || aType == FromNonPaged)
		ExFreePool(uStr.Buffer);
	uStr.Buffer = NULL;
	uStr.Length = 0;
	uStr.MaximumLength = 0;
}

CUString::CUString(const CUString& orig) {	// copy constructor (required)
	uStr.Length = 0;
	uStr.MaximumLength = orig.uStr.MaximumLength;
	uStr.Buffer = (PWSTR)
		ExAllocatePoolWithTag(PagedPool, uStr.MaximumLength, 1633);
	aType = FromPaged;
	RtlCopyUnicodeString(&uStr, (PUNICODE_STRING)&orig.uStr);
	uStr.Buffer[uStr.Length/2] = UNICODE_NULL;
}

CUString CUString::operator=(const CUString& rop) {	// assignment operator overload (required)
	if (&rop != this) {	// lop == rop ??? why was I called
		if (rop.uStr.Length >= uStr.Length || 	// does it fit?
			(aType != FromPaged && aType != FromNonPaged) ) {
			// it doesn't fit - free up existing buffer
			if (aType == FromPaged || aType == FromNonPaged)
				ExFreePool(uStr.Buffer);
			uStr.Length = 0;
			uStr.MaximumLength = rop.uStr.MaximumLength;
			// and allocate fresh space
			uStr.Buffer = (PWSTR)
				ExAllocatePoolWithTag(PagedPool, uStr.MaximumLength, 1633);
			aType = FromPaged;
		}
		RtlCopyUnicodeString(&uStr, (PUNICODE_STRING)&rop.uStr);
		uStr.Buffer[uStr.Length/2] = UNICODE_NULL;
	}
	return *this;
}
BOOLEAN CUString::operator ==(const CUString& rop) const {
	return RtlEqualUnicodeString(&this->uStr, &rop.uStr, FALSE);	// case matters
}

CUString::operator PWSTR() const {
	return uStr.Buffer;
}

CUString::operator UNICODE_STRING &() {
	return uStr;
}

CUString CUString::operator+(const CUString& rop) const {
	CUString retVal;
	retVal.uStr.Length = this->uStr.Length + rop.uStr.Length;
	retVal.uStr.MaximumLength = max(32, retVal.uStr.Length+2);
	retVal.uStr.Buffer = (PWSTR)
		ExAllocatePoolWithTag(PagedPool, retVal.uStr.MaximumLength, 1633);
	RtlCopyUnicodeString(&retVal.uStr, (PUNICODE_STRING)&this->uStr);
	RtlAppendUnicodeStringToString(&retVal.uStr, (PUNICODE_STRING)&rop.uStr);
	retVal.uStr.Buffer[retVal.uStr.Length/2] = UNICODE_NULL;

	return retVal;
}

CUString& CUString::operator+=(const CUString& rop) {
	*this = *this + rop;
	return *this;
}

CUString::operator ULONG() const {
	ULONG retVal;
	RtlUnicodeStringToInteger((PUNICODE_STRING)&uStr, 0, &retVal);
	return retVal;
}

CUString::CUString(ULONG value) {
	// Converts from a ULONG into a CUString
	uStr.Length = 0;
	uStr.MaximumLength = 32;
	uStr.Buffer = (PWSTR)
		ExAllocatePoolWithTag(PagedPool, uStr.MaximumLength, 1633);
	aType = FromPaged;
	RtlIntegerToUnicodeString(value, 0, &uStr);
}

WCHAR& CUString::operator[](int idx) {
	// accesses an individual WCHAR in CUString buffer
	if (idx >= 0  && idx < uStr.MaximumLength/2)
		return uStr.Buffer[idx];
	else
		return uStr.Buffer[0];	// got to return something
}
