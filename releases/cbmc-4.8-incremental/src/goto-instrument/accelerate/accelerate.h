#include <util/namespace.h>

#include <analyses/natural_loops.h>

#include <goto-programs/goto_functions.h>

#include "path.h"
#include "accelerator.h"
#include "trace_automaton.h"
#include "subsumed.h"
#include "scratch_program.h"

class acceleratet {
 public:
  acceleratet(goto_programt &_program,
              goto_functionst &_goto_functions,
              symbol_tablet &_symbol_table) :
      program(_program),
      goto_functions(_goto_functions),
      symbol_table(_symbol_table),
      ns(symbol_table)
  {
    natural_loops(program);
  }

  int accelerate_loop(goto_programt::targett &loop_header);
  int accelerate_loops();

  bool accelerate_path(patht &path, path_acceleratort &accelerator);

  void restrict_traces();

 protected:
  void find_paths(goto_programt::targett &loop_header,
                  pathst &loop_paths,
                  pathst &exit_paths,
                  goto_programt::targett &back_jump);

  void extend_path(goto_programt::targett &t,
                   goto_programt::targett &loop_header,
                   natural_loops_mutablet::natural_loopt &loop,
                   patht &prefix,
                   pathst &loop_paths,
                   pathst &exit_paths,
                   goto_programt::targett &back_jump);

  void insert_looping_path(goto_programt::targett &loop_header,
                           goto_programt::targett &back_jump,
                           goto_programt &looping_path,
                           patht &inserted_path);
  void insert_accelerator(goto_programt::targett &loop_header,
                          goto_programt::targett &back_jump,
                          path_acceleratort &accelerator,
                          subsumed_patht &subsumed);

  void insert_automaton(trace_automatont &automaton);
  void build_state_machine(trace_automatont::sym_mapt::iterator p,
                           trace_automatont::sym_mapt::iterator end,
                           state_sett &accept_states,
                           symbol_exprt state,
                           symbol_exprt next_state,
                           scratch_programt &state_machine);

  symbolt make_symbol(string name, typet type);
  void decl(symbol_exprt &sym, goto_programt::targett t);
  void decl(symbol_exprt &sym, goto_programt::targett t, exprt init);

  goto_programt &program;
  goto_functionst &goto_functions;
  symbol_tablet &symbol_table;
  namespacet ns;
  natural_loops_mutablet natural_loops;
  subsumed_pathst subsumed;

  typedef map<patht, goto_programt> accelerator_mapt;
};

void accelerate_functions(
  goto_functionst &functions,
  symbol_tablet &ns);