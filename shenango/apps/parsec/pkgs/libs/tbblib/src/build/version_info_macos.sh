#!/bin/sh
#
# Copyright 2005-2010 Intel Corporation.  All Rights Reserved.
#
# This file is part of Threading Building Blocks.
#
# Threading Building Blocks is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# Threading Building Blocks is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Threading Building Blocks; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As a special exception, you may use this file as part of a free software
# library without restriction.  Specifically, if other files instantiate
# templates or use macros or inline functions from this file, or you compile
# this file and link it with other files to produce an executable, this
# file does not by itself cause the resulting executable to be covered by
# the GNU General Public License.  This exception does not however
# invalidate any other reasons why the executable file might be covered by
# the GNU General Public License.

# Script used to generate version info string
echo "#define __TBB_VERSION_STRINGS \\"
echo '"TBB:' "BUILD_HOST\t\t"`hostname -s`" ("`arch`")"'" ENDL \'
echo '"TBB:' "BUILD_OS\t\t"`sw_vers -productName`" version "`sw_vers -productVersion`'" ENDL \'
echo '"TBB:' "BUILD_KERNEL\t"`uname -v`'" ENDL \'
echo '"TBB:' "BUILD_GCC\t\t"`gcc -v </dev/null 2>&1 | grep 'version'`'" ENDL \'
[ -z "$COMPILER_VERSION" ] || echo '"TBB:' "BUILD_COMPILER\t"$COMPILER_VERSION'" ENDL \'
echo '"TBB:' "BUILD_TARGET\t$arch on $runtime"'" ENDL \'
echo '"TBB:' "BUILD_COMMAND\t"$*'" ENDL \'
echo ""
echo "#define __TBB_DATETIME \""`date -u`"\""
