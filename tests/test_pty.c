/*
 * Tests for running the pty child as another user, used to lock terminals.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../src/misc/pty.c"

static char tmpdir[] = "/tmp/kmscon-test-pty-XXXXXX";

static void fail(const char *what)
{
	perror(what);
	exit(EXIT_FAILURE);
}

static void make_file(const char *path, mode_t mode)
{
	int fd;

	unlink(path);
	fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
	if (fd < 0)
		fail("open");
	close(fd);
	if (chmod(path, mode))
		fail("chmod");
}

/* Check @path as the user @uid, or as the test itself if @uid is negative */
static bool can_exec(const char *path, long uid)
{
	char *argv[] = {(char *)path, NULL};
	struct kmscon_pty pty = {.fd = -1, .argv = path ? argv : NULL};
	bool ret;

	if (uid >= 0 && kmscon_pty_set_user(&pty, uid))
		fail("kmscon_pty_set_user");
	ret = kmscon_pty_can_exec(&pty);

	free(pty.user);
	free(pty.home);
	free(pty.shell);
	free(pty.groups);
	return ret;
}

/* Check @path as the test's own user, with @home as its home directory */
static bool can_exec_from(const char *home, const char *path)
{
	char *argv[] = {(char *)path, NULL};
	struct kmscon_pty pty = {.fd = -1, .argv = argv};
	bool ret;

	if (kmscon_pty_set_user(&pty, getuid()))
		fail("kmscon_pty_set_user");
	free(pty.home);
	pty.home = strdup(home);
	if (!pty.home)
		fail("strdup");
	ret = kmscon_pty_can_exec(&pty);

	free(pty.user);
	free(pty.home);
	free(pty.shell);
	free(pty.groups);
	return ret;
}

static void test_can_exec(void)
{
	char dir[sizeof(tmpdir) + 8], file[sizeof(tmpdir) + 16];
	struct passwd *nobody;

	snprintf(dir, sizeof(dir), "%s/dir", tmpdir);
	snprintf(file, sizeof(file), "%s/cmd", dir);
	if (mkdir(dir, 0700))
		fail("mkdir");

	/* no program, missing file, directory */
	assert(!kmscon_pty_can_exec(NULL));
	assert(!can_exec(NULL, -1));
	assert(!can_exec(file, -1));
	assert(!can_exec(dir, -1));

	if (getuid() == 0) {
		/* root: any execute bit */
		make_file(file, 0644);
		assert(!can_exec(file, 0));
		make_file(file, 0601);
		assert(can_exec(file, 0));

		/* another user: checked as that user, directories included */
		nobody = getpwnam("nobody");
		if (nobody && !chmod(tmpdir, 0711) && !chmod(dir, 0711)) {
			make_file(file, 0755);
			if (can_exec(file, nobody->pw_uid)) {
				make_file(file, 0751);
				assert(!can_exec(file, nobody->pw_uid));
				chmod(dir, 0700);
				make_file(file, 0755);
				assert(!can_exec(file, nobody->pw_uid));
			} else {
				printf("skipped other users: cannot switch to \"nobody\"\n");
			}
		}
	} else {
		/* the test's own user: no switch, as with kmscon under libseat */
		make_file(file, 0500);
		assert(can_exec(file, getuid()));
		make_file(file, 0100);
		assert(!can_exec(file, getuid()));
		make_file(file, 0077);
		assert(!can_exec(file, getuid()));

		/* a directory on the way cannot be searched */
		make_file(file, 0700);
		if (chmod(dir, 0600))
			fail("chmod");
		assert(!can_exec(file, getuid()));
		if (chmod(dir, 0700))
			fail("chmod");
	}

	/* the program is looked up from the user's home, like when it runs */
	make_file(file, 0700);
	assert(can_exec_from(dir, "cmd"));
	assert(!can_exec_from(tmpdir, "cmd"));

	unlink(file);
	rmdir(dir);
}

static void test_set_user(void)
{
	struct kmscon_pty pty = {.fd = -1};
	struct passwd *pw;
	int ret;

	assert(kmscon_pty_set_user(NULL, 0) == -EINVAL);

	/* without root, only the test's own user is possible */
	if (getuid() != 0)
		assert(kmscon_pty_set_user(&pty, 0) == -EPERM);

	pw = getpwuid(getuid());
	if (!pw)
		fail("getpwuid");
	ret = kmscon_pty_set_user(&pty, getuid());
	assert(ret == 0);
	assert(!strcmp(pty.user, pw->pw_name));
	assert(pty.uid == pw->pw_uid);
	assert(pty.gid == pw->pw_gid);
	assert(pty.home && pty.shell);
	assert(pty.ngroups >= 1 && pty.groups[0] == pw->pw_gid);

	free(pty.user);
	free(pty.home);
	free(pty.shell);
	free(pty.groups);
}

int main(void)
{
	if (!mkdtemp(tmpdir))
		fail("mkdtemp");

	test_can_exec();
	test_set_user();

	rmdir(tmpdir);
	return 0;
}
