/*******************************************************************\

Module: Symbolic Execution of ANSI-C

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <fstream>
#include <iostream>
#include <memory>
#include <limits>


#include <util/string2int.h>
#include <util/i2string.h>
#include <util/string2int.h>
#include <util/location.h>
#include <util/time_stopping.h>
#include <util/message_stream.h>
#include <util/signal_catcher.h>

#include <langapi/mode.h>
#include <langapi/languages.h>
#include <langapi/language_util.h>

#include <ansi-c/ansi_c_language.h>

#include <goto-programs/xml_goto_trace.h>
#include <goto-programs/graphml_goto_trace.h>

#include <goto-symex/build_goto_trace.h>
#include <goto-symex/slice.h>
#include <goto-symex/slice_by_trace.h>
#include <goto-symex/memory_model_sc.h>
#include <goto-symex/memory_model_tso.h>
#include <goto-symex/memory_model_pso.h>

#include <solvers/sat/satcheck_minisat2.h>

#include "bmc.h"
#include "bv_cbmc.h"
#include "dimacs.h"
#include "counterexample_beautification.h"

/*******************************************************************\

Function: bmct::do_unwind_module

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::do_unwind_module(
  decision_proceduret &decision_procedure)
{
}

/*******************************************************************\

Function: bmct::error_trace

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::error_trace(const prop_convt &prop_conv)
{
  if(options.get_bool_option("stop-when-unsat")) return; 

  status() << "Building error trace" << eom;

  goto_tracet &goto_trace=safety_checkert::error_trace;
  build_goto_trace(equation, prop_conv, ns, goto_trace);
  
  #if 0
  if(options.get_option("vcd")!="")
  {
    if(options.get_option("vcd")=="-")
      output_vcd(ns, goto_trace, std::cout);
    else
    {
      std::ofstream out(options.get_option("vcd").c_str());
      output_vcd(ns, goto_trace, out);
    }
  }
  #endif
  
  switch(ui)
  {
  case ui_message_handlert::PLAIN:
    std::cout << "\n" << "Counterexample:" << "\n";
    show_goto_trace(std::cout, ns, goto_trace);
    break;
  
  case ui_message_handlert::XML_UI:
    {
      xmlt xml;
      convert(ns, goto_trace, xml);
      std::cout << xml << "\n";
    }
    break;
  
  default:
    assert(false);
  }

  const std::string graphml=options.get_option("graphml-cex");
  if(!graphml.empty())
  {
    graphmlt cex_graph;
    convert(ns, goto_trace, cex_graph);

    if(graphml=="-")
      write_graphml(cex_graph, std::cout);
    else
    {
      std::ofstream out(graphml.c_str());
      write_graphml(cex_graph, out);
    }
  }
}

/*******************************************************************\

Function: bmct::do_conversion

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::do_conversion(prop_convt &prop_conv)
{
  // convert HDL
  do_unwind_module(prop_conv);

  // convert SSA
  equation.convert(prop_conv);

  // the 'extra constraints'
  forall_expr_list(it, bmc_constraints)
    prop_conv.set_to_true(*it);
}

/*******************************************************************\

Function: bmct::run_decision_procedure

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

decision_proceduret::resultt
bmct::run_decision_procedure(prop_convt &prop_conv)
{
  status() << "Passing problem to " 
           << prop_conv.decision_procedure_text() << eom;

  prop_conv.set_message_handler(get_message_handler());

  // stop the time
  absolute_timet sat_start=current_time();

#if 0
  statistics() << "ignored: " <<  equation.count_ignored_SSA_steps() << eom;
  statistics() << "converted: " <<  equation.count_converted_SSA_steps() << eom;
#endif
  
  do_conversion(prop_conv);  

#if 0
  statistics() << "ignored: " <<  equation.count_ignored_SSA_steps() << eom;
  statistics() << "converted: " <<  equation.count_converted_SSA_steps() << eom;
#endif

  status() << "Running " << prop_conv.decision_procedure_text() << eom;

  decision_proceduret::resultt dec_result=prop_conv.dec_solve();
  // output runtime

  {
    absolute_timet sat_stop=current_time();
    status() << "Runtime decision procedure: "
             << (sat_stop-sat_start) << "s" << eom;
  }

  return dec_result;
}

/*******************************************************************\

Function: bmct::report_success

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::report_success()
{
  result() << "VERIFICATION SUCCESSFUL" << eom;

  switch(ui)
  {
  case ui_message_handlert::PLAIN:
    break;
    
  case ui_message_handlert::XML_UI:
    {
      xmlt xml("cprover-status");
      xml.data="SUCCESS";
      std::cout << xml;
      std::cout << "\n";
    }
    break;
    
  default:
    assert(false);
  }
}

/*******************************************************************\

Function: bmct::report_failure

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::report_failure()
{
  result() << "VERIFICATION FAILED" << eom;

  switch(ui)
  {
  case ui_message_handlert::PLAIN:
    break;
    
  case ui_message_handlert::XML_UI:
    {
      xmlt xml("cprover-status");
      xml.data="FAILURE";
      std::cout << xml;
      std::cout << "\n";
    }
    break;
    
  default:
    assert(false);
  }
}

/*******************************************************************\

Function: bmct::show_program

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::show_program()
{
  unsigned count=1;

  languagest languages(ns, new_ansi_c_language());
  
  std::cout << "\n" << "Program constraints:" << "\n";

  for(symex_target_equationt::SSA_stepst::const_iterator
      it=equation.SSA_steps.begin();
      it!=equation.SSA_steps.end(); it++)
  {
    if(it->is_assignment())
    {
      std::string string_value;
      languages.from_expr(it->cond_expr, string_value);
      std::cout << "(" << count << ") " << string_value << "\n";

      if(!it->guard.is_true())
      {
        languages.from_expr(it->guard, string_value);
        std::cout << std::string(i2string(count).size()+3, ' ');
        std::cout << "guard: " << string_value << "\n";
      }
      
      count++;
    }
    else if(it->is_assert())
    {
      std::string string_value;
      languages.from_expr(it->cond_expr, string_value);
      std::cout << "(" << count << ") ASSERT("
                << string_value <<") " << "\n";

      if(!it->guard.is_true())
      {
        languages.from_expr(it->guard, string_value);
        std::cout << std::string(i2string(count).size()+3, ' ');
        std::cout << "guard: " << string_value << "\n";
      }

      count++;
    }  
  }
}

/*******************************************************************\

Function: bmct::run

  Inputs: goto functions

 Outputs: true, if FAILED or an error occurred, false if SUCCEEDED

 Purpose: run BMC

\*******************************************************************/

safety_checkert::resultt bmct::run(
  const goto_functionst &goto_functions)
{
  const std::string mm=options.get_option("mm");
  std::unique_ptr<memory_model_baset> memory_model;
  
  if(mm.empty() || mm=="sc")
    memory_model=std::unique_ptr<memory_model_baset>(new memory_model_sct(ns));
  else if(mm=="tso")
    memory_model=std::unique_ptr<memory_model_baset>(new memory_model_tsot(ns));
  else if(mm=="pso")
    memory_model=std::unique_ptr<memory_model_baset>(new memory_model_psot(ns));
  else
  {
    error() << "Invalid memory model " << mm
            << " -- use one of sc, tso, pso" << eom;
    return safety_checkert::ERROR;
  }

  symex.set_message_handler(get_message_handler());
  symex.options=options;

  status() << "Starting Bounded Model Checking" << eom;

  symex.last_source_location.make_nil();

  try
  {
    // get unwinding info
    setup_unwind();

    safety_checkert::resultt verification_result = safety_checkert::SAFE;

    irep_idt entry_point=goto_functions.entry_point();
    goto_functionst::function_mapt::const_iterator it=
      goto_functions.function_map.find(entry_point);
    assert(it!=goto_functions.function_map.end());
    const goto_programt &body=it->second.body;
    goto_symext::statet symex_state;

    // perform symbolic execution
    bool symex_done = false;

    //THE MAIN LOOP FOR INCREMENTAL UNWINDING
    while(!symex_done) { 

      symex.total_vccs=0;
      symex.remaining_vccs=0;
      symex_done = symex(symex_state,goto_functions,body);

      undo_slice(equation); //undo all previous slicings

#if 0
      equation.output(std::cout);
#endif

      // add a partial ordering, if required    
      if(equation.has_threads())
      {
        memory_model->set_message_handler(get_message_handler());
        (*memory_model)(equation); // TODO: not clear whether supports incremental symex
      }

      statistics() << "size of program expression: "
		   << equation.SSA_steps.size()
		   << " steps" << eom;

      try
      {
        if(options.get_option("slice-by-trace")!="")
        {
          symex_slice_by_tracet symex_slice_by_trace(ns);

          symex_slice_by_trace.slice_by_trace
     	    (options.get_option("slice-by-trace"), equation);
	}

	if(equation.has_threads())
	{
	  // we should build a thread-aware SSA slicer
	  statistics() << "no slicing due to threads" << eom;
	}
	else
	{
	  if(options.get_bool_option("slice-formula"))
	  {
	    slice(equation);
	    statistics() << "slicing removed "
			       << equation.count_ignored_SSA_steps()
			       << " assignments" << eom;
	  }
	  else
	  {
	    simple_slice(equation);
	    statistics() << "simple slicing removed "
			       << equation.count_ignored_SSA_steps()
			       << " assignments" << eom;
	  }
	}

	if(options.get_bool_option("program-only"))
	{
	  show_program();
	  return safety_checkert::SAFE;
	}

	{
	  statistics() << "Generated " << symex.total_vccs
			 << " VCC(s), " << symex.remaining_vccs
			 << " remaining after simplification" << eom;
	}

	if(options.get_bool_option("show-vcc"))
	{
	  show_vcc();
	  if(!symex.is_incremental) 
            return safety_checkert::SAFE; // to indicate non-error
	}
  
	if(options.get_option("cover")!="")
	{
	  satcheckt satcheck;
	  satcheck.set_message_handler(get_message_handler());
	  bv_cbmct bv_cbmc(ns, satcheck);
	  bv_cbmc.set_message_handler(get_message_handler());

	  if(options.get_option("arrays-uf")=="never")
	    bv_cbmc.unbounded_array=bv_cbmct::U_NONE;
	  else if(options.get_option("arrays-uf")=="always")
	    bv_cbmc.unbounded_array=bv_cbmct::U_ALL;
        
	  std::string criterion=options.get_option("cover");
	  return cover(goto_functions, bv_cbmc, criterion)?
	    safety_checkert::ERROR:safety_checkert::SAFE;
	}

	if(symex.remaining_vccs==0)
	{
	  report_success();

          if(symex.is_incremental && !options.get_bool_option("stop-when-unsat"))
          {
            //at this point all other assertions have been checked
	    if(symex.add_loop_check())
	    {
              bool result = !decide(symex.prop_conv,false);
              if(options.get_bool_option("earliest-loop-exit"))
                result = !result;
              symex.update_loop_info(result);
	    } 
            continue;
	  }
          else return safety_checkert::SAFE;  //nothing to check, exit
	}

        //call decision procedure
	if(options.get_bool_option("all-properties")) 
        {
	  if(all_properties(goto_functions,symex.prop_conv)) 
            return safety_checkert::UNSAFE; //all properties FAILED, exit
	}
        else 
        {
         if(symex.remaining_vccs>0)
	  {
             verification_result = decide(symex.prop_conv);
            if(options.get_bool_option("stop-when-unsat") ? 
               !verification_result : //verification succeeds, exit
               verification_result)  //bug found, exit
              return verification_result;
	  }
	    
          if(symex.is_incremental)
          {
            //at this point all other assertions have been checked
            if(symex.add_loop_check())
	    {
              symex.update_loop_info(!decide(symex.prop_conv,false));
	    }
	  }
	}
      }
      catch(std::string &error_str)
      {
        error() << error_str << eom;
        return safety_checkert::ERROR;
      }
      catch(const char *error_str)
      {
        error() << error_str << eom;
        return safety_checkert::ERROR;
      }

    } //while

    return verification_result;
  }
  catch(std::string &error_str)
  {
    error() << error_str << eom;
    return safety_checkert::ERROR;
  }
  catch(const char *error_str)
  {
    error() << error_str << eom;
    return safety_checkert::ERROR;
  }
  catch(std::bad_alloc)
  {
    error() << "Out of memory" << eom;
    return safety_checkert::ERROR;
  }
}

/*******************************************************************\

Function: bmct::decide

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

safety_checkert::resultt bmct::decide(prop_convt &prop_conv, bool show_report)
{
  prop_conv.set_message_handler(get_message_handler());
 
  if(options.get_bool_option("dimacs")) {
    do_conversion(prop_conv);
    return write_dimacs(prop_conv);
  }

  switch(run_decision_procedure(prop_conv))
  {
  case decision_proceduret::D_UNSATISFIABLE:
    report_success();
    return safety_checkert::SAFE;

  case decision_proceduret::D_SATISFIABLE:
    if(options.get_bool_option("beautify")) {
      bv_cbmct& bv_cbmc = dynamic_cast<bv_cbmct&>(prop_conv);
      counterexample_beautificationt()(
        bv_cbmc, equation, ns);
    }
    if(show_report) error_trace(prop_conv);
    report_failure();
    return safety_checkert::UNSAFE;

  default:
    error() << "decision procedure failed" << eom;
    return safety_checkert::ERROR;
  }
}

/*******************************************************************\

Function: bmct::write_dimacs

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

safety_checkert::resultt bmct::write_dimacs(prop_convt& prop_conv) {
  return dynamic_cast<dimacst&>(prop_conv).write_dimacs(
    options.get_option("outfile")) ? 
    safety_checkert::ERROR : safety_checkert::SAFE;
}

/*******************************************************************\

Function: bmct::setup_unwind

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::setup_unwind()
{
  const std::string &set=options.get_option("unwindset");
  std::string::size_type length=set.length();

  for(std::string::size_type idx=0; idx<length; idx++)
  {
    std::string::size_type next=set.find(",", idx);
    std::string val=set.substr(idx, next-idx);

    unsigned thread_nr;
    bool thread_nr_set=false;

    if(!val.empty() &&
       isdigit(val[0]) &&
       val.find(":")!=std::string::npos)
    {
      std::string nr=val.substr(0, val.find(":"));
      thread_nr=unsafe_string2unsigned(nr);
      thread_nr_set=true;
      val.erase(0, nr.size()+1);
    }

    unsigned uw = std::numeric_limits<unsigned>::max();
    std::string id = val;
    if(val.rfind(":")!=std::string::npos)
    {
      id=val.substr(0, val.rfind(":"));
      uw=safe_string2unsigned(val.substr(val.rfind(":")+1).c_str());
    }

    if(thread_nr_set)
    {
      symex.set_unwind_thread_loop_limit(thread_nr, id, uw);
    }
    else
    {
      symex.set_unwind_loop_limit(id, uw);
    }
    
    if(next==std::string::npos) break;
    idx=next;
  }

  if(options.get_option("unwind")!="")
    symex.set_unwind_limit(options.get_unsigned_int_option("unwind"));

  symex.incr_min_unwind=options.get_unsigned_int_option("unwind-min");
  symex.incr_max_unwind=options.get_unsigned_int_option("unwind-max");
  if(symex.incr_max_unwind==0) symex.incr_max_unwind = 
                                 std::numeric_limits<unsigned>::max();
  symex.ignore_assertions = (symex.incr_min_unwind>=2) &&
      options.get_bool_option("ignore-assertions-before-unwind-min");
 
  symex.incr_loop_id = options.get_option("incremental-check");

  //freeze variables where unrollings are stitched together
  if(symex.incr_loop_id!="" || options.get_bool_option("incremental")) 
  {
    status() << "Using incremental mode" << eom;
    symex.is_incremental = true;
    symex.prop_conv.set_all_frozen();
    equation.is_incremental = true;
  }
  //freeze for refinement
  if(options.get_bool_option("refine-arrays"))
  {
    //TODO: make more selective
    symex.prop_conv.set_all_frozen();
  }
}

