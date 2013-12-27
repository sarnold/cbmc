/*******************************************************************\

Module: Concrete Symbolic Transformer

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <cstdlib>
#include <iostream>

#include <util/i2string.h>
#include <util/expr_util.h>
#include <util/simplify_expr.h>
#include <util/byte_operators.h>
#include <util/prefix.h>
#include <util/pointer_offset_size.h>

#include <util/arith_tools.h>

#include <ansi-c/c_types.h>

#include "symex.h"

// #define DEBUG

class symext
{
public:
  symext(
    const locst &_locs,
    nodest &_nodes,
    const namespacet &_ns,
    const optionst &_options):
    locs(_locs),
    nodes(_nodes),
    ns(_ns),
    options(_options)
  {
    max_unwind=options.get_int_option("unwind");

    if(max_unwind==0)
      max_unwind=100000000;
  }
  
  void operator()(
    statet &state,
    std::list<statet> &furter_states,
    loc_reft next_loc);

protected:
  const locst &locs;
  nodest &nodes;
  const namespacet &ns;
  const optionst &options;

  unsigned long max_unwind;

  static exprt as_expr(const exprt::operandst &guard);

  void function_call(
    statet &state,
    const code_function_callt &function_call);
    
  void return_from_function(
    statet &state,
    const exprt &return_value);

  void symex_malloc(
	statet &state,
    exprt::operandst &guard,
  	const exprt &lhs,
	const side_effect_exprt &code,
    const std::string &suffix,
    const typet& lhs_suffix_type);


  void assign(statet &state, const exprt &lhs, const exprt &rhs)
  {
	    exprt::operandst _guard; // start with empty guard
  		assign_rec(state, _guard, lhs, rhs, std::string(), ns.follow(lhs.type()));
  }

  inline void assign(statet &state, const code_assignt &assign)
  {
    this->assign(state, assign.lhs(), assign.rhs());
  }

  void assign_rec(
    statet &state,
    exprt::operandst &guard,
    const exprt &lhs, // not instantiated
    const exprt &rhs, // not instantiated
    const std::string &suffix,
    const typet& lhs_suffix_type);

  static bool propagate(const exprt &src);
};

/*******************************************************************\

Function: symext::propagate

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool symext::propagate(const exprt &src)
{
  // propagate things that are 'simple enough'
  bool result = false;  


  if(src.is_constant())
    result=true;
  else if(src.id()==ID_member)
  {
    result=true;
  }
  else if (src.id()==ID_index)
    {
      const index_exprt &src_index_expr=to_index_expr(src);
      result=src_index_expr.index().id()==ID_constant
             && src_index_expr.array().id()==ID_symbol;
  }
  else if(src.id()==ID_typecast)
  {
    assert(src.operands().size()==1);
 
    result=propagate(src.op0());
  }
  else if(src.id()==ID_symbol)
    result=true;
  else if(src.id()==ID_plus)
  {
    result=propagate(src.op0()) && propagate(src.op1());
  }  
  
  #ifdef DEBUG
    /* 
    std::cout << "symext::propagate: " << (result ? "yes" : "no") 
              << " " << src.id_string() << std::endl;
     */
  #endif
 

  return result;
}

/*******************************************************************\

Function: symext::as_expr

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

exprt symext::as_expr(const exprt::operandst &guard)
{
  if(guard.empty())
    return true_exprt();
  else if(guard.size()==1)
    return guard.front();
  else
  {
    exprt result(ID_and, bool_typet());
    result.operands()=guard;
    return result;
  }
}


/*******************************************************************\

Function: symext::symex_malloc

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/


inline static typet c_sizeof_type_rec(const exprt &expr)
{
  const irept &sizeof_type=expr.find(ID_C_c_sizeof_type);

  if(!sizeof_type.is_nil())
  {
    return static_cast<const typet &>(sizeof_type);
  }
  else if(expr.id()==ID_mult)
  {
    forall_operands(it, expr)
    {
      typet t=c_sizeof_type_rec(*it);
      if(t.is_not_nil()) return t;
    }
  }
  
  return nil_typet();
}

void symext::symex_malloc(
  statet &state,
  exprt::operandst &guard, // not instantiated
  const exprt &lhs,
  const side_effect_exprt &code,
  const std::string &suffix,
  const typet& lhs_suffix_type)
{
  if(code.operands().size()!=1)
    throw "malloc expected to have one operand";
    
  if(lhs.is_nil())
    return; // ignore

  // increment dynamic object counter
  unsigned dynamic_count=++state.var_map.dynamic_count;
  
  exprt size=code.op0();
  typet object_type=nil_typet();
  
  {
    exprt tmp_size=state.read(size); // to allow constant propagation
    
    // special treatment for sizeof(T)*x
    if(tmp_size.id()==ID_mult &&
       tmp_size.operands().size()==2 &&
       tmp_size.op0().find(ID_C_c_sizeof_type).is_not_nil())
    {
      object_type=array_typet(
        c_sizeof_type_rec(tmp_size.op0()),
        tmp_size.op1());      
    }
    else
    {
      typet tmp_type=c_sizeof_type_rec(tmp_size);
      
      if(tmp_type.is_not_nil())
      {
        // Did the size get multiplied?
        mp_integer elem_size=pointer_offset_size(ns, tmp_type);
        mp_integer alloc_size;
        if(elem_size<0 || to_integer(tmp_size, alloc_size))
        {
        }
        else
        {
          if(alloc_size==elem_size)
            object_type=tmp_type;
          else
          {
            mp_integer elements=alloc_size/elem_size;
            
            if(elements*elem_size==alloc_size)
              object_type=array_typet(tmp_type, from_integer(elements, tmp_size.type()));
          }
        }
      }
    }
    
    if(object_type.is_nil())
      object_type=array_typet(unsigned_char_type(), tmp_size);

    // we introduce a fresh symbol for the size
    // to prevent any issues of the size getting ever changed
    
    if(object_type.id()==ID_array &&
       !to_array_type(object_type).size().is_constant())
    {
      exprt &size=to_array_type(object_type).size();
    
      symbolt size_symbol;

      size_symbol.base_name="dynamic_object_size"+i2string(dynamic_count);
      size_symbol.name="symex::"+id2string(size_symbol.base_name);
      size_symbol.is_lvalue=true;
      size_symbol.type=tmp_size.type();
      size_symbol.mode=ID_C;

      state.var_map(size_symbol.name, suffix, size_symbol.type);


      assign_rec(state,
                 guard,
                 size_symbol.symbol_expr(), 
                 size,
                 suffix,
                 lhs_suffix_type);

      size=size_symbol.symbol_expr();
    }
  }
  
  // value
  symbolt value_symbol;

  value_symbol.base_name="dynamic_object"+i2string(state.var_map.dynamic_count);
  value_symbol.name="symex::"+id2string(value_symbol.base_name);
  value_symbol.is_lvalue=true;
  value_symbol.type=object_type;
  value_symbol.type.set("#dynamic", true);
  value_symbol.mode=ID_C;

  state.var_map(value_symbol.name, suffix, value_symbol.type);

  address_of_exprt rhs;
  
  if(object_type.id()==ID_array)
  {
    rhs.type()=pointer_typet(value_symbol.type.subtype());
    index_exprt index_expr(value_symbol.type.subtype());
    index_expr.array()=value_symbol.symbol_expr();
    index_expr.index()=gen_zero(index_type());
    rhs.op0()=index_expr;
  }
  else
  {
    rhs.op0()=value_symbol.symbol_expr();
    rhs.type()=pointer_typet(value_symbol.type);
  }
  
  if(rhs.type()!=lhs.type())
    rhs.make_typecast(lhs.type());

  assign_rec(state,
             guard,
             lhs,
             rhs,
             suffix,
             object_type);
}



/*******************************************************************\

Function: symext::assign_rec

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symext::assign_rec(
  statet &state,
  exprt::operandst &guard, 
  const exprt &lhs, 
  const exprt &rhs, 
  const std::string &suffix,
  const typet& lhs_suffix_type) 
{
  if(lhs_suffix_type.id()==ID_struct) // lhs is a struct
  {
	const struct_typet &struct_type=to_struct_type(lhs_suffix_type);
	const struct_typet::componentst &components=struct_type.components();

	bool is_struct_const=rhs.id() == ID_struct;

	int i=0;

	for(struct_typet::componentst::const_iterator
      it=components.begin();
      it!=components.end();
      it++)
	{
		const typet &subtype=it->type();	
		const irep_idt& component_name=it->get_name();
		typet lhs_type=ns.follow(subtype);

		// add to suffix
		const std::string new_suffix=
			suffix + "."+id2string(component_name); // note the order

		exprt component;

		if(is_struct_const) 
		{
			component=rhs.operands()[i];
		}
		else
		{
			component=member_exprt(rhs, component_name, lhs_type);
		}
		
		assign_rec(state, guard, lhs, component, new_suffix, lhs_type);
		++i;
	 }
	 return;
  } 
  else if(rhs.id()==ID_sideeffect) // catch side effects on rhs
	{
		const side_effect_exprt &side_effect_expr=to_side_effect_expr(rhs);
		const irep_idt &statement=side_effect_expr.get_statement();

		if(statement==ID_malloc)
		{
			symex_malloc(state, guard, lhs, side_effect_expr, suffix, lhs_suffix_type);
			return;
		}
	} 
	
  if(lhs.id()==ID_symbol)
  {
    // We might have SSA variables if this comes from dereferenced point.
    // TODO: Please, check!     
    exprt original_lhs=state.original_name(lhs);
    exprt original_rhs=state.original_name(rhs);

    // Deal with LHS.
    const symbol_exprt &symbol_expr=to_symbol_expr(original_lhs);
    const irep_idt &identifier=symbol_expr.get_identifier();
    
    typet lhs_type= suffix.size() ? lhs_suffix_type : lhs.type();

    var_mapt::var_infot &var_info=
      state.var_map(identifier, suffix, lhs_type );

    // increase SSA counter and produce symbol expression
    var_info.increment_ssa_counter();
 
    symbol_exprt ssa_lhs_post = symbol_exprt(var_info.ssa_identifier(state.get_current_thread()), 
                                             var_info.type);

    // setup final RHS: deal with arrays on LHS
    exprt final_rhs=original_rhs ;

    // now dereference, instantiate, propagate RHS

    exprt rhs_deref=state.dereference(final_rhs);
    exprt ssa_rhs_no_prop=state.read_no_propagate(rhs_deref);

    exprt ssa_rhs_prop=state.read(rhs_deref);


    
    // make sure that rhs and lhs have matching types

	/*
    if(ssa_rhs_no_prop.is_not_nil() && ssa_rhs_no_prop.type() != lhs_type)
    {

      std::cout << "different types " << std::endl;
      exprt rhs_typecasted=ssa_rhs_no_prop;
      rhs_typecasted.make_typecast(lhs_type);
      ssa_rhs_no_prop=rhs_typecasted;
    }
    */

    // record the step
    statet::stept &step=state.record_step();
    step.guard=as_expr(guard);
    step.guard_no_prop=as_expr(guard);
    step.lhs=lhs;
    step.ssa_lhs=ssa_lhs_post;
    step.ssa_rhs=ssa_rhs_prop;
    step.ssa_rhs_no_prop=ssa_rhs_no_prop;

    statet::var_statet &var_state=state.get_var_state(var_info);
    var_state.identifier=ssa_lhs_post.get_identifier();

    // propagate the rhs?
    var_state.value=propagate(ssa_rhs_prop) ? ssa_rhs_prop : nil_exprt();
    var_state.step_number=state.history.size()-1;

    #ifdef DEBUG
    std::cout << "assign_rec ID_symbol " << identifier << suffix << std::endl;
    #endif
  }
  else if(lhs.id()==ID_member)
  {
    #ifdef DEBUG
    std::cout << "assign_rec ID_member" << std::endl;
    #endif

    const member_exprt &lhs_member_expr=to_member_expr(lhs);
  
    // add to suffix
    const std::string new_suffix=
      "."+id2string(lhs_member_expr.get_component_name())+suffix;

    const exprt& struct_op = lhs_member_expr.struct_op();

    typet lhs_type=state.var_map.ns.follow(lhs_suffix_type);

    assign_rec(state, guard, struct_op, rhs, new_suffix, lhs_type);
  }
  else if(lhs.id()==ID_index)
  {
    #ifdef DEBUG
    std::cout << "assign_rec ID_index" << std::endl;
    #endif

    const index_exprt &lhs_index_expr=to_index_expr(lhs);

    // lhs must be index operand
    // that takes two operands: the first must be an array
    // the second is the index

    if(lhs.operands().size()!=2)
      throw "index must have two operands";

    const exprt &lhs_array=lhs_index_expr.array();
    const exprt &lhs_index=lhs_index_expr.index();
    const typet &lhs_type=ns.follow(lhs_array.type());
    
    if(lhs_type.id()!=ID_array)
      throw "index must take array type operand, but got `"+
        lhs_type.id_string()+"'\n in expr " + lhs.pretty(2) + "\n";
    
    exprt new_rhs(ID_with, lhs_type);
    new_rhs.copy_to_operands(lhs_array, lhs_index, rhs);
       
    assign_rec(state, guard, lhs_array, new_rhs, suffix, lhs_suffix_type);
  }
  else if(lhs.id()==ID_dereference)
  {
    #ifdef DEBUG
    std::cout << "assign_rec ID_dereference" << std::endl;
    #endif

    exprt tmp_lhs=state.dereference(lhs);

	  simplify(tmp_lhs, state.var_map.ns);

    if(tmp_lhs.id() == ID_dereference) // otherwise nontermination
    {
	    throw "symext::operator(): unable to resolve pointer of lhs " + tmp_lhs.pretty(2);
    }

    assign_rec(state, guard, tmp_lhs, rhs, suffix, lhs_suffix_type);

    #ifdef DEBUG
    //std::cout << "assign_rec ID_dereference (done)" << std::endl;
    #endif

  }
  else if(lhs.id()==ID_if)
  {
    #ifdef DEBUG
    std::cout << "assign_rec ID_if" << std::endl;
    #endif

    const if_exprt &lhs_if_expr=to_if_expr(lhs);
    exprt cond=lhs_if_expr.cond();

    // true
    guard.push_back(cond);
    assign_rec(state, guard, lhs_if_expr.true_case(), rhs, suffix, lhs_suffix_type);
    guard.pop_back();
    
    // false
    guard.push_back(not_exprt(cond));
    assign_rec(state, guard, lhs_if_expr.false_case(), rhs, suffix, lhs_suffix_type);
    guard.pop_back();
  }
  else if(lhs.id()==ID_byte_extract_little_endian ||
          lhs.id()==ID_byte_extract_big_endian)
  {
    #ifdef DEBUG
    std::cout << "assign_rec ID_byte_extract" << std::endl;
    #endif

    const byte_extract_exprt &byte_extract_expr=
      to_byte_extract_expr(lhs);
  
    // assignment to byte_extract operators:
    // turn into byte_update operator
    
    // This requires a split over any struct fields, which
    // is todo.
    
    irep_idt new_id;
    
    if(lhs.id()==ID_byte_extract_little_endian)
      new_id=ID_byte_update_little_endian;
    else if(lhs.id()==ID_byte_extract_big_endian)
      new_id=ID_byte_update_big_endian;
    else
      assert(false);

    byte_update_exprt new_rhs(new_id);

    new_rhs.type()=byte_extract_expr.op().type();
    new_rhs.op()=byte_extract_expr.op();
    new_rhs.offset()=byte_extract_expr.offset();
    new_rhs.value()=rhs;

    assign_rec(state, guard, lhs.op0(), new_rhs, suffix, lhs_suffix_type);
  }
  else if(lhs.id()==ID_string_constant ||
          lhs.id()=="NULL-object" ||
          lhs.id()=="zero_string" ||
          lhs.id()=="is_zero_string" ||
          lhs.id()=="zero_string_length")
  {
    // ignore
  }
  else
  {
	  // ignore
    //throw "symext::assign_rec(): unexpected lhs: ";

  }
}

/*******************************************************************\

Function: symext::function_call

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symext::function_call(
  statet &state,
  const code_function_callt &function_call)
{
  assert(function_call.function().id()==ID_symbol);

  const irep_idt& function_identifier=to_symbol_expr(function_call.function()).get_identifier();

  // find the function
  var_mapt::function_mapt::const_iterator f_it=
    state.var_map.function_map.find(function_identifier);

  if(f_it==state.var_map.function_map.end())
    throw "failed to find `"+id2string(function_identifier)+"' in function_map";
  
  const var_mapt::function_entryt& function_entry (f_it->second);

  loc_reft function_entry_point=function_entry.first;
  
  // do we have a body?
  if(function_entry_point==loc_reft())
  {
    // no body
    state.next_pc();
    return;
  }
  
  // push a frame on the call stack
  statet::threadt &thread=state.threads[state.get_current_thread()];
  thread.call_stack.push_back(statet::framet());
  thread.call_stack.back().return_location=thread.pc.next_loc();
  thread.call_stack.back().return_lhs=function_call.lhs();
  thread.call_stack.back().saved_local_vars=thread.local_vars;

   const code_typet &code_type=
    to_code_type(ns.follow(function_entry.second));

  const code_typet::argumentst &function_arguments=code_type.arguments();

  const exprt::operandst &call_arguments=function_call.arguments();
  

  // now assign the argument values
  for(unsigned i=0; i<call_arguments.size(); i++)
  {
    if(i<function_arguments.size())
    {
      const code_typet::argumentt &function_argument=function_arguments[i];
      irep_idt identifier=function_argument.get_identifier();

      if(identifier==irep_idt())
        throw "function_call " + id2string(function_identifier) + " no identifier for function argument";

      symbol_exprt lhs(identifier, function_argument.type());
            
      // TODO: need to save+restore

      assign(state, lhs, call_arguments[i]);
    }
  }

  // set the new PC
  thread.pc=function_entry_point;
}

/*******************************************************************\

Function: symext::return_from_function

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symext::return_from_function(
  statet &state,
  const exprt &return_value)
{
  if(state.threads[state.get_current_thread()].call_stack.empty())
  {
    state.threads[state.get_current_thread()].active=false;
  }
  else
  {
    statet::threadt &thread=state.threads[state.get_current_thread()];
    thread.pc=thread.call_stack.back().return_location;

    if(return_value.is_not_nil() &&
       thread.call_stack.back().return_lhs.is_not_nil())
      assign(state, thread.call_stack.back().return_lhs, return_value);

    thread.call_stack.pop_back();
  }
}

/*******************************************************************\

Function: symext::operator()

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symext::operator()(
  statet &state,
  std::list<statet> &further_states,
  loc_reft next_loc)
{
  unsigned thread_nr=state.get_current_thread();
  const statet::threadt &thread=state.threads[thread_nr];
  const loct &loc=locs[thread.pc];
  const goto_programt::instructiont &instruction=*loc.target;

  #ifdef DEBUG

  std::cout << "EXE ";

  std::cout << "( ";

  for(unsigned i=0; i<state.threads.size(); ++i)
  {
    const statet::threadt &thread=state.threads[i];

    if(i==thread_nr)
    {
      std::cout << "<";
    }

    std::cout << thread.pc;

    if(i==thread_nr)
    {
      std::cout << "> ";
    }
    else
    {
      std::cout << " ";
    }
  }

  std::cout << ")";

  std::cout << " N" << state.node->number
            << " " << as_string(ns, instruction)
            << std::endl;
  #endif


  #ifndef DEBUG  
  try {
  #endif

  switch(instruction.type)
  {
  case END_FUNCTION:
    // pop the call stack
    state.record_step();
    return_from_function(state, nil_exprt());
    break;
    
  case RETURN:
    // pop the call stack
    {
      state.record_step();
      exprt v=instruction.code.operands().size()==1?
              instruction.code.op0():nil_exprt();
      return_from_function(state, v);
    }
    break;
    
  case START_THREAD:
    {
      assert(loc.targets.size()==1);
      state.record_step();
      state.next_pc();
      
      // ordering of the following matters due to vector instability
      statet::threadt &new_thread=state.add_thread();
      statet::threadt &old_thread=state.threads[state.get_current_thread()];
      new_thread.pc=loc.targets.front();
      new_thread.local_vars=old_thread.local_vars;
    }
    break;
    
  case END_THREAD:
    state.record_step();
    state.threads[thread_nr].active=false;
    break;
    
  case GOTO:
    state.record_step();
    
    if(instruction.is_backwards_goto())
    {
      unsigned unwinding_steps=++state.unwind_map[state.pc().loc_number];


      if(unwinding_steps > max_unwind) {
        std::cout << "unwinding steps " << state.pc().loc_number << " : " << unwinding_steps << std::endl;
        state.threads[thread_nr].active=false;
        break;
      }
    }

    // first do hard, deterministic gotos
    if(instruction.guard.is_false())
    {
      // really a SKIP
      state.next_pc();
    }
    else if(instruction.guard.is_true() &&
            loc.targets.size()==1)
    {
      // unconditional GOTO
      state.set_pc(loc.targets.front());
    }
    else
    {
      // we force that both branches are pursued (if the target is unknown)
      exprt deref_guard=state.dereference(instruction.guard);
      exprt guard=state.read(deref_guard);
      exprt guard_no_prop=state.read_no_propagate(deref_guard);
      
      if(next_loc.loc_number==(unsigned)-1) // branch target unknown
      {
        // branch taken (possibly more than one target)
        for(loct::targetst::const_iterator
            t_it=loc.targets.begin();
            t_it!=loc.targets.end();
            t_it++)
        {
          further_states.push_back(state); // fresh copy
          statet &state_taken=further_states.back(); 
          state_taken.history.back().guard=guard;
          state_taken.history.back().guard_no_prop=guard_no_prop;
          state_taken.set_pc(*t_it);
        }
        
        // branch not taken -- order is important!
        if(guard.is_true())
        {
          state.history.back().guard=false_exprt();
        }
        else if(guard.is_false())
        {
          state.history.back().guard=true_exprt();
        }
        else {
          state.history.back().guard=not_exprt(guard);
        }

        state.history.back().guard_no_prop=not_exprt(guard_no_prop);

        assert(!state.history.back().guard_no_prop.is_nil());

        state.next_pc();
      } else { // branch target known
        // true branch or false branch?
        bool branch_taken=false;

        // branch taken?
        for(loct::targetst::const_iterator
            t_it=loc.targets.begin();
            t_it!=loc.targets.end();
            t_it++)
        {
          if(*t_it == next_loc)
          {
            branch_taken=true;
            break;          
          }
        }

        if(branch_taken)
        {
          state.history.back().guard=guard;
          state.history.back().guard_no_prop=guard_no_prop;
        } else {
          if(guard.is_true())
          {
            state.history.back().guard=false_exprt();
          }
          else if(guard.is_false())
          {
            state.history.back().guard=true_exprt();
          }
          else {
            state.history.back().guard=not_exprt(guard);
          }
          state.history.back().guard_no_prop=not_exprt(guard_no_prop);
        }        
        state.set_pc(next_loc);
      }
    }

    break;
    
  case CATCH:
    // ignore for now
    state.record_step();
    state.next_pc();
    break;
    
  case THROW:
    state.record_step();
    throw "THROW not yet implemented";
    
  case ASSUME:
    state.record_step();
    if(instruction.guard.is_false())
      state.threads[thread_nr].active=false;
    else
    {
      exprt deref_guard=state.dereference(instruction.guard);
      exprt guard=state.read(deref_guard);
      exprt guard_no_prop=state.read_no_propagate(deref_guard);
      state.history.back().guard=guard; //gen_not(guard);
      state.history.back().guard_no_prop=guard_no_prop;
      state.next_pc();
    }
    break;
    
  case ASSERT:
  case SKIP:
  case LOCATION:
  case DEAD:
    state.record_step();
    state.next_pc();
    break;

  case DECL:
    // an RHS of NIL means 'nondet'
    assign(state, to_code_decl(instruction.code).symbol(), nil_exprt());
    state.next_pc();
    break;

  case ATOMIC_BEGIN:
    state.record_step();
    state.next_pc();
    ++state.atomic_section_count;
    break;

  case ATOMIC_END:
    if(state.atomic_section_count==0)
      throw "ATOMIC_END unmatched";

    state.record_step();
    state.next_pc();
    --state.atomic_section_count;
    break;
    
  case ASSIGN:
    assign(state, to_code_assign(instruction.code));
    state.next_pc();
    break;
    
  case FUNCTION_CALL:
    state.record_step();
    function_call(state, to_code_function_call(instruction.code));
    break;
    
  case OTHER:
    state.record_step();

    {
      const codet &code=instruction.code;
      const irep_idt &statement=code.get_statement();

      if(statement==ID_expression)
      {
        // like SKIP
      }
      else if(statement==ID_printf)
      {
        // ignore for now (should record stuff printed)
      }
      else if(statement==ID_asm)
      {
        // ignore for now
      }
      else if(statement==ID_fence)
      {
        // ignore for SC
      }
      else
      {
        std::cerr << "unexpected OTHER statement: " << statement << std::endl;
        // ignore
      }
    }

    state.next_pc();
    break;

  default:
    std::cerr << "Not implemented: " << instruction.type << std::endl;
    // ignore
  }
  
  #ifndef DEBUG
  }
  catch (std::string& msg)
  {
    state.show_vcc(state.history[0].node,true_exprt(),true_exprt(), true, std::cout);

    std::string new_msg(" T" + i2string(thread_nr) 
                      + " Node " + i2string(state.node->number)
                      + " " + as_string(ns, instruction));

    throw "symext::operator(): " + new_msg + "\n  " + msg;
  }
  #endif
}

/*******************************************************************\

Function: symex

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symex(
  const locst &locs,
  nodest &nodes,
  statet &state,
  std::list<statet> &further_states,
  const namespacet &ns,
  const optionst &options)
{
  symext symex_class(locs, nodes, ns, options);
  
  loc_reft next_loc;

  symex_class(state, further_states, next_loc);
}

/*******************************************************************\

Function: symex

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void symex(
  const locst &locs,
  nodest &nodes,
  statet &state,
  nodet* node,
  const namespacet &ns,
  const optionst &options)
{
  unsigned path_length=0;

  #ifdef DEBUG
  std::cout << "path symex " << std::endl;
  #endif

  if(node == state.node)
	  return;

  for(nodet* 
      current=node;
      current&&current!=state.node;
      current=current->parent)
  {
    ++path_length;
  }

  std::vector<nodet*> path;

  path.reserve(path_length);

  nodet* current;

  for(current=node;
      current&&current!=state.node;
      current=current->parent)
  {
    path.push_back(current);
  }

  assert(current == state.node);

  symext symex_class(locs, nodes, ns, options);

  std::list<statet> further_states;

  for(int 
      i=path.size()-1;
      i>=0;
      --i)
  {
    nodet* current=path[i];

    // get the class of the node
    node_equiv_classt *class_ptr=current->class_ptr;    

    // get the location in the current thread
    const global_vectort& global_vector=*(class_ptr->global_vector);

    unsigned thread=current->thread;

    assert(thread!=unsigned(-1));

    loc_reft next_loc=global_vector[thread].back(); 
    state.set_current_thread(thread);

    symex_class(state, further_states, next_loc);
    state.node=current;
  }

  assert(state.node==node);

  assert(further_states.empty());  

  #ifdef DEBUG
  std::cout << "End of path symex " << std::endl;
  #endif

}



