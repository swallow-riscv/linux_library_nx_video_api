Name:    nx-video-api
Version: 1.0.2
Release: 1
License: LGPLv2+
Summary: Nexell video APIs
Group: Development/Libraries
Source:  %{name}-%{version}.tar.gz

BuildRequires:  pkgconfig automake autoconf libtool
BuildRequires:	libdrm-devel
Requires: libdrm

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Nexell video APIs

%package devel
Summary: Nexell video API development
Group: Development/Libraries
License: Apache 2.0
Requires: %{name} = %{version}-%{release}

%description devel
Nexell scaler (devel)

%prep
%setup -q

%build
autoreconf -v --install || exit 1
%configure
make %{?_smp_mflags}

%postun -p /sbin/ldconfig

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

find %{buildroot} -type f -name "*.la" -delete

%files
%{_libdir}/libnx_video_api.so
%{_libdir}/libnx_video_api.so.*
%license LICENSE.LGPLv2+

%files devel
%{_includedir}/*
%license LICENSE.LGPLv2+
