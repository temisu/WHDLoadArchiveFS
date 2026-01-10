#!/bin/bash

# param: test file

test -f $1 || exit 1

# run this in the main directory

export DESCRIPTION="$(awk '/DESCRIPTION:/{flag=1;next}/--END/{flag=0}flag' $1)"
echo -n "$DESCRIPTION ... "
export PARAMS="$(awk '/PARAMS:/{flag=1;next}/--END/{flag=0}flag' $1)"
export RESULT=$(mktemp)
echo -n "(./test $PARAMS) "
./test $PARAMS >> $RESULT || {
	echo "Failed"
	echo "./test $PARAMS"
	rm $RESULT
	exit 1
}
export VERIFY=$(mktemp)
awk '/OUTPUT:/{flag=1;next}/--END/{flag=0}flag' $1 >> $VERIFY
test -z "$(diff -q $RESULT $VERIFY)" || {
	echo "Failed"
	echo "./test $PARAMS"
	diff -u $VERIFY $RESULT
	rm $RESULT
	rm $VERIFY
	exit 1
}
echo "Pass"
rm $RESULT
rm $VERIFY
