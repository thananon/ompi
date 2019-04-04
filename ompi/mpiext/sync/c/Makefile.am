AM_CPPFLAGS = -DOMPI_PROFILE_LAYER=0

ompidir = $(ompiincludedir)/mpiext

noinst_LTLIBRARIES = libmpiext_sync_c.la
ompi_HEADERS = mpiext_sync_c.h

libmpiext_sync_c_la_SOURCES = \
		$(ompi_HEADERS) \
		mpiext_sync.c

libmpiext_sync_c_la_LDFLAGS = -module -avoid-version

