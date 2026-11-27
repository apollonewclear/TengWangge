Name:           tengwangge
Version:        0.1.0
Release:        1%{?dist}
Summary:        滕王阁（TengWangge）可扩展桌面工具箱平台

License:        Proprietary
URL:            https://github.com/apollonewclear/TengWangge
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++ >= 10
BuildRequires:  cmake >= 3.16
BuildRequires:  make
BuildRequires:  qt5-qtbase-devel >= 5.15

Requires:       qt5-qtbase >= 5.15
Requires:       qt5-qtbase-gui >= 5.15

%description
面向麒麟 KylinSec / openEuler 桌面的“外壳 + 多工具模块”可扩展工具箱平台，
提供统一两层导航、浅色工业工作台主题、RAII 导航锁、多线程任务运行器与滚动消息日志。

%prep
%autosetup -n %{name}-%{version}

%build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DTENGWANGGE_PREFER_SYSTEM_QT=ON \
    -DTENGWANGGE_WARNINGS_AS_ERRORS=ON \
    -DTENGWANGGE_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"

%check
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

%install
DESTDIR=%{buildroot} cmake --install build

%files
%{_bindir}/TengWangge
%{_datadir}/applications/TengWangge.desktop
%{_datadir}/icons/hicolor/scalable/apps/tengwangge.svg
%doc %{_docdir}/TengWangge/README.md

%changelog
* Fri Nov 27 2026 TengWangge Team <dev@example.com> - 0.1.0-1
- Initial KylinSec OS 3.5 verified release package
