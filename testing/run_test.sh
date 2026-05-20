#!/bin/sh

# param: test file

test -f $1 || exit 1

# run this in the main directory

export DESCRIPTION="$(awk '/DESCRIPTION:/{flag=1;next}/--END/{flag=0}flag' $1)"
export PARAMS="$(awk '/PARAMS:/{flag=1;next}/--END/{flag=0}flag' $1)"
export RESULT=test.result
export VERIFY=test.verify
trap 'rm -f $RESULT $VERIFY' EXIT

echo -n "$DESCRIPTION ... "
#echo -n "(./test $PARAMS) "
if ./test $PARAMS > $RESULT
then
	awk '/OUTPUT:/{flag=1;next}/--END/{flag=0}flag' $1 > $VERIFY
	if cmp -s $RESULT $VERIFY
	then
		echo "Pass"
		exit 0
	else
		echo "Failed"
		echo "./test $PARAMS"
		diff -u $VERIFY $RESULT
	fi
else
	echo "Failed"
	echo "./test $PARAMS"
fi
exit 1

