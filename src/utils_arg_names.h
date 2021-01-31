/*
 * Command line arguments name list
 *
 * Copyright (C) 2020-2021 Red Hat, Inc. All rights reserved.
 * Copyright (C) 2020-2021 Ondrej Kozina
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

#ifndef UTILS_ARG_NAMES_H
#define UTILS_ARG_NAMES_H

#define OPT_ACTIVE_NAME			"active-name"
#define OPT_ALIGN_PAYLOAD		"align-payload"
#define OPT_ALLOW_DISCARDS		"allow-discards"
#define OPT_BATCH_MODE			"batch-mode"
#define OPT_BITMAP_FLUSH_TIME		"bitmap-flush-time"
#define OPT_BITMAP_SECTORS_PER_BIT	"bitmap-sectors-per-bit"
#define OPT_BLOCK_SIZE			"block-size"
#define OPT_BUFFER_SECTORS		"buffer-sectors"
#define OPT_CANCEL_DEFERRED		"cancel-deferred"
#define OPT_CHECK_AT_MOST_ONCE		"check-at-most-once"
#define OPT_CIPHER			"cipher"
#define OPT_DATA_BLOCK_SIZE		"data-block-size"
#define OPT_DATA_BLOCKS			"data-blocks"
#define OPT_DATA_DEVICE			"data-device"
#define OPT_DEBUG			"debug"
#define OPT_DEBUG_JSON			"debug-json"
#define OPT_DEFERRED			"deferred"
#define OPT_DEVICE_SIZE			"device-size"
#define OPT_DECRYPT			"decrypt"
#define OPT_DISABLE_KEYRING		"disable-keyring"
#define OPT_DISABLE_LOCKS		"disable-locks"
#define OPT_DUMP_JSON			"dump-json-metadata"
#define OPT_DUMP_MASTER_KEY		"dump-master-key"
#define OPT_ENCRYPT			"encrypt"
#define OPT_FEC_DEVICE			"fec-device"
#define OPT_FEC_OFFSET			"fec-offset"
#define OPT_FEC_ROOTS			"fec-roots"
#define OPT_FORCE_PASSWORD		"force-password"
#define OPT_FORMAT			"format"
#define OPT_HASH			"hash"
#define OPT_HASH_BLOCK_SIZE		"hash-block-size"
#define OPT_HASH_OFFSET			"hash-offset"
#define OPT_HEADER			"header"
#define OPT_HEADER_BACKUP_FILE		"header-backup-file"
#define OPT_HOTZONE_SIZE		"hotzone-size"
#define OPT_IGNORE_CORRUPTION		"ignore-corruption"
#define OPT_IGNORE_ZERO_BLOCKS		"ignore-zero-blocks"
#define OPT_INIT_ONLY			"init-only"
#define OPT_INTEGRITY			"integrity"
#define OPT_INTEGRITY_BITMAP_MODE	"integrity-bitmap-mode"
#define OPT_INTEGRITY_KEY_FILE		"integrity-key-file"
#define OPT_INTEGRITY_KEY_SIZE		"integrity-key-size"
#define OPT_INTEGRITY_LEGACY_PADDING	"integrity-legacy-padding"
#define OPT_INTEGRITY_LEGACY_HMAC	"integrity-legacy-hmac"
#define OPT_INTEGRITY_LEGACY_RECALC	"integrity-legacy-recalculate"
#define OPT_INTEGRITY_NO_JOURNAL	"integrity-no-journal"
#define OPT_INTEGRITY_NO_WIPE		"integrity-no-wipe"
#define OPT_INTEGRITY_RECALCULATE	"integrity-recalculate"
#define OPT_INTEGRITY_RECOVERY_MODE	"integrity-recovery-mode"
#define OPT_INTERLEAVE_SECTORS		"interleave-sectors"
#define OPT_ITER_TIME			"iter-time"
#define OPT_IV_LARGE_SECTORS		"iv-large-sectors"
#define OPT_JSON_FILE			"json-file"
#define OPT_JOURNAL_COMMIT_TIME		"journal-commit-time"
#define OPT_JOURNAL_CRYPT		"journal-crypt"
#define OPT_JOURNAL_CRYPT_KEY_FILE	"journal-crypt-key-file"
#define OPT_JOURNAL_CRYPT_KEY_SIZE	"journal-crypt-key-size"
#define OPT_JOURNAL_INTEGRITY		"journal-integrity"
#define OPT_JOURNAL_INTEGRITY_KEY_FILE	"journal-integrity-key-file"
#define OPT_JOURNAL_INTEGRITY_KEY_SIZE	"journal-integrity-key-size"
#define OPT_JOURNAL_SIZE		"journal-size"
#define OPT_JOURNAL_WATERMARK		"journal-watermark"
#define OPT_KEEP_KEY			"keep-key"
#define OPT_KEY_DESCRIPTION		"key-description"
#define OPT_KEY_FILE			"key-file"
#define OPT_KEY_SIZE			"key-size"
#define OPT_KEY_SLOT			"key-slot"
#define OPT_KEYFILE_OFFSET		"keyfile-offset"
#define OPT_KEYFILE_SIZE		"keyfile-size"
#define OPT_KEYSLOT_CIPHER		"keyslot-cipher"
#define OPT_KEYSLOT_KEY_SIZE		"keyslot-key-size"
#define OPT_NO_SUPERBLOCK		"no-superblock"
#define OPT_NO_WIPE			"no-wipe"
#define OPT_LABEL			"label"
#define OPT_LUKS2_KEYSLOTS_SIZE		"luks2-keyslots-size"
#define OPT_LUKS2_METADATA_SIZE		"luks2-metadata-size"
#define OPT_MASTER_KEY_FILE		"master-key-file"
#define OPT_NEW				"new"
#define OPT_NEW_KEYFILE_OFFSET		"new-keyfile-offset"
#define OPT_NEW_KEYFILE_SIZE		"new-keyfile-size"
#define OPT_OFFSET			"offset"
#define OPT_PANIC_ON_CORRUPTION		"panic-on-corruption"
#define OPT_PBKDF			"pbkdf"
#define OPT_PBKDF_FORCE_ITERATIONS	"pbkdf-force-iterations"
#define OPT_PBKDF_MEMORY		"pbkdf-memory"
#define OPT_PBKDF_PARALLEL		"pbkdf-parallel"
#define OPT_PERF_NO_READ_WORKQUEUE	"perf-no_read_workqueue"
#define OPT_PERF_NO_WRITE_WORKQUEUE	"perf-no_write_workqueue"
#define OPT_PERF_SAME_CPU_CRYPT		"perf-same_cpu_crypt"
#define OPT_PERF_SUBMIT_FROM_CRYPT_CPUS	"perf-submit_from_crypt_cpus"
#define OPT_PERSISTENT			"persistent"
#define OPT_PLUGIN			"plugin"
#define OPT_PRIORITY			"priority"
#define OPT_PROGRESS_FREQUENCY		"progress-frequency"
#define OPT_READONLY			"readonly"
#define OPT_REDUCE_DEVICE_SIZE		"reduce-device-size"
#define OPT_REFRESH			"refresh"
#define OPT_RESILIENCE			"resilience"
#define OPT_RESILIENCE_HASH		"resilience-hash"
#define OPT_RESTART_ON_CORRUPTION	"restart-on-corruption"
#define OPT_RESUME_ONLY			"resume-only"
#define OPT_ROOT_HASH_SIGNATURE		"root-hash-signature"
#define OPT_SALT			"salt"
#define OPT_SECTOR_SIZE			"sector-size"
#define OPT_SERIALIZE_MEMORY_HARD_PBKDF	"serialize-memory-hard-pbkdf"
#define OPT_SHARED			"shared"
#define OPT_SIZE			"size"
#define OPT_SKIP			"skip"
#define OPT_SUBSYSTEM			"subsystem"
#define OPT_TAG_SIZE			"tag-size"
#define OPT_TCRYPT_BACKUP		"tcrypt-backup"
#define OPT_TCRYPT_HIDDEN		"tcrypt-hidden"
#define OPT_TCRYPT_SYSTEM		"tcrypt-system"
#define OPT_TEST_ARGS			"test-args"
#define OPT_TEST_PASSPHRASE		"test-passphrase"
#define OPT_TIMEOUT			"timeout"
#define OPT_TOKEN_ID			"token-id"
#define OPT_TOKEN_ONLY			"token-only"
#define OPT_TRIES			"tries"
#define OPT_TYPE			"type"
#define OPT_UNBOUND			"unbound"
#define OPT_USE_DIRECTIO		"use-directio"
#define OPT_USE_FSYNC			"use-fsync"
#define OPT_USE_RANDOM			"use-random"
#define OPT_USE_URANDOM			"use-urandom"
#define OPT_UUID			"uuid"
#define OPT_VERACRYPT			"veracrypt"
#define OPT_VERACRYPT_PIM		"veracrypt-pim"
#define OPT_VERACRYPT_QUERY_PIM		"veracrypt-query-pim"
#define OPT_VERBOSE			"verbose"
#define OPT_VERIFY_PASSPHRASE		"verify-passphrase"
#define OPT_WRITE_LOG			"write-log"

#endif
