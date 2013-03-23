#
# spec file for package hal
#
# Copyright (c) 2011 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# norootforbuild

Name:           libhal1
Summary:        HAL library for Flash plugin
%define         dbus_version 0.61
%define         dbus_release 1
%define         hal_info_version 20091130
Version:        0.1
Release:        1
Url:            https://github.com/cshorler/hal-flash
License:        GPL-2.0+
Group:          System/Daemons
AutoReqProv:    on
BuildRequires:  fdupes pkg-config libtool
BuildRequires:  dbus-1-glib-devel glib2-devel
#
Requires:       dbus-1 >= %{dbus_version}-%{dbus_release}
Requires:       dbus-1-glib >= %{dbus_version}-%{dbus_release}
Requires:       udisks
#
Provides:       libhal-flash
# Sources:
Source0:        hal-flash-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
#

%description
HAL is a hardware abstraction layer.  HAL is no longer used on modern
Linux systems - with the advent of tools such as UDev and UDisks the same and
improved functionality is provided by other means.

The Flash plugin has not been updated by Adobe to use the more modern tools for
Linux.  The plugin uses these to playback drm content.  

This library provides a compatibility layer for that purpose.
This library does NOT provide a full HAL interface or daemon.

%prep
%setup -n hal-flash-%{version}

%build
libtoolize -c
autoreconf -i
%configure \
	--libexecdir=%{_prefix}/lib/hal				\
	--with-os-type=suse 					\
	--docdir=%{_datadir}/doc/packages/hal			\
	--disable-static \
	--with-pic
CFLAGS="${RPM_OPT_FLAGS} -fstack-protector" make %{?_smp_mflags}

%install
%makeinstall
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/libhal.so
%fdupes %{buildroot}

%clean
rm -rf %{buildroot}

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files 
%defattr(-, root, root)
%{_libdir}/*hal*.so.*

%changelog
