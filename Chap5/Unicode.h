// Unicode.h
//
// Copyright (C) 2000 by Jerry Lozano
//
//

#pragma once


class CUString {
public:
	CUString() {Init(); }	// constructor relies on internal Init function
	CUString(const char* pAnsiString);
	CUString(PCWSTR pWideString);
	~CUString();			// destructor gives back buffer allocation
	void Init();			// performs "real" initialization
	void Free();			// performs real destruct
	CUString(const CUString& orig);	// copy constructure (required)
	CUString operator=(const CUString& rop);	// assignment operator overload (required)
	BOOLEAN operator==(const CUString& rop) const;	// comparison operator overload
	CUString operator+(const CUString& rop) const;	// concatenation operator
	operator PWSTR() const;		// cast operator into wchar_t
	operator ULONG() const;		// cast operator into ULONG
	CUString(ULONG value);		// converter:  ULONG->CUString
	WCHAR& operator[](int idx);	// buffer access operator
	USHORT Length() {return uStr.Length/2;}

protected:
	UNICODE_STRING uStr;	// W2K kernel structure for Unicode string
	enum ALLOC_TYPE {Empty, FromCode, FromPaged, FromNonPaged};
	ALLOC_TYPE	aType;		// where buffer is allocated
};
