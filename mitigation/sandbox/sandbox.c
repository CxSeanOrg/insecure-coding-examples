#include "fake_fork.h"

#include <errno.h>
#include <ifaddrs.h>
#include <grp.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/unistd.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsuid.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Based on Michael Kerrisk's 'The Linux Programming Interface'
 */
enum Mode {
  USER_NAMESPACE,
  NETWORK_NAMESPACE,
  PID_NAMESPACE,
  CHROOT_DETATCH,
  SECCOMP_SANDBOX,
  MODE_ERROR
};

pid_t create_user_namespace();

pid_t create_network_namespace();

pid_t create_pid_namespace();

void detatch_from_filesystem();

void restrict_syscalls();

enum Mode getMode(char *mode) {

  if (!mode)
    return MODE_ERROR;

  if (strcmp("-user", mode) == 0)
    return USER_NAMESPACE;
  if (strcmp("-network", mode) == 0)
    return NETWORK_NAMESPACE;
  if (strcmp("-pid", mode) == 0)
    return PID_NAMESPACE;
  if (strcmp("-chroot", mode) == 0)
    return CHROOT_DETATCH;
  if (strcmp("-seccomp", mode) == 0)
    return SECCOMP_SANDBOX;

  return MODE_ERROR;
}

void wait_for_child(pid_t pid) {

  if (pid == getpid()) {
    printf("* no child to wait for %d\n", getpid());
    return;
  }

  int status = -1;
  pid_t child_pid = waitpid(pid, &status, WUNTRACED | WCONTINUED);
  if (child_pid != -1) {
    printf("* child %d exited, parent exiting %d\n", child_pid, getpid());
    exit(EXIT_SUCCESS);
  } else {
    printf(
        "* waitpid returned an error when parent %d waited for child %d: %s\n",
        getpid(), pid, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

char *user_name(uid_t uid) {
  struct passwd *pwd = getpwuid(uid);
  return pwd ? pwd->pw_name : NULL;
}

char *group_name(gid_t gid) {
  struct group *grp = getgrgid(gid);
  return grp ? grp->gr_name : NULL;
}

int count_interfaces(struct ifaddrs *ifaddr) {
  int num_interfaces = 0;
  for (struct ifaddrs *ifa = ifaddr;
       ifa != NULL;
       ifa = ifa->ifa_next, num_interfaces++);
  return num_interfaces;
}

int num_network_interfaces(void) {
  struct ifaddrs *ifaddr = NULL;

  if (getifaddrs(&ifaddr) == -1) {
    printf("* getifaddrs failed\n");
    return 0;
  }

  int num_interfaces = count_interfaces(ifaddr);
  freeifaddrs(ifaddr);
  return num_interfaces;
}

void explore_state(int i) {
  printf("\n%d. Current\n", i);
  char *cwd = get_current_dir_name();

  uid_t ruid, euid, suid = 0;
  gid_t rgid, egid, sgid = 0;

  if (getresuid(&ruid, &euid, &suid) == -1)
    printf("* getresuid failed\n");

  if (getresgid(&rgid, &egid, &sgid) == -1)
    printf("* getresgid failed\n");

  uid_t fsuid = setfsuid(0);
  gid_t fsgid = setfsgid(0);

  printf("--------------------------------------\n");
  printf("pid  = %d\n", getpid());
  printf("ppid = %d\n", getppid());
  printf("UID real      (%d): %s\n", ruid, user_name(ruid));
  printf("UID effective (%d): %s\n", euid, user_name(euid));
  printf("UID saved     (%d): %s\n", suid, user_name(suid));
  printf("UID fs        (%d): %s\n", fsuid, user_name(fsuid));
  printf("GID real      (%d): %s\n", rgid, group_name(rgid));
  printf("GID effective (%d): %s\n", egid, group_name(egid));
  printf("GID saved     (%d): %s\n", sgid, group_name(sgid));
  printf("GID fs        (%d): %s\n", fsgid, group_name(fsgid));
  printf("cwd  = %s\n", cwd);
  printf("num of network interfaces: %d\n", num_network_interfaces());
  printf("--------------------------------------\n");

  free(cwd);
}

int main(int argc, char *argv[]) {

  if (argc < 2) {
    printf("Usage: ./sandbox [-user|-network|-pid|-chroot|-seccomp]\n");
  }

  for (int i = 1; i < argc; i++) {

    enum Mode mode = getMode(argv[i]);
    pid_t pid = getpid();
    explore_state(i);

    switch (mode) {
      case USER_NAMESPACE:
        pid = create_user_namespace();
        break;
      case NETWORK_NAMESPACE:
        pid = create_network_namespace();
        break;
      case PID_NAMESPACE:
        pid = create_pid_namespace();
        break;
      case CHROOT_DETATCH:
        detatch_from_filesystem();
        break;
      case SECCOMP_SANDBOX:
        restrict_syscalls();
        break;
      case MODE_ERROR:
      default:
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
      printf("Child   - child pid %d parent pid %d\n", getpid(), getppid());
    else
      printf("Parent  - child pid %d parent pid %d\n", pid, getpid());

    if (pid != 0) {
      wait_for_child(pid);
    }
  }
}

pid_t create_user_namespace() {
  printf("\n%s\n", __PRETTY_FUNCTION__);
  return fake_fork(CLONE_NEWUSER);
}

pid_t create_network_namespace() {
  printf("\n%s\n", __PRETTY_FUNCTION__);
  return fake_fork(CLONE_NEWNET);
}

pid_t create_pid_namespace() {
  printf("\n%s\n", __PRETTY_FUNCTION__);
  return fake_fork(CLONE_NEWPID);
}

void detatch_from_filesystem() {
  printf("\n%s\n", __PRETTY_FUNCTION__);
  if (!detach_in_child()) {
    printf("* failed to detach in child for pid %d\n", getpid());
    exit(EXIT_FAILURE);
  }
}

void restrict_syscalls() {
  printf("\n%s\n", __PRETTY_FUNCTION__);

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
    printf("* prctl(PR_SET_NO_NEW_PRIVS) failed\n");

  //prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);

#if 0
  struct sock_filter filter[] = {
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(
          struct seccomp_data, arch))),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS, (offsetof(
          struct seccomp_data, nr))),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_read, 1, 0),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_write, 1, 0),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_exit, 1, 0),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_rt_sigreturn, 1, 0),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL)
  };

  const struct sock_fprog prog = {
      .len = (unsigned short) (sizeof(filter) / sizeof(filter[0])),
      .filter = filter,
  };

  if (prctl(PR_SET_SECCOMP, 2, &prog) != 0)
    printf("* prctl(PR_SET_SECCOMP) failed\n");
#endif
}
