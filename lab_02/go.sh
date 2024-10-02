#!/bin/bash
progs="./prog_?.c"
for prog in $progs; do
	cur_prog=$(basename "$prog" .c)
	gcc $prog -std=c99 -o $cur_prog
done

tasks="./task_??.c"
for task in $tasks; do
    cur_task=$(basename "$task" .c)
	exe=$(echo "$cur_task"".exe")
	echo -e "\033[32m""$cur_task""\033[0m"
    gcc $task -std=c99 -o $exe
	./$exe
	if [ "$cur_task" = "task_01" ]; then
		echo
		sleep 4
	fi
done

rm *.exe
