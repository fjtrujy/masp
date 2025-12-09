/* sb.c - string buffer manipulation routines
   Copyright 1994, 1995, 2000 Free Software Foundation, Inc.
   Copyright 2003 Johann Gunnar Oskarsson

   Written by Steve and Judy Chamberlain of Cygnus Support,
      sac@cygnus.com

   Maintained by Johann Gunnar Oskarsson
      <myrkraverk@users.souceforge.net>

   This file is part of MASP, the Assembly Preprocessor

   MASP is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   MASPis distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with MASP; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "compat.h"
#include "sb.h"

/* These routines are about manipulating strings.

   They are managed in things called `sb's which is an abbreviation
   for string buffers.  An sb has to be created, things can be glued
   on to it, and at the end of it's life it should be freed.  The
   contents should never be pointed at whilst it is still growing,
   since it could be moved at any time

   eg:
   sb_new (&foo);
   sb_grow... (&foo,...);
   use foo->ptr[*];
   sb_kill (&foo);

*/

#define dsize 5

static void sb_check(sb *ptr, int len);

/* Statistics of sb structures.  */

int string_count[sb_max_power_two];

/* initializes an sb.  */

void
sb_build (sb *ptr, int size)
{
  sb_element *e;

  if (size > sb_max_power_two)
    abort();
  if (size < 0)
    abort();

  /* Always allocate fresh - simpler and more reliable */
  /* Use calloc to zero-initialize and catch uninitialized memory bugs */
  size_t total_size = sizeof (sb_element) + (1 << size);
  e = (sb_element *) calloc (1, total_size);
  if (!e)
    abort();
  e->next = NULL;
  e->size = 1 << size;
  string_count[size]++;

  /* copy into callers world */
  ptr->ptr = e->data;
  ptr->pot = size;
  ptr->len = 0;
  ptr->item = e;
}

void
sb_new (ptr)
     sb *ptr;
{
  sb_build (ptr, dsize);
}

/* deallocate the sb at ptr */

void
sb_kill (sb *ptr)
{
  /* Validate parameters */
  if (ptr->pot < 0 || ptr->pot >= sb_max_power_two)
    abort();
  if (ptr->item == NULL)
    abort();

  /* Free the memory */
  free(ptr->item);

  /* Clear the sb to catch use-after-free */
  ptr->ptr = NULL;
  ptr->len = 0;
  ptr->item = NULL;
}

/* add the sb at s to the end of the sb at ptr */

void
sb_add_sb (sb *ptr, const sb *s)
{
  sb_check (ptr, s->len);
  memcpy (ptr->ptr + ptr->len, s->ptr, s->len);
  ptr->len += s->len;
}

/* make sure that the sb at ptr has room for another len characters,
   and grow it if it doesn't.  */

static void
sb_check (sb *ptr, int len)
{
  /* Validate input */
  if (len < 0)
    abort();
  if (ptr->pot < 0 || ptr->pot >= sb_max_power_two)
    abort();
  if (ptr->len < 0)
    abort();

  if (ptr->len + len > 1 << ptr->pot)
    {
      sb tmp;
      int pot = ptr->pot;

      while (ptr->len + len > 1 << pot)
	{
	  pot++;
	  if (pot >= sb_max_power_two)
	    abort();
	}

      sb_build (&tmp, pot);
      sb_add_sb (&tmp, ptr);

      /* Kill the old buffer before reassigning */
      sb_kill (ptr);

      /* Verify tmp is valid before copying */
      if (tmp.pot != pot || tmp.item == NULL)
        abort();

      *ptr = tmp;
    }
}

/* make the sb at ptr point back to the beginning.  */

void
sb_reset (sb *ptr)
{
  ptr->len = 0;
}

/* add character c to the end of the sb at ptr.  */

void
sb_add_char (sb *ptr, int c)
{
  sb_check (ptr, 1);
  /* Defensive check: ensure we're not writing past the buffer */
  if (ptr->len >= (1 << ptr->pot))
    abort();
  ptr->ptr[ptr->len++] = c;
}

/* add null terminated string s to the end of sb at ptr.  */

void
sb_add_string (sb *ptr, const char *s)
{
  int len = strlen (s);
  sb_check (ptr, len);
  /* Defensive check */
  if (ptr->len + len > (1 << ptr->pot))
    abort();
  memcpy (ptr->ptr + ptr->len, s, len);
  ptr->len += len;
}

/* add string at s of length len to sb at ptr */

void
sb_add_buffer (sb *ptr, const char *s, int len)
{
  sb_check (ptr, len);
  /* Defensive check */
  if (ptr->len + len > (1 << ptr->pot))
    abort();
  memcpy (ptr->ptr + ptr->len, s, len);
  ptr->len += len;
}

/* print the sb at ptr to the output file */

void
sb_print (FILE *outfile, sb *ptr)
{
  int i;
  int nc = 0;

  for (i = 0; i < ptr->len; i++)
    {
      if (nc)
	{
	  fprintf (outfile, ",");
	}
      fprintf (outfile, "%d", ptr->ptr[i]);
      nc = 1;
    }
}

void
sb_print_at (FILE *outfile, int idx, sb *ptr)
{
  int i;
  for (i = idx; i < ptr->len; i++)
    putc (ptr->ptr[i], outfile);
}

/* put a null at the end of the sb at in and return the start of the
   string, so that it can be used as an arg to printf %s.  */

char *
sb_name (sb *in)
{
  /* stick a null on the end of the string */
  sb_add_char (in, 0);
  return in->ptr;
}

/* like sb_name, but don't include the null byte in the string.  */

char *
sb_terminate (sb *in)
{
  sb_add_char (in, 0);
  --in->len;
  return in->ptr;
}

/* start at the index idx into the string in sb at ptr and skip
   whitespace. return the index of the first non whitespace character */

int
sb_skip_white (int idx, const sb *ptr)
{
  while (idx < ptr->len
	 && (ptr->ptr[idx] == ' '
	     || ptr->ptr[idx] == '\t'))
    idx++;
  return idx;
}

/* start at the index idx into the sb at ptr. skips whitespace,
   a comma and any following whitespace. returnes the index of the
   next character.  */

int
sb_skip_comma (int idx, const sb *ptr)
{
  while (idx < ptr->len
	 && (ptr->ptr[idx] == ' '
	     || ptr->ptr[idx] == '\t'))
    idx++;

  if (idx < ptr->len
      && ptr->ptr[idx] == ',')
    idx++;

  while (idx < ptr->len
	 && (ptr->ptr[idx] == ' '
	     || ptr->ptr[idx] == '\t'))
    idx++;

  return idx;
}

// Eat literal until end, must start with " or '

int sb_eat_literal( int idx, sb *out, const sb *in )
{
  if ( idx < in->len && ( in->ptr[ idx ] == '"' || in->ptr[ idx ] == '\'' ) )
    {
      char str_type = in->ptr[ idx ];
      sb_add_char( out, in->ptr[ idx++ ] );
      while ( idx < in->len )
	{
	  if ( in->ptr[ idx ] == '\\' && idx < in->len - 1 )
	    {
	      sb_add_char( out, in->ptr[ ++idx ]);
	      idx++;
	    }
	  else if ( in->ptr[ idx ] == str_type )
	    {
	      sb_add_char( out, in->ptr[ idx++ ]);
	      return idx;
	    }
	  else
	    sb_add_char( out, in->ptr[ idx++ ]);
	}
      return idx;
    }
  else
    return idx;
}
