Name:           lh-sdr-demod-fm
Version:        0.1.0
Release:        1%{?dist}
Summary:        lh-sdr-fm - Quadrature Polar Discriminator & Gram-Schmidt IQ Balance
License:        Apache-2.0
URL:            https://github.com/Logihertz-Systems-OPC-Pvt-Ltd/lh-sdr-demod-fm
Source0:        %{name}-%{version}-src.tar.gz
Packager:       Sameer Tawade <ops@logihertz.com>
BuildArch:      %{_arch}

%description
C++17 SIMD-accelerated (AVX2 & ARM NEON) Wideband & Narrowband FM quadrature polar discriminator, Gram-Schmidt IQ imbalance compensator, 50us/75us IIR de-emphasis filter, and SDR receiver posture auditor.

%prep
%autosetup -n %{name}-0.1.0

%build
# Industrial clean-room build with zero external runtime dependencies

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_docdir}/%{name}
mkdir -p %{buildroot}%{_mandir}/man1

if [ -f bin/lh-sdr-fm ]; then
    install -m 0755 bin/lh-sdr-fm %{buildroot}%{_bindir}/lh-sdr-fm
elif [ -f lh-sdr-fm ]; then
    install -m 0755 lh-sdr-fm %{buildroot}%{_bindir}/lh-sdr-fm
fi

install -m 0644 README.md %{buildroot}%{_docdir}/%{name}/README.md
install -m 0644 SPEC.md %{buildroot}%{_docdir}/%{name}/SPEC.md
install -m 0644 LICENSE %{buildroot}%{_docdir}/%{name}/LICENSE
install -m 0644 NOTICE %{buildroot}%{_docdir}/%{name}/NOTICE

%files
%doc %{_docdir}/%{name}/README.md
%doc %{_docdir}/%{name}/SPEC.md
%doc %{_docdir}/%{name}/NOTICE
%license %{_docdir}/%{name}/LICENSE

%changelog
* Mon Nov 16 2026 Sameer Tawade <ops@logihertz.com> - 0.1.0-1
- Initial production release for Logihertz Systems Ecosystem and STRUCT OS.
