/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#ifndef CPROVER_PROPSOLVE_AIG_PROP_H
#define CPROVER_PROPSOLVE_AIG_PROP_H

#include <cassert>

#include <util/threeval.h>

#include "aig.h"

class aig_propt:public propt
{
public:
  explicit inline aig_propt(aigt &_dest):
    dest(_dest)
  {
  }

  virtual bool has_set_to() const { return false; }
 
  virtual literalt land(literalt a, literalt b);
  virtual literalt lor(literalt a, literalt b);
  virtual literalt land(const bvt &bv);
  virtual literalt lor(const bvt &bv);
  virtual void lcnf(const bvt &clause);
  virtual literalt lnot(literalt a);
  virtual literalt lxor(literalt a, literalt b);
  virtual literalt lxor(const bvt &bv);
  virtual literalt lnand(literalt a, literalt b);
  virtual literalt lnor(literalt a, literalt b);
  virtual literalt lequal(literalt a, literalt b);
  virtual literalt limplies(literalt a, literalt b);
  virtual literalt lselect(literalt a, literalt b, literalt c); // a?b:c
  virtual void set_equal(literalt a, literalt b);

  virtual void l_set_to(literalt a, bool value)
  {
    constraints.push_back(a.cond_negation(value));
  }

  virtual literalt new_variable()
  {
    return dest.new_node();
  }
  
  virtual unsigned no_variables() const
  { return dest.number_of_nodes(); }

  virtual const std::string solver_text()
  { return "conversion into and-inverter graph"; }

  virtual tvt l_get(literalt a) const
  { assert(0); return tvt(tvt::TV_UNKNOWN); }
  
  virtual resultt prop_solve()
  { assert(0); return P_ERROR; }
  
protected:
  aigt &dest;

  typedef std::vector<literalt> constraintst;
  constraintst constraints;
};

#endif
