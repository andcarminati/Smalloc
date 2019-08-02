/*
User space memory allocator
Copyright (C) 2016  Andreu Carminati
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "allocation.h"
#include <stdlib.h> 
#include <stdio.h>
#include <unistd.h> 

#define MIN_BLOCK 16
#define FREE_CHUNK 0x1
#define BUSY_CHUNK 0x2
#define INITIAL_BLOCK_SIZE 512

#define chunk_size(s) (sizeof(CHUNK) + s)

#define get_next_chunk(ch) ((CHUNK*) ((char*)ch+sizeof(CHUNK) + ch->size))

#define locate_size_addr(ptr) (ptr + sizeof(CHUNK) + ptr->size)

#define is_valid_pointer(ptr) (ptr == (void*) -1)

#define min_chunk_size() (chunk_size(MIN_BLOCK))

#define get_location() sbrk(0)

#define expand_mem(inc) sbrk(inc)

/*chunks/memory layout
	---------------------------------
chunk1  | flags (4 bytes)		|
	| size (4 bytes)		|
        | prev (pointer of 4/8 bytes)   |
	| next (pointer of 4/8 bytes)	|
	| ............			|
	| ... data ...			|
	| ... data ...	(free or in use)|
	| ............			|
	---------------------------------
	---------------------------------
chunk2  | flags (4 bytes)		|
	| size (4 bytes)		|
        | prev (pointer of 4/8 bytes)   |
	| next (pointer of 4/8 bytes)	|
	| ............			|
	| ... data ...			|
	| ... data ...	(free or in use)|
	| ............			|
	---------------------------------
	
*/

/*used types*/
typedef void* __pointer;
typedef int __size;
typedef int* __size_pointer;

/*main data structure*/
typedef struct chunk {
	int flags;
	__size size;
	struct chunk *prev;
	struct chunk *next;
} CHUNK;

static int initialized = 0;

/*chunk list*/
static CHUNK *first;
static CHUNK *last;
static __pointer *limit;

/*internal functions*/
static int initialize(void);
static int calculate_nblocks(__size);
static CHUNK* find_free_chunk(__size);
static CHUNK* get_chunk(__pointer);
static void subdivide_chunk(CHUNK*, __size);

/*chunk ch->size = sz*/
static void subdivide_chunk(CHUNK *ch, __size sz){
	CHUNK *new_ch, *aux;
	__size  r_size;
	__size_pointer size_addr;
	
	/*rest of free area*/
	r_size = (ch->size - sz - sizeof(CHUNK));
	
	/*change size of left chunk*/
	ch->size = sz;

	/*calculate address of the new right chunk*/
	new_ch = get_next_chunk(ch);
	new_ch->size = r_size;
	new_ch->flags = FREE_CHUNK;
	
	/*change links*/
	aux = ch->next;
	ch->next = new_ch;
	new_ch->prev = ch; 
	new_ch->next = aux;
	
	if(aux){
		aux->prev = new_ch;
	} else {
		last = new_ch;
	}
}

// /*get chunk address by data pointer*/
static CHUNK* get_chunk(__pointer ptr){
	CHUNK *chunk;
	
	chunk = ptr - sizeof(CHUNK);
	/*check chunk*/
	if(chunk < first || chunk > last){
		return NULL;
	}
	/*may fail*/
	if(chunk->flags == FREE_CHUNK || chunk->flags == BUSY_CHUNK){
		
		return chunk;
	}
	return NULL;	
}

/*first fit search on the chunk list*/
static CHUNK* find_free_chunk(__size size){
	CHUNK *chunk;
	
	chunk = first;
	while(chunk){
		if(chunk->flags & FREE_CHUNK){
			if(chunk->size >= size){
				return chunk;
			}
		}
		chunk = chunk->next;
	}
	return NULL;
}
/*
static int calculate_final_size(__size size){
	int logb2 = 1, i, f_size = 1;
	
	size--;
	while((size /= 2) != 0){	
		logb2++;
	}
	for(i = 0; i < logb2; i++){
		f_size *= 2; 
	}

	return f_size;	
}
*/

/*round to a multiple of 4*/
static int calculate_final_size(__size size){
	int mod, div;
	__size final_size;

	mod = size%4;
	div = size/4;
	
	final_size = div*4;
	if(mod != 0){
		final_size +=4;
	}
	return final_size;
}

/*initialize the data structures of the allocator*/
static int initialize(void){
	__pointer initial;
	CHUNK *chunk;
	int error;

	/*determine the start address of the heap*/
	initial = get_location();
	if(is_valid_pointer(initial)){
		return 0;
	}
	/*try to expand the heap size*/
	if(is_valid_pointer(expand_mem(chunk_size(INITIAL_BLOCK_SIZE)))){
		return 0;
	}
	limit = get_location();
	chunk = initial;
	chunk->flags = FREE_CHUNK;
	chunk->next = NULL;
	chunk->prev = NULL;
	chunk->size = INITIAL_BLOCK_SIZE;

	first = chunk;
	last = chunk;

	initialized = 1;

	return 1;
}

void *Alloc(__size size){
	CHUNK *ch;
	__pointer *ptr;

	if(size == 0){
		return NULL;
	}

	/*initialize if not yet done*/
	if(!initialized){
		if(!initialize()){
			return NULL;
		}
	}
	/*set size to one block, if size is too small*/
	if(size <= MIN_BLOCK){
		size =  MIN_BLOCK;
	} else {
		size = calculate_final_size(size);
	}
	ch = find_free_chunk(size);
	if(!ch){
		CHUNK *new_ch;

		/*try to expand the last chunk*/
		if(last->flags == FREE_CHUNK){
			__size extra_size;
			
			extra_size = size - last->size;
			if(is_valid_pointer(expand_mem(extra_size))){
			  return NULL;
			}
			limit = get_location();
			last->size = size;
			last->flags = BUSY_CHUNK;
		
			ptr = (__pointer) last + sizeof(CHUNK);

			return ptr;

		}
		
		/*create a new last chunk at the end*/
		if(is_valid_pointer(expand_mem(chunk_size(size)))){
			return NULL;
		}
		new_ch = (CHUNK*)limit;
		new_ch->next = NULL;
		new_ch->prev = last;
		new_ch->flags = BUSY_CHUNK;
		new_ch->size = size;
		last->next = new_ch;
		last = new_ch;
		limit = get_location();

		ptr = (__pointer) new_ch + sizeof(CHUNK);

		return ptr;

	/*a free chunk was found, let us use then*/	
	} else {
		__size block_size = ch->size;
		__size rest_size = block_size - size;

		/*divide? not divide?*/
		if((rest_size >= min_chunk_size())){
			subdivide_chunk(ch, size);
		}
	}
	/*here, the chunk exists*/
	ch->flags = BUSY_CHUNK;
	ptr = (__pointer) ch + sizeof(CHUNK);

	return ptr;
}

void Free(void * ptr){
	CHUNK *ch;
	
	ch = get_chunk(ptr);
	
	/*the pointer is not valid...*/
	if(!ch){
		return;
	}

	/*the pointer was already freed, we need not touch it*/
	if(ch->flags == FREE_CHUNK){
		return;
	}

	ch->flags = FREE_CHUNK;
	CHUNK *r_ch;
	__size extra_size = 0;

	/*
	*the prev is free? if so, we will merge.
	*the next is free? if so, we will merge.
	*/
	if(ch->prev){
		CHUNK *prev;
		prev = ch->prev;
		if(!(prev->flags & BUSY_CHUNK)){
			ch = prev;
		}
	}
		
	r_ch = ch->next;
	while(r_ch){
		if(r_ch->flags & BUSY_CHUNK){
			break;
		}
		extra_size += chunk_size(r_ch->size);
		r_ch = r_ch->next;
	}
	if(extra_size > 0){
		ch->size += extra_size;
		ch->next = r_ch;
		if(r_ch){
			r_ch->prev = ch;
		} else {
			last = ch;
		}
	}
}

void *Realloc(__pointer ptr, __size new_size){
	char *new_alloc, *old_alloc;
	__size old_size;
	CHUNK *next_ch, *ch;
	int i;

	/*is to liberate the area?*/
	if(new_size == 0){
		Free(ptr);
		return NULL;
	}

	/*is to allocate a new area?*/
	if(ptr == NULL){
		return Alloc(new_size);
	}
	ch = get_chunk(ptr);
	next_ch = ch->next;
	old_size = ch->size;
	__size diff_size = new_size - old_size;
	
	/*is to shrink the area?*/
	if(diff_size < 0 && diff_size >= min_chunk_size()){
	  subdivide_chunk(ch,new_size);
	  return ptr;
	}
	
	/*the next is free, we must, if possible, lend more space!*/
	if(next_ch){
		if(next_ch->flags & FREE_CHUNK){
			__size next_size, next_r_size;
	
			next_size = next_ch->size;
			next_r_size = next_size - diff_size;

			/*expand chunk*/
			if(next_r_size > min_chunk_size() && next_r_size > 0){
				CHUNK *mv_ch;
				mv_ch = (__pointer) ch + new_size; 
				mv_ch->size = next_r_size;
				mv_ch->next = next_ch->next;
				mv_ch->prev = next_ch->prev;
				mv_ch->flags = FREE_CHUNK;

				ch->next = mv_ch;
				ch->size = new_size;
				
				if(mv_ch->next){
					mv_ch->next->prev = mv_ch;
				}

				return ptr;
			}
			/*if the next chunk is too small and is the last one we may go over it*/
			if(next_ch == last){
			  	__size missing_size;
			  	next_size = next_ch->size;
			  	ch->next = NULL;
			  	missing_size = diff_size - chunk_size(next_size);
			  	if(is_valid_pointer(expand_mem(missing_size))){
			  		return NULL;
			 	}
			  	ch->size = new_size;
			  	last = ch;
			  	limit = get_location();
			  	return ptr;
			}
		}
	}
	
	if(ch == last){
		if(is_valid_pointer(expand_mem(diff_size))){
			return NULL;
	  	}
	  	ch->size = new_size;
	  	limit = get_location();
	  	return ptr;
	}
	/*allocate a new area and copy the data*/
	new_alloc = Alloc(new_size);
	if(!new_alloc){
		return NULL;
	}
	old_alloc = (char*) ptr;
	
	for(i = 0; i < old_size; i ++){
		new_alloc[i] = old_alloc[i];
	}
	Free(old_alloc);
	
	return (void*) new_alloc;
}

void dump(void){
	CHUNK *ch;
	int count = 0;

	ch = first;
	printf("##Alloc Memory Dump Start: \n");
	while(0 && ch){
		printf("<Chunk%d Start>\n", count);
		printf("\t<Size: %d>\n", ch->size);
		if(ch->flags & FREE_CHUNK){
			printf("\t<State: UNUSED>\n", ch->size);
		} else {
			printf("\t<State: USED>\n", ch->size);
		}
		printf("\t<Boundary tags start address: %d>\n", (long long) ch);
		printf("\t<Boundary tags end address:   %d>\n", (long long) ((char*)ch+sizeof(CHUNK)) - 1);
		printf("\t<Data start address:          %d>\n", (long long) ((char*)ch+sizeof(CHUNK)));
		printf("\t<Data end address:            %d>\n", (long long) ((char*)ch+sizeof(CHUNK)) + ch->size - 1);

		printf("<Chunk%d End>\n", count++);
		ch = ch->next;
		if(!ch){
			break;
		}
	}
	ch = first;
	while(ch){
		printf("[");
		if(ch->flags & FREE_CHUNK){
			printf("LIVRE: %d bytes", ch->size);
		} else {
			printf("USADO: %d bytes", ch->size);
		}

		ch = ch->next;
		if(!ch){
		        printf("]\n");
			break;
		}
		printf("]----->");
	}
	
	//printf("Break : %ld\n", (long long) limit);
	printf("##Alloc Memory Dump End: \n");
}
