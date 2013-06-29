/*******************************************************************\

Module: Memory model for partial order concurrency

Author: Michael Tautschnig, michael.tautschnig@cs.ox.ac.uk

\*******************************************************************/

#include <util/std_expr.h>
#include <util/i2string.h>

#include "memory_model_sc.h"

/*******************************************************************\

Function: memory_model_sct::operator()

  Inputs: 

 Outputs:

 Purpose:

\*******************************************************************/

void memory_model_sct::operator()(symex_target_equationt &equation)
{
  print(8, "Adding SC constraints");

  build_event_lists(equation);
  build_clock_type(equation);
  
  read_from(equation);
  write_serialization_internal(equation);
  write_serialization_external(equation);
  program_order(equation);
  from_read(equation);
}

/*******************************************************************\

Function: memory_model_sct::program_order

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void memory_model_sct::program_order(
  symex_target_equationt &equation)
{
  // this orders the events within a thread

  per_thread_mapt per_thread_map;
  
  for(eventst::const_iterator
      e_it=equation.SSA_steps.begin();
      e_it!=equation.SSA_steps.end();
      e_it++)
  {
    // concurreny-related?
    if(!is_shared_read(e_it) &&
       !is_shared_write(e_it) &&
       !is_spawn(e_it)) continue;

    per_thread_map[e_it->source.thread_nr].push_back(e_it);
  }
  
  // iterate over threads

  for(per_thread_mapt::const_iterator
      t_it=per_thread_map.begin();
      t_it!=per_thread_map.end();
      t_it++)
  {
    const event_listt &events=t_it->second;
    
    // iterate over relevant events in the thread
    
    event_it previous=equation.SSA_steps.end();
    
    for(event_listt::const_iterator
        e_it=events.begin();
        e_it!=events.end();
        e_it++)
    {
      if(previous==equation.SSA_steps.end())
      {
        // first one?
        previous=*e_it;
        continue;
      }

      equation.constraint(
        true_exprt(),
        before(previous, *e_it),
        "po",
        (*e_it)->source);

      previous=*e_it;
    }
  }

  // thread spawn: the spawn precedes the first
  // instruction of the new thread in program order
  
  unsigned next_thread_id=0;
  for(eventst::const_iterator
      e_it=equation.SSA_steps.begin();
      e_it!=equation.SSA_steps.end();
      e_it++)
  {
    if(is_spawn(e_it))
    {
      per_thread_mapt::const_iterator next_thread=
        per_thread_map.find(++next_thread_id);
      if(next_thread!=per_thread_map.end())
        equation.constraint(
          true_exprt(),
          before(e_it, next_thread->second.front()),
          "thread-spawn",
          e_it->source);
    }
  }
}

/*******************************************************************\

Function: memory_model_sct::write_serialization_internal

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void memory_model_sct::write_serialization_internal(
  symex_target_equationt &equation)
{
  per_thread_mapt per_thread_write_map;

  for(eventst::const_iterator
      e_it=equation.SSA_steps.begin();
      e_it!=equation.SSA_steps.end();
      e_it++)
    if(is_shared_write(e_it))
      per_thread_write_map[e_it->source.thread_nr].push_back(e_it);

  // the zero-initialisation of a shared variable precedes any
  // write in any of the threads
  typedef hash_map_cont<irep_idt, event_it, irep_id_hash> previoust;
  previoust init;
  for(address_mapt::const_iterator
      a_it=address_map.begin();
      a_it!=address_map.end();
      a_it++)
  {
    const a_rect &a_rec=a_it->second;

    assert(!a_rec.writes.empty());
    event_it init_write=a_rec.writes.front();

    // only writes with a true guard are considered initialisation
    if(init_write->guard.is_true())
      init.insert(std::make_pair(address(init_write), init_write));
  }

  // iterate over threads

  for(per_thread_mapt::const_iterator
      t_it=per_thread_write_map.begin();
      t_it!=per_thread_write_map.end();
      t_it++)
  {
    const event_listt &events=t_it->second;

    // iterate over relevant events in the thread

    previoust previous(init);

    for(event_listt::const_iterator
        e_it=events.begin();
        e_it!=events.end();
        e_it++)
    {
      previoust::iterator p_entry=
        previous.insert(std::make_pair(address(*e_it), *e_it)).first;

      event_it w_prev=p_entry->second;
      event_it w=*e_it;
      const exprt &w_value=w->ssa_lhs;

      equal_exprt eq(write_symbol_primed(w),
                     // if guard is true or this is the first write
                     w->guard.is_true() || p_entry->second==*e_it ?
                     // value' equals value
                     w_value :
                     // value' equals preceding write if guard is false
                     if_exprt(w->guard,
                              w_value,
                              write_symbol_primed(w_prev)));
      equation.constraint(
        true_exprt(),
        eq,
        "ws-preceding",
        (*e_it)->source);

      p_entry->second=*e_it;
    }
  }
}

/*******************************************************************\

Function: memory_model_sct::write_serialization_external

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void memory_model_sct::write_serialization_external(
  symex_target_equationt &equation)
{
  for(address_mapt::const_iterator
      a_it=address_map.begin();
      a_it!=address_map.end();
      a_it++)
  {
    const a_rect &a_rec=a_it->second;

    // This is quadratic in the number of writes
    // per address. Perhaps some better encoding
    // based on 'places'?    
    for(event_listt::const_iterator
        w_it1=a_rec.writes.begin();
        w_it1!=a_rec.writes.end();
        ++w_it1)
    {
      event_listt::const_iterator next=w_it1;
      ++next;

      for(event_listt::const_iterator w_it2=next;
          w_it2!=a_rec.writes.end();
          ++w_it2)
      {
        // external?
        if((*w_it1)->source.thread_nr==
           (*w_it2)->source.thread_nr)
          continue;

        // ws is a total order, no two elements have the same rank
        // s -> w_evt1 before w_evt2; !s -> w_evt2 before w_evt1

        symbol_exprt s=nondet_bool_symbol("ws-ext");

        // write-to-write edge
        equation.constraint(
          true_exprt(),
          implies_exprt(s, before(*w_it1, *w_it2)),
          "ws-ext",
          (*w_it1)->source);

        equation.constraint(
          true_exprt(),
          implies_exprt(not_exprt(s), before(*w_it2, *w_it1)),
          "ws-ext",
          (*w_it1)->source);
      }
    }
  }
}

/*******************************************************************\

Function: memory_model_sct::from_read

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void memory_model_sct::from_read(symex_target_equationt &equation)
{
  // from-read: (w', w) in ws and (w', r) in rf -> (r, w) in fr
  
  for(address_mapt::const_iterator
      a_it=address_map.begin();
      a_it!=address_map.end();
      a_it++)
  {
    const a_rect &a_rec=a_it->second;

    // This is quadratic in the number of writes per address.
    for(event_listt::const_iterator
        w_prime=a_rec.writes.begin();
        w_prime!=a_rec.writes.end();
        ++w_prime)
    {
      event_listt::const_iterator next=w_prime;
      ++next;

      for(event_listt::const_iterator w=next;
          w!=a_rec.writes.end();
          ++w)
      {
        exprt ws1, ws2;
        
        if(po(*w_prime, *w))
        {
          ws1=true_exprt(); // true on SC only!
          ws2=false_exprt(); // true on SC only!
        }
        else if(po(*w, *w_prime))
        {
          ws1=false_exprt(); // true on SC only!
          ws2=true_exprt(); // true on SC only!
        }
        else
        {
          ws1=before(*w_prime, *w);
          ws2=before(*w, *w_prime);
        }

        // smells like cubic
        for(choice_symbolst::const_iterator
            c_it=choice_symbols.begin();
            c_it!=choice_symbols.end();
            c_it++)
        {
          event_it r=c_it->first.first;
          exprt rf=c_it->second;
          exprt cond;
          cond.make_nil();
        
          if(c_it->first.second==*w_prime && !ws1.is_false())
          {
            exprt fr=before(r, *w);

            cond=
              implies_exprt(
                and_exprt(r->guard, (*w_prime)->guard, ws1, rf),
                fr);
          }
          else if(c_it->first.second==*w && !ws2.is_false())
          {
            exprt fr=before(r, *w_prime);

            cond=
              implies_exprt(
                and_exprt(r->guard, (*w)->guard, ws2, rf),
                fr);
          }

          if(cond.is_not_nil())
            equation.constraint(
              true_exprt(), cond, "fr", r->source);
        }
        
      }
    }
  }
}

