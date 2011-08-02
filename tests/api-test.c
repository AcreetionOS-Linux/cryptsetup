/*
 * cryptsetup library API check functions
 *
 * Copyright (C) 2009-2010 Red Hat, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <libdevmapper.h>

#include "libcryptsetup.h"
#include "utils_loop.h"

#define DMDIR "/dev/mapper/"

#define DEVICE_1_UUID "28632274-8c8a-493f-835b-da802e1c576b"
#define DEVICE_EMPTY_name "crypt_zero"
#define DEVICE_EMPTY DMDIR DEVICE_EMPTY_name
#define DEVICE_ERROR_name "crypt_error"
#define DEVICE_ERROR DMDIR DEVICE_ERROR_name

#define CDEVICE_1 "ctest1"
#define CDEVICE_2 "ctest2"
#define CDEVICE_WRONG "O_o"

#define IMAGE1 "compatimage.img"
#define IMAGE_EMPTY "empty.img"

#define KEYFILE1 "key1.file"
#define KEY1 "compatkey"

#define KEYFILE2 "key2.file"
#define KEY2 "0123456789abcdef"

#define PASSPHRASE "blabla"

#define DEVICE_TEST_UUID "12345678-1234-1234-1234-123456789abc"

#define DEVICE_WRONG "/dev/Ooo_"
#define DEVICE_CHAR "/dev/zero"
#define TMP_FILE_TEMPLATE "cryptsetuptst.XXXXXX"

#define SECTOR_SHIFT 9L

static int _debug   = 0;
static int _verbose = 1;

static char global_log[4096];
static int global_lines = 0;

static char *DEVICE_1 = NULL;
static char *DEVICE_2 = NULL;
static char *H_DEVICE = NULL;

static char *tmp_file_1 = NULL;
static char *tmp_file_2 = NULL;

// Helpers

static int device_size(const char *device, uint64_t *size)
{
	int devfd, r = 0;

	devfd = open(device, O_RDONLY);
	if(devfd == -1)
		return -EINVAL;

	if (ioctl(devfd, BLKGETSIZE64, size) < 0)
		r = -EINVAL;
	close(devfd);
	return r;
}

// Get key from kernel dm mapping table using dm-ioctl
static int _get_key_dm(const char *name, char *buffer, unsigned int buffer_size)
{
	struct dm_task *dmt;
	struct dm_info dmi;
	uint64_t start, length;
	char *target_type, *rcipher, *key, *params;
	void *next = NULL;
	int r = -EINVAL;

	if (!(dmt = dm_task_create(DM_DEVICE_TABLE)))
		goto out;
	if (!dm_task_set_name(dmt, name))
		goto out;
	if (!dm_task_run(dmt))
		goto out;
	if (!dm_task_get_info(dmt, &dmi))
		goto out;
	if (!dmi.exists)
		goto out;

	next = dm_get_next_target(dmt, next, &start, &length, &target_type, &params);
	if (!target_type || strcmp(target_type, "crypt") != 0)
		goto out;

	rcipher = strsep(&params, " ");
	key = strsep(&params, " ");

	if (buffer_size <= strlen(key))
		goto out;

	strncpy(buffer, key, buffer_size);
	r = 0;
out:
	if (dmt)
		dm_task_destroy(dmt);

	return r;
}

static int _prepare_keyfile(const char *name, const char *passphrase, int size)
{
	int fd, r;

	fd = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR);
	if (fd != -1) {
		r = write(fd, passphrase, size);
		close(fd);
	} else
		r = 0;

	return r == size ? 0 : 1;
}

static void _remove_keyfiles(void)
{
	remove(KEYFILE1);
	remove(KEYFILE2);
}

// Decode key from its hex representation
static int crypt_decode_key(char *key, char *hex, unsigned int size)
{
	char buffer[3];
	char *endp;
	unsigned int i;

	buffer[2] = '\0';

	for (i = 0; i < size; i++) {
		buffer[0] = *hex++;
		buffer[1] = *hex++;

		key[i] = (unsigned char)strtoul(buffer, &endp, 16);

		if (endp != &buffer[2])
			return -1;
	}

	if (*hex != '\0')
		return -1;

	return 0;
}

static void cmdLineLog(int level, char *msg)
{
	strncat(global_log, msg, sizeof(global_log) - strlen(global_log));
	global_lines++;
}

static void new_log(int level, const char *msg, void *usrptr)
{
	cmdLineLog(level, (char*)msg);
}

static void reset_log()
{
	memset(global_log, 0, sizeof(global_log));
	global_lines = 0;
}

static void _system(const char *command, int warn)
{
	if (system(command) < 0 && warn)
		printf("System command failed: %s", command);
}

static void _cleanup(void)
{
	char *h_str;
	struct stat st;

	//_system("udevadm settle", 0);

	if (!stat(DMDIR CDEVICE_1, &st))
		_system("dmsetup remove " CDEVICE_1, 0);

	if (!stat(DMDIR CDEVICE_2, &st))
		_system("dmsetup remove " CDEVICE_2, 0);

	if (!stat(DEVICE_EMPTY, &st))
		_system("dmsetup remove " DEVICE_EMPTY_name, 0);

	if (!stat(DEVICE_ERROR, &st))
		_system("dmsetup remove " DEVICE_ERROR_name, 0);

	if (crypt_loop_device(DEVICE_1))
		crypt_loop_detach(DEVICE_1);

	if (crypt_loop_device(DEVICE_2))
		crypt_loop_detach(DEVICE_2);

	_system("rm -f " IMAGE_EMPTY, 0);
	_system("rm -f " IMAGE1, 0);

	if (tmp_file_1)
		remove(tmp_file_1);
	if (tmp_file_2)
		remove(tmp_file_2);

	if (H_DEVICE && crypt_loop_device(H_DEVICE)) {
		h_str = crypt_loop_backing_file(H_DEVICE);
		if(!crypt_loop_detach(H_DEVICE) && h_str)
			remove(h_str);
	}

	_remove_keyfiles();
}

static int _setup(void)
{
	int fd, ro = 0;
	char *h_str, cmd[128];

	tmp_file_1 = strdup(TMP_FILE_TEMPLATE);
	if ((fd=mkstemp(tmp_file_1))==-1) {
		printf("cannot create temporary file with template %s\n",tmp_file_1);
		return 1;
	}
	close(fd);
	snprintf(cmd,sizeof(cmd),"dd if=/dev/zero of=%s bs=512 count=100", tmp_file_1);
	_system(cmd,1);

	if (!H_DEVICE)
		H_DEVICE = crypt_loop_get_device();
	if (!H_DEVICE) {
		printf("Cannot find free loop device.\n");
		return 1;
	}
	h_str = strdup(TMP_FILE_TEMPLATE);
	if ((fd=mkstemp(h_str))==-1) {
		printf("cannot create temporary file with template %s\n",tmp_file_1);
		return 1;
	}
	close(fd);
	snprintf(cmd,sizeof(cmd),"dd if=/dev/zero of=%s bs=512 count=1", h_str);
	_system(cmd,1);
	if (crypt_loop_device(H_DEVICE)) {
		fd = crypt_loop_attach(H_DEVICE,h_str,0,0,&ro);
		close(fd);
	}
	free(h_str);

	tmp_file_2 = strdup(TMP_FILE_TEMPLATE);
	if ((fd=mkstemp(tmp_file_2))==-1) {
		printf("cannot create temporary file with template %s\n",tmp_file_2);
		return 1;
	}
	close(fd);

	_system("dmsetup create " DEVICE_EMPTY_name " --table \"0 10000 zero\"", 1);
	_system("dmsetup create " DEVICE_ERROR_name " --table \"0 10000 error\"", 1);
	if (!DEVICE_1)
		DEVICE_1 = crypt_loop_get_device();
	if (!DEVICE_1) {
		printf("Cannot find free loop device.\n");
		return 1;
	}
	if (crypt_loop_device(DEVICE_1)) {
		_system(" [ ! -e " IMAGE1 " ] && bzip2 -dk " IMAGE1 ".bz2", 1);
		fd = crypt_loop_attach(DEVICE_1, IMAGE1, 0, 0, &ro);
		close(fd);
	}
	if (!DEVICE_2)
		DEVICE_2 = crypt_loop_get_device();
	if (!DEVICE_2) {
		printf("Cannot find free loop device.\n");
		return 1;
	}
	if (crypt_loop_device(DEVICE_2)) {
		_system("dd if=/dev/zero of=" IMAGE_EMPTY " bs=1M count=4", 1);
		fd = crypt_loop_attach(DEVICE_2, IMAGE_EMPTY, 0, 0, &ro);
		close(fd);
	}
	return 0;
}

void check_ok(int status, int line, const char *func)
{
	char buf[256];

	if (status) {
		crypt_get_error(buf, sizeof(buf));
		printf("FAIL line %d [%s]: code %d, %s\n", line, func, status, buf);
		_cleanup();
		exit(-1);
	}
}

void check_ko(int status, int line, const char *func)
{
	char buf[256];

	memset(buf, 0, sizeof(buf));
	crypt_get_error(buf, sizeof(buf));
	if (status >= 0) {
		printf("FAIL line %d [%s]: code %d, %s\n", line, func, status, buf);
		_cleanup();
		exit(-1);
	} else if (_verbose)
		printf("   => errno %d, errmsg: %s\n", status, buf);
}

void check_equal(int line, const char *func)
{
	printf("FAIL line %d [%s]: expected equal values differs.\n", line, func);
	_cleanup();
	exit(-1);
}

void xlog(const char *msg, const char *tst, const char *func, int line, const char *txt)
{
	if (_verbose) {
		if (txt)
			printf(" [%s,%s:%d] %s [%s]\n", msg, func, line, tst, txt);
		else
			printf(" [%s,%s:%d] %s\n", msg, func, line, tst);
	}
}
#define OK_(x)		do { xlog("(success)", #x, __FUNCTION__, __LINE__, NULL); \
			     check_ok((x), __LINE__, __FUNCTION__); \
			} while(0)
#define FAIL_(x, y)	do { xlog("(fail)   ", #x, __FUNCTION__, __LINE__, y); \
			     check_ko((x), __LINE__, __FUNCTION__); \
			} while(0)
#define EQ_(x, y)	do { xlog("(equal)  ", #x " == " #y, __FUNCTION__, __LINE__, NULL); \
			     if ((x) != (y)) check_equal(__LINE__, __FUNCTION__); \
			} while(0)

#define RUN_(x, y)		do { printf("%s: %s\n", #x, (y)); x(); } while (0)

#if 0
static int yesDialog(char *msg)
{
	return 1;
}

static struct interface_callbacks cmd_icb = {
	.yesDialog = yesDialog,
	.log = cmdLineLog,
};

// OLD API TESTS
static void LuksUUID(void)
{
	struct crypt_options co = { .icb = &cmd_icb };

	co.device = DEVICE_EMPTY;
	EQ_(crypt_luksUUID(&co), -EINVAL);

	co.device = DEVICE_ERROR;
	EQ_(crypt_luksUUID(&co), -EINVAL);

	reset_log();
	co.device = DEVICE_1;
	OK_(crypt_luksUUID(&co));
	EQ_(strlen(global_log), 37); /* UUID + "\n" */
	EQ_(strncmp(global_log, DEVICE_1_UUID, strlen(DEVICE_1_UUID)), 0);

}

static void IsLuks(void)
{
	struct crypt_options co = {  .icb = &cmd_icb };

	co.device = DEVICE_EMPTY;
	EQ_(crypt_isLuks(&co), -EINVAL);

	co.device = DEVICE_ERROR;
	EQ_(crypt_isLuks(&co), -EINVAL);

	co.device = DEVICE_1;
	OK_(crypt_isLuks(&co));
}

static void LuksOpen(void)
{
	struct crypt_options co = {
		.name = CDEVICE_1,
		//.passphrase = "blabla",
		.icb = &cmd_icb,
	};

	OK_(_prepare_keyfile(KEYFILE1, KEY1, strlen(KEY1)));
	co.key_file = KEYFILE1;

	co.device = DEVICE_EMPTY;
	EQ_(crypt_luksOpen(&co), -EINVAL);

	co.device = DEVICE_ERROR;
	EQ_(crypt_luksOpen(&co), -EINVAL);

	co.device = DEVICE_1;
	OK_(crypt_luksOpen(&co));
	FAIL_(crypt_luksOpen(&co), "already open");

	_remove_keyfiles();
}

static void query_device(void)
{
	struct crypt_options co = {.icb = &cmd_icb };

	co.name = CDEVICE_WRONG;
	EQ_(crypt_query_device(&co), 0);

	co.name = CDEVICE_1;
	EQ_(crypt_query_device(&co), 1);

	OK_(strncmp(crypt_get_dir(), DMDIR, 11));
	OK_(strcmp(co.cipher, "aes-cbc-essiv:sha256"));
	EQ_(co.key_size, 16);
	EQ_(co.offset, 1032);
	EQ_(co.flags & CRYPT_FLAG_READONLY, 0);
	EQ_(co.skip, 0);
	crypt_put_options(&co);
}

static void remove_device(void)
{
	int fd;
	struct crypt_options co = {.icb = &cmd_icb };

	co.name = CDEVICE_WRONG;
	EQ_(crypt_remove_device(&co), -ENODEV);

	fd = open(DMDIR CDEVICE_1, O_RDONLY);
	co.name = CDEVICE_1;
	FAIL_(crypt_remove_device(&co), "device busy");
	close(fd);

	OK_(crypt_remove_device(&co));
}

static void LuksFormat(void)
{
	struct crypt_options co = {
		.device = DEVICE_2,
		.key_size = 256 / 8,
		.key_slot = -1,
		.cipher = "aes-cbc-essiv:sha256",
		.hash = "sha1",
		.flags = 0,
		.iteration_time = 10,
		.align_payload = 0,
		.icb = &cmd_icb,
	};

	OK_(_prepare_keyfile(KEYFILE1, KEY1, strlen(KEY1)));

	co.new_key_file = KEYFILE1;
	co.device = DEVICE_ERROR;
	FAIL_(crypt_luksFormat(&co), "error device");

	co.device = DEVICE_2;
	OK_(crypt_luksFormat(&co));

	co.new_key_file = NULL;
	co.key_file = KEYFILE1;
	co.name = CDEVICE_2;
	OK_(crypt_luksOpen(&co));
	OK_(crypt_remove_device(&co));
	_remove_keyfiles();
}

static void LuksKeyGame(void)
{
	int i;
	struct crypt_options co = {
		.device = DEVICE_2,
		.key_size = 256 / 8,
		.key_slot = -1,
		.cipher = "aes-cbc-essiv:sha256",
		.hash = "sha1",
		.flags = 0,
		.iteration_time = 10,
		.align_payload = 0,
		.icb = &cmd_icb,
	};

	OK_(_prepare_keyfile(KEYFILE1, KEY1, strlen(KEY1)));
	OK_(_prepare_keyfile(KEYFILE2, KEY2, strlen(KEY2)));

	co.new_key_file = KEYFILE1;
	co.device = DEVICE_2;
	co.key_slot = 8;
	FAIL_(crypt_luksFormat(&co), "wrong slot #");

	co.key_slot = 7; // last slot
	OK_(crypt_luksFormat(&co));

	co.new_key_file = KEYFILE1;
	co.key_file = KEYFILE1;
	co.key_slot = 8;
	FAIL_(crypt_luksAddKey(&co), "wrong slot #");
	co.key_slot = 7;
	FAIL_(crypt_luksAddKey(&co), "slot already used");

	co.key_slot = 6;
	OK_(crypt_luksAddKey(&co));

	co.key_file = KEYFILE2 "blah";
	co.key_slot = 5;
	FAIL_(crypt_luksAddKey(&co), "keyfile not found");

	co.new_key_file = KEYFILE2; // key to add
	co.key_file = KEYFILE1;
	co.key_slot = -1;
	for (i = 0; i < 6; i++)
		OK_(crypt_luksAddKey(&co)); //FIXME: EQ_(i)?

	FAIL_(crypt_luksAddKey(&co), "all slots full");

	// REMOVE KEY
	co.new_key_file = KEYFILE1; // key to remove
	co.key_file = NULL;
	co.key_slot = 8; // should be ignored
	 // only 2 slots should use KEYFILE1
	OK_(crypt_luksRemoveKey(&co));
	OK_(crypt_luksRemoveKey(&co));
	FAIL_(crypt_luksRemoveKey(&co), "no slot with this passphrase");

	co.new_key_file = KEYFILE2 "blah";
	co.key_file = NULL;
	FAIL_(crypt_luksRemoveKey(&co), "keyfile not found");

	// KILL SLOT
	co.new_key_file = NULL;
	co.key_file = NULL;
	co.key_slot = 8;
	FAIL_(crypt_luksKillSlot(&co), "wrong slot #");
	co.key_slot = 7;
	FAIL_(crypt_luksKillSlot(&co), "slot already wiped");

	co.key_slot = 5;
	OK_(crypt_luksKillSlot(&co));

	_remove_keyfiles();
}

size_t _get_device_size(const char *device)
{
	unsigned long size = 0;
	int fd;

	fd = open(device, O_RDONLY);
	if (fd == -1)
		return 0;
	(void)ioctl(fd, BLKGETSIZE, &size);
	close(fd);

	return size;
}

void DeviceResizeGame(void)
{
	size_t orig_size;
	struct crypt_options co = {
		.name = CDEVICE_2,
		.device = DEVICE_2,
		.key_size = 128 / 8,
		.cipher = "aes-cbc-plain",
		.hash = "sha1",
		.offset = 333,
		.skip = 0,
		.icb = &cmd_icb,
	};

	orig_size = _get_device_size(DEVICE_2);

	OK_(_prepare_keyfile(KEYFILE2, KEY2, strlen(KEY2)));

	co.key_file = KEYFILE2;
	co.size = 1000;
	OK_(crypt_create_device(&co));
	EQ_(_get_device_size(DMDIR CDEVICE_2), 1000);

	co.size = 2000;
	OK_(crypt_resize_device(&co));
	EQ_(_get_device_size(DMDIR CDEVICE_2), 2000);

	co.size = 0;
	OK_(crypt_resize_device(&co));
	EQ_(_get_device_size(DMDIR CDEVICE_2), (orig_size - 333));
	co.size = 0;
	co.offset = 444;
	co.skip = 555;
	co.cipher = "aes-cbc-essiv:sha256";
	OK_(crypt_update_device(&co));
	EQ_(_get_device_size(DMDIR CDEVICE_2), (orig_size - 444));

	memset(&co, 0, sizeof(co));
	co.icb = &cmd_icb,
	co.name = CDEVICE_2;
	EQ_(crypt_query_device(&co), 1);
	EQ_(strcmp(co.cipher, "aes-cbc-essiv:sha256"), 0);
	EQ_(co.key_size, 128 / 8);
	EQ_(co.offset, 444);
	EQ_(co.skip, 555);
	crypt_put_options(&co);

	// dangerous switch device still works
	memset(&co, 0, sizeof(co));
	co.name = CDEVICE_2,
	co.device = DEVICE_1;
	co.key_file = KEYFILE2;
	co.key_size = 128 / 8;
	co.cipher = "aes-cbc-plain";
	co.hash = "sha1";
	co.icb = &cmd_icb;
	OK_(crypt_update_device(&co));

	memset(&co, 0, sizeof(co));
	co.icb = &cmd_icb,
	co.name = CDEVICE_2;
	EQ_(crypt_query_device(&co), 1);
	EQ_(strcmp(co.cipher, "aes-cbc-plain"), 0);
	EQ_(co.key_size, 128 / 8);
	EQ_(co.offset, 0);
	EQ_(co.skip, 0);
	// This expect lookup returns prefered /dev/loopX
	EQ_(strcmp(co.device, DEVICE_1), 0);
	crypt_put_options(&co);

	memset(&co, 0, sizeof(co));
	co.icb = &cmd_icb,
	co.name = CDEVICE_2;
	OK_(crypt_remove_device(&co));

	_remove_keyfiles();
}
#endif

// NEW API tests

static void AddDevicePlain(void)
{
	struct crypt_device *cd, *cd2;
	struct crypt_params_plain params = {
		.hash = "sha1",
		.skip = 0,
		.offset = 0,
		.size = 0
	};
	int fd;
	char key[128], key2[128], path[128];

	char *passphrase = PASSPHRASE;
	// hashed hex version of PASSPHRASE
	char *mk_hex = "bb21158c733229347bd4e681891e213d94c685be6a5b84818afe7a78a6de7a1a";
	size_t key_size = strlen(mk_hex) / 2;
	char *cipher = "aes";
	char *cipher_mode = "cbc-essiv:sha256";

	uint64_t size, r_size;

	crypt_decode_key(key, mk_hex, key_size);
	FAIL_(crypt_init(&cd, ""), "empty device string");
	FAIL_(crypt_init(&cd, DEVICE_WRONG), "nonexistent device name ");
	FAIL_(crypt_init(&cd, DEVICE_CHAR), "character device as backing device");
	OK_(crypt_init(&cd, tmp_file_1));
	crypt_free(cd);

	// test crypt_format, crypt_get_cipher, crypt_get_cipher_mode, crypt_get_volume_key_size
	OK_(crypt_init(&cd,DEVICE_1));
	params.skip = 3;
	params.offset = 42;
	FAIL_(crypt_format(cd,CRYPT_PLAIN,NULL,cipher_mode,NULL,NULL,key_size,&params),"cipher param is null");
	FAIL_(crypt_format(cd,CRYPT_PLAIN,cipher,NULL,NULL,NULL,key_size,&params),"cipher_mode param is null");
	OK_(crypt_format(cd,CRYPT_PLAIN,cipher,cipher_mode,NULL,NULL,key_size,&params));
	OK_(strcmp(cipher_mode,crypt_get_cipher_mode(cd)));
	OK_(strcmp(cipher,crypt_get_cipher(cd)));
	EQ_(key_size, crypt_get_volume_key_size(cd));
	EQ_(params.skip, crypt_get_iv_offset(cd));
	EQ_(params.offset, crypt_get_data_offset(cd));
	params.skip = 0;
	params.offset = 0;

	// crypt_set_uuid()
	FAIL_(crypt_set_uuid(cd,DEVICE_1_UUID),"can't set uuid to plain device");

	// crypt_load() should fail for PLAIN
	FAIL_(crypt_load(cd,CRYPT_PLAIN,NULL),"can't load header from plain device");

	crypt_free(cd);

	// default is "plain" hash - no password hash
	OK_(crypt_init(&cd, DEVICE_1));
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, NULL));
	FAIL_(crypt_activate_by_volume_key(cd, NULL, key, key_size, 0), "cannot verify key with plain");
	OK_(crypt_activate_by_volume_key(cd, CDEVICE_1, key, key_size, 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_1));
	crypt_free(cd);

	// test boundaries in offset parameter
	device_size(DEVICE_1,&size);
	params.hash = NULL;
	// zero sectors length
	params.offset = size >> SECTOR_SHIFT;
	OK_(crypt_init(&cd, DEVICE_1));
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	EQ_(crypt_get_data_offset(cd),params.offset);
	// device size is 0 sectors
	FAIL_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0), "invalid device size (0 blocks)");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_INACTIVE);
	// data part of crypt device is of 1 sector size
	params.offset = (size >> SECTOR_SHIFT) - 1;
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	OK_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	snprintf(path, sizeof(path), "%s/%s", crypt_get_dir(), CDEVICE_1);
	if (device_size(path, &r_size) >= 0)
		EQ_(r_size>>SECTOR_SHIFT, 1);
	OK_(crypt_deactivate(cd, CDEVICE_1));

	// size > device_size
	params.offset = 0;
	params.size = (size >> SECTOR_SHIFT) + 1;
	crypt_init(&cd, DEVICE_1);
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	FAIL_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0),"Device too small");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_INACTIVE);

	// offset == device_size (autodetect size)
	params.offset = (size >> SECTOR_SHIFT);
	params.size = 0;
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	FAIL_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0),"Device too small");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_INACTIVE);

	// offset == device_size (user defined size)
	params.offset = (size >> SECTOR_SHIFT);
	params.size = 123;
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	FAIL_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0),"Device too small");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_INACTIVE);

	// offset+size > device_size
	params.offset = 42;
	params.size = (size >> SECTOR_SHIFT) - params.offset + 1;
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	FAIL_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0),"Offset and size are beyond device real size");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_INACTIVE);

	// offset+size == device_size
	params.offset = 42;
	params.size = (size >> SECTOR_SHIFT) - params.offset;
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	OK_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	if (!device_size(path, &r_size))
		EQ_((r_size >> SECTOR_SHIFT),params.size);
	OK_(crypt_deactivate(cd,CDEVICE_1));

	crypt_free(cd);
	params.hash = "sha1";
	params.offset = 0;
	params.size = 0;
	params.skip = 0;

	// Now use hashed password
	OK_(crypt_init(&cd, DEVICE_1));
	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));
	FAIL_(crypt_activate_by_passphrase(cd, NULL, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0),
	      "cannot verify passphrase with plain" );
	OK_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0));

	// device status check
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	snprintf(path, sizeof(path), "%s/%s", crypt_get_dir(), CDEVICE_1);
	fd = open(path, O_RDONLY);
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_BUSY);
	FAIL_(crypt_deactivate(cd, CDEVICE_1), "Device is busy");
	close(fd);
	OK_(crypt_deactivate(cd, CDEVICE_1));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_INACTIVE);

	// crypt_init_by_name_and_header
	OK_(crypt_init(&cd,DEVICE_1));
	OK_(crypt_format(cd, CRYPT_PLAIN,cipher,cipher_mode,NULL,NULL,key_size,&params));
	OK_(crypt_activate_by_volume_key(cd,CDEVICE_1,key,key_size,0));
	FAIL_(crypt_init_by_name_and_header(&cd2,CDEVICE_1,H_DEVICE),"can't init plain device by header device");
	OK_(crypt_init_by_name(&cd2,CDEVICE_1));
	OK_(crypt_deactivate(cd,CDEVICE_1));
	crypt_free(cd);
	crypt_free(cd2);

	OK_(crypt_init(&cd,DEVICE_1));
	OK_(crypt_format(cd,CRYPT_PLAIN,cipher,cipher_mode,NULL,NULL,key_size,&params));
	params.size = 0;
	params.offset = 0;

	// crypt_set_data_device
	FAIL_(crypt_set_data_device(cd,H_DEVICE),"can't set data device for plain device");

	// crypt_get_type
	OK_(strcmp(crypt_get_type(cd),CRYPT_PLAIN));

	OK_(crypt_activate_by_volume_key(cd, CDEVICE_1, key, key_size, 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);

	// crypt_resize()
	OK_(crypt_resize(cd,CDEVICE_1,size>>SECTOR_SHIFT)); // same size
	if (!device_size(path,&r_size))
		EQ_(r_size, size);

	// size overlaps
	FAIL_(crypt_resize(cd, CDEVICE_1, ULLONG_MAX),"Backing device is too small");
	FAIL_(crypt_resize(cd, CDEVICE_1, (size>>SECTOR_SHIFT)+1),"crypt device overlaps backing device");

	// resize ok
	OK_(crypt_resize(cd,CDEVICE_1, 123));
	if (!device_size(path,&r_size))
		EQ_(r_size>>SECTOR_SHIFT, 123);
	OK_(crypt_resize(cd,CDEVICE_1,0)); // full size (autodetect)
	if (!device_size(path,&r_size))
		EQ_(r_size, size);
	OK_(crypt_deactivate(cd,CDEVICE_1));
	EQ_(crypt_status(cd,CDEVICE_1),CRYPT_INACTIVE);

	// offset tests
	OK_(crypt_init(&cd,DEVICE_1));
	params.offset = 42;
	params.size = (size>>SECTOR_SHIFT) - params.offset - 10;
	OK_(crypt_format(cd,CRYPT_PLAIN,cipher,cipher_mode,NULL,NULL,key_size,&params));
	OK_(crypt_activate_by_volume_key(cd,CDEVICE_1,key,key_size,0));
	if (!device_size(path,&r_size))
		EQ_(r_size>>SECTOR_SHIFT, params.size);
	// resize to fill remaining capacity
	OK_(crypt_resize(cd,CDEVICE_1,params.size + 10));
	if (!device_size(path,&r_size))
		EQ_(r_size>>SECTOR_SHIFT, params.size + 10);

	// 1 sector beyond real size
	FAIL_(crypt_resize(cd,CDEVICE_1,params.size + 11), "new device size overlaps backing device"); // with respect to offset
	if (!device_size(path,&r_size))
		EQ_(r_size>>SECTOR_SHIFT, params.size + 10);
	EQ_(crypt_status(cd,CDEVICE_1),CRYPT_ACTIVE);
	fd = open(path, O_RDONLY);
	close(fd);
	OK_(fd < 0);

	// resize to minimal size
	OK_(crypt_resize(cd,CDEVICE_1, 1)); // minimal device size
	if (!device_size(path,&r_size))
		EQ_(r_size>>SECTOR_SHIFT, 1);
	// use size of backing device (autodetect with respect to offset)
	OK_(crypt_resize(cd,CDEVICE_1,0));
	if (!device_size(path,&r_size))
		EQ_(r_size>>SECTOR_SHIFT, (size >> SECTOR_SHIFT)- 42);
	OK_(crypt_deactivate(cd,CDEVICE_1));

	params.size = 0;
	params.offset = 0;
	OK_(crypt_format(cd,CRYPT_PLAIN,cipher,cipher_mode,NULL,NULL,key_size,&params));
	OK_(crypt_activate_by_volume_key(cd,CDEVICE_1,key,key_size,0));

	// suspend/resume tests
	FAIL_(crypt_suspend(cd,CDEVICE_1),"cannot suspend plain device");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	FAIL_(crypt_resume_by_passphrase(cd,CDEVICE_1,CRYPT_ANY_SLOT,passphrase, strlen(passphrase)),"cannot resume plain device");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);

	// retrieve volume key check
	memset(key2, 0, key_size);
	key_size--;
	// small buffer
	FAIL_(crypt_volume_key_get(cd, CRYPT_ANY_SLOT, key2, &key_size, passphrase, strlen(passphrase)), "small buffer");
	key_size++;
	OK_(crypt_volume_key_get(cd, CRYPT_ANY_SLOT, key2, &key_size, passphrase, strlen(passphrase)));

	OK_(memcmp(key, key2, key_size));
	OK_(strcmp(cipher, crypt_get_cipher(cd)));
	OK_(strcmp(cipher_mode, crypt_get_cipher_mode(cd)));
	EQ_(key_size, crypt_get_volume_key_size(cd));
	EQ_(0, crypt_get_data_offset(cd));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	// now with keyfile
	OK_(_prepare_keyfile(KEYFILE1, KEY1, strlen(KEY1)));
	OK_(_prepare_keyfile(KEYFILE2, KEY2, strlen(KEY2)));
	FAIL_(crypt_activate_by_keyfile(cd, NULL, CRYPT_ANY_SLOT, KEYFILE1, 0, 0), "cannot verify key with plain");
	EQ_(0, crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, 0, 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_1));
	_remove_keyfiles();

	OK_(crypt_init(&cd,DEVICE_1));
	OK_(crypt_format(cd,CRYPT_PLAIN,cipher,cipher_mode,NULL,NULL,key_size,&params));

	// crypt_keyslot_*()
	FAIL_(crypt_keyslot_add_by_passphrase(cd,CRYPT_ANY_SLOT,passphrase,strlen(passphrase),passphrase,strlen(passphrase)), "can't add keyslot to plain device");
	FAIL_(crypt_keyslot_add_by_volume_key(cd,CRYPT_ANY_SLOT	,key,key_size,passphrase,strlen(passphrase)),"can't add keyslot to plain device");
	FAIL_(crypt_keyslot_add_by_keyfile(cd,CRYPT_ANY_SLOT,KEYFILE1,strlen(KEY1),KEYFILE2,strlen(KEY2)),"can't add keyslot to plain device");
	FAIL_(crypt_keyslot_destroy(cd,1),"can't manipulate keyslots on plain device");
	EQ_(crypt_keyslot_status(cd, 0), CRYPT_SLOT_INVALID);
	_remove_keyfiles();

	crypt_free(cd);
}

#define CALLBACK_ERROR "calback_error xyz"
static int pass_callback_err(const char *msg, char *buf, size_t length, void *usrptr)
{
	struct crypt_device *cd = usrptr;

	assert(cd);
	assert(length);
	assert(msg);

	crypt_log(cd, CRYPT_LOG_ERROR, CALLBACK_ERROR);
	return -EINVAL;
}

static int pass_callback_ok(const char *msg, char *buf, size_t length, void *usrptr)
{
	assert(length);
	assert(msg);
	strcpy(buf, PASSPHRASE);
	return strlen(buf);
}

static void CallbacksTest(void)
{
	struct crypt_device *cd;
	struct crypt_params_plain params = {
		.hash = "sha1",
		.skip = 0,
		.offset = 0,
	};

	size_t key_size = 256 / 8;
	char *cipher = "aes";
	char *cipher_mode = "cbc-essiv:sha256";
	char *passphrase = PASSPHRASE;

	OK_(crypt_init(&cd, DEVICE_1));
	crypt_set_log_callback(cd, &new_log, NULL);
	//crypt_set_log_callback(cd, NULL, NULL);

	OK_(crypt_format(cd, CRYPT_PLAIN, cipher, cipher_mode, NULL, NULL, key_size, &params));

	OK_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_1));

	reset_log();
	crypt_set_password_callback(cd, pass_callback_err, cd);
	FAIL_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, NULL, 0, 0), "callback fails");
	EQ_(strncmp(global_log, CALLBACK_ERROR, strlen(CALLBACK_ERROR)), 0);

	crypt_set_password_callback(cd, pass_callback_ok, NULL);
	OK_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, NULL, 0, 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_1));

	crypt_free(cd);
}

static void UseLuksDevice(void)
{
	struct crypt_device *cd;
	char key[128];
	size_t key_size;

	OK_(crypt_init(&cd, DEVICE_1));
	OK_(crypt_load(cd, CRYPT_LUKS1, NULL));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_INACTIVE);
	OK_(crypt_activate_by_passphrase(cd, NULL, CRYPT_ANY_SLOT, KEY1, strlen(KEY1), 0));
	OK_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEY1, strlen(KEY1), 0));
	FAIL_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEY1, strlen(KEY1), 0), "already open");
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_1));
	FAIL_(crypt_deactivate(cd, CDEVICE_1), "no such device");

	key_size = 16;
	OK_(strcmp("aes", crypt_get_cipher(cd)));
	OK_(strcmp("cbc-essiv:sha256", crypt_get_cipher_mode(cd)));
	OK_(strcmp(DEVICE_1_UUID, crypt_get_uuid(cd)));
	EQ_(key_size, crypt_get_volume_key_size(cd));
	EQ_(1032, crypt_get_data_offset(cd));

	EQ_(0, crypt_volume_key_get(cd, CRYPT_ANY_SLOT, key, &key_size, KEY1, strlen(KEY1)));
	OK_(crypt_volume_key_verify(cd, key, key_size));
	OK_(crypt_activate_by_volume_key(cd, NULL, key, key_size, 0));
	OK_(crypt_activate_by_volume_key(cd, CDEVICE_1, key, key_size, 0));
	EQ_(crypt_status(cd, CDEVICE_1), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_1));

	key[1] = ~key[1];
	FAIL_(crypt_volume_key_verify(cd, key, key_size), "key mismatch");
	FAIL_(crypt_activate_by_volume_key(cd, CDEVICE_1, key, key_size, 0), "key mismatch");
	crypt_free(cd);
}

static void SuspendDevice(void)
{
	int suspend_status;
	struct crypt_device *cd;

	OK_(crypt_init(&cd, DEVICE_1));
	OK_(crypt_load(cd, CRYPT_LUKS1, NULL));
	OK_(crypt_activate_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEY1, strlen(KEY1), 0));

	suspend_status = crypt_suspend(cd, CDEVICE_1);
	if (suspend_status == -ENOTSUP) {
		printf("WARNING: Suspend/Resume not supported, skipping test.\n");
		goto out;
	}
	OK_(suspend_status);
	FAIL_(crypt_suspend(cd, CDEVICE_1), "already suspended");

	FAIL_(crypt_resume_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEY1, strlen(KEY1)-1), "wrong key");
	OK_(crypt_resume_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEY1, strlen(KEY1)));
	FAIL_(crypt_resume_by_passphrase(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEY1, strlen(KEY1)), "not suspended");

	OK_(_prepare_keyfile(KEYFILE1, KEY1, strlen(KEY1)));
	OK_(crypt_suspend(cd, CDEVICE_1));
	FAIL_(crypt_resume_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1 "blah", 0), "wrong keyfile");
	OK_(crypt_resume_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, 0));
	FAIL_(crypt_resume_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, 0), "not suspended");
	_remove_keyfiles();
out:
	OK_(crypt_deactivate(cd, CDEVICE_1));
	crypt_free(cd);
}

static void AddDeviceLuks(void)
{
	struct crypt_device *cd;
	struct crypt_params_luks1 params = {
		.hash = "sha512",
		.data_alignment = 2048, // 4M, data offset will be 4096
	};
	char key[128], key2[128];

	char *passphrase = "blabla";
	char *mk_hex = "bb21158c733229347bd4e681891e213d94c685be6a5b84818afe7a78a6de7a1a";
	size_t key_size = strlen(mk_hex) / 2;
	char *cipher = "aes";
	char *cipher_mode = "cbc-essiv:sha256";

	crypt_decode_key(key, mk_hex, key_size);

	OK_(crypt_init(&cd, DEVICE_2));
	OK_(crypt_format(cd, CRYPT_LUKS1, cipher, cipher_mode, NULL, key, key_size, &params));

	// even with no keyslots defined it can be activated by volume key
	OK_(crypt_volume_key_verify(cd, key, key_size));
	OK_(crypt_activate_by_volume_key(cd, CDEVICE_2, key, key_size, 0));
	EQ_(crypt_status(cd, CDEVICE_2), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_2));

	// now with keyslot
	EQ_(7, crypt_keyslot_add_by_volume_key(cd, 7, key, key_size, passphrase, strlen(passphrase)));
	EQ_(CRYPT_SLOT_ACTIVE_LAST, crypt_keyslot_status(cd, 7));
	EQ_(7, crypt_activate_by_passphrase(cd, CDEVICE_2, CRYPT_ANY_SLOT, passphrase, strlen(passphrase), 0));
	EQ_(crypt_status(cd, CDEVICE_2), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_2));

	EQ_(1, crypt_keyslot_add_by_volume_key(cd, 1, key, key_size, KEY1, strlen(KEY1)));
	OK_(_prepare_keyfile(KEYFILE1, KEY1, strlen(KEY1)));
	OK_(_prepare_keyfile(KEYFILE2, KEY2, strlen(KEY2)));
	EQ_(2, crypt_keyslot_add_by_keyfile(cd, 2, KEYFILE1, 0, KEYFILE2, 0));
	FAIL_(crypt_activate_by_keyfile(cd, CDEVICE_2, CRYPT_ANY_SLOT, KEYFILE2, strlen(KEY2)-1, 0), "key mismatch");
	EQ_(2, crypt_activate_by_keyfile(cd, NULL, CRYPT_ANY_SLOT, KEYFILE2, 0, 0));
	EQ_(2, crypt_activate_by_keyfile(cd, CDEVICE_2, CRYPT_ANY_SLOT, KEYFILE2, 0, 0));
	OK_(crypt_keyslot_destroy(cd, 1));
	OK_(crypt_keyslot_destroy(cd, 2));
	OK_(crypt_deactivate(cd, CDEVICE_2));
	_remove_keyfiles();

	FAIL_(crypt_keyslot_add_by_volume_key(cd, 7, key, key_size, passphrase, strlen(passphrase)), "slot used");
	key[1] = ~key[1];
	FAIL_(crypt_keyslot_add_by_volume_key(cd, 6, key, key_size, passphrase, strlen(passphrase)), "key mismatch");
	key[1] = ~key[1];
	EQ_(6, crypt_keyslot_add_by_volume_key(cd, 6, key, key_size, passphrase, strlen(passphrase)));
	EQ_(CRYPT_SLOT_ACTIVE, crypt_keyslot_status(cd, 6));

	FAIL_(crypt_keyslot_destroy(cd, 8), "invalid keyslot");
	FAIL_(crypt_keyslot_destroy(cd, CRYPT_ANY_SLOT), "invalid keyslot");
	FAIL_(crypt_keyslot_destroy(cd, 0), "keyslot not used");
	OK_(crypt_keyslot_destroy(cd, 7));
	EQ_(CRYPT_SLOT_INACTIVE, crypt_keyslot_status(cd, 7));
	EQ_(CRYPT_SLOT_ACTIVE_LAST, crypt_keyslot_status(cd, 6));

	EQ_(6, crypt_volume_key_get(cd, CRYPT_ANY_SLOT, key2, &key_size, passphrase, strlen(passphrase)));
	OK_(crypt_volume_key_verify(cd, key2, key_size));

	OK_(memcmp(key, key2, key_size));
	OK_(strcmp(cipher, crypt_get_cipher(cd)));
	OK_(strcmp(cipher_mode, crypt_get_cipher_mode(cd)));
	EQ_(key_size, crypt_get_volume_key_size(cd));
	EQ_(4096, crypt_get_data_offset(cd));
	OK_(strcmp(DEVICE_2, crypt_get_device_name(cd)));

	reset_log();
	crypt_set_log_callback(cd, &new_log, NULL);
	OK_(crypt_dump(cd));
	OK_(!(global_lines != 0));
	crypt_set_log_callback(cd, NULL, NULL);
	reset_log();

	FAIL_(crypt_set_uuid(cd, "blah"), "wrong UUID format");
	OK_(crypt_set_uuid(cd, DEVICE_TEST_UUID));
	OK_(strcmp(DEVICE_TEST_UUID, crypt_get_uuid(cd)));

	FAIL_(crypt_deactivate(cd, CDEVICE_2), "not active");
	crypt_free(cd);
}

static void UseTempVolumes(void)
{
	struct crypt_device *cd;
	char tmp[256];

	// Tepmporary device without keyslot but with on-disk LUKS header
	OK_(crypt_init(&cd, DEVICE_2));
	FAIL_(crypt_activate_by_volume_key(cd, CDEVICE_2, NULL, 0, 0), "not yet formatted");
	OK_(crypt_format(cd, CRYPT_LUKS1, "aes", "cbc-essiv:sha256", NULL, NULL, 16, NULL));
	OK_(crypt_activate_by_volume_key(cd, CDEVICE_2, NULL, 0, 0));
	EQ_(crypt_status(cd, CDEVICE_2), CRYPT_ACTIVE);
	crypt_free(cd);

	OK_(crypt_init_by_name(&cd, CDEVICE_2));
	OK_(crypt_deactivate(cd, CDEVICE_2));
	crypt_free(cd);

	// Dirty checks: device without UUID
	// we should be able to remove it but not manuipulate with it
	snprintf(tmp, sizeof(tmp), "dmsetup create %s --table \""
		"0 100 crypt aes-cbc-essiv:sha256 deadbabedeadbabedeadbabedeadbabe 0 "
		"%s 2048\"", CDEVICE_2, DEVICE_2);
	_system(tmp, 1);
	OK_(crypt_init_by_name(&cd, CDEVICE_2));
	OK_(crypt_deactivate(cd, CDEVICE_2));
	FAIL_(crypt_activate_by_volume_key(cd, CDEVICE_2, NULL, 0, 0), "No known device type");
	crypt_free(cd);

	// Dirty checks: device with UUID but LUKS header key fingerprint must fail)
	snprintf(tmp, sizeof(tmp), "dmsetup create %s --table \""
		"0 100 crypt aes-cbc-essiv:sha256 deadbabedeadbabedeadbabedeadbabe 0 "
		"%s 2048\" -u CRYPT-LUKS1-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-ctest1",
		 CDEVICE_2, DEVICE_2);
	_system(tmp, 1);
	OK_(crypt_init_by_name(&cd, CDEVICE_2));
	OK_(crypt_deactivate(cd, CDEVICE_2));
	FAIL_(crypt_activate_by_volume_key(cd, CDEVICE_2, NULL, 0, 0), "wrong volume key");
	crypt_free(cd);

	// No slots
	OK_(crypt_init(&cd, DEVICE_2));
	OK_(crypt_load(cd, CRYPT_LUKS1, NULL));
	FAIL_(crypt_activate_by_volume_key(cd, CDEVICE_2, NULL, 0, 0), "volume key is lost");
	crypt_free(cd);

	// Plain device
	OK_(crypt_init(&cd, DEVICE_2));
	OK_(crypt_format(cd, CRYPT_PLAIN, "aes", "cbc-essiv:sha256", NULL, NULL, 16, NULL));
	FAIL_(crypt_activate_by_volume_key(cd, NULL, "xxx", 3, 0), "cannot verify key with plain");
	FAIL_(crypt_volume_key_verify(cd, "xxx", 3), "cannot verify key with plain");
	FAIL_(crypt_activate_by_volume_key(cd, CDEVICE_2, "xxx", 3, 0), "wrong key lenght");
	OK_(crypt_activate_by_volume_key(cd, CDEVICE_2, "volumekeyvolumek", 16, 0));
	EQ_(crypt_status(cd, CDEVICE_2), CRYPT_ACTIVE);
	OK_(crypt_deactivate(cd, CDEVICE_2));
	crypt_free(cd);
}

static void HashDevicePlain(void)
{
	struct crypt_device *cd;
	struct crypt_params_plain params = {
		.hash = NULL,
		.skip = 0,
		.offset = 0,
	};

	size_t key_size;
	char *mk_hex, *keystr, key[256];

	OK_(crypt_init(&cd, DEVICE_1));
	OK_(crypt_format(cd, CRYPT_PLAIN, "aes", "cbc-essiv:sha256", NULL, NULL, 16, &params));

	// hash PLAIN, short key
	OK_(_prepare_keyfile(KEYFILE1, "tooshort", 8));
	FAIL_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, 16, 0), "not enough data in keyfile");
	_remove_keyfiles();

	// hash PLAIN, exact key
	//         0 1 2 3 4 5 6 7 8 9 a b c d e f
	mk_hex = "caffeecaffeecaffeecaffeecaffee88";
	key_size = 16;
	crypt_decode_key(key, mk_hex, key_size);
	OK_(_prepare_keyfile(KEYFILE1, key, key_size));
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, key_size, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	OK_(strcmp(key, mk_hex));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	// Limit plain key
	mk_hex = "caffeecaffeecaffeecaffeeca000000";
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, key_size - 3, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	OK_(strcmp(key, mk_hex));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	_remove_keyfiles();

	// hash PLAIN, long key
	//         0 1 2 3 4 5 6 7 8 9 a b c d e f
	mk_hex = "caffeecaffeecaffeecaffeecaffee88babebabe";
	key_size = 16;
	crypt_decode_key(key, mk_hex, key_size);
	OK_(_prepare_keyfile(KEYFILE1, key, strlen(mk_hex) / 2));
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, key_size, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	FAIL_(strcmp(key, mk_hex), "only key length used");
	OK_(strncmp(key, mk_hex, key_size));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	// Now without explicit limit
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, 0, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	FAIL_(strcmp(key, mk_hex), "only key length used");
	OK_(strncmp(key, mk_hex, key_size));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	_remove_keyfiles();

	// hash sha256
	params.hash = "sha256";
	OK_(crypt_format(cd, CRYPT_PLAIN, "aes", "cbc-essiv:sha256", NULL, NULL, 16, &params));

	//         0 1 2 3 4 5 6 7 8 9 a b c d e f
	mk_hex = "c62e4615bd39e222572f3a1bf7c2132e";
	keystr = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
	key_size = strlen(keystr); // 32
	OK_(_prepare_keyfile(KEYFILE1, keystr, strlen(keystr)));
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, key_size, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	OK_(strcmp(key, mk_hex));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	// Read full keyfile
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, 0, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	OK_(strcmp(key, mk_hex));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	_remove_keyfiles();

	// Limit keyfile read
	keystr = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxAAAAAAAA";
	OK_(_prepare_keyfile(KEYFILE1, keystr, strlen(keystr)));
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, key_size, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	OK_(strcmp(key, mk_hex));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	// Full keyfile
	OK_(crypt_activate_by_keyfile(cd, CDEVICE_1, CRYPT_ANY_SLOT, KEYFILE1, 0, 0));
	OK_(_get_key_dm(CDEVICE_1, key, sizeof(key)));
	OK_(strcmp(key, "0e49cb34a1dee1df33f6505e4de44a66"));
	OK_(crypt_deactivate(cd, CDEVICE_1));

	_remove_keyfiles();

	// FIXME: add keyfile="-" tests somehow

	crypt_free(cd);
}

// Check that gcrypt is properly initialised in format
static void NonFIPSAlg(void)
{
	struct crypt_device *cd;
	struct crypt_params_luks1 params = {0};
	char key[128] = "";
	size_t key_size = 128;
	char *cipher = "aes";
	char *cipher_mode = "cbc-essiv:sha256";
	int ret;

	OK_(crypt_init(&cd, DEVICE_2));
	params.hash = "sha256";
	OK_(crypt_format(cd, CRYPT_LUKS1, cipher, cipher_mode, NULL, key, key_size, &params));

	params.hash = "whirlpool";
	ret = crypt_format(cd, CRYPT_LUKS1, cipher, cipher_mode, NULL, key, key_size, &params);
	if (ret < 0) {
		printf("WARNING: whirlpool not supported, skipping test.\n");
		crypt_free(cd);
		return;
	}

	params.hash = "md5";
	FAIL_(crypt_format(cd, CRYPT_LUKS1, cipher, cipher_mode, NULL, key, key_size, &params),
	      "MD5 unsupported, too short");
	crypt_free(cd);
}

int main (int argc, char *argv[])
{
	int i;

	if (getuid() != 0) {
		printf("You must be root to run this test.\n");
		exit(0);
	}

	for (i = 1; i < argc; i++) {
		if (!strcmp("-v", argv[i]) || !strcmp("--verbose", argv[i]))
			_verbose = 1;
		else if (!strcmp("--debug", argv[i]))
			_debug = _verbose = 1;
	}

	_cleanup();
	if (_setup())
		goto out;

	crypt_set_debug_level(_debug ? CRYPT_DEBUG_ALL : CRYPT_DEBUG_NONE);

	RUN_(NonFIPSAlg, "Crypto is properly initialised in format"); //must be the first!
#if 0
	RUN_(LuksUUID, "luksUUID API call");
	RUN_(IsLuks, "isLuks API call");
	RUN_(LuksOpen, "luksOpen API call");
	RUN_(query_device, "crypt_query_device API call");
	RUN_(remove_device, "crypt_remove_device API call");
	RUN_(LuksFormat, "luksFormat API call");
	RUN_(LuksKeyGame, "luksAddKey, RemoveKey, KillSlot API calls");
	RUN_(DeviceResizeGame, "regular crypto, resize calls");
#endif
	RUN_(AddDevicePlain, "plain device API creation exercise");
	RUN_(HashDevicePlain, "plain device API hash test");
	RUN_(AddDeviceLuks, "Format and use LUKS device");
	RUN_(UseLuksDevice, "Use pre-formated LUKS device");
	RUN_(SuspendDevice, "Suspend/Resume test");
	RUN_(UseTempVolumes, "Format and use temporary encrypted device");

	RUN_(CallbacksTest, "API callbacks test");
out:
	_cleanup();
	return 0;
}
