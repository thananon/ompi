#
# Copyright (c) 2004-2007 The Trustees of Indiana University.
#                         All rights reserved.
# Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
#                         All rights reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2017      IBM Corporation.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

CFLAGS = $(crs_blcr_CFLAGS)
AM_CPPFLAGS = $(crs_blcr_CPPFLAGS)

dist_opaldata_DATA = help-opal-crs-blcr.txt

sources = \
        crs_blcr.h \
        crs_blcr_component.c \
        crs_blcr_module.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_opal_crs_blcr_DSO
component_noinst  =
component_install = mca_crs_blcr.la
else
component_noinst  = libmca_crs_blcr.la
component_install =
endif

mcacomponentdir = $(opallibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_crs_blcr_la_SOURCES  = $(sources)
mca_crs_blcr_la_LDFLAGS  = -module -avoid-version $(crs_blcr_LDFLAGS)
mca_crs_blcr_la_LIBADD = $(top_builddir)/opal/lib@OPAL_LIB_PREFIX@open-pal.la \
	$(crs_blcr_LIBS)

noinst_LTLIBRARIES = $(component_noinst)
libmca_crs_blcr_la_SOURCES = $(sources)
libmca_crs_blcr_la_LDFLAGS = -module -avoid-version $(crs_blcr_LDFLAGS)
libmca_crs_blcr_la_LIBADD  = $(crs_blcr_LIBS)
