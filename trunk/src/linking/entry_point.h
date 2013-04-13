/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#ifndef CPROVER_C_MAIN_H
#define CPROVER_C_MAIN_H

#include <util/symbol_table.h>
#include <util/message.h>
#include <util/std_code.h>

bool entry_point(
  symbol_tablet &symbol_table,
  const std::string &standard_main,
  message_handlert &message_handler);

bool static_lifetime_init(
  symbol_tablet &symbol_table,
  const locationt &location,
  message_handlert &message_handler);

#endif
