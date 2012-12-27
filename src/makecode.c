/*
** RopGadget - Release v3.4.2
** Jonathan Salwan - http://twitter.com/JonathanSalwan
** Allan Wirth - http://allanwirth.com/
** http://shell-storm.org
** 2012-11-11
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ropgadget.h"

/* linked list for gadgets */
t_list_inst *add_element(t_list_inst *old_element, char *instruction, Address addr)
{
  t_list_inst *new_element;

  new_element = xmalloc(sizeof(t_list_inst));
  new_element->addr        = addr;
  new_element->instruction = xmalloc((strlen(instruction)+1)*sizeof(char));
  strcpy(new_element->instruction, instruction);
  new_element->next        = old_element;

  return (new_element);
}

/* free linked list */
void free_list_inst(t_list_inst *element)
{
  t_list_inst *tmp;

  while (element)
    {
      tmp = element;
      element = tmp->next;
      free(tmp->instruction);
      free(tmp);
    }
}

/* returns addr of instruction */
Address ret_addr_makecodefunc(t_list_inst *list_ins, const char *instruction)
{
  char  *p;

  for (; list_ins; list_ins = list_ins->next)
    for (p = list_ins->instruction; *p != 0; p++)
      if (match(p, instruction))
        return list_ins->addr;

  return 0;
}

void sc_print_code(Size word, size_t len, const char *comment)
{
  if (len == 4)
    fprintf(stdout, "\t\t%sp += pack(\"<I\", 0x%.8x) # %s%s\n", BLUE, (unsigned int)word, comment, ENDC);
}

void sc_print_str(const char *quad, size_t len, const char *comment)
{
  char *tmp = xmalloc(len+1);
  memset(tmp, '\0', len+1);
  strncpy(tmp, quad, len);
  /* assume that the caller will deal with overflow */
  while(strlen(tmp) < len)
    tmp[strlen(tmp)] = 'A';
  fprintf(stdout, "\t\t%sp += \"%s\" # %s%s\n", BLUE, tmp, comment?comment:tmp, ENDC);
  free(tmp);
}

/* display padding */
void sc_print_padding(size_t i, size_t len)
{
  char *tmp = xmalloc(len+1);
  memset(tmp, 'A', len);
  tmp[len] = '\0';
  for (; i != 0; i--)
    sc_print_str(tmp, len, "padding");
  free(tmp);
}

void sc_print_sect_addr(int offset, int data, size_t bytes)
{
  char comment[32] = {0};
  sprintf(comment, (offset==0)?"@ %s":"@ %s + %d", data?".data":".got", offset);
  sc_print_code((data?Addr_sData:Addr_sGot)+offset, bytes, comment);
}



/* returns the numbers of pop in the gadget. */
int how_many_pop(const char *gadget)
{
  int  cpt = 0;

  for (; *gadget != '\0'; gadget++)
    if (!strncmp(gadget, "pop", 3))
      cpt++;

  return cpt;
}

/* returns first/second reg in "mov %e?x,(%e?x)" instruction */
char *get_reg(const char *gadget, int first)
{
  char *p;

  p = xmalloc(4 * sizeof(char));
  while (*gadget != '(' && *gadget != '\0')
    gadget++;

  gadget += (first?-4:2);
  strncpy(p, gadget, 3);
  return p;
}

/* returns the numbers of "pop" befor pop_reg */
int how_many_pop_before(const char *gadget, const char *pop_reg)
{
  int cpt = 0;

  for (; strncmp(gadget, pop_reg, strlen(pop_reg)) && *gadget != '\0'; gadget++)
    if (!strncmp(gadget, "pop", 3))
      cpt++;

  return cpt;
}

/* returns the numbers of "pop" after pop_reg */
int how_many_pop_after(const char *gadget, const char *pop_reg)
{
  int cpt = 0;

  for(; strncmp(gadget, pop_reg, strlen(pop_reg)); gadget++)
    if (*gadget == '\0')
      return 0;

  gadget += strlen(pop_reg);

  for (; *gadget != '\0'; gadget++)
    if (!strncmp(gadget, "pop", 3))
      cpt++;

  return cpt;
}

void sc_print_sect_addr_pop(const t_asm *inst, const char *instruction, int offset, int data, size_t bytes)
{
  sc_print_code(inst->addr, bytes, (syntaxins==INTEL)?inst->instruction_intel:inst->instruction);
  sc_print_padding(how_many_pop_before(inst->instruction, instruction), bytes);
  sc_print_sect_addr(offset, data, bytes);
  sc_print_padding(how_many_pop_after(inst->instruction, instruction), bytes);
}

void sc_print_str_pop(const t_asm *inst, const char *instruction, const char *str, size_t bytes)
{
  sc_print_code(inst->addr, bytes, (syntaxins==INTEL)?inst->instruction_intel:inst->instruction);
  sc_print_padding(how_many_pop_before(inst->instruction, instruction), bytes);
  sc_print_str(str, bytes, NULL);
  sc_print_padding(how_many_pop_after(inst->instruction, instruction), bytes);
}

void sc_print_addr_pop(const t_asm *inst, const char *instruction, Address addr, const char *comment, size_t bytes)
{
  sc_print_code(inst->addr, bytes, (syntaxins==INTEL)?inst->instruction_intel:inst->instruction);
  sc_print_padding(how_many_pop_before(inst->instruction, instruction), bytes);
  sc_print_code(addr, bytes, comment);
  sc_print_padding(how_many_pop_after(inst->instruction, instruction), bytes);
}

/* a 'solo inst' is an instruction that isn't a pop so all the pops must be padded */
void sc_print_solo_inst(const t_asm *inst, size_t bytes)
{
  sc_print_code(inst->addr, bytes, (syntaxins==INTEL)?inst->instruction_intel:inst->instruction);
  sc_print_padding(how_many_pop(inst->instruction), bytes);
}

void sc_print_string(const char *str, const t_rop_writer *wr, int offset_start, int data, size_t bytes)
{
  int i;
  int l = strlen(str);
  for (i = 0; i <= l; i += bytes)
    {
      sc_print_sect_addr_pop(wr->pop_target, wr->reg_target, offset_start + i, data, bytes);
      if (i < l)
        sc_print_str_pop(wr->pop_data, wr->reg_data, str+i, bytes);
      else
        sc_print_solo_inst(wr->zero_data, bytes);
      sc_print_solo_inst(wr->mov, bytes);
    }
}

void sc_print_vector(const int *args, const t_rop_writer *wr, int offset_start, int data, size_t bytes)
{
  int i;

  for (i = 0; 1; i++)
    {
      sc_print_sect_addr_pop(wr->pop_target, wr->reg_target, offset_start + bytes *i, data, bytes);
      if (args[i] != -1)
        sc_print_sect_addr_pop(wr->pop_data, wr->reg_data, args[i], data, bytes);
      else
        sc_print_solo_inst(wr->zero_data, bytes);
      sc_print_solo_inst(wr->mov, bytes);
      if (args[i] == -1)
        return;
    }
}

size_t sc_print_argv(const char * const *args, const t_rop_writer *wr, int offset_start, int data, size_t bytes, int *argv_start, int *envp_start)
{
  int num_args, i, offset;
  int *vector;

  for (num_args = 0; args[num_args]; num_args++) ;

  vector = xmalloc(sizeof(int) * (num_args+1));

  offset = offset_start;

  for (i = 0; args[i]; i++) {
    sc_print_string(args[i], wr, offset, data, bytes);
    vector[i] = offset;
    offset += strlen(args[i])+1;
  }

  if (argv_start != NULL)
    *argv_start = offset;

  vector[i] = -1;

  sc_print_vector(vector, wr, offset, data, bytes);
  free(vector);

  if (envp_start != NULL)
    *envp_start = offset + (num_args)*bytes;

  return (offset - offset_start) + (num_args+1)*bytes;
}

int sc_print_gotwrite(const t_importsc_writer *wr, size_t bytes)
{
  size_t i;
  char comment[32] = {0};

  /* check if all opcodes about shellcode was found in .text */
  if (!check_opcode_was_found())
    {
      fprintf(stdout, "\t%s/!\\ Impossible to generate your shellcode because some opcode was not found.%s\n", RED, ENDC);
      return 0;
    }

  fprintf(stdout, "\t\t%s# Shellcode imported! Generated by RopGadget v3.4.2%s\n", BLUE, ENDC);

  for (i = 0; i != importsc_mode.size && importsc_mode.poctet != NULL; i++, importsc_mode.poctet = importsc_mode.poctet->back)
    {
      /* pop %edx */
      sprintf(comment, "0x%.2x", importsc_mode.poctet->octet);
      sc_print_addr_pop(wr->pop_gad, wr->pop_reg, importsc_mode.poctet->addr, comment, bytes);

      /* mov (%edx),%ecx */
      sc_print_solo_inst(wr->mov_gad2, bytes);
      if (wr->mov_gad3)
        /* mov %ecx,%eax */
        sc_print_solo_inst(wr->mov_gad3, bytes);
      /* pop %edx */
      sc_print_sect_addr_pop(wr->pop_gad, wr->pop_reg, i, FALSE, bytes);
      /* mov %eax,(%edx) */
      sc_print_solo_inst(wr->mov_gad4, bytes);
    }
  sc_print_code(Addr_sGot, bytes, "jump to our shellcode in .got");
  return 1;
}
