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
#include <solvers/sat/dimacs_cnf.h>


#include "cbmc_solvers.h"

#include "bv_cbmc.h"
#include "dimacs.h"
#include "counterexample_beautification.h"
#include "version.h"


// Solvers with additional objects

class cbmc_solver_with_propt : public cbmc_solverst::solvert {
 public:
  cbmc_solver_with_propt(prop_convt* _prop_conv, propt* _prop) 
      : cbmc_solverst::solvert(_prop_conv) {
    assert(_prop!=NULL);
    prop = _prop;
  }
  ~cbmc_solver_with_propt() {
    assert(prop!=NULL);
    delete prop;
  }

 protected:
  propt* prop;
};

class cbmc_solver_with_aigpropt : public cbmc_solver_with_propt {
 public:
  cbmc_solver_with_aigpropt(prop_convt* _prop_conv, propt* _prop, aigt* _aig) 
    : cbmc_solver_with_propt(_prop_conv,_prop) {
    assert(_aig!=NULL);
    aig = _aig;
  }
  ~cbmc_solver_with_aigpropt() {
    assert(aig!=NULL);
    delete aig;
  }

 protected:
  aigt* aig;
};


/*******************************************************************\

Function: cbmc_solverst::get_default

  Inputs:

 Outputs:

 Purpose: Get the default decision procedure

\*******************************************************************/

cbmc_solverst::solvert* cbmc_solverst::get_default()
{
  solvert* solver;
  if(options.get_bool_option("beautify") || 
     options.get_bool_option("all-claims") ||
     options.get_bool_option("cover-assertions") ||
     !options.get_bool_option("sat-preprocessor") //no Minisat simplifier
    )
  {
    // simplifier won't work with beautification
    propt* prop = new satcheck_minisat_no_simplifiert();
    prop->set_message_handler(get_message_handler());
    prop->set_verbosity(get_verbosity());
    
    bv_cbmct* bv_cbmc = new bv_cbmct(ns, *prop);
    
    if(options.get_option("arrays-uf")=="never")
      bv_cbmc->unbounded_array=bv_cbmct::U_NONE;
    else if(options.get_option("arrays-uf")=="always")
      bv_cbmc->unbounded_array=bv_cbmct::U_ALL;
   
    solver = new cbmc_solver_with_propt(bv_cbmc,prop);
  }
  else //with simplifier
  {
  #if 1
    propt* prop = new satcheckt();
    prop->set_message_handler(get_message_handler());
    prop->set_verbosity(get_verbosity());
    bv_cbmct* bv_cbmc = new bv_cbmct(ns, *prop);
    solver = new cbmc_solver_with_propt(bv_cbmc,prop);
  #else
    aigt* aig = new aigt();
    propt* prop = new aig_propt(*aig);
    prop->set_message_handler(get_message_handler());
    prop->set_verbosity(get_verbosity());
    bv_cbmct* bv_cbmc = new bv_cbmct(ns, *prop);
    solver = new cbmc_solver_with_aigpropt(bv_cbmc,prop,aig);
  #endif

    if(options.get_option("arrays-uf")=="never")
      bv_cbmc->unbounded_array=bv_cbmct::U_NONE;
    else if(options.get_option("arrays-uf")=="always")
      bv_cbmc->unbounded_array=bv_cbmct::U_ALL;
  }

  return solver;
}

/*******************************************************************\

Function: cbmc_solverst::get_dimacs

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
 
cbmc_solverst::solvert* cbmc_solverst::get_dimacs()
{
  dimacs_cnft* prop = new dimacs_cnft();
  prop->set_message_handler(get_message_handler());

  return new cbmc_solver_with_propt(new dimacst(ns, *prop),prop);
}


/*******************************************************************\

Function: cbmc_solverst::get_bv_refinement

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
 
cbmc_solverst::solvert* cbmc_solverst::get_bv_refinement()
{
  propt* prop;

  // We offer the option to disable the SAT preprocessor
  if(options.get_bool_option("sat-preprocessor")) {
    no_beautification();
    prop=new satcheckt();
  }
  else
    prop=new satcheck_minisat_no_simplifiert();
  
  prop->set_message_handler(get_message_handler());
  prop->set_verbosity(get_verbosity());

  bv_refinementt* bv_refinement = new bv_refinementt(ns, *prop);

  // we allow setting some parameters  
  if(options.get_option("max-node-refinement")!="")
    bv_refinement->max_node_refinement=options.get_int_option("max-node-refinement");

  return new cbmc_solver_with_propt(bv_refinement,prop);
}

/*******************************************************************\

Function: cbmc_solverst::get_smt1

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
 
cbmc_solverst::solvert* cbmc_solverst::get_smt1(smt1_dect::solvert solver)
{
  no_beautification();
  no_incremental_check();

  const std::string &filename=options.get_option("outfile");
  
  if(filename=="")
  {
    smt1_dect* smt1_dec = new smt1_dect(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      solver);
    return new solvert(smt1_dec);
  }
  else if(filename=="-") {
    smt1_convt* smt1_conv = new smt1_convt(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      std::cout);
    smt1_conv->set_message_handler(get_message_handler());
    return new solvert(smt1_conv);
  }
  else
  {
    std::ofstream out(filename.c_str());
    if(!out)
    {
      std::cerr << "failed to open " << filename << std::endl;
      return false;
    }
    smt1_convt* smt1_conv = new smt1_convt(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      std::cout);
    smt1_conv->set_message_handler(get_message_handler());
    return new solvert(smt1_conv);
  }
}
  
/*******************************************************************\

Function: cbmc_solverst::get_smt2

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
   
cbmc_solverst::solvert* cbmc_solverst::get_smt2(smt2_dect::solvert solver)
{
  no_beautification();
  no_incremental_check();

  const std::string &filename=options.get_option("outfile");
  
  if(filename=="")
  {
    smt2_dect* smt2_dec = new smt2_dect(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      solver);

    if(options.get_bool_option("fpa"))
      smt2_dec->use_FPA_theory=true;

    return new solvert(smt2_dec);
  }
  else if(filename=="-") {
    smt2_convt* smt2_conv = new smt2_convt(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      std::cout);

    if(options.get_bool_option("fpa"))
      smt2_conv->use_FPA_theory=true;

    smt2_conv->set_message_handler(get_message_handler());
    return new solvert(smt2_conv);
  }
  else
  {
    std::ofstream out(filename.c_str());
    if(!out)
    {
      std::cerr << "failed to open " << filename << std::endl;
      return false;
    }
    smt2_convt* smt2_conv = new smt2_convt(
      ns,
      "cbmc",
      "Generated by CBMC " CBMC_VERSION,
      "QF_AUFBV",
      out);

    if(options.get_bool_option("fpa"))
      smt2_conv->use_FPA_theory=true;

    smt2_conv->set_message_handler(get_message_handler());
    return new solvert(smt2_conv);
  }
}

/*******************************************************************\

Function: cbmc_solverst::get_cvc

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
     
cbmc_solverst::solvert* cbmc_solverst::get_cvc()
{
  return get_smt1(smt1_dect::CVC3);
}
    
/*******************************************************************\

Function: cbmc_solverst::get_boolector

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
      
cbmc_solverst::solvert* cbmc_solverst::get_boolector()
{
  return get_smt1(smt1_dect::BOOLECTOR);
}

/*******************************************************************\

Function: cbmc_solverst::get_mathsat

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
       
cbmc_solverst::solvert* cbmc_solverst::get_mathsat()
{
  if(options.get_bool_option("smt2"))
    return get_smt2(smt2_dect::MATHSAT);
  else
    return get_smt1(smt1_dect::MATHSAT);
}
      
/*******************************************************************\

Function: cbmc_solverst::get_opensmt

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
	
cbmc_solverst::solvert* cbmc_solverst::get_opensmt()
{
  return get_smt1(smt1_dect::OPENSMT);
}

/*******************************************************************\

Function: cbmc_solverst::get_z3

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
	 
cbmc_solverst::solvert* cbmc_solverst::get_z3()
{
  return get_smt2(smt2_dect::Z3);
}
	 
/*******************************************************************\

Function: cbmc_solverst::get_yices

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/
	  
cbmc_solverst::solvert* cbmc_solverst::get_yices()
{
  return get_smt1(smt1_dect::YICES);
}
	  
/*******************************************************************\

Function: cbmc_solverst::no_beautification

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cbmc_solverst::no_beautification() {
  if(options.get_bool_option("beautify-pbs") ||
     options.get_bool_option("beautify-greedy"))
    throw "sorry, this solver does not support beautification";
}

void cbmc_solverst::no_incremental_check() {
  if(options.get_bool_option("all-claims") ||
     options.get_bool_option("cover-assertions") ||
     options.get_option("incremental-check")!="")
    throw "sorry, this solver does not support incremental solving";
}
