#
# spec file for package hal-flash
#
# Copyright (c) 2014 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Name:           hal-flash
Summary:        HAL library for Flash plugin
License:        GPL-2.0+ or AFL-2.1
Group:          System/Daemons
%define         dbus_version 0.61
%define         dbus_release 1
Version:        0.3.0
Release:        0
Url:            https://github.com/cshorler/hal-flash
BuildRequires:  dbus-1-devel >= %{dbus_version}-%{dbus_release}
BuildRequires:  glib2-devel
BuildRequires:  libtool
BuildRequires:  pkg-config
#
Requires:       dbus-1 >= %{dbus_version}-%{dbus_release}
Requires:       udisks2
#
Provides:       hal-flash
#
Conflicts:      hal
# Sources:
Source0:        hal-flash_%{version}.tar.bz2
Source1:        libhal1-flash-rpmlintrc
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
#

%description
HAL is a hardware abstraction layer.  HAL is no longer used on modern
Linux systems - with the advent of tools such as UDev and UDisks the same and
improved functionality is provided by other means.  This is a compatibility
wrapper.

%package -n libhal1-flash
Summary:        Shared library for Flash hardware identification
Group:          System/Daemons

%description -n libhal1-flash
The Flash plugin currently requires libhal for playback of drm content. 

This library provides a compatibility layer and minimal libhal implementation for that purpose.
This library does NOT provide a full HAL interface or daemon.

%prep
%setup -n hal-flash_%{version}

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

%post -n libhal1-flash -p /sbin/ldconfig

%postun -n libhal1-flash -p /sbin/ldconfig

%files -n libhal1-flash
%defattr(-, root, root)
%{_libdir}/*hal*.so.*

%changelog
