#!/bin/bash

for i in `ls */*.xsd`
do
	fname=$(basename $i)
	CONST_NAME=${fname//'.'/'_'};
	cat license.header >$i.cpp
	echo "#include <string>" >>$i.cpp
	echo >>$i.cpp
	echo -n "std::string ${CONST_NAME}_str = \"" >>$i.cpp;
	cat $i | sed 's/$/ \\/g' | sed 's/"/\\"/g' >>$i.cpp
	echo -n '";' >>$i.cpp
	echo >>$i.cpp
done
