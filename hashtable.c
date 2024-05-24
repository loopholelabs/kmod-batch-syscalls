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

#include <linux/rhashtable.h>
#include <linux/uuid.h>
#include <linux/slab.h>

#include "hashtable.h"
#include "log.h"

const static struct rhashtable_params hashtable_object_params = {
	.key_len = sizeof(unsigned char) * UUID_SIZE,
	.key_offset = offsetof(struct hashtable_object, key),
	.head_offset = offsetof(struct hashtable_object, linkage),
};

void hashtable_object_free_fn(void *ptr, void *arg)
{
	log_trace("start hashtable_object_free_fn");
	kvfree(ptr);
	log_trace("end hashtable_object_free_fn");
}

struct hashtable *hashtable_setup(void (*free)(void *data))
{
	log_trace("start hashtable_setup");
	struct hashtable *hashtable =
		kvmalloc(sizeof(struct hashtable), GFP_KERNEL);
	if (!hashtable) {
		log_crit("unable to allocate memory for hashtable");
		log_trace("end hashtable_setup");
		return NULL;
	}
	generate_random_uuid(hashtable->id);
	hashtable->free = free;
	log_trace("start hashtable_setup for hashtable with id '%pUB'",
		  hashtable->id);
	int ret = rhashtable_init(&hashtable->rhashtable,
				  &hashtable_object_params);
	if (ret) {
		log_crit("unable to initialize hashtable with id '%pUB': '%d'",
			 hashtable->id, ret);
		log_trace("end hashtable_setup for hashtable with id '%pUB'",
			  hashtable->id);
		kvfree(hashtable);
		log_trace("end hashtable_setup");
		return NULL;
	}
	log_trace("end hashtable_setup for hashtable with id '%pUB'",
		  hashtable->id);
	log_trace("end hashtable_setup");
	return hashtable;
}

int hashtable_insert(struct hashtable *hashtable,
		     const unsigned char key[UUID_SIZE], void *data)
{
	log_trace("start hashtable_insert for hashtable with id '%pUB'",
		  hashtable->id);
	struct hashtable_object *object =
		kvmalloc(sizeof(struct hashtable_object), GFP_KERNEL);
	if (!object) {
		log_error(
			"unable to allocate memory for hashtable object for hashtable with id '%pUB'",
			hashtable->id);
		log_trace("end hashtable_insert for hashtable with id '%pUB'",
			  hashtable->id);
		return -ENOMEM;
	}
	object->data = data;
	memcpy(object->key, key, sizeof(unsigned char) * UUID_SIZE);
	log_debug(
		"inserting hashtable object with key '%pUB' for hashtable with id '%pUB'",
		key, hashtable->id);
	int ret = rhashtable_lookup_insert_fast(&hashtable->rhashtable,
						&object->linkage,
						hashtable_object_params);
	log_trace("end hashtable_insert for hashtable with id '%pUB'",
		  hashtable->id);
	return ret;
}

void *hashtable_lookup(struct hashtable *hashtable,
		       const unsigned char key[UUID_SIZE])
{
	log_trace("called hashtable_lookup for hashtable with id '%pUB'",
		  hashtable->id);
	void *data = NULL;
	rcu_read_lock();
	struct hashtable_object *object = rhashtable_lookup(
		&hashtable->rhashtable, key, hashtable_object_params);
	if (object) {
		data = object->data;
	} else {
		log_debug(
			"hashtable object with key '%pUB' not found for hashtable with id '%pUB'",
			key, hashtable->id);
	}
	rcu_read_unlock();
	log_trace("end hashtable_lookup for hashtable with id '%pUB'",
		  hashtable->id);
	return data;
}

void *hashtable_delete(struct hashtable *hashtable,
		       const unsigned char key[UUID_SIZE])
{
	log_trace("called hashtable_delete for hashtable with id '%pUB'",
		  hashtable->id);
	void *ret = NULL;
	rcu_read_lock();
	struct hashtable_object *object = rhashtable_lookup(
		&hashtable->rhashtable, key, hashtable_object_params);
	if (object) {
		if (!rhashtable_remove_fast(&hashtable->rhashtable,
					    &object->linkage,
					    hashtable_object_params)) {
			ret = object->data;
			kvfree_rcu(object, rcu_read);
			log_debug(
				"removed hashtable object '%pUB' for hashtable with id '%pUB'",
				key, hashtable->id);
		} else {
			log_error(
				"unable to remove hashtable object '%pUB' for hashtable with id '%pUB'",
				key, hashtable->id);
		}
	} else {
		log_debug(
			"hashtable object with key '%pUB' not found for hashtable with id '%pUB'",
			key, hashtable->id);
	}
	rcu_read_unlock();
	log_trace("end hashtable_delete for hashtable with id '%pUB'",
		  hashtable->id);
	return ret;
}

void hashtable_cleanup(struct hashtable *hashtable)
{
	log_trace("start hashtable_cleanup");
	log_trace("start hashtable_cleanup for hashtable with id '%pUB'",
		  hashtable->id);
	if (hashtable->free) {
		log_debug("freeing hashtable with id '%pUB'", hashtable->id);
		struct rhashtable_iter iter;
		struct hashtable_object *object;
		rhashtable_walk_enter(&hashtable->rhashtable, &iter);
		rhashtable_walk_start(&iter);
		while ((object = rhashtable_walk_next(&iter)) != NULL) {
			if (IS_ERR(object)) {
				log_warn(
					"found an error object while walking through hashtable with id '%pUB'",
					hashtable->id);
				continue;
			}
			rhashtable_walk_stop(&iter);
			log_debug(
				"freeing hashtable object with key '%pUB' for hashtable with id '%pUB'",
				object->key, hashtable->id);
			hashtable->free(object->data);
			rhashtable_walk_start(&iter);
		}
		rhashtable_walk_stop(&iter);
		rhashtable_walk_exit(&iter);
	}
	rhashtable_free_and_destroy(&hashtable->rhashtable,
				    &hashtable_object_free_fn, NULL);
	log_trace("end hashtable_cleanup for hashtable with id '%pUB'",
		  hashtable->id);
	kvfree(hashtable);
	log_trace("end hashtable_cleanup");
}
