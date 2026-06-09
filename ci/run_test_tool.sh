#!/bin/bash -x

TARGET_IQN="${TARGET_IQN:-iqn.2003-01.org.linux-iscsi:libiscsi:ci}"
chaps=""
[[ -n "${ISCSI_USER}${ISCSI_PASS}" ]] && chaps="${ISCSI_USER}%${ISCSI_PASS}@"

# Assume we are run from the compiled source.
# Only run Read/Write10 tests for now. They're quick and pass against LIO.
test-tool/iscsi-test-cu --dataloss --test "ALL.Read10" \
	"iscsi://${chaps}127.0.0.1:3260/${TARGET_IQN}/0" || exit 1
test-tool/iscsi-test-cu --dataloss --test "ALL.Write10" \
	"iscsi://${chaps}127.0.0.1:3260/${TARGET_IQN}/0" || exit 1
