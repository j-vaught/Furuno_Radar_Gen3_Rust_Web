#ifndef FECTYPE
#define FECTYPE

typedef signed char			CHAR;
typedef unsigned char		UCHAR;
typedef signed char			INT8;
typedef unsigned char		UINT8;
typedef short int			INT16;
typedef	unsigned short int	UINT16;

typedef int			INT32;
typedef	unsigned int	UINT32;

typedef signed long long int	INT64;
typedef unsigned long long int	UINT64;

typedef	short int				FIXED16;
typedef	int				FIXED32;

typedef char TEXT;
typedef	unsigned char UTINY;
typedef	signed char TINY;
typedef short int COUNT;
typedef unsigned short int UCOUNT;
typedef long LONG;
typedef unsigned long ULONG;
typedef double DOUBLE;				/* 64bit floating point data */

#define GLOBAL	extern
#define IMPORT	extern
#define	LOCAL	static
#define	RDONLY	const

#define		LSB		(0)
#define		MSB		(1)

#define		MSBH	(3)
#define		MSBL	(2)
#define		LSBH	(1)
#define		LSBL	(0)

#endif
