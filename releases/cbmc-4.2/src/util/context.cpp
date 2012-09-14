/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "context.h"

/*******************************************************************\

Function: contextt::value

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

const irept &contextt::value(const irep_idt &name) const
{
  symbolst::const_iterator it=symbols.find(name);
  if(it==symbols.end()) return get_nil_irep();
  return it->second.value;
}

/*******************************************************************\

Function: contextt::add

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool contextt::add(const symbolt &symbol)
{
  if(!symbols.insert(std::pair<irep_idt, symbolt>(symbol.name, symbol)).second)
    return true;
    
  symbol_base_map.insert(std::pair<irep_idt, irep_idt>(symbol.base_name, symbol.name));
  symbol_module_map.insert(std::pair<irep_idt, irep_idt>(symbol.module, symbol.name));

  return false;
}

/*******************************************************************\

Function: contextt::move

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool contextt::move(symbolt &symbol, symbolt *&new_symbol)
{
  symbolt tmp;

  std::pair<symbolst::iterator, bool> result=
    symbols.insert(std::pair<irep_idt, symbolt>(symbol.name, tmp));

  if(!result.second)
  {
    new_symbol=&result.first->second;
    return true;
  }
    
  symbol_base_map.insert(std::pair<irep_idt, irep_idt>(symbol.base_name, symbol.name));
  symbol_module_map.insert(std::pair<irep_idt, irep_idt>(symbol.module, symbol.name));

  result.first->second.swap(symbol);
  new_symbol=&result.first->second;

  return false;
}

/*******************************************************************\

Function: contextt::remove

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool contextt::remove(const irep_idt &name)
{
  symbolst::iterator entry=symbols.find(name);
  
  if(entry==symbols.end())
    return true;

  for(symbol_base_mapt::iterator
      it=symbol_base_map.lower_bound(entry->second.base_name),
      it_end=symbol_base_map.upper_bound(entry->second.base_name);
      it!=it_end;
      ++it)
    if(it->second==name)
    {
      symbol_base_map.erase(it);
      break;
    }

  for(symbol_module_mapt::iterator
      it=symbol_module_map.lower_bound(entry->second.module),
      it_end=symbol_module_map.upper_bound(entry->second.module);
      it!=it_end;
      ++it)
    if(it->second==name)
    {
      symbol_module_map.erase(it);
      break;
    }

  symbols.erase(entry);

  return false;
}

/*******************************************************************\

Function: contextt::show

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void contextt::show(std::ostream &out) const
{
  out << std::endl << "Symbols:" << std::endl;

  forall_symbols(it, symbols)
    out << it->second;
}

/*******************************************************************\

Function: contextt::lookup

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

const symbolt &contextt::lookup(const irep_idt &identifier) const
{
  symbolst::const_iterator it=symbols.find(identifier);
      
  if(it==symbols.end())
    throw "symbol "+id2string(identifier)+" not found";
                    
  return it->second;
}

/*******************************************************************\

Function: contextt::lookup

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

symbolt &contextt::lookup(const irep_idt &identifier)
{
  symbolst::iterator it=symbols.find(identifier);
      
  if(it==symbols.end())
    throw "symbol "+id2string(identifier)+" not found";
                    
  return it->second;
}

/*******************************************************************\

Function: operator <<

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

std::ostream &operator << (std::ostream &out, const contextt &context)
{
  context.show(out);
  return out;
}
