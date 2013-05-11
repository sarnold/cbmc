/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#define USE_SPRINTF

#ifdef USE_SPRINTF

#include <cstdio>
#include <cstring>

#include "i2string.h"

#else

#include <sstream>

#include "i2string.h"

#endif

/*******************************************************************\

Function: i2string

  Inputs: signed integer

 Outputs: string class

 Purpose: convert signed integer to string class

\*******************************************************************/

std::string i2string(int i)
{
  #ifdef USE_SPRINTF
  char buffer[100];
  sprintf(buffer, "%d", i);
  return buffer;
  #else
  std::ostringstream strInt;
  strInt << i;
  return strInt.str();
  #endif
}

/*******************************************************************\

Function: i2string

  Inputs: signed long integer

 Outputs: string class

 Purpose: convert signed integer to string class

\*******************************************************************/

std::string i2string(signed long int i)
{
  #ifdef USE_SPRINTF
  char buffer[100];
  #ifdef _WIN32
  #ifdef __MINGW32__
  snprintf(buffer, sizeof(buffer), "%ld", i);
  #else
  sprintf_s(buffer, sizeof(buffer), "%ld", i);
  #endif
  #else
  snprintf(buffer, sizeof(buffer), "%ld", i);
  #endif
  return buffer;
  #else
  std::ostringstream strInt;
  strInt << i;
  return strInt.str();
  #endif
}

/*******************************************************************\

Function: i2string

  Inputs: unsigned integer

 Outputs: string class

 Purpose: convert unsigned integer to string class

\*******************************************************************/

std::string i2string(unsigned i)
{
  #ifdef USE_SPRINTF
  char buffer[100];
  sprintf(buffer, "%u", i);
  return buffer;
  #else
  std::ostringstream strInt;
  strInt << i;
  return strInt.str();
  #endif
}

/*******************************************************************\

Function: i2string

  Inputs: unsigned long integer

 Outputs: string class

 Purpose: convert unsigned integer to string class

\*******************************************************************/

std::string i2string(unsigned long int i)
{
  #ifdef USE_SPRINTF
  char buffer[100];
  sprintf(buffer, "%lu", i);
  return buffer;
  #else
  std::ostringstream strInt;
  strInt << i;
  return strInt.str();
  #endif
}

/*******************************************************************\

Function: i2string

  Inputs: signed long long

 Outputs: string class

 Purpose: convert signed integer to string class

\*******************************************************************/

std::string i2string(signed long long i)
{
  #ifdef USE_SPRINTF
  char buffer[100];
  sprintf(buffer, "%lld", i);
  return buffer;
  #else
  std::ostringstream strInt;

  strInt << i;
  std::string str;
  strstream2string(strInt, str);

  return str; 
  #endif
}

/*******************************************************************\

Function: i2string

  Inputs: unsigned long long

 Outputs: string class

 Purpose: convert unsigned integer to string class

\*******************************************************************/

std::string i2string(unsigned long long i)
{
  #ifdef USE_SPRINTF
  char buffer[100];
  sprintf(buffer, "%llu", i);
  return buffer;
  #else
  std::ostringstream strInt;

  strInt << i;
  std::string str;
  strstream2string(strInt, str);

  return str; 
  #endif
}
