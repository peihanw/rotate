1) Purpose:

   Some ugly applications will write to stdout endlessly. Normally the start script will redirect stdout to an ‘.out’ file, but this will cause a disk space hog for a daemon process, and periodic restarts have to be done to release disk space.

2) A tool called 'rotate'

   A maintenance friendly tool called 'rotate' is developed to solve the problem. 'rotate' is inspired by Apache rotatelogs, and is enhanced with a timestamp prepending function.

3) Usage & example
````
$ ./rotate
usage: ./rotate -o outFileNm [-s sizeLimit(MB)] [-t 0|1] [-a 1|0]
       -o : output file name
       -s : size of file toggle trigger, in MB, default '100'
       -t : timestame flag, 0:without timestamp, 1:prepend timestamp, default '0'
       -a : append mode, 1:append, 0:trunk, default '1'
eg.    ./rotate -o app.out -s 200 -t 1

$ nohup $JAVA_HOME/bin/java ... | $APP_HOME/bin/rotate -o app.out -t 1 &

$ ll -rt|grep app.out
-rw-r--r-- 1 user group 104857678 Dec 31 12:13 app.out.20151231121343
-rw-r--r-- 1 user group  11684754 Dec 31 12:20 app.out

