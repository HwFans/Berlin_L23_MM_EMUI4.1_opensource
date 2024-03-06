AM_CPPFLAGS = ${libuuid_CFLAGS} -I$(top_srcdir)/include
AM_CFLAGS = -Wall -DWITH_BLKDISCARD
sbin_PROGRAMS = resize.f2fs_s
resize_f2fs_SOURCES = f2fs_resize_main.c f2fs_resize.c f2fs_resize.h $(top_srcdir)/include/f2fs_fs.h
resize_f2fs_LDADD = ${libuuid_LIBS} $(top_builddir)/lib/libf2fs.la
