/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Routines to handle mallocing of results which will be freed the same time */

#include <ma_global.h>
#include <ma_sys.h>
#include <ma_string.h>

void ma_init_ma_alloc_root(MA_MEM_ROOT *mem_root, size_t block_size, size_t pre_alloc_size)
{
  mem_root->free=mem_root->used=0;
  mem_root->min_malloc=32;
  mem_root->block_size=block_size-MALLOC_OVERHEAD-sizeof(MA_USED_MEM)-8;
  mem_root->error_handler=0;
#if !(defined(HAVE_purify) && defined(EXTRA_DEBUG))
  if (pre_alloc_size)
  {
    if ((mem_root->free = mem_root->pre_alloc=
	 (MA_USED_MEM*) malloc(pre_alloc_size+ ALIGN_SIZE(sizeof(MA_USED_MEM)))))
    {
      mem_root->free->size=pre_alloc_size+ALIGN_SIZE(sizeof(MA_USED_MEM));
      mem_root->free->left=pre_alloc_size;
      mem_root->free->next=0;
    }
  }
#endif
}

gptr ma_alloc_root(MA_MEM_ROOT *mem_root, size_t Size)
{
#if defined(HAVE_purify) && defined(EXTRA_DEBUG)
  reg1 MA_USED_MEM *next;
  Size+=ALIGN_SIZE(sizeof(MA_USED_MEM));

  if (!(next = (MA_USED_MEM*) malloc(Size)))
  {
    if (mem_root->error_handler)
      (*mem_root->error_handler)();
    return((gptr) 0);				/* purecov: inspected */
  }
  next->next=mem_root->used;
  mem_root->used=next;
  return (gptr) (((char*) next)+ALIGN_SIZE(sizeof(MA_USED_MEM)));
#else
  size_t get_size,max_left;
  gptr point;
  reg1 MA_USED_MEM *next;
  reg2 MA_USED_MEM **prev;

  Size= ALIGN_SIZE(Size);
  prev= &mem_root->free;
  max_left=0;
  for (next= *prev ; next && next->left < Size ; next= next->next)
  {
    if (next->left > max_left)
      max_left=next->left;
    prev= &next->next;
  }
  if (! next)
  {						/* Time to alloc new block */
    get_size= Size+ALIGN_SIZE(sizeof(MA_USED_MEM));
    if (max_left*4 < mem_root->block_size && get_size < mem_root->block_size)
      get_size=mem_root->block_size;		/* Normal alloc */

    if (!(next = (MA_USED_MEM*) calloc(1, get_size)))
    {
      if (mem_root->error_handler)
	(*mem_root->error_handler)();
      return((gptr) 0);				/* purecov: inspected */
    }
    next->next= *prev;
    next->size= get_size;
    next->left= get_size-ALIGN_SIZE(sizeof(MA_USED_MEM));
    *prev=next;
  }
  point= (gptr) ((char*) next+ (next->size-next->left));
  if ((next->left-= Size) < mem_root->min_malloc)
  {						/* Full block */
    *prev=next->next;				/* Remove block from list */
    next->next=mem_root->used;
    mem_root->used=next;
  }
  return(point);
#endif
}

	/* deallocate everything used by ma_alloc_root */

void ma_free_root(MA_MEM_ROOT *root, myf MyFlags)
{
  reg1 MA_USED_MEM *next,*old;

  if (!root)
    return; /* purecov: inspected */
  if (!(MyFlags & MY_KEEP_PREALLOC))
    root->pre_alloc=0;

  for ( next=root->used; next ;)
  {
    old=next; next= next->next ;
    if (old != root->pre_alloc)
      free(old);
  }
  for (next= root->free ; next ; )
  {
    old=next; next= next->next ;
    if (old != root->pre_alloc)
      free(old);
  }
  root->used=root->free=0;
  if (root->pre_alloc)
  {
    root->free=root->pre_alloc;
    root->free->left=root->pre_alloc->size-ALIGN_SIZE(sizeof(MA_USED_MEM));
    root->free->next=0;
  }
  return;
}


char *ma_strdup_root(MA_MEM_ROOT *root,const char *str)
{
  size_t len= strlen(str)+1;
  char *pos;
  if ((pos=ma_alloc_root(root,len)))
    memcpy(pos,str,len);
  pos[len]= 0;
  return pos;
}


char *ma_memdup_root(MA_MEM_ROOT *root, const char *str, size_t len)
{
  char *pos;
  if ((pos=ma_alloc_root(root,len)))
    memcpy(pos,str,len);
  return pos;
}
