/*******************************************************************\

Module: Symbolic Execution

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <cassert>
#include <iostream>

#include <util/i2string.h>
#include <util/std_expr.h>
#include <util/expr_util.h>

#include <langapi/language_util.h>
#include <solvers/prop/prop_conv.h>
#include <solvers/prop/prop.h>

#include "goto_symex_state.h"
#include "symex_target_equation.h"

/*******************************************************************\

Function: symex_target_equationt::symex_target_equationt

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

symex_target_equationt::symex_target_equationt(
  const namespacet &_ns):ns(_ns)
{
}

/*******************************************************************\

Function: symex_target_equationt::~symex_target_equationt

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

symex_target_equationt::~symex_target_equationt()
{
}

/*******************************************************************\

Function: symex_target_equationt::shared_read

  Inputs:

 Outputs:

 Purpose: read from a shared variable

\*******************************************************************/

void symex_target_equationt::shared_read(
  const exprt &guard,
  const symbol_exprt &ssa_object,
  const symbol_exprt &original_object,
  unsigned atomic_section_id,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.ssa_lhs=ssa_object;
  SSA_step.original_lhs_object=original_object;
  SSA_step.type=goto_trace_stept::SHARED_READ;
  SSA_step.atomic_section_id=atomic_section_id;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::shared_write

  Inputs:

 Outputs:

 Purpose: write to a sharedvariable

\*******************************************************************/

void symex_target_equationt::shared_write(
  const exprt &guard,
  const symbol_exprt &ssa_object,
  const symbol_exprt &original_object,
  unsigned atomic_section_id,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.ssa_lhs=ssa_object;
  SSA_step.original_lhs_object=original_object;
  SSA_step.type=goto_trace_stept::SHARED_WRITE;
  SSA_step.atomic_section_id=atomic_section_id;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::spawn

  Inputs:

 Outputs:

 Purpose: spawn a new thread

\*******************************************************************/

void symex_target_equationt::spawn(
  const exprt &guard,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::SPAWN;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::memory_barrier

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symex_target_equationt::memory_barrier(
  const exprt &guard,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::MEMORY_BARRIER;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::atomic_begin

  Inputs:

 Outputs:

 Purpose: start an atomic section

\*******************************************************************/

void symex_target_equationt::atomic_begin(
  const exprt &guard,
  unsigned atomic_section_id,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::ATOMIC_BEGIN;
  SSA_step.atomic_section_id=atomic_section_id;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::atomic_end

  Inputs:

 Outputs:

 Purpose: end an atomic section

\*******************************************************************/

void symex_target_equationt::atomic_end(
  const exprt &guard,
  unsigned atomic_section_id,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::ATOMIC_END;
  SSA_step.atomic_section_id=atomic_section_id;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::assignment

  Inputs:

 Outputs:

 Purpose: write to a variable

\*******************************************************************/

void symex_target_equationt::assignment(
  const exprt &guard,
  const symbol_exprt &ssa_lhs,
  const symbol_exprt &original_lhs_object,
  const exprt &ssa_full_lhs,
  const exprt &original_full_lhs,
  const exprt &ssa_rhs,
  const sourcet &source,
  assignment_typet assignment_type)
{
  assert(ssa_lhs.is_not_nil());
  
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.ssa_lhs=ssa_lhs;
  SSA_step.original_lhs_object=original_lhs_object;
  SSA_step.ssa_full_lhs=ssa_full_lhs;
  SSA_step.original_full_lhs=original_full_lhs;
  SSA_step.ssa_rhs=ssa_rhs;
  SSA_step.assignment_type=assignment_type;

  SSA_step.cond_expr=equal_exprt(SSA_step.ssa_lhs, SSA_step.ssa_rhs);
  SSA_step.type=goto_trace_stept::ASSIGNMENT;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::decl

  Inputs:

 Outputs:

 Purpose: declare a fresh variable

\*******************************************************************/

void symex_target_equationt::decl(
  const exprt &guard,
  const symbol_exprt &ssa_lhs,
  const symbol_exprt &original_lhs_object,
  const sourcet &source)
{
  assert(ssa_lhs.is_not_nil());
  
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.ssa_lhs=ssa_lhs;
  SSA_step.ssa_full_lhs=ssa_lhs;
  SSA_step.original_lhs_object=original_lhs_object;
  SSA_step.original_full_lhs=original_lhs_object;
  SSA_step.type=goto_trace_stept::DECL;
  SSA_step.source=source;

  // the condition is trivially true, and only
  // there so we see the symbols
  SSA_step.cond_expr=equal_exprt(SSA_step.ssa_lhs, SSA_step.ssa_lhs);

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::dead

  Inputs:

 Outputs:

 Purpose: declare a fresh variable

\*******************************************************************/

void symex_target_equationt::dead(
  const exprt &guard,
  const symbol_exprt &ssa_lhs,
  const symbol_exprt &original_lhs_object,
  const sourcet &source)
{
  // we currently don't record these
}

/*******************************************************************\

Function: symex_target_equationt::location

  Inputs:

 Outputs:

 Purpose: just record a location

\*******************************************************************/

void symex_target_equationt::location(
  const exprt &guard,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::LOCATION;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::function_call

  Inputs:

 Outputs:

 Purpose: just record a location

\*******************************************************************/

void symex_target_equationt::function_call(
  const exprt &guard,
  const irep_idt &identifier,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::FUNCTION_CALL;
  SSA_step.source=source;
  SSA_step.identifier=identifier;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::function_return

  Inputs:

 Outputs:

 Purpose: just record a location

\*******************************************************************/

void symex_target_equationt::function_return(
  const exprt &guard,
  const irep_idt &identifier,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::FUNCTION_RETURN;
  SSA_step.source=source;
  SSA_step.identifier=identifier;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::output

  Inputs:

 Outputs:

 Purpose: just record output

\*******************************************************************/

void symex_target_equationt::output(
  const exprt &guard,
  const sourcet &source,
  const irep_idt &output_id,
  const std::list<exprt> &args)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::OUTPUT;
  SSA_step.source=source;
  SSA_step.io_args=args;
  SSA_step.io_id=output_id;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::output_fmt

  Inputs:

 Outputs:

 Purpose: just record formatted output

\*******************************************************************/

void symex_target_equationt::output_fmt(
  const exprt &guard,
  const sourcet &source,
  const irep_idt &output_id,
  const irep_idt &fmt,
  const std::list<exprt> &args)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::OUTPUT;
  SSA_step.source=source;
  SSA_step.io_args=args;
  SSA_step.io_id=output_id;
  SSA_step.formatted=true;
  SSA_step.format_string=fmt;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::input

  Inputs:

 Outputs:

 Purpose: just record input

\*******************************************************************/

void symex_target_equationt::input(
  const exprt &guard,
  const sourcet &source,
  const irep_idt &input_id,
  const std::list<exprt> &args)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.type=goto_trace_stept::INPUT;
  SSA_step.source=source;
  SSA_step.io_args=args;
  SSA_step.io_id=input_id;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::assumption

  Inputs: 

 Outputs:

 Purpose: record an assumption

\*******************************************************************/

void symex_target_equationt::assumption(
  const exprt &guard,
  const exprt &cond,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.cond_expr=cond;
  SSA_step.type=goto_trace_stept::ASSUME;
  SSA_step.source=source;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::assertion

  Inputs:

 Outputs:

 Purpose: record an assertion

\*******************************************************************/

void symex_target_equationt::assertion(
  const exprt &guard,
  const exprt &cond,
  const std::string &msg,
  const sourcet &source)
{
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=guard;
  SSA_step.cond_expr=cond;
  SSA_step.type=goto_trace_stept::ASSERT;
  SSA_step.source=source;
  SSA_step.comment=msg;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::constraint

  Inputs: 

 Outputs:

 Purpose: record a constraint

\*******************************************************************/

void symex_target_equationt::constraint(
  const exprt &cond,
  const std::string &msg,
  const sourcet &source)
{
  // like assumption, but with global effect
  SSA_steps.push_back(SSA_stept());
  SSA_stept &SSA_step=SSA_steps.back();
  
  SSA_step.guard=true_exprt();
  SSA_step.cond_expr=cond;
  SSA_step.type=goto_trace_stept::CONSTRAINT;
  SSA_step.source=source;
  SSA_step.comment=msg;

  merge_ireps(SSA_step);
}

/*******************************************************************\

Function: symex_target_equationt::convert

 Inputs: converter, first SSA step to be converted

 Outputs: last converted SSA step

 Purpose: converts from given SSA step

\*******************************************************************/

symex_target_equationt::SSA_stepst::iterator symex_target_equationt::convert(
   prop_convt &prop_conv, SSA_stepst::iterator step)
{
  #if 0
  std::cout << "Converted SSA steps: " << std::endl;
  for(SSA_stepst::iterator it=step;
      it!=SSA_steps.end(); it++) {
    it->output(ns,std::cout);
  }
  #endif

  convert_guards(prop_conv,step);
  convert_assignments(prop_conv,step);
  convert_decls(prop_conv,step);
  convert_assumptions(prop_conv,step);
  convert_assertions(prop_conv,step);
  convert_io(prop_conv,step);
  convert_constraints(prop_conv,step);

  SSA_stepst::iterator ret = SSA_steps.end();
  ret--;
  return ret;
}

/*******************************************************************\

Function: symex_target_equationt::convert

  Inputs: converter

 Outputs: -

 Purpose: converts all SSA steps

\*******************************************************************/

void symex_target_equationt::convert(
  prop_convt &prop_conv)
{
  activate_assertions.clear();
  convert(prop_conv,SSA_steps.begin());
}

/*******************************************************************\

Function: symex_target_equationt::convert_assignments

  Inputs: decision procedure, first SSA step to be converted

 Outputs: -

 Purpose: converts assignments from the given SSA step

\*******************************************************************/

void symex_target_equationt::convert_assignments(
  decision_proceduret &decision_procedure, SSA_stepst::const_iterator step) const
{
  for(SSA_stepst::const_iterator it=step;
      it!=SSA_steps.end(); it++)
  {
    if(it->is_assignment() && !it->ignore)
    {
      //it->asserted_expr = it->cond_expr;
      decision_procedure.set_to_true(it->cond_expr);
    }
  }
}
/*******************************************************************\

Function: symex_target_equationt::convert_assignments

  Inputs: decision procedure

 Outputs: -

 Purpose: converts all assignments from beginning

\*******************************************************************/

void symex_target_equationt::convert_assignments(
  decision_proceduret &decision_procedure) const
{
  SSA_stepst::const_iterator step = SSA_steps.begin(); 
  convert_assignments(decision_procedure,step);
}

/*******************************************************************\

Function: symex_target_equationt::convert_decls

  Inputs: converter, first SSA step to be converted

 Outputs: -

 Purpose: converts declarations from given SSA step

\*******************************************************************/

void symex_target_equationt::convert_decls(
  prop_convt &prop_conv, SSA_stepst::const_iterator step) const
{
  for(SSA_stepst::const_iterator it=step;
      it!=SSA_steps.end(); it++)
  {
    if(it->is_decl() && !it->ignore)
    {
      // The result is not used, these have no impact on
      // the satisfiability of the formula.
      prop_conv.convert(it->cond_expr);
    }
  }
}

/*******************************************************************\

Function: symex_target_equationt::convert_decls

  Inputs: converter

 Outputs: -

 Purpose: converts all declarations from beginning

\*******************************************************************/

void symex_target_equationt::convert_decls(
  prop_convt &prop_conv) const
{
  SSA_stepst::const_iterator step = SSA_steps.begin(); 
  convert_decls(prop_conv,step);
}

/*******************************************************************\

Function: symex_target_equationt::convert_guards

  Inputs: converter, first SSA step to be converted

 Outputs: -

 Purpose: converts guards from given SSA step

\*******************************************************************/

void symex_target_equationt::convert_guards(
  prop_convt &prop_conv, SSA_stepst::iterator step)
{
  for(SSA_stepst::iterator it=step;
      it!=SSA_steps.end(); it++)
  {
    if(it->ignore)
      it->guard_literal=const_literal(false);
    else
      it->guard_literal=prop_conv.convert(it->guard);
  }
}

/*******************************************************************\

Function: symex_target_equationt::convert_guards

  Inputs: converter

 Outputs: -

 Purpose: converts guards from beginning

\*******************************************************************/

void symex_target_equationt::convert_guards(
  prop_convt &prop_conv)
{
  convert_guards(prop_conv,SSA_steps.begin());
}

/*******************************************************************\

Function: symex_target_equationt::convert_assumptions

  Inputs: converter, first SSA step to be converted

 Outputs: -

 Purpose: converts assumptions from given SSA step

\*******************************************************************/

void symex_target_equationt::convert_assumptions(
  prop_convt &prop_conv, SSA_stepst::iterator step)
{
  for(SSA_stepst::iterator it=step;
      it!=SSA_steps.end(); it++)
  {
    if(it->is_assume())
    {
      if(it->ignore)
        it->cond_literal=const_literal(true);
      else {
        it->cond_literal=prop_conv.convert(it->cond_expr);
      }
    }
  }
}

/*******************************************************************\

Function: symex_target_equationt::convert_assumptions

  Inputs: converter

 Outputs: -

 Purpose: converts assumptions from beginning

\*******************************************************************/

void symex_target_equationt::convert_assumptions(
  prop_convt &prop_conv)
{
  convert_assumptions(prop_conv,SSA_steps.begin());
}


/*******************************************************************\

Function: symex_target_equationt::convert_constraints

  Inputs: decision procedure, first SSA step to be converted

 Outputs: -

 Purpose: converts constraints from given SSA step

\*******************************************************************/

void symex_target_equationt::convert_constraints(
  decision_proceduret &decision_procedure, SSA_stepst::const_iterator step) const
{
  for(SSA_stepst::const_iterator it=step;
      it!=SSA_steps.end();
      it++)
  {
    if(it->is_constraint())
    {
      if(it->ignore)
        continue;

      decision_procedure.set_to_true(it->cond_expr);
    }
  }
}

/*******************************************************************\

Function: symex_target_equationt::convert_constraints

  Inputs: decision procedure

 Outputs: -

 Purpose: converts constraints from beginning

\*******************************************************************/

void symex_target_equationt::convert_constraints(
  decision_proceduret &decision_procedure) const
{
  SSA_stepst::const_iterator step = SSA_steps.begin();
  convert_constraints(decision_procedure,step);
}

/*******************************************************************\

Function: symex_target_equationt::convert_assertions

  Inputs: converter, first SSA step to be converted

 Outputs: -

 Purpose: converts assertions from given SSA step

\*******************************************************************/

void symex_target_equationt::convert_assertions(
  prop_convt &prop_conv, SSA_stepst::iterator step)
{
  unsigned number_of_assertions=count_assertions();

  if(number_of_assertions==0)
    return;
    
  /* // not much advantage in the incremental case
  if(number_of_assertions==1)
  {
    for(SSA_stepst::iterator it=SSA_steps.begin();
        it!=SSA_steps.end(); it++)
      if(it->is_assert())
      {
        prop_conv.set_to_false(it->cond_expr);
        //it->asserted_expr = it->cond_expr;
        it->cond_literal=const_literal(false);
        return; // prevent further assumptions!
      }
      else if(it->is_assume())
      {
        //it->asserted_expr = it->cond_expr;
        prop_conv.set_to_true(it->cond_expr);
      }

    assert(false); // unreachable
  }
  */

  bvt bv;
  bv.reserve(number_of_assertions+1); 
  
  literalt assumption_literal=const_literal(true);

  //literal a_k to be added to assertions clauses to de-/activate them for incr. solving
  literalt activation_literal=prop_conv.prop.new_variable();

  //assumptions for incremental solving: (a_0 ... -a_k-1) --> (a_0 ... a_k-1 -a_k)
  bv.push_back(activation_literal);
  if(!activate_assertions.empty()) {
    literalt last_activation_literal = activate_assertions.back();
    activate_assertions.pop_back();
    activate_assertions.push_back(prop_conv.prop.lnot(last_activation_literal));    
  }
  activate_assertions.push_back(prop_conv.prop.lnot(activation_literal));

  for(SSA_stepst::iterator it=SSA_steps.begin();
      it!=SSA_steps.end(); it++) {
    if(it->is_assert())
    {
      // do the expression
      literalt tmp_literal=prop_conv.convert(it->cond_expr);
      it->cond_literal=prop_conv.prop.limplies(assumption_literal, tmp_literal);
      bv.push_back(prop_conv.prop.lnot(it->cond_literal));
    }
    else if(it->is_assume()) {
      assumption_literal=
        prop_conv.prop.land(assumption_literal, it->cond_literal);
    }
  }

  if(!bv.empty())
    prop_conv.prop.lcnf(bv);

  //set assumptions (a_0 ... -a_k) for incremental solving
  prop_conv.prop.set_assumptions(activate_assertions);  
}


/*******************************************************************\

Function: symex_target_equationt::convert_assertions

  Inputs: converter

 Outputs: -

 Purpose: converts assertions from beginning

\*******************************************************************/

void symex_target_equationt::convert_assertions(
  prop_convt &prop_conv)
{
  convert_assertions(prop_conv,SSA_steps.begin());
}

/*******************************************************************\

Function: symex_target_equationt::convert_io

  Inputs: decision procedure, first SSA step to be converted

 Outputs: -

 Purpose: converts I/O from given SSA step

\*******************************************************************/

void symex_target_equationt::convert_io(
  decision_proceduret &dec_proc, SSA_stepst::iterator step)
{
  unsigned io_count=0;

  for(SSA_stepst::iterator it=step;
      it!=SSA_steps.end(); it++)
    if(!it->ignore)
    {
      for(std::list<exprt>::const_iterator
          o_it=it->io_args.begin();
          o_it!=it->io_args.end();
          o_it++)
      {
        exprt tmp=*o_it;
        
        if(tmp.is_constant() ||
           tmp.id()==ID_string_constant)
          it->converted_io_args.push_back(tmp);
        else
        {
          symbol_exprt symbol;
          symbol.type()=tmp.type();
          symbol.set_identifier("symex::io::"+i2string(io_count++));

          equal_exprt eq(tmp, symbol);
          merge_irep(eq);

          dec_proc.set_to(eq, true);
          it->converted_io_args.push_back(symbol);
        }
      }
    }
}

/*******************************************************************\

Function: symex_target_equationt::merge_ireps

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symex_target_equationt::merge_ireps(SSA_stept &SSA_step)
{
  merge_irep(SSA_step.guard);

  merge_irep(SSA_step.ssa_lhs);
  merge_irep(SSA_step.original_lhs_object);
  merge_irep(SSA_step.ssa_full_lhs);
  merge_irep(SSA_step.original_full_lhs);
  merge_irep(SSA_step.ssa_rhs);

  merge_irep(SSA_step.cond_expr);

  for(std::list<exprt>::iterator
      it=SSA_step.io_args.begin();
      it!=SSA_step.io_args.end();
      ++it)
    merge_irep(*it);
  // converted_io_args is merged in convert_io
}

/*******************************************************************\

Function: symex_target_equationt::convert_io

  Inputs: decision procedure

 Outputs: -

 Purpose: converts I/O from beginning

\*******************************************************************/

void symex_target_equationt::convert_io(
  decision_proceduret &dec_proc)
{
  convert_constraints(dec_proc,SSA_steps.begin());
}

/*******************************************************************\

Function: symex_target_equationt::output

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symex_target_equationt::output(std::ostream &out) const
{
  for(SSA_stepst::const_iterator
      it=SSA_steps.begin();
      it!=SSA_steps.end();
      it++)
  {
    it->output(ns, out);    
    out << "--------------" << std::endl;
  }
}

/*******************************************************************\

Function: symex_target_equationt::SSA_stept::output

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symex_target_equationt::SSA_stept::output(
  const namespacet &ns,
  std::ostream &out) const
{
  if(source.is_set)
  {
    out << "Thread " << source.thread_nr;

    if(source.pc->location.is_not_nil())
      out << " " << source.pc->location << std::endl;
    else
      out << std::endl;
  }

  switch(type)
  {
  case goto_trace_stept::ASSERT: out << "ASSERT" << std::endl; break;
  case goto_trace_stept::ASSUME: out << "ASSUME" << std::endl; break;
  case goto_trace_stept::LOCATION: out << "LOCATION" << std::endl; break;
  case goto_trace_stept::INPUT: out << "INPUT" << std::endl; break;
  case goto_trace_stept::OUTPUT: out << "OUTPUT" << std::endl; break;

  case goto_trace_stept::DECL:
    out << "DECL" << std::endl;
    out << from_expr(ns, "", ssa_lhs) << std::endl;
    break;

  case goto_trace_stept::ASSIGNMENT:
    out << "ASSIGNMENT (";
    switch(assignment_type)
    {
    case HIDDEN: out << "HIDDEN"; break;
    case STATE: out << "STATE"; break;
    case PHI: out << "PHI"; break;
    case GUARD: out << "GUARD"; break; 
    default:;
    }

    out << ")" << std::endl;
    break;
    
  case goto_trace_stept::DEAD: out << "DEAD" << std::endl; break;
  case goto_trace_stept::FUNCTION_CALL: out << "FUNCTION_CALL" << std::endl; break;
  case goto_trace_stept::FUNCTION_RETURN: out << "FUNCTION_RETURN" << std::endl; break;
  case goto_trace_stept::CONSTRAINT: out << "CONSTRAINT" << std::endl; break;
  case goto_trace_stept::SHARED_READ: out << "SHARED READ" << std::endl; break;
  case goto_trace_stept::SHARED_WRITE: out << "SHARED WRITE" << std::endl; break;
  case goto_trace_stept::ATOMIC_BEGIN: out << "ATOMIC_BEGIN" << std::endl; break;
  case goto_trace_stept::ATOMIC_END: out << "AUTOMIC_END" << std::endl; break;
  case goto_trace_stept::SPAWN: out << "SPAWN" << std::endl; break;
  case goto_trace_stept::MEMORY_BARRIER: out << "MEMORY_BARRIER" << std::endl; break;

  default: assert(false);
  }

  if(is_assert() || is_assume() || is_assignment() || is_constraint())
    out << from_expr(ns, "", cond_expr) << std::endl;
  
  if(is_assert() || is_constraint())
    out << comment << std::endl;

  if(is_shared_read() || is_shared_write())
    out << from_expr(ns, "", ssa_lhs) << std::endl;

  out << "Guard: " << from_expr(ns, "", guard) << std::endl;
}

/*******************************************************************\

Function: operator <<

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::ostream &operator<<(
  std::ostream &out,
  const symex_target_equationt &equation)
{
  equation.output(out);
  return out;
}

/*******************************************************************\

Function: operator <<

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::ostream &operator<<(
  std::ostream &out,
  const symex_target_equationt::SSA_stept &step)
{
  // may cause lookup failures, since it's blank
  symbol_tablet symbol_table;
  namespacet ns(symbol_table);
  step.output(ns, out);
  return out;
}

