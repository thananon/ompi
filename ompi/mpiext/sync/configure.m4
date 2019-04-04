# -*- shell-script -*-
#
# Copyright (c) 2004-2010 The Trustees of Indiana University.
#                         All rights reserved.
# Copyright (c) 2012-2015 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2015      Intel, Inc. All rights reserved.
# Copyright (c) 2015      NVIDIA, Inc.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# OMPI_MPIEXT_sync_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([OMPI_MPIEXT_sync_CONFIG],[
    AC_CONFIG_FILES([ompi/mpiext/sync/Makefile])
    AC_CONFIG_FILES([ompi/mpiext/sync/c/Makefile])
    AC_CONFIG_HEADER([ompi/mpiext/sync/c/mpiext_sync_c.h])

    AC_DEFINE_UNQUOTED([MPIX_SYNC_AWARE_SUPPORT],[$SYNC_SUPPORT],
                       [Macro that is set to 1 when SYNC-aware support is configured in and 0 when it is not])

    # We compile this whether SYNC support was requested or not. It allows
    # us to to detect if we have SYNC support.
    AS_IF([test "$ENABLE_sync" = "1" || \
           test "$ENABLE_EXT_ALL" = "1"],
          [$1],
          [$2])
])
