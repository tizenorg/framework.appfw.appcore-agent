
Name:       appcore-agent
Summary:    Agent Application basic
Version:    1.0
Release:    2
Group:      TO_BE/FILLED_IN
License:    SAMSUNG
Source0:    appcore-agent-%{version}.tar.gz
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  cmake
BuildRequires:  sysman-devel


%description
SLP agent application basic


%package devel
Summary:    appcore agent
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
%description devel
appcore agent (developement files)

%prep
%setup -q


%build

CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" cmake . -DCMAKE_INSTALL_PREFIX=/usr

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
/usr/lib/libappcore-agent.so.1
/usr/lib/libappcore-agent.so.1.1

%files devel
%defattr(-,root,root,-)
/usr/lib/pkgconfig/appcore-agent.pc
/usr/lib/libappcore-agent.so
/usr/include/appcore-agent/appcore-agent.h
