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

#ifdef USE_AIG
#include <solvers/prop/aig_prop.h>
#endif

#include "bmc.h"
#include "bv_cbmc.h"
#include "counterexample_beautification.h"
#include "version.h"

/*******************************************************************\

Function: bmct::decide_default

  Inputs:

 Outputs:

 Purpose: Decide using "default" decision procedure

\*******************************************************************/

bool bmct::decide_default()
{
  bool result=true;
  
  #ifdef USE_AIG
  satcheckt sub_solver;
  #endif
  
  std::auto_ptr<propt> solver;

  // SAT preprocessor won't work with beautification.
  if(options.get_bool_option("sat-preprocessor") &&
     !options.get_bool_option("beautify"))
  {
    #ifdef USE_AIG
    solver=std::auto_ptr<propt>(new aig_prop_solvert(sub_solver));
    #else
    solver=std::auto_ptr<propt>(new satcheckt);
    #endif
  }
  else
    solver=std::auto_ptr<propt>(new satcheck_minisat_no_simplifiert);

  solver->set_message_handler(get_message_handler());
  solver->set_verbosity(get_verbosity());
    
  bv_cbmct bv_cbmc(ns, *solver);
    
  if(options.get_option("arrays-uf")=="never")
    bv_cbmc.unbounded_array=bv_cbmct::U_NONE;
  else if(options.get_option("arrays-uf")=="always")
    bv_cbmc.unbounded_array=bv_cbmct::U_ALL;
    
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

Function: bmct::bv_refinement

  Inputs:

 Outputs:

 Purpose: Decide using refinement decision procedure

\*******************************************************************/

bool bmct::decide_bv_refinement()
{
  std::auto_ptr<propt> solver;

  // We offer the option to disable the SAT preprocessor
  if(options.get_bool_option("sat-preprocessor"))
    solver=std::auto_ptr<propt>(new satcheckt);
  else
    solver=std::auto_ptr<propt>(new satcheck_minisat_no_simplifiert);
  
  solver->set_message_handler(get_message_handler());
  solver->set_verbosity(get_verbosity());

  bv_refinementt bv_refinement(ns, *solver);
  
  return decide(bv_refinement);
}

/*******************************************************************\

Function: bmct::decide_smt1

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_smt1(smt1_dect::solvert solver)
{
  const std::string &filename=options.get_option("outfile");
  
  if(filename=="")
  {
    smt1_dect smt1_dec(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      solver);
    return decide(smt1_dec);
  }
  else if(filename=="-")
    smt1_convert(std::cout);
  else
  {
    std::ofstream out(filename.c_str());
    if(!out)
    {
      std::cerr << "failed to open " << filename << std::endl;
      return false;
    }
    
    smt1_convert(out);
  }
  
  return false;
}

/*******************************************************************\

Function: bmct::smt1_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::smt1_convert(std::ostream &out)
{
  smt1_convt smt1_conv(
    ns,
    "cbmc",
    "Generated by CBMC " CBMC_VERSION,
    "QF_AUFBV",
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

bool bmct::decide_smt2(smt2_dect::solvert solver)
{
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

    return decide(smt2_dec);
  }
  else if(filename=="-")
    smt2_convert(std::cout);
  else
  {
    std::ofstream out(filename.c_str());
    if(!out)
    {
      std::cerr << "failed to open " << filename << std::endl;
      return false;
    }
    
    smt2_convert(out);
  }
  
  return false;
}

/*******************************************************************\

Function: bmct::smt2_convert

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void bmct::smt2_convert(std::ostream &out)
{
  smt2_convt smt2_conv(
    ns,
    "cbmc",
    "Generated by CBMC " CBMC_VERSION,
    "QF_AUFBV",
    out);

  if(options.get_bool_option("fpa"))
    smt2_conv.use_FPA_theory=true;

  smt2_conv.set_message_handler(get_message_handler());
  
  do_conversion(smt2_conv);

  smt2_conv.dec_solve();
}

/*******************************************************************\

Function: bmct::decide_cvc

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_cvc()
{
  return decide_smt1(smt1_dect::CVC3);
}

/*******************************************************************\

Function: bmct::decide_boolector

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_boolector()
{
  return decide_smt1(smt1_dect::BOOLECTOR);
}

/*******************************************************************\

Function: bmct::decide_mathsat

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_mathsat()
{
  if(options.get_bool_option("smt2"))
    return decide_smt2(smt2_dect::MATHSAT);
  else
    return decide_smt1(smt1_dect::MATHSAT);
}

/*******************************************************************\

Function: bmct::decide_opensmt

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_opensmt()
{
  return decide_smt1(smt1_dect::OPENSMT);
}

/*******************************************************************\

Function: bmct::decide_z3

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_z3()
{
  return decide_smt1(smt1_dect::Z3);
}

/*******************************************************************\

Function: bmct::decide_yices

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool bmct::decide_yices()
{
  return decide_smt1(smt1_dect::YICES);
}
