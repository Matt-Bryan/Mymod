mymod : mymod.c
	gcc -Wall -Werror -O3 -o mymod mymod.c
	echo "mymod built!"
debug : mymod.c
	gcc -Wall -Werror -g -o mymod -DDEBUG mymod.c
	echo "debug built"
