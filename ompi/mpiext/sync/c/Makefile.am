AM_CPPFLAGS = -DOMPI_PROFILE_LAYER=0

ompidir = $(ompiincludedir)/ompi/mpiext/sync/c

noinst_LTLIBRARIES = libmpiext_sync_c.la
ompi_HEADERS = mpiext_sync_c.h

libmpiext_sync_c_la_SOURCES = \
		$(ompi_HEADERS) \
		mpiext_sync.c \
		mpiext_sync_module.c \
		mpiext_sync_request.c \
		mpiext_sync_types.h

libmpiext_sync_c_la_LDFLAGS = -module -avoid-version

