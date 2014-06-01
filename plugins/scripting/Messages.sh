#! /bin/sh
source ../../../calligra_xgettext.sh

$EXTRACTRC *.ui *.rc >> rc.cpp || exit 11
$EXTRACTATTR --attr=collection,text --attr=collection,comment --attr=script,text --attr=script,comment scripts/*.rc >> rc.cpp || exit 11

POT_CPP=$(mktemp $podir/_cpp_XXXXXXXX.pot)
POT_PY=$(mktemp $podir/_py_XXXXXXXX.pot)
calligra_xgettext *.cpp > "${POT_CPP}"
CXG_EXTRA_ARGS="--language=Python" calligra_xgettext scripts/*.py > "${POT_PY}"
${MSGCAT} -F "${POT_CPP}" "${POT_PY}" --use-first > $podir/krossmoduleplan.pot
rm -f "${POT_CPP}" "${POT_PY}"
