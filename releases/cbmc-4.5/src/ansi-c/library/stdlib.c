/* FUNCTION: abs */

#undef abs

inline int abs(int i) { return __CPROVER_abs(i); }

/* FUNCTION: labs */

#undef labs

inline long int labs(long int i) { return __CPROVER_labs(i); }

/* FUNCTION: exit */

#undef exit

inline void exit(int status)
{
  (void)status;
  __CPROVER_assume(0);
}

/* FUNCTION: abort */

#undef abort

inline void abort(void)
{
  __CPROVER_assume(0);
}

/* FUNCTION: calloc */

#undef calloc
#undef malloc

inline void *malloc(__CPROVER_size_t malloc_size);

inline void *calloc(__CPROVER_size_t nmemb, __CPROVER_size_t size)
{
  __CPROVER_HIDE:;
  __CPROVER_size_t total_size=nmemb*size;
  void *res;
  res=malloc(total_size);
  #ifdef __CPROVER_STRING_ABSTRACTION
  __CPROVER_is_zero_string(res)=1;
  __CPROVER_zero_string_length(res)=0;
  //for(int i=0; i<nmemb*size; i++) res[i]=0;
  #else
  // there should be memset here
  //char *p=res;
  //for(int i=0; i<total_size; i++) p[i]=0;
  #endif
  return res;
}

/* FUNCTION: malloc */

#undef malloc

inline void *malloc(__CPROVER_size_t malloc_size)
{
  // realistically, malloc may return NULL,
  // and __CPROVER_malloc doesn't, but no one cares
  __CPROVER_HIDE:;
  void *res;
  res=__CPROVER_malloc(malloc_size);

  // make sure it's not recorded as deallocated
  __CPROVER_deallocated=(res==__CPROVER_deallocated)?0:__CPROVER_deallocated;
  
  // record the object size for non-determistic bounds checking
  _Bool record_malloc;
  __CPROVER_malloc_object=record_malloc?res:__CPROVER_malloc_object;
  __CPROVER_malloc_size=record_malloc?malloc_size:__CPROVER_malloc_size;
  __CPROVER_malloc_is_new_array=record_malloc?0:__CPROVER_malloc_is_new_array;
  
  return res;
}

/* FUNCTION: free */

#undef free

inline void free(void *ptr)
{
  __CPROVER_HIDE:;
  // If ptr is NULL, no operation is performed.
  if(ptr!=0)
  {
    // is it dynamic?
    __CPROVER_assert(__CPROVER_DYNAMIC_OBJECT(ptr),
                     "free argument is dynamic object");
    __CPROVER_assert(__CPROVER_POINTER_OFFSET(ptr)==0,
                     "free argument has offset zero");

    // catch double free
    if(__CPROVER_deallocated==ptr)
      __CPROVER_assert(0, "double free");
      
    // catch people who try to use free(...) for stuff
    // allocated with new[]
    __CPROVER_assert(__CPROVER_malloc_object!=ptr ||
                     !__CPROVER_malloc_is_new_array,
                     "free called for new[] object");
    
    // non-deterministically record as deallocated
    _Bool record;
    if(record) __CPROVER_deallocated=ptr;
  }
}

/* FUNCTION: strtol */

#ifndef __CPROVER_ERRNO_H_INCLUDED
#include <errno.h>
#define __CPROVER_ERRNO_H_INCLUDED
#endif

#undef strtol
#undef isdigit
#undef isspace

int isspace(int);
int isdigit(int);

inline long strtol(const char *nptr, char **endptr, int base)
{
  __CPROVER_HIDE:;
  #ifdef __CPROVER_STRING_ABSTRACTION
  __CPROVER_assert(__CPROVER_is_zero_string(nptr),
    "zero-termination of argument of strtol");
  #endif

  if(base==1 || base<0 || base>36)
  {
    errno=EINVAL;
    return 0;
  }

  int res=0;
  _Bool in_number=0;
  char sign=0;

  // 32 chars is an arbitrarily chosen limit
  int i=0;
  for( ; i<31; ++i)
  {
    char ch=nptr[i];
    if(ch==0)
      break;
    else if((base==0 || base==16) && !in_number &&
            ch=='0' && (nptr[i+1]=='x' || nptr[i+1]=='X'))
    {
      base=16;
      in_number=1;
      ++i;
    }
    else if(base==0 && !in_number && ch=='0')
    {
      base=8;
      in_number=1;
    }
    else if(!in_number && !sign && isspace(ch))
      continue;
    else if(!in_number && !sign && (ch=='-' || ch=='+'))
      sign=ch;
    else if(base>10 && ch>='a' && ch-'a'<base-10)
    {
      in_number=1;
      res=res*base+ch-'a'+10;
    }
    else if(base>10 && ch>='A' && ch-'A'<base-10)
    {
      in_number=1;
      res=res*base+ch-'A'+10;
    }
    else if(isdigit(ch))
    {
      in_number=1;
      base=base==0 ? 10 : base;
      res=res*base+ch-'0';
    }
    else
      break;
  }

  if(endptr!=0)
    *endptr=(char*)nptr+i;

  if(sign=='-')
    res*=-1;

  return res;
}

/* FUNCTION: atoi */

#undef atoi
#undef strtol

long strtol(const char *nptr, char **endptr, int base);

inline int atoi(const char *nptr)
{
  __CPROVER_HIDE:;
  return (int)strtol(nptr, (char **)0, 10);
}

/* FUNCTION: atol */

#undef atol
#undef strtol

long strtol(const char *nptr, char **endptr, int base);

inline long atol(const char *nptr)
{
  __CPROVER_HIDE:;
  return strtol(nptr, (char **)0, 10);
}

/* FUNCTION: getenv */

#undef getenv

inline char *getenv(const char *name)
{
  __CPROVER_HIDE:;

  (void)*name;
  #ifdef __CPROVER_STRING_ABSTRACTION
  __CPROVER_assert(__CPROVER_is_zero_string(name),
    "zero-termination of argument of getenv");
  #endif

  _Bool found;
  if(!found) return 0;

  char *buffer;
  __CPROVER_size_t buf_size;

  __CPROVER_assume(buf_size>=1);
  buffer=(char *)__CPROVER_malloc(buf_size);
  buffer[buf_size-1]=0;
  return buffer;
}

/* FUNCTION: realloc */

inline void *realloc(void *ptr, __CPROVER_size_t malloc_size)
{
  __CPROVER_HIDE:;

  // if ptr is NULL, this behaves like malloc
  if(ptr==0)
    return malloc(malloc_size);

  // if malloc-size is 0, allocate new minimum sized object
  // and free original
  if(malloc_size==0)
  {
    free(ptr);
    return malloc(1);
  }

  __CPROVER_assert(__CPROVER_DYNAMIC_OBJECT(ptr),
                   "realloc argument is dynamic object");

  // this shouldn't move if the new size isn't bigger
  void *res;
  res=malloc(malloc_size);
  __CPROVER_array_copy(res, ptr);
  free(ptr);

  return res;
}

/* FUNCTION: valloc */

inline void *valloc(__CPROVER_size_t malloc_size)
{
  // The allocated memory is aligned on a page
  // boundary, which we don't model.
     
  __CPROVER_HIDE:;
  return malloc(malloc_size);
}