/*
    Copyright (C) 2024 Loophole Labs

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef MEMORY_OVERLAY_HASHTABLE_COMMON_H
#define MEMORY_OVERLAY_HASHTABLE_COMMON_H

#ifndef UUID_SIZE
#define UUID_SIZE 16
#endif

#include <linux/rhashtable.h>
#include <linux/uuid.h>

struct hashtable_object {
	unsigned long key;
	struct rhash_head linkage;
	void *data;
	struct rcu_head rcu_read;
};

struct hashtable {
	unsigned char id[UUID_SIZE];
	struct rhashtable rhashtable;
	void (*free)(void *ptr);
};

struct hashtable *hashtable_setup(void (*free)(void *ptr));
int hashtable_insert(struct hashtable *hashtable, const unsigned long key,
		     void *data);
void *hashtable_lookup(struct hashtable *hashtable, const unsigned long key);
void *hashtable_delete(struct hashtable *hashtable, const unsigned long key);
void hashtable_cleanup(struct hashtable *hashtable);

#endif //MEMORY_OVERLAY_HASHTABLE_COMMON_H
