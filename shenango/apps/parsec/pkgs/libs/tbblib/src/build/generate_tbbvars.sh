#!/bin/bash
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

# Script used to generate tbbvars.[c]sh scripts
bin_dir="$PWD"  # 
cd "$tbb_root"  # keep this comments here
tbb_root="$PWD" # to make it unsensible
cd "$bin_dir"   # to EOL encoding
[ "`uname`" = "Darwin" ] && dll_path="DYLD_LIBRARY_PATH" || dll_path="LD_LIBRARY_PATH" #
[ -f ./tbbvars.sh ] || cat >./tbbvars.sh <<EOF
#!/bin/bash
export TBB30_INSTALL_DIR="${tbb_root}" #
tbb_bin="${bin_dir}" #
if [ -z "\$CPATH" ]; then #
    export CPATH="\${TBB30_INSTALL_DIR}/include" #
else #
    export CPATH="\${TBB30_INSTALL_DIR}/include:\$CPATH" #
fi #
if [ -z "\$LIBRARY_PATH" ]; then #
    export LIBRARY_PATH="\${tbb_bin}" #
else #
    export LIBRARY_PATH="\${tbb_bin}:\$LIBRARY_PATH" #
fi #
if [ -z "\$${dll_path}" ]; then #
    export ${dll_path}="\${tbb_bin}" #
else #
    export ${dll_path}="\${tbb_bin}:\$${dll_path}" #
fi #
${TBB_CUSTOM_VARS_SH} #
EOF
[ -f ./tbbvars.csh ] || cat >./tbbvars.csh <<EOF
#!/bin/csh
setenv TBB30_INSTALL_DIR "${tbb_root}" #
setenv tbb_bin "${bin_dir}" #
if (! \$?CPATH) then #
    setenv CPATH "\${TBB30_INSTALL_DIR}/include" #
else #
    setenv CPATH "\${TBB30_INSTALL_DIR}/include:\$CPATH" #
endif #
if (! \$?LIBRARY_PATH) then #
    setenv LIBRARY_PATH "\${tbb_bin}" #
else #
    setenv LIBRARY_PATH "\${tbb_bin}:\$LIBRARY_PATH" #
endif #
if (! \$?${dll_path}) then #
    setenv ${dll_path} "\${tbb_bin}" #
else #
    setenv ${dll_path} "\${tbb_bin}:\$${dll_path}" #
endif #
${TBB_CUSTOM_VARS_CSH} #
EOF
