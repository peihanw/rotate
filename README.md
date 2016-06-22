1) Purpose:

   Some ugly applications will write to stdout endlessly. Normally the start script will redirect stdout to an ‘.out’ file, but this will cause a disk space hog for a daemon process, and periodic restarts have to be done to release disk space.

2) A tool called 'rotate'

   A maintenance friendly tool called 'rotate' is developed to solve the problem. 'rotate' is inspired by Apache rotatelogs, and is enhanced with a timestamp/pid prepending function.

3) Usage & example

````
## command line usage
$ ./rotate
usage: ./rotate -o outFileNm [-s sizeLimit(MB)] [-t 1|0] [-p 1|0] [-a 1|0]
       -o : output file name
       -s : size of file toggle trigger, in MB, default '100'
       -t : timestame flag, 1:prepend timestame, 0:without timestamp, default '1'
       -p : pid flag, 1:prepend pid, 0:no pid, default '1' (pid may incorrect when app forked afterwards)
       -a : append mode, 1:append, 0:trunk, default '1'
eg.    ./rotate -o app.out -s 200

## an example
$ nohup $JAVA_HOME/bin/java ... | $APP_HOME/bin/rotate -o app.out &
$ ll -rt|grep app.out
-rw-r--r-- 1 user group 104857678 Dec 31 12:13 app.out.20151231121343
-rw-r--r-- 1 user group  11684754 Dec 31 12:20 app.out

## Apache Kafka production environment example
$ diff kafka-run-class.sh.orig kafka-run-class.sh
151c151
\<   nohup $JAVA $KAFKA_HEAP_OPTS $KAFKA_JVM_PERFORMANCE_OPTS $KAFKA_GC_LOG_OPTS $KAFKA_JMX_OPTS $KAFKA_LOG4J_OPTS -cp $CLASSPATH $KAFKA_OPTS “$@” > “$CONSOLE_OUTPUT_FILE” 2>&1 < /dev/null &
---
\>   nohup $JAVA $KAFKA_HEAP_OPTS $KAFKA_JVM_PERFORMANCE_OPTS $KAFKA_GC_LOG_OPTS $KAFKA_JMX_OPTS $KAFKA_LOG4J_OPTS -cp $CLASSPATH $KAFKA_OPTS “$@” < /dev/null | $HOME/bin/rotate -o $CONSOLE_OUTPUT_FILE &
$ head -3 kafkaServer.out
20160114123209.739334 12345 [2016-01-14 12:32:09,738] INFO Verifying properties (kafka.utils.VerifiableProperties)
20160114123209.809179 12345 [2016-01-14 12:32:09,808] INFO Property broker.id is overridden to 83 (kafka.utils.VerifiableProperties)
20160114123209.809471 12345 [2016-01-14 12:32:09,809] INFO Property controller.message.queue.size is overridden to 10 (kafka.utils.VerifiableProperties)

