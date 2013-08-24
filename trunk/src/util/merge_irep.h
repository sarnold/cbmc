/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#ifndef CPROVER_MERGE_IREP_H
#define CPROVER_MERGE_IREP_H

#include "irep.h"
#include "hash_cont.h"

class merge_irept
{
public:
  void operator()(irept &);

protected:
  typedef hash_set_cont<irept, irep_hash> irep_storet;
  irep_storet irep_store;     
};

#endif
