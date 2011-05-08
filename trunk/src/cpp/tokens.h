// tokens

enum {

Identifier   = 258,
Constant     = 262,
CharConst    = 263,
StringL      = 264,
AssignOp     = 267,
EqualOp      = 268,
RelOp        = 269,
ShiftOp      = 270,
LogOrOp      = 271,
ANDAND       = 272,
IncOp        = 273,
Scope        = 274,
Ellipsis     = 275,
PmOp         = 276,
ArrowOp      = 277,
BadToken     = 278,
AUTO         = 281,
CHAR         = 282,
CLASS        = 283,
CONST        = 284,
DELETE       = 285,
DOUBLE       = 286,
ENUM         = 287,
EXTERN       = 288,
FLOAT        = 289,
FRIEND       = 290,
INLINE       = 291,
INT          = 292,
LONG         = 293,
NEW          = 294,
OPERATOR     = 295,
PRIVATE      = 296,
PROTECTED    = 297,
PUBLIC       = 298,
REGISTER     = 299,
SHORT        = 300,
SIGNED       = 301,
STATIC       = 302,
STRUCT       = 303,
TYPEDEF      = 304,
UNION        = 305,
UNSIGNED     = 306,
VIRTUAL      = 307,
VOID         = 308,
VOLATILE     = 309,
TEMPLATE     = 310,
MUTABLE      = 311,
BREAK        = 312,
CASE         = 313,
CONTINUE     = 314,
DEFAULT      = 315,
DO           = 316,
ELSE         = 317,
FOR          = 318,
GOTO         = 319,
IF           = 320,
RETURN       = 321,
SIZEOF       = 322,
SWITCH       = 323,
THIS         = 324,
WHILE        = 325,
ATTRIBUTE    = 326,  //=g++,
//METACLASS  = 327,
UserKeyword  = 328,
UserKeyword2 = 329,
UserKeyword3 = 330,
UserKeyword4 = 331,
BOOLEAN      = 332,
EXTENSION    = 333,  //=g++,
TRY          = 334,
CATCH        = 335,
THROW        = 336,
UserKeyword5 = 337,
NAMESPACE    = 338,
USING        = 339,
TYPEID       = 340,
WideStringL  = 341,
WideCharConst= 342,
WCHAR        = 343,
EXPLICIT     = 344,
TYPENAME     = 345,
WCHAR_T      = 346,
INT8         = 347,
INT16        = 348,
INT32        = 349,
INT64        = 350,
PTR32        = 351,
PTR64        = 352,
COMPLEX      = 353,
REAL         = 354,
IMAG         = 355,

Ignore       = 500,
GCC_ASM      = 501,
DECLSPEC     = 502,
TYPEOF       = 504,
MSC_ASM      = 505,
THREAD_LOCAL = 506,
DECLTYPE     = 507,
CDECL        = 508,
STDCALL      = 509,
FASTCALL     = 510,
CLRCALL      = 511,
STATIC_ASSERT= 512,

// MSC-specific
MSC_TYPE_PREDICATE = 513,
MSC_BINARY_TYPE_PREDICATE = 514
};

