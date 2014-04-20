#
# spec file for package libhal1
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

Name:           libhal1-flash
Summary:        HAL library for Flash plugin
%define         dbus_version 0.61
%define         dbus_release 1
%define         hal_info_version 20091130
Version:        0.2.0rc1
Release:        1
Url:            https://github.com/cshorler/hal-flash
License:        GPL-2.0+ or AFL-2.1
Group:          System/Daemons
AutoReqProv:    on
BuildRequires:  pkg-config libtool
BuildRequires:  dbus-1-devel >= %{dbus_version}-%{dbus_release}
#
Requires:       dbus-1 >= %{dbus_version}-%{dbus_release}
Requires:       udisks
#
Provides:       hal-flash
#
Conflicts:      hal
# Sources:
Source0:        libhal1-flash_%{version}.tar.gz
Source1:        libhal1-flash-rpmlintrc
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
#

%description
HAL is a hardware abstraction layer.  HAL is no longer used on modern
Linux systems - with the advent of tools such as UDev and UDisks the same and
improved functionality is provided by other means.

The flash plugin currently requires libhal for playback of drm content. 

This library provides a compatibility layer and minimal libhal implementation for that purpose.
This library does NOT provide a full HAL interface or daemon.

%prep
%setup -n libhal1-flash_%{version}

%build
autoreconf -i -f
%configure \
	--libexecdir=%{_prefix}/lib/hal				\
	--docdir=%{_datadir}/doc/packages/hal			\
	--disable-static \
	--with-pic
CFLAGS="${RPM_OPT_FLAGS} -fstack-protector" make %{?_smp_mflags}

%install
make DESTDIR=$RPM_BUILD_ROOT install
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/libhal.so

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files 
%defattr(-, root, root)
%{_libdir}/*hal*.so.*

%changelog
