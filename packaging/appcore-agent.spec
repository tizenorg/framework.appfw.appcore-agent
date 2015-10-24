Name:       appcore-agent
Summary:    Agent Application basic
Version:    1.0.4
Release:    1
Group:      TO_BE/FILLED_IN
License:    Apache-2.0
Source0:    appcore-agent-%{version}.tar.gz
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(capi-appfw-app-control)
BuildRequires:  pkgconfig(capi-appfw-app-common)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(vconf-internal-keys)
BuildRequires:  pkgconfig(appcore-common)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(bundle)
BuildRequires:  cmake


%description
SLP agent application basic

%if "%{?tizen_profile_name}" == "wearable"
%define appfw_feature_background_management 1
%else
%if "%{?tizen_profile_name}" == "mobile"
%define appfw_feature_background_management 1
%else
%if "%{?tizen_profile_name}" == "tv"
%define appfw_feature_background_management 0
%endif
%endif
%endif

%package devel
Summary:    appcore agent
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
%description devel
appcore agent (developement files)

%package -n capi-appfw-service-application-devel
Summary:    service appliation
Group:      Development/Libraries
Requires:    appcore-agent-devel = %{version}-%{release}
%description -n capi-appfw-service-application-devel
service application (developement files)

%prep
%setup -q


%build
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif

%if 0%{?appfw_feature_background_management}
_APPFW_FEATURE_BACKGROUND_MANAGEMENT=ON
%endif

export CFLAGS="$CFLAGS -Wall -Werror -Wno-unused-function -Wno-unused-but-set-variable"
CFLAGS="$CFLAGS"
LDFLAGS="$LDFLAGS"

cmake -DCMAKE_INSTALL_PREFIX=/usr \
	-D_APPFW_FEATURE_BACKGROUND_MANAGEMENT:BOOL=${_APPFW_FEATURE_BACKGROUND_MANAGEMENT} \
	.

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/usr/lib/pkgconfig
cp capi-appfw-service-application.pc %{buildroot}/usr/lib/pkgconfig
mkdir -p %{buildroot}/usr/share/license
cp LICENSE %{buildroot}/usr/share/license/%{name}


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest appcore-agent.manifest
%defattr(-,root,root,-)
/usr/lib/libappcore-agent.so
/usr/lib/libappcore-agent.so.1
/usr/lib/libappcore-agent.so.1.1
/usr/share/license/%{name}

%files devel
%defattr(-,root,root,-)
/usr/lib/pkgconfig/appcore-agent.pc
/usr/include/appcore-agent/appcore-agent.h

%files -n capi-appfw-service-application-devel
/usr/include/appcore-agent/service_app.h
/usr/include/appcore-agent/service_app_extension.h
/usr/lib/pkgconfig/capi-appfw-service-application.pc

