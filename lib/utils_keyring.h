/*
 * kernel keyring syscall wrappers
 *
 * Copyright (C) 2016-2023 Red Hat, Inc. All rights reserved.
 * Copyright (C) 2016-2023 Ondrej Kozina
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _UTILS_KEYRING
#define _UTILS_KEYRING

#include <stddef.h>
#include <stdint.h>

#ifndef HAVE_KEY_SERIAL_T
#define HAVE_KEY_SERIAL_T
typedef int32_t key_serial_t;
#endif

typedef enum { LOGON_KEY = 0, USER_KEY, BIG_KEY, TRUSTED_KEY, ENCRYPTED_KEY, INVALID_KEY } key_type_t;

const char *key_type_name(key_type_t ktype);
key_type_t key_type_by_name(const char *name);
int32_t keyring_find_key_id_by_name(const char *key_name);
int32_t keyring_find_keyring_id_by_name(const char *keyring_name);

int keyring_check(void);

int keyring_get_user_key(const char *key_desc,
		    char **key,
		    size_t *key_size);

int keyring_find_and_get_key_by_name(const char *key_name,
		      char **key,
		      size_t *key_size);

int keyring_add_key_in_thread_keyring(
	key_type_t ktype,
	const char *key_desc,
	const void *key,
	size_t key_size);

int keyring_add_key_in_user_keyring(
	key_type_t ktype,
	const char *key_desc,
	const void *key,
	size_t key_size);

int keyring_add_key_in_keyring(
	key_type_t ktype,
	const char *key_desc,
	const void *key,
	size_t key_size,
	key_serial_t keyring_id);

int keyring_revoke_and_unlink_key(key_type_t ktype, const char *key_desc);
int keyring_add_key_to_custom_keyring(key_type_t ktype, const char *key_desc, const void *key,
				      size_t key_size, key_serial_t keyring_to_link);

#endif
