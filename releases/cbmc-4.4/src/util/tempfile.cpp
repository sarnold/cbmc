/*******************************************************************\

Module: 

Author: Daniel Kroening

\*******************************************************************/

#ifdef _WIN32
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <tchar.h>
#define getpid _getpid
#define open _open
#define close _close
#endif

#include <cstdlib>
#include <cstring>

#if defined(__linux__) || \
    defined(__FreeBSD_kernel__) || \
    defined(__GNU__) || \
    defined(__CYGWIN__) || \
    defined(__MACH__)
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#endif

#include "i2string.h"
#include "tempfile.h"

/*******************************************************************\

Function: get_temporary_file

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

// the code below is LGPL, really

int my_mkstemps(char *template_str, int suffix_len)
{
  static const char letters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static long long unsigned int value;
  char *XXXXXX;
  size_t len;
  int count;

  len = strlen (template_str);

  if((int) len < 6 + suffix_len ||
     strncmp (&template_str[len - 6 - suffix_len], "XXXXXX", 6))
   return -1;

  XXXXXX = &template_str[len - 6 - suffix_len];

  /* Get some more or less random data.  */
  #ifndef _WIN32
  struct timeval tv;
  gettimeofday (&tv, NULL);
  value += ((long long unsigned int) tv.tv_usec << 16) ^ tv.tv_sec ^ getpid();
  #else
  value += getpid();
  #endif
  
  #define TEMP_FILE_TMP_MAX 10000

  for (count = 0; count < TEMP_FILE_TMP_MAX; ++count)
  {
    long long unsigned int v = value;
    int fd;

    /* Fill in the random bits.  */
    XXXXXX[0] = letters[v % 62];
    v /= 62;
    XXXXXX[1] = letters[v % 62];
    v /= 62;
    XXXXXX[2] = letters[v % 62];
    v /= 62;
    XXXXXX[3] = letters[v % 62];
    v /= 62;
    XXXXXX[4] = letters[v % 62];
    v /= 62;
    XXXXXX[5] = letters[v % 62];

    fd = open(template_str, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (fd >= 0)
      /* The file does not exist.  */
      return fd;

    /* This is a random value.  It is only necessary that the next
       TMP_MAX values generated by adding 7777 to VALUE are different
       with (module 2^32).  */
    value += 7777;
  }

  /* We return the null string if we can't find a unique file name.  */
  template_str[0] = '\0';
  return -1;
}

/*******************************************************************\

Function: get_temporary_file

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::string get_temporary_file(
  const std::string &prefix,
  const std::string &suffix)
{
  #ifdef _WIN32
  char lpTempPathBuffer[MAX_PATH];
  DWORD dwRetVal;

  dwRetVal = GetTempPathA(MAX_PATH,          // length of the buffer
                          lpTempPathBuffer); // buffer for path 

  if (dwRetVal > MAX_PATH || (dwRetVal == 0))
    throw "GetTempPath failed";

  // the path returned by GetTempPath ends with a backslash
  std::string t_template=
    std::string(lpTempPathBuffer)+prefix+
    i2string(getpid())+".XXXXXX"+suffix;
  #else
  std::string t_template=
    "/tmp/"+prefix+i2string(getpid())+".XXXXXX"+suffix;
  #endif

  char *t_ptr=strdup(t_template.c_str());
  
  int fd=my_mkstemps(t_ptr, suffix.size());

  if(fd<0)
    throw "mkstemps failed";  
    
  close(fd);

  std::string result=std::string(t_ptr);  
  free(t_ptr);
  return result;
}

