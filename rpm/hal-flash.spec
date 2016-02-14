#
# spec file for package hal-flash
#
# Copyright (c) 2016 SUSE LINUX GmbH, Nuernberg, Germany.
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
Version:        0.3.3
Release:        0
Url:            https://github.com/cshorler/hal-flash
BuildRequires:  dbus-1-devel
BuildRequires:  glib2-devel
BuildRequires:  libtool
BuildRequires:  pkg-config
#
Requires:       glib2
Requires:       libdbus-1-3
Requires:       udisks2
#
#
# Sources:
Source0:        hal-flash-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
#

%description
HAL is a hardware abstraction layer.  HAL is no longer used on modern
Linux systems - with the advent of tools such as UDev and UDisks the same and
improved functionality is provided by other means.  This is a compatibility
wrapper.

%package -n libhal1
Summary:        Shared library for Flash hardware identification
Group:          System/Daemons
Provides:       hal-flash
Conflicts:      hal

%description -n libhal1
The Flash plugin currently requires libhal for playback of drm content. 

This library provides a compatibility layer and minimal libhal implementation for that purpose.
This library does NOT provide a full HAL interface or daemon.

%prep
%setup -n hal-flash-%{version}

%build
%configure --disable-static --with-pic
CFLAGS="${RPM_OPT_FLAGS} -fstack-protector" make %{?_smp_mflags}

%install
make DESTDIR=$RPM_BUILD_ROOT install
rm -f $RPM_BUILD_ROOT%{_libdir}/*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/libhal.so

%clean
rm -rf %{buildroot}

%post -n libhal1 -p /sbin/ldconfig

%postun -n libhal1 -p /sbin/ldconfig

%files -n libhal1
%defattr(-, root, root)
%{_libdir}/*hal*.so.*
%doc README FAQ.txt COPYING

%changelog
