Name:    video_api_test
Version: 0.0.1
Release: 0
License: Apache 2.0
Summary: Nexell Video API Test
Group: Development/Appcliation
Source:  %{name}-%{version}.tar.gz

BuildRequires:  pkgconfig automake autoconf libtool
BuildRequires:	glibc-devel
BuildRequires:	libstdc++-devel
BuildRequires:	ffmpeg-devel
BuildRequires:	libdrm-devel
BuildRequires:	nx-video-api-devel

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Nexell Video API Test

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
%{_bindir}/video_api_test
