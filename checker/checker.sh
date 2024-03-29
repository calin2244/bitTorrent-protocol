#!/bin/bash

correct=0
total=0

# show final score
function show_score {
	echo "Total: $total/4"
}

function run_timeout {
    { time -p sh -c "timeout 20 $1" ; } &> time.txt
    ret=$?

    if [ $ret == 124 ]
    then
        echo "W: Programul a durat mai mult de 20 de secunde"
    elif [ $ret != 0 ]
    then
        echo "W: Rularea nu s-a putut executa cu succes"
        cat time.txt | sed '$d' | sed '$d' | sed '$d'
    fi

    rm -rf time.txt
}

# se compara doua fisiere (parametri: fisier1 fisier2)
function compare_files {
    ok=0

    diff -q -w $1 $2
    if [ $? == 0 ]
    then
        correct=$((correct+1))
    else
        echo "W: Exista diferente intre fisierele $1 si $2"
    fi
}

function test1 {
	echo "Running test 1..."
	cp tests/test1/* .
	correct=0
	run_timeout "mpirun --oversubscribe -np 4 ./bitprot"
	compare_files client1_file3 out3.txt
	compare_files client2_file1 out1.txt
	compare_files client3_file1 out1.txt
	compare_files client3_file2 out2.txt
	compare_files client3_file3 out3.txt
	if [ $correct == 5 ]
	then
	    total=$((total+1))
	    echo "OK"
	else
		echo "Test 1 failed"
	fi
	rm -rf client*_file*
	rm -rf in*txt
	rm -rf out*txt
	echo ""
}

function test2 {
	echo "Running test 2..."
	cp tests/test2/* .
	correct=0
	run_timeout "mpirun --oversubscribe -np 6 ./bitprot"
	compare_files client1_file7 out7.txt
	compare_files client2_file6 out6.txt
	compare_files client3_file4 out4.txt
	compare_files client4_file2 out2.txt
	compare_files client5_file1 out1.txt
	compare_files client5_file4 out4.txt
	compare_files client5_file5 out5.txt
	if [ $correct == 7 ]
	then
	    total=$((total+1))
	    echo "OK"
	else
		echo "Test 2 failed"
	fi
	rm -rf client*_file*
	rm -rf in*txt
	rm -rf out*txt
	echo ""
}

function test3 {
	echo "Running test 3..."
	cp tests/test3/* .
	correct=0
	run_timeout "mpirun --oversubscribe -np 5 ./bitprot"
	compare_files client1_file2 out2.txt
	compare_files client2_file1 out1.txt
	compare_files client3_file1 out1.txt
	compare_files client4_file2 out2.txt
	if [ $correct == 4 ]
	then
	    total=$((total+1))
	    echo "OK"
	else
		echo "Test 3 failed"
	fi
	rm -rf client*_file*
	rm -rf in*txt
	rm -rf out*txt
	echo ""
}

function test4 {
	echo "Running test 4..."
	cp tests/test4/* .
	correct=0
	run_timeout "mpirun --oversubscribe -np 7 ./bitprot"
	compare_files client3_file1 out1.txt
	compare_files client4_file1 out1.txt
	compare_files client5_file1 out1.txt
	compare_files client6_file1 out1.txt
	if [ $correct == 4 ]
	then
	    total=$((total+1))
	    echo "OK"
	else
		echo "Test 4 failed"
	fi
	rm -rf client*_file*
	rm -rf in*txt
	rm -rf out*txt
	echo ""
}

# printeaza informatii despre rulare
echo "TESTING"
date

export OMPI_ALLOW_RUN_AS_ROOT=1
export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

# se compileaza tema
cd ../src
make clean &> /dev/null
make build &> build.txt

if [ ! -f bitprot ]
then
    echo "E: Couldn't compile."
    cat build.txt
    show_score
    rm -rf build.txt
    exit
fi

rm -rf build.txt

mv bitprot ../checker
cd ../checker

echo ""
test1
test2
test3
test4

make clean &> /dev/null

show_score