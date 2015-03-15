/*******************************************************************\

Module: Solvers for VCs Generated by Symbolic Execution of ANSI-C

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <memory>
#include <iostream>

#include <solvers/sat/satcheck.h>
#include <solvers/sat/satcheck_minisat2.h>

#include <solvers/refinement/bv_refinement.h>
#include <solvers/smt1/smt1_dec.h>
#include <solvers/smt2/smt2_dec.h>
#include <solvers/cvc/cvc_dec.h>

#include <solvers/prop/aig_prop.h>

#include "bmc.h"
#include "bv_cbmc.h"
#include "counterexample_beautification.h"
#include "version.h"

/*******************************************************************\

Function: bmct::get_smt1_solver_type

  Inputs: None

 Outputs: An smt1_dect::solvert giving the solver to use.

 Purpose: Uses the options to pick an SMT 1.2 solver

\*******************************************************************/

smt1_dect::solvert bmct::get_smt1_solver_type() const
{
  assert(options.get_bool_option("smt1"));

  smt1_dect::solvert s = smt1_dect::GENERIC;
  
  if(options.get_bool_option("boolector"))
    s = smt1_dect::BOOLECTOR;
  else if(options.get_bool_option("mathsat"))
    s = smt1_dect::MATHSAT;
  else if(options.get_bool_option("cvc3"))
    s = smt1_dect::CVC3;
  else if(options.get_bool_option("cvc4"))
    s = smt1_dect::CVC4;
  else if(options.get_bool_option("opensmt"))
    s = smt1_dect::OPENSMT;
  else if(options.get_bool_option("yices"))
    s = smt1_dect::YICES;
  else if(options.get_bool_option("z3"))
    s = smt1_dect::Z3;
  else if(options.get_bool_option("generic"))
    s = smt1_dect::GENERIC;

  return s;
}

/*******************************************************************\

Function: bmct::get_smt2_solver_type

  Inputs: None

 Outputs: An smt2_dect::solvert giving the solver to use.

 Purpose: Uses the options to pick an SMT 2.0 solver

\*******************************************************************/

smt2_dect::solvert bmct::get_smt2_solver_type() const
{
  assert(options.get_bool_option("smt2"));

  smt2_dect::solvert s = smt2_dect::GENERIC;
  
  if(options.get_bool_option("boolector"))
    s = smt2_dect::BOOLECTOR;
  else if(options.get_bool_option("mathsat"))
    s = smt2_dect::MATHSAT;
  else if(options.get_bool_option("cvc3"))
    s = smt2_dect::CVC3;
  else if(options.get_bool_option("cvc4"))
    s = smt2_dect::CVC4;
  else if(options.get_bool_option("opensmt"))
    s = smt2_dect::OPENSMT;
  else if(options.get_bool_option("yices"))
    s = smt2_dect::YICES;
  else if(options.get_bool_option("z3"))
    s = smt2_dect::Z3;
  else if(options.get_bool_option("generic"))
    s = smt2_dect::GENERIC;

  return s;
}

/*******************************************************************\

Function: bmct::solver_factory

  Inputs:

 Outputs:

 Purpose: Decide using "default" decision procedure

\*******************************************************************/

prop_convt *bmct::solver_factory()
{
  //const std::string &filename=options.get_option("outfile");
  
  if(options.get_bool_option("boolector"))
  {
  }
  else if(options.get_bool_option("mathsat"))
  {
  }
  else if(options.get_bool_option("cvc"))
  {
  }
  else if(options.get_bool_option("dimacs"))
  {
  }
  else if(options.get_bool_option("opensmt"))
  {
  }
  else if(options.get_bool_option("refine"))
  {
  }
  else if(options.get_bool_option("aig"))
  {
  }
  else if(options.get_bool_option("smt1"))
  {
  }
  else if(options.get_bool_option("smt2"))
  {
  }
  else if(options.get_bool_option("yices"))
  {
  }
  else if(options.get_bool_option("z3"))
  {
  }
  else
  {
    // THE DEFAULT

    #if 0
    // SAT preprocessor won't work with beautification.
    if(options.get_bool_option("sat-preprocessor") &&
       !options.get_bool_option("beautify"))
    {
      solver=std::auto_ptr<propt>(new satcheckt);
    }
    else
      solver=std::auto_ptr<propt>(new satcheck_minisat_no_simplifiert);

    solver->set_message_handler(get_message_handler());
      
    bv_cbmct bv_cbmc(ns, *solver);
      
    if(options.get_option("arrays-uf")=="never")
      bv_cbmc.unbounded_array=bv_cbmct::U_NONE;
    else if(options.get_option("arrays-uf")=="always")
      bv_cbmc.unbounded_array=bv_cbmct::U_ALL;
    #endif
  }      

  return 0;
}

/*******************************************************************\

Function: bmct::decide_default

  Inputs:

 Outputs:

 Purpose: Decide using "default" decision procedure

\*******************************************************************/

bool bmct::decide_default(const goto_functionst &goto_functions)
{
  bool result=true;
  
  std::auto_ptr<propt> solver;

  // SAT preprocessor won't work with beautification.
  if(options.get_bool_option("sat-preprocessor") &&
     !options.get_bool_option("beautify"))
  {
    solver=std::auto_ptr<propt>(new satcheckt);
  }
  else
    solver=std::auto_ptr<propt>(new satcheck_minisat_no_simplifiert);

  solver->set_message_handler(get_message_handler());
    
  bv_cbmct bv_cbmc(ns, *solver);
    
  if(options.get_option("arrays-uf")=="never")
    bv_cbmc.unbounded_array=bv_cbmct::U_NONE;
  else if(options.get_option("arrays-uf")=="always")
    bv_cbmc.unbounded_array=bv_cbmct::U_ALL;

  if(options.get_bool_option("all-properties"))
    return all_properties(goto_functions, bv_cbmc);

  switch(run_decision_procedure(bv_cbmc))
  {
  case decision_proceduret::D_UNSATISFIABLE:
    result=false;
    report_success();
    break;

  case decision_proceduret::D_SATISFIABLE:
    if(options.get_bool_option("beautify"))
      counterexample_beautificationt()(
        bv_cbmc, equation, ns);

    error_trace(bv_cbmc);
    report_failure();
    break;

  default:
    error() << "decision procedure failed" << eom;
  }

  return result;
}

/*******************************************************************\

Function: bmct::decide_aig

  Inputs:

 Outputs:

 Purpose: Decide using AIG followed by SAT

\*******************************************************************/

bool bmct::decide_aig(const goto_functionst &goto_functions)
{
  std::auto_ptr<propt> sub_solver;

  if(options.get_bool_option("sat-preprocessor"))
    sub_solver=std::auto_ptr<propt>(new satcheckt);
  else
    sub_solver=std::auto_ptr<propt>(new satcheck_minisat_no_simplifiert);

  aig_prop_solvert solver(*sub_solver);

  solver.set_message_handler(get_message_handler());
    
  bv_cbmct bv_cbmc(ns, solver);
    
  if(options.get_option("arrays-uf")=="never")
    bv_cbmc.unbounded_array=bv_cbmct::U_NONE;
  else if(options.get_option("arrays-uf")=="always")
    bv_cbmc.unbounded_array=bv_cbmct::U_ALL;

  return decide(goto_functions, bv_cbmc);
}

/*******************************************************************\

Function: bmct::bv_refinement

  Inputs:

 Outputs:

 Purpose: Decide using refinement decision procedure

\*******************************************************************/

bool bmct::decide_bv_refinement(const goto_functionst &goto_functions)
{
  std::auto_ptr<propt> solver;

  // We offer the option to disable the SAT preprocessor
  if(options.get_bool_option("sat-preprocessor"))
    solver=std::auto_ptr<propt>(new satcheckt);
  else
    solver=std::auto_ptr<propt>(new satcheck_minisat_no_simplifiert);
  
  solver->set_message_handler(get_message_handler());

  bv_refinementt bv_refinement(ns, *solver);
  bv_refinement.set_ui(ui);

  // we allow setting some parameters  
  if(options.get_option("max-node-refinement")!="")
    bv_refinement.max_node_refinement=
      options.get_unsigned_int_option("max-node-refinement");
  
  return decide(goto_functions, bv_refinement);
}

/*******************************************************************\

Function: bmct::decide_smt1

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_smt1(const goto_functionst &goto_functions)
{
  smt1_dect::solvert solver=get_smt1_solver_type();
  const std::string &filename=options.get_option("outfile");
  
  if(filename=="")
  {
    smt1_dect smt1_dec(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      solver);

    return decide(goto_functions, smt1_dec);
  }
  else if(filename=="-")
    smt1_convert(solver, std::cout);
  else
  {
    std::ofstream out(filename.c_str());
    if(!out)
    {
      std::cerr << "failed to open " << filename << std::endl;
      return false;
    }
    
    smt1_convert(solver, out);
  }
  
  return false;
}

/*******************************************************************\

Function: bmct::smt1_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::smt1_convert(smt1_dect::solvert solver, std::ostream &out)
{
  smt1_convt smt1_conv(
    ns,
    "cbmc",
    "Generated by CBMC " CBMC_VERSION,
    "QF_AUFBV",
    solver,
    out);

  smt1_conv.set_message_handler(get_message_handler());
  
  do_conversion(smt1_conv);

  smt1_conv.dec_solve();
}

/*******************************************************************\

Function: bmct::decide_smt2

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_smt2(const goto_functionst &goto_functions)
{
  smt2_dect::solvert solver=get_smt2_solver_type();
  const std::string &filename=options.get_option("outfile");
  
  if(filename=="")
  {
    smt2_dect smt2_dec(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      solver);

    if(options.get_bool_option("fpa"))
      smt2_dec.use_FPA_theory=true;

    return decide(goto_functions, smt2_dec);
  }
  else if(filename=="-")
    smt2_convert(solver, std::cout);
  else
  {
    std::ofstream out(filename.c_str());
    if(!out)
    {
      std::cerr << "failed to open " << filename << std::endl;
      return false;
    }
    
    smt2_convert(solver, out);
  }
  
  return false;
}

/*******************************************************************\

Function: bmct::smt2_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::smt2_convert(
  smt2_dect::solvert solver,
  std::ostream &out)
{
  smt2_convt smt2_conv(
    ns,
    "cbmc",
    "Generated by CBMC " CBMC_VERSION,
    "QF_AUFBV",
    solver,
    out);

  if(options.get_bool_option("fpa"))
    smt2_conv.use_FPA_theory=true;

  smt2_conv.set_message_handler(get_message_handler());
  
  do_conversion(smt2_conv);

  smt2_conv.dec_solve();
}

