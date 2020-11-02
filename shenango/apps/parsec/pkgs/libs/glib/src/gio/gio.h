/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef __G_IO_H__
#define __G_IO_H__

#define __GIO_GIO_H_INSIDE__

#include <gio/giotypes.h>

#include <gio/gappinfo.h>
#include <gio/gasyncresult.h>
#include <gio/gasyncinitable.h>
#include <gio/gbufferedinputstream.h>
#include <gio/gbufferedoutputstream.h>
#include <gio/gcancellable.h>
#include <gio/gcontenttype.h>
#include <gio/gconverter.h>
#include <gio/gconverterinputstream.h>
#include <gio/gconverteroutputstream.h>
#include <gio/gcharsetconverter.h>
#include <gio/gdatainputstream.h>
#include <gio/gdataoutputstream.h>
#include <gio/gdrive.h>
#include <gio/gemblemedicon.h>
#include <gio/gfile.h>
#include <gio/gfileattribute.h>
#include <gio/gfileenumerator.h>
#include <gio/gfileicon.h>
#include <gio/gfileinfo.h>
#include <gio/gfileinputstream.h>
#include <gio/gfilemonitor.h>
#include <gio/gfilenamecompleter.h>
#include <gio/gfileoutputstream.h>
#include <gio/gfilterinputstream.h>
#include <gio/gfilteroutputstream.h>
#include <gio/gicon.h>
#include <gio/ginetaddress.h>
#include <gio/ginetsocketaddress.h>
#include <gio/ginputstream.h>
#include <gio/gioenums.h>
#include <gio/gioenumtypes.h>
#include <gio/gioerror.h>
#include <gio/giomodule.h>
#include <gio/ginitable.h>
#include <gio/gioscheduler.h>
#include <gio/giostream.h>
#include <gio/gfileiostream.h>
#include <gio/gloadableicon.h>
#include <gio/gmemoryinputstream.h>
#include <gio/gmemoryoutputstream.h>
#include <gio/gmount.h>
#include <gio/gmountoperation.h>
#include <gio/gnativevolumemonitor.h>
#include <gio/gnetworkaddress.h>
#include <gio/gnetworkservice.h>
#include <gio/goutputstream.h>
#include <gio/gresolver.h>
#include <gio/gseekable.h>
#include <gio/gsimpleasyncresult.h>
#include <gio/gsocket.h>
#include <gio/gsocketaddress.h>
#include <gio/gsocketaddressenumerator.h>
#include <gio/gsocketclient.h>
#include <gio/gsocketconnectable.h>
#include <gio/gsocketconnection.h>
#include <gio/gsocketcontrolmessage.h>
#include <gio/gsocketlistener.h>
#include <gio/gsocketservice.h>
#include <gio/gtcpconnection.h>
#include <gio/gthreadedsocketservice.h>
#include <gio/gsrvtarget.h>
#include <gio/gthemedicon.h>
#include <gio/gvfs.h>
#include <gio/gvolume.h>
#include <gio/gvolumemonitor.h>
#include <gio/gzlibcompressor.h>
#include <gio/gzlibdecompressor.h>

#undef __GIO_GIO_H_INSIDE__

#endif /* __G_IO_H__ */
