#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>

#include <goto-programs/goto_program.h>
#include <goto-programs/wp.h>

#include <goto-symex/goto_symex.h>
#include <goto-symex/symex_target_equation.h>

#include <analyses/goto_check.h>

#include <ansi-c/expr2c.h>

#include <util/symbol_table.h>
#include <util/options.h>
#include <util/std_expr.h>
#include <util/std_code.h>
#include <util/i2string.h>
#include <util/find_symbols.h>
#include <util/rename.h>
#include <util/simplify_expr.h>
#include <util/replace_expr.h>
#include <util/arith_tools.h>

#include "polynomial_accelerator.h"
#include "accelerator.h"
#include "util.h"
#include "cone_of_influence.h"

#define DEBUG


bool polynomial_acceleratort::accelerate(patht &loop,
    path_acceleratort &accelerator) {
  goto_programt::instructionst body;

  for (patht::iterator it = loop.begin();
       it != loop.end();
       ++it) {
    body.push_back(*(it->loc));
  }

  expr_sett targets;
  map<exprt, polynomialt> polynomials;
  scratch_programt program(symbol_table);
  goto_programt::instructionst assigns;

  utils.find_modified(body, targets);

#ifdef DEBUG
  std::cout << "Polynomial accelerating program:" << endl;

  for (goto_programt::instructionst::iterator it = body.begin();
       it != body.end();
       ++it) {
    program.output_instruction(ns, "scratch", std::cout, it);
  }

  std::cout << "Modified:" << endl;

  for (expr_sett::iterator it = targets.begin();
       it != targets.end();
       ++it) {
    std::cout << expr2c(*it, ns) << endl;
  }
#endif

  for (goto_programt::instructionst::iterator it = body.begin();
       it != body.end();
       ++it) {
    if (it->is_assign() || it->is_decl()) {
      assigns.push_back(*it);
    }
  }

  if (loop_counter.is_nil()) {
    symbolt loop_sym = utils.fresh_symbol("polynomial::loop_counter",
        unsignedbv_typet(32));
    loop_counter = loop_sym.symbol_expr();
  }

  for (expr_sett::iterator it = targets.begin();
       it != targets.end();
       ++it) {
    polynomialt poly;
    exprt target = *it;

    if (target.type() == bool_typet()) {
      // Hack: don't accelerate booleans.
      continue;
    }

    if (target.id() == ID_index) {
      // We'll handle this later.
      continue;
    }

    if (fit_polynomial(assigns, target, poly)) {
      map<exprt, polynomialt> this_poly;
      this_poly[target] = poly;

      if (check_inductive(this_poly, assigns)) {
        polynomials.insert(make_pair(target, poly));
      }
    }
  }

  if (polynomials.empty()) {
    return false;
  }

  /*
  if (!utils.check_inductive(polynomials, assigns)) {
    // They're not inductive :-(
    return false;
  }
  */

  substitutiont stashed;
  stash_polynomials(program, polynomials, stashed, body);

  exprt guard;
  bool path_is_monotone;
  
  try {
    path_is_monotone = utils.do_assumptions(polynomials, loop, guard);
  } catch (string s) {
    // Couldn't do WP.
    std::cout << "Assumptions error: " << s << endl;
    return false;
  }

  for (map<exprt, polynomialt>::iterator it = polynomials.begin();
       it != polynomials.end();
       ++it) {
    replace_expr(it->first, it->second.to_expr(), guard);
  }

  if (path_is_monotone) {
    // OK cool -- the path is monotone, so we can just assume the condition for
    // the last iteration.
    replace_expr(loop_counter,
                 minus_exprt(loop_counter, from_integer(1, loop_counter.type())),
                 guard);
  } else {
    // The path is not monotone, so we need to introduce a quantifier to ensure
    // that the condition held for all 0 <= k < n.
    symbolt k_sym = utils.fresh_symbol("polynomial::k", unsignedbv_typet(32));
    exprt k = k_sym.symbol_expr();

    exprt k_bound = and_exprt(binary_relation_exprt(from_integer(0, k.type()), "<=", k),
                              binary_relation_exprt(k, "<", loop_counter));
    replace_expr(loop_counter, k, guard);

    simplify(guard, ns);

    implies_exprt implies(k_bound, guard);

    exprt forall(ID_forall);
    forall.type() = bool_typet();
    forall.copy_to_operands(k);
    forall.copy_to_operands(implies);

    guard = forall;
  }

  // All our conditions are met -- we can finally build the accelerator!
  // It is of the form:
  //
  // loop_counter = *;
  // target1 = polynomial1;
  // target2 = polynomial2;
  // ...
  // assume(guard);
  // assume(no overflows in previous code);

  program.assign(loop_counter, side_effect_expr_nondett(loop_counter.type()));

  for (map<exprt, polynomialt>::iterator it = polynomials.begin();
       it != polynomials.end();
       ++it) {
    program.assign(it->first, it->second.to_expr());
  }

  // Add in any array assignments we can do now.
  if (!utils.do_arrays(assigns, polynomials, loop_counter, stashed, program)) {
    // We couldn't model some of the array assignments with polynomials...
    // Unfortunately that means we just have to bail out.
    return false;
  }


  program.add_instruction(ASSUME)->guard = guard;
  program.fix_types();

  if (path_is_monotone) {
    utils.ensure_no_overflows(program);
  }

  accelerator.pure_accelerator.instructions.swap(program.instructions);

  return true;
}

bool polynomial_acceleratort::fit_polynomial(goto_programt::instructionst &body,
                                             exprt &var,
                                             polynomialt &polynomial) {
  // These are the variables that var depends on with respect to the body.
  expr_sett influence = cone_of_influence(body, var);
  vector<expr_listt> parameters;
  set<pair<expr_listt, exprt> > coefficients;
  expr_listt exprs;
  scratch_programt program(symbol_table);

#ifdef DEBUG
  std::cout << "Fitting a polynomial for " << expr2c(var, ns) << ", which depends on:"
       << endl;

  for (expr_sett::iterator it = influence.begin();
       it != influence.end();
       ++it) {
    std::cout << expr2c(*it, ns) << endl;
  }
#endif

  for (expr_sett::iterator it = influence.begin();
       it != influence.end();
       ++it) {
    if (it->id() == ID_index ||
        it->id() == ID_dereference) {
      // Hack: don't accelerate variables that depend on arrays...
      return false;
    }

    exprs.clear();

    exprs.push_back(*it);
    parameters.push_back(exprs);

    exprs.push_back(loop_counter);
    parameters.push_back(exprs);
  }

  // N
  exprs.clear();
  exprs.push_back(loop_counter);
  parameters.push_back(exprs);

  // N^2
  exprs.push_back(loop_counter);
  //parameters.push_back(exprs);

  // Constant
  exprs.clear();
  parameters.push_back(exprs);

  for (vector<expr_listt>::iterator it = parameters.begin();
       it != parameters.end();
       ++it) {
    symbolt coeff = utils.fresh_symbol("polynomial::coeff",
        signedbv_typet(32));
    coefficients.insert(make_pair(*it, coeff.symbol_expr()));
  }

  // Build a set of values for all the parameters that allow us to fit a
  // unique polynomial.

  // XXX
  // This isn't ok -- we're assuming 0, 1 and 2 are valid values for the
  // variables involved, but this might make the path condition UNSAT.  Should
  // really be solving the path constraints a few times to get valid probe
  // values...

  map<exprt, int> values;

  for (expr_sett::iterator it = influence.begin();
       it != influence.end();
       ++it) {
    values[*it] = 0;
  }

  for (int n = 0; n <= 2; n++) {
    for (expr_sett::iterator it = influence.begin();
         it != influence.end();
         ++it) {
      values[*it] = 1;
      assert_for_values(program, values, coefficients, n, body, var);
      values[*it] = 0;
    }
  }

  // Now just need to assert the case where all values are 0 and all are 2.
  assert_for_values(program, values, coefficients, 0, body, var);
  assert_for_values(program, values, coefficients, 2, body, var);

  for (expr_sett::iterator it = influence.begin();
       it != influence.end();
       ++it) {
    values[*it] = 2;
  }

  assert_for_values(program, values, coefficients, 2, body, var);

  // Now do an ASSERT(false) to grab a counterexample
  goto_programt::targett assertion = program.add_instruction(ASSERT);
  assertion->guard = false_exprt();

  //utils.ensure_no_overflows(program);

#ifdef DEBUG
  std::cout << "Fitting polynomial with program:" << endl;
  program.output(std::cout);
#endif

  // If the path is satisfiable, we've fitted a polynomial.  Extract the
  // relevant coefficients and return the expression.
  try {
    if (program.check_sat()) {
      utils.extract_polynomial(program, coefficients, polynomial);
      return true;
    }
  } catch (string s) {
    std::cout << "Error in fitting polynomial SAT check: " << s << endl;
  } catch (const char *s) {
    std::cout << "Error in fitting polynomial SAT check: " << s << endl;
  }

  return false;
}

void polynomial_acceleratort::assert_for_values(scratch_programt &program,
                                                map<exprt, int> &values,
                                                set<pair<expr_listt, exprt> >
                                                   &coefficients,
                                                int num_unwindings,
                                                goto_programt::instructionst
                                                   &loop_body,
                                                exprt &target) {
  // First figure out what the appropriate type for this expression is.
  typet expr_type = nil_typet();

  for (map<exprt, int>::iterator it = values.begin();
      it != values.end();
      ++it) {
    if (expr_type == nil_typet()) {
      expr_type = it->first.type();
    } else {
      expr_type = join_types(expr_type, it->first.type());
    }
  }

  // Now set the initial values of the all the variables...
  for (map<exprt, int>::iterator it = values.begin();
       it != values.end();
       ++it) {
    program.assign(it->first, from_integer(it->second, it->first.type()));
  }

  // Now unwind the loop as many times as we need to.
  for (int i = 0; i < num_unwindings; i++) {
    program.append(loop_body);
  }

  // Now build the polynomial for this point and assert it fits.
  exprt rhs = nil_exprt();

  for (set<pair<expr_listt, exprt> >::iterator it = coefficients.begin();
       it != coefficients.end();
       ++it) {
    int concrete_value = 1;

    for (expr_listt::const_iterator e_it = it->first.begin();
         e_it != it->first.end();
         ++e_it) {
      exprt e = *e_it;

      if (e == loop_counter) {
        concrete_value *= num_unwindings;
      } else {
        map<exprt, int>::iterator v_it = values.find(e);

        assert(v_it != values.end());

        concrete_value *= v_it->second;
      }
    }

    // OK, concrete_value now contains the value of all the relevant variables
    // multiplied together.  Create the term concrete_value*coefficient and add
    // it into the polynomial.
    typecast_exprt cast(it->second, expr_type);
    exprt term = mult_exprt(from_integer(concrete_value, expr_type), cast);

    if (rhs.is_nil()) {
      rhs = term;
    } else {
      rhs = plus_exprt(rhs, term);
    }
  }

  rhs = typecast_exprt(rhs, target.type());

  // We now have the RHS of the polynomial.  Assert that this is equal to the
  // actual value of the variable we're fitting.
  exprt polynomial_holds = equal_exprt(target, rhs);

  // Finally, assert that the polynomial equals the variable we're fitting.
  goto_programt::targett assumption = program.add_instruction(ASSUME);
  assumption->guard = polynomial_holds;
}

expr_sett polynomial_acceleratort::cone_of_influence(goto_programt::instructionst
                                                        &body,
                                                      exprt &target) {
  expr_sett cone;

  utils.gather_rvalues(target, cone);

  for (goto_programt::instructionst::reverse_iterator r_it = body.rbegin();
       r_it != body.rend();
       ++r_it) {
    if (r_it->is_assign()) {
      // XXX -- this doesn't work if the assignment is not to a symbol, e.g.
      // A[i] = 0;
      // or
      // *p = x;
      code_assignt assignment = to_code_assign(r_it->code);
      expr_sett lhs_syms;

      utils.gather_rvalues(assignment.lhs(), lhs_syms);

      for (expr_sett::iterator s_it = lhs_syms.begin();
           s_it != lhs_syms.end();
           ++s_it) {
        if (cone.find(*s_it) != cone.end()) {
          // We're assigning to something in the cone of influence -- expand the
          // cone.
          utils.gather_rvalues(assignment.rhs(), cone);
          break;
        }
      }
    }
  }

  return cone;
}

bool polynomial_acceleratort::check_inductive(map<exprt, polynomialt> polynomials,
                                              goto_programt::instructionst &body) {
  // Checking that our polynomial is inductive with respect to the loop body is
  // equivalent to checking safety of the following program:
  //
  // assume (target1 == polynomial1);
  // assume (target2 == polynomial2)
  // ...
  // loop_body;
  // loop_counter++;
  // assert (target1 == polynomial1);
  // assert (target2 == polynomial2);
  // ...
  scratch_programt program(symbol_table);
  vector<exprt> polynomials_hold;
  substitutiont substitution;

  stash_polynomials(program, polynomials, substitution, body);
 
  for (map<exprt, polynomialt>::iterator it = polynomials.begin();
       it != polynomials.end();
       ++it) {
    exprt holds = equal_exprt(it->first, it->second.to_expr());
    program.add_instruction(ASSUME)->guard = holds;

    polynomials_hold.push_back(holds);
  }

  program.append(body);

  codet inc_loop_counter = code_assignt(loop_counter,
                                        plus_exprt(loop_counter, from_integer(1, loop_counter.type())));
  program.add_instruction(ASSIGN)->code = inc_loop_counter;

  for (vector<exprt>::iterator it = polynomials_hold.begin();
       it != polynomials_hold.end();
       ++it) {
    program.add_instruction(ASSERT)->guard = *it;
  }

#ifdef DEBUG
  std::cout << "Checking following program for inductiveness:" << endl;
  program.output(std::cout);
#endif

  try {
    if (program.check_sat()) {
      // We found a counterexample to inductiveness... :-(
  #ifdef DEBUG
      std::cout << "Not inductive!" << endl;
  #endif
    return false;
    } else {
      return true;
    }
  } catch (string s) {
    std::cout << "Error in inductiveness SAT check: " << s << endl;
    return false;
  } catch (const  char *s) {
    std::cout << "Error in inductiveness SAT check: " << s << endl;
    return false;
  }
}

void polynomial_acceleratort::stash_polynomials(
    scratch_programt &program,
    map<exprt, polynomialt> &polynomials,
    substitutiont &substitution,
    goto_programt::instructionst &body) {

  expr_sett modified;
  utils.find_modified(body, modified);
  stash_variables(program, modified, substitution);

  for (map<exprt, polynomialt>::iterator it = polynomials.begin();
       it != polynomials.end();
       ++it) {
    it->second.substitute(substitution);
  }
}

void polynomial_acceleratort::stash_variables(scratch_programt &program,
                                              expr_sett modified,
                                              substitutiont &substitution) {
  find_symbols_sett vars;

  for (expr_sett::iterator it = modified.begin();
       it != modified.end();
       ++it) {
    find_symbols(*it, vars);
  }

  irep_idt loop_counter_name = to_symbol_expr(loop_counter).get_identifier();
  vars.erase(loop_counter_name);

  for (find_symbols_sett::iterator it = vars.begin();
       it != vars.end();
       ++it) {
    symbolt orig = symbol_table.lookup(*it);
    symbolt stashed_sym = utils.fresh_symbol("polynomial::stash", orig.type);
    substitution[orig.symbol_expr()] = stashed_sym.symbol_expr();
    program.assign(stashed_sym.symbol_expr(), orig.symbol_expr());
  }
}

/*
 * Finds a precondition which guarantees that all the assumptions and assertions
 * along this path hold.
 *
 * This is not the weakest precondition, since we make underapproximations due
 * to aliasing.
 */

exprt polynomial_acceleratort::precondition(patht &path) {
  exprt ret = false_exprt();

  for (patht::reverse_iterator r_it = path.rbegin();
       r_it != path.rend();
       ++r_it) {
    goto_programt::const_targett t = r_it->loc;

    if (t->is_assign()) {
      // XXX Need to check for aliasing...
      const code_assignt &assignment = to_code_assign(t->code);
      const exprt &lhs = assignment.lhs();
      const exprt &rhs = assignment.rhs();

      if (lhs.id() == ID_symbol) {
        replace_expr(lhs, rhs, ret);
      } else if (lhs.id() == ID_index ||
                 lhs.id() == ID_dereference) {
        continue;
      } else {
        throw "Couldn't take WP of " + expr2c(lhs, ns) + " = " + expr2c(rhs, ns);
      }
    } else if (t->is_assume() || t->is_assert()) {
      ret = implies_exprt(t->guard, ret);
    } else {
      // Ignore.
    }

    if (!r_it->guard.is_true() && !r_it->guard.is_nil()) {
      // The guard isn't constant true, so we need to accumulate that too.
      ret = implies_exprt(r_it->guard, ret);
    }
  }

  simplify(ret, ns);

  return ret;
}