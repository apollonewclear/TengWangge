#!/usr/bin/env bash
# 在麒麟 KylinSec / openEuler 环境构建 RPM 安装包（需预先安装 rpm-build）。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERSION="0.1.0"
RPM_TOP="${RPM_TOPDIR:-${HOME}/rpmbuild}"

if ! command -v rpmbuild >/dev/null 2>&1; then
    echo "错误：未找到 rpmbuild，请先执行 sudo dnf install rpm-build" >&2
    exit 1
fi

mkdir -p "${RPM_TOP}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
cp "${SCRIPT_DIR}/tengwangge.spec" "${RPM_TOP}/SPECS/"

TARBALL="${RPM_TOP}/SOURCES/tengwangge-${VERSION}.tar.gz"
if [ -d "${ROOT_DIR}/.git" ] && command -v git >/dev/null 2>&1; then
    git -C "${ROOT_DIR}" archive --format=tar.gz --prefix="tengwangge-${VERSION}/" -o "${TARBALL}" HEAD
else
    tar -czf "${TARBALL}" \
        --exclude='./build*' \
        --exclude='./.git' \
        --transform="s,^\.,tengwangge-${VERSION}," \
        -C "${ROOT_DIR}" .
fi

rpmbuild --define "_topdir ${RPM_TOP}" -ba "${RPM_TOP}/SPECS/tengwangge.spec"
echo "RPM 构建完成：$(ls -1 "${RPM_TOP}"/RPMS/*/*tengwangge*.rpm)"
