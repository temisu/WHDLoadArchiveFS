#!/bin/sh

# param: test file [binary]

test -f $1 || exit 1

# run this in the main directory

TESTBIN=${2:-./test}
export DESCRIPTION="$(awk '/DESCRIPTION:/{flag=1;next}/--END/{flag=0}flag' $1)"
export PARAMS="$(awk '/PARAMS:/{flag=1;next}/--END/{flag=0}flag' $1)"
export RESULT=test.result
export VERIFY=test.verify
trap 'rm -f $RESULT $VERIFY' EXIT

printf "$DESCRIPTION $2 ... "
if $TESTBIN $PARAMS > $RESULT
then
	awk '/OUTPUT:/{flag=1;next}/--END/{flag=0}flag' $1 > $VERIFY
	if cmp -s $RESULT $VERIFY
	then
		echo "Pass"
		exit 0
	else
		echo "Failed"
		echo "$TESTBIN $PARAMS"
		diff -u $VERIFY $RESULT
	fi
else
	echo "Failed"
	echo "$TESTBIN $PARAMS"
fi
exit 1

