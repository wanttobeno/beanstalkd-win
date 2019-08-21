#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([beanstalkd], [1.0])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ sys/socket.h sys/time.h unistd.h ])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_WAIT3
AC_CHECK_FUNCS([atexit clock_gettime dup2 floor ftruncate gettimeofday memchr memmove memset mkdir rmdir select socket strerror strspn strstr strtol strtoul strtoull uname kqueue])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
